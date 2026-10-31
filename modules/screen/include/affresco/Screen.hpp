#pragma once

/// @file
/// @brief Screen: a live UI described in the AffrescoUI Screen Format, with all the modules put together.

#include "affresco/core/CallProgress.hpp"
#include "affresco/core/Channel.hpp"
#include "affresco/core/DataModel.hpp"
#include "affresco/core/Executor.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/net/Client.hpp"
#include "affresco/net/RemotePeers.hpp"
#include "affresco/net/SignIn.hpp"
#include "affresco/render/Application.hpp"
#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/Embedding.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/GpuSurface.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/Window.hpp"
#include "affresco/format/App.hpp"
#include "affresco/format/Features.hpp"
#include "affresco/services/ServiceRegistry.hpp"

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::audio {
class Engine;
class Source;
} // namespace affresco::audio

namespace affresco::data {
class Store;
} // namespace affresco::data

namespace affresco {

/// @brief A malformed screen file; what() names the place, e.g. "root.children[1].anchors: unknown edge 'lefft'".
using ScreenError = DocumentError;

/// @brief A live UI described in the AffrescoUI Screen Format (JSON, see SCREEN_FORMAT.md), built into a Window.
///
/// The composition root of the library: format::Rules (DocumentRules), vcs::Repository (VersionControl),
/// format::Runtime (panels, reconciliation, actions) and one channel::Session per peer, talking only through the
/// interfaces in affresco/core. Use the modules directly to put them together differently. The window must outlive the
/// Screen; a Screen destroyed first takes back its timers, deferred tasks and signal connections, so the window can
/// go on receiving input (its panels stay, inert).
class Screen {
public:
	using Action = std::function<void(Panel& sender)>; ///< A native action.
	/// A native function for "call" steps: gets the step's "with" as JSON text, returns what "into" stores as JSON
	/// text. Throwing std::exception runs the step's "onError" steps.
	using Call = std::function<std::string(const std::string& argumentsJson)>;
	/// A slow native function for "call" steps, run on a worker thread: like Call, and it may report progress and
	/// partial results (JSON values) and should stop soon once cancelled, both through @p call.
	using AsyncCall = std::function<std::string(const std::string& argumentsJson, CallProgress& call)>;
	using Factory = std::function<std::unique_ptr<Panel>()>; ///< Creates a custom type.
	/// Applies a custom type's own properties to one of its panels: the node's evaluated values for the names given at
	/// registration, as a JSON object in text (only those it has; colours as written, see resolveColor()). Called when
	/// they change, so a name that is missing means its default. Throwing std::exception reports the error at the node.
	using ApplyProperties = std::function<void(Panel& panel, const std::string& propertiesJson)>;
	using UnhandledAction = std::function<void(const std::string& action, Panel& sender)>; ///< Fallback handler.
	using ProposalInfo = affresco::ProposalInfo;                                           ///< An open proposal.
	using CommitInfo = affresco::CommitInfo;                                               ///< A commit.

	/// @brief How fromFile() keeps a screen's version-control history.
	enum class History {
		OnDisk,  ///< In the data folder, so that it survives a restart (VERSION_CONTROL.md, "History on disk").
		InMemory ///< In memory only, e.g. for a one-frame screenshot, which shows the file as it is.
	};

	/// @brief The folder fromFile() keeps screens' data in: $AFFRESCOUI_DATA_DIR if it is set, else ~/.affrescoui
	/// (%APPDATA%\\affrescoui on Windows).
	/// Give each parallel session its own (like the socket folder), so that their histories never meet.
	/// @return The folder.
	[[nodiscard]] static std::filesystem::path defaultDataDirectory();

	/// @brief Loads a screen file, with its history and "persist" state from defaultDataDirectory()
	/// (setHistoryDirectory(), setStateFile()). The entry screen of an app is loaded as the app (appOf()).
	/// @param path The file; relative font and "include" paths are resolved from its directory.
	/// @return The screen, not yet built.
	/// @throws ScreenError If the file can't be read, is not a valid screen, or its history on disk is damaged.
	[[nodiscard]] static Screen fromFile(const std::filesystem::path& path);

	/// @brief Loads a screen file, with its data in a folder of the caller's choice. The entry screen of an app (an
	/// app.json beside it names it: appOf()) is loaded as the app, with fromApp(), as the host opens it.
	/// @param path The file; relative font and "include" paths are resolved from its directory.
	/// @param dataDirectory Where "<file name>.history/" and "<file name>.state.json" are kept.
	/// @param history Whether the history is kept there or in memory only; the "persist" state is kept there either
	/// way.
	/// @return The screen, not yet built.
	/// @throws ScreenError If the file can't be read, is not a valid screen, or its history on disk is damaged.
	[[nodiscard]] static Screen fromFile(const std::filesystem::path& path, const std::filesystem::path& dataDirectory,
	                                     History history = History::OnDisk);

	/// @brief Loads an app (SCREEN_FORMAT.md, "Apps: app.json"): a folder with an app.json, or that file. Its entry
	/// screen is read with the app's own kits and themes and its data models merged in (the screen's own entries win),
	/// each seed file given to its model, and its data kept in appDataDirectory() (moved there, the first time, from
	/// where fromFile() kept the entry screen's).
	/// @param location The app's folder, or its app.json.
	/// @param dataDirectory The data folder: the app's own is made in it. Empty to keep nothing on disk, as
	/// fromString() does (a check).
	/// @param history Whether the history is kept there or in memory only.
	/// @return The screen, not yet built; app() is its manifest.
	/// @throws ScreenError If the manifest or a file it names can't be read or isn't valid, a file it names is
	/// outside its folder, or the entry screen can't be loaded.
	[[nodiscard]] static Screen fromApp(const std::filesystem::path& location,
	                                    const std::filesystem::path& dataDirectory, History history = History::OnDisk);

	/// @brief The app a screen file is the entry of: the app.json in its folder, if that names it as its entry.
	/// @param screenFile The screen file.
	/// @return The app.json, or an empty path if there is none or it names another entry.
	/// @throws ScreenError If the app.json beside it can't be read or isn't valid.
	[[nodiscard]] static std::filesystem::path appOf(const std::filesystem::path& screenFile);

	/// @brief The folder an app keeps the person's data in: its history, "persist" state, records and usage.
	/// @param dataDirectory The data folder.
	/// @param id The app's id (format::AppManifest::id).
	/// @return `<dataDirectory>/apps/<id>`.
	[[nodiscard]] static std::filesystem::path appDataDirectory(const std::filesystem::path& dataDirectory,
	                                                            std::string_view id);

	/// @brief The folder the app at a place keeps the person's data in, and its host the person's decisions: an id is
	/// the app's own choice, so it is bound to the first place that opens it (recorded in the folder's "location"),
	/// and an app elsewhere with the same id gets a folder of its own, never the other's data or permissions.
	/// @param dataDirectory The data folder.
	/// @param id The app's id (format::AppManifest::id).
	/// @param location The app's folder (or a bare screen file a host names by id).
	/// @return `<dataDirectory>/apps/<id>` if it is this place's, else `<dataDirectory>/apps/<id>~<hash of the place>`.
	/// @throws ScreenError If neither can be this place's.
	[[nodiscard]] static std::filesystem::path appDataDirectory(const std::filesystem::path& dataDirectory,
	                                                            std::string_view id,
	                                                            const std::filesystem::path& location);

	/// @brief The manifest of the app the screen was loaded from (fromApp()).
	/// @return It, or nullptr for a screen loaded otherwise.
	[[nodiscard]] const format::AppManifest* app() const noexcept;

	/// @brief Loads a screen from JSON text.
	/// @param content The document.
	/// @param baseDirectory Where relative paths (fonts, "include") are resolved from.
	/// @return The screen, not yet built.
	/// @throws ScreenError If it is not a valid screen.
	[[nodiscard]] static Screen fromString(std::string_view content, std::filesystem::path baseDirectory = {});

	/// @brief Takes over another screen, which can then only be destroyed or assigned to.
	Screen(Screen&&) noexcept;
	/// @brief Takes over another screen, which can then only be destroyed or assigned to.
	/// @return This screen.
	Screen& operator=(Screen&&) noexcept;
	/// @brief A screen has one window and one history: it can't be copied.
	Screen(const Screen&) = delete;
	/// @brief A screen has one window and one history: it can't be copied.
	/// @return Never.
	Screen& operator=(const Screen&) = delete;
	/// @brief Stops listening, closes the peers and channels, and takes back the window's timers and signals.
	~Screen();

	/* Before building */

	/// @brief Registers a custom "type" usable in the file. It has the properties of the built-in type its panel
	/// derives from (e.g. a TextEditor subclass has those of "editor"), plus its own.
	/// @param name The type name.
	/// @param factory Creates its panel.
	/// @param properties Its own property names.
	/// @param apply Applies them; may be empty if there are none.
	void registerType(std::string name, Factory factory, std::vector<std::string> properties = {},
	                  ApplyProperties apply = {});

	/// @brief Registers a GPU surface that "surface" nodes may name (GpuSurface.hpp): the file only names it and gives
	/// it "params"; it never supplies shader source.
	/// @param name Its name.
	/// @param factory Makes one for each panel that shows it.
	/// @param options Its framebuffer's options (a depth and stencil buffer).
	void registerSurface(std::string name, SurfaceFactory factory, SurfaceOptions options = {});

	/// @brief Makes unknown types plain panels instead of an error.
	/// @param enabled true to allow them.
	void setPlaceholderTypes(bool enabled);
	/// @brief Lets the screen show the open and save dialogs (`saveFile`, `openFile`, `export`, an image's open and
	/// save), or not: a step refused runs its "onError" with the code "notAllowed". On by default; a host that asks
	/// the person (the stock host's "files" capability) turns it off until they allow it.
	/// @param allowed Whether it may.
	void allowFileDialogs(bool allowed);
	/// @brief Lets the screen open the windows of its "windows" section, or not: an `openWindow` refused opens nothing
	/// and says so. On by default; a host that asks the person (the stock host's "windows" capability) turns it off
	/// until they allow it.
	/// @param allowed Whether it may.
	void allowOtherWindows(bool allowed);
	/// @brief Sets the computer's locale (format::systemLocale()), for screens whose "strings" name no "locale".
	/// @param tag "fr-CA"; "" for the screen's default.
	void setLocale(std::string tag);

	/* Building */

	/// @brief Creates a window from the "window" section, then builds the screen into it.
	/// @param app The application that owns the window.
	/// @return The window.
	/// @throws ScreenError If the screen can't be built.
	Window& createWindow(Application& app);
	/// @brief Creates a second window from the "window" section, sharing another window's OpenGL context and default
	/// font (Application::createWindow()), then builds the screen into it: one screen per window, e.g. a presenter's
	/// audience view next to the screen that drives it (SCREEN_FORMAT.md, "Second windows").
	/// @param app The application that owns both windows.
	/// @param sharing The window whose context and font it shares.
	/// @return The window.
	/// @throws ScreenError If the screen can't be built.
	Window& createWindow(Application& app, const Window& sharing);

	/// @brief Builds the screen into an embedding's window, inside the host's OpenGL context: its slow calls,
	/// services and channels post their results to the embedding, which runs them on the host's thread.
	/// @param embedding The embedding (outlives the Screen).
	/// @throws ScreenError If the screen can't be built.
	void embed(Embedding& embedding);

	/// @brief Loads the fonts, then builds "root" into a window's root panel.
	/// @param window The window.
	/// @throws ScreenError If the screen can't be built.
	void build(Window& window);

	/// @brief Builds a copy of a template as a new child ("repeat" in the file is usually simpler).
	/// @param name The template.
	/// @param parent Where to add it.
	/// @return The new panel.
	/// @throws ScreenError If there is no such template.
	Panel& instantiate(std::string_view name, Panel& parent);

	/* Behaviour */

	/// @brief Registers a native action, used for "onClick": "name" or a {"call": "name"} step when the file has none.
	/// @param name The action.
	/// @param action The handler.
	void onAction(std::string name, Action action);

	/// @brief Registers a native function for `{"call": name, "with": {...}, "into": "state.x"}` steps.
	/// @param name The function's name.
	/// @param call The function.
	void onCall(std::string name, Call call);

	/// @brief Registers a slow native function for "call" steps: it runs on a worker thread, so no frame waits for it,
	/// with the step's "loading", "progress", "timeout" and "then", and `{"cancel": name}` to stop it (see
	/// format::Runtime::onAsyncCall()). Destroying the Screen waits for the functions running.
	/// @param name The function's name.
	/// @param call The function; called on a worker thread, so it must not touch panels or the Screen.
	void onAsyncCall(std::string name, AsyncCall call);

	/// @brief Sets where the results of slow calls and services are posted, for a window the Screen didn't create
	/// (createWindow() uses the Application).
	/// @param executor The executor (outlives the Screen), or nullptr to run calls at once and start no service.
	void setExecutor(Executor* executor) noexcept;

	/// @brief The services the file's "services" section may use (RFC 0002): the host grants folders
	/// (`services().addRoot("workspace", path)`) and programs (`allowProcess`), and may add providers; a declaration
	/// outside the grants is refused. Services start at their first use, once there is an executor.
	/// @return The registry, owned by the Screen.
	[[nodiscard]] services::ServiceRegistry& services() noexcept;

	/// @brief The catalogues and publishers the `features` service installs from (RFC 0004): nothing is trusted
	/// until the host says so (`features().trustPublisher("ed25519:...")`, then `addCatalog(index)`).
	/// @return Them, owned by the Screen.
	[[nodiscard]] format::Features& features() noexcept;

	/// @brief The network the file's "connections" section uses (RFC 0003), made at the first call: until then, and
	/// for every origin the host doesn't allow (`network().allow("https://api.example.com")`), each connection is
	/// refused. Its handlers run through the executor (setExecutor() or startChannels()); with none, its results are
	/// dropped.
	/// @return The network, owned by the Screen.
	[[nodiscard]] net::Client& network();

	/// @brief Sets how the connections' "auth" signs in (net::SignInOptions): where refresh tokens are kept and how
	/// the browser opens. Without a call, network() signs in with tokens kept in memory, for this run only; an
	/// application passes `net::systemSecretStore("<its name>")` to stay signed in. Signing in again starts over.
	/// @param options The options.
	void setSignInOptions(net::SignInOptions options);

	/// @brief Registers a data model the screen shows with `"repeat": {"in": "models.<name>"}`: rows the application
	/// owns, read one at a time; a change of a row updates only what reads it. Agents see its count, shape and a sample
	/// in the catalog, never every row.
	/// @param name Its name: letters, digits and "_", not starting with a digit.
	/// @param model The model (e.g. a ListModel), or nullptr to remove it.
	/// @throws std::invalid_argument If the name isn't one.
	void setModel(std::string name, std::shared_ptr<DataModel> model);

	/// @brief Sets what happens with actions nobody handles (default: a message on stderr).
	/// @param fallback The handler.
	void onUnhandledAction(UnhandledAction fallback);

	/// @brief What the format offers (node types, properties, events, steps, operators, registered types and natives),
	/// as agents get it over the live protocol ("getCatalog").
	/// @return JSON text.
	[[nodiscard]] std::string catalogJson() const;

	/// @brief The JSON Schema of the format, with the types registered here (registerType()) among the built-in ones.
	/// @return The schema.
	[[nodiscard]] json schema() const;

	/// @brief Writes SCREEN_FORMAT.md's generated tables (between "<!-- generated: name -->" and
	/// "<!-- end generated -->") from what this screen has registered.
	/// @param markdown The document.
	/// @return The document with its tables generated.
	/// @throws std::invalid_argument If a marker has no end, or names a table that doesn't exist.
	[[nodiscard]] std::string referenceTables(std::string_view markdown) const;

	/* Capabilities the file declares: "channels" (programs to talk to) and "persist" (state kept on disk) */

	/// @brief Where programs on this machine meet by default, and where relative "channels" socket paths are found:
	/// $AFFRESCOUI_SOCKET_DIR if it is set, else a folder of this user's: %LOCALAPPDATA%\\affrescoui\\sockets on
	/// Windows, `/tmp/affrescoui-<uid>` elsewhere. mcp/affrescoui_mcp.py is to follow the same rule (roadmap 1.1d);
	/// until then it ignores the variable and defaults to `/tmp/affrescoui-<uid>`, so pass it --socket.
	/// @return The directory; it may not exist yet.
	[[nodiscard]] static std::filesystem::path defaultSocketDirectory();

	/// @brief Sets where relative "channels" socket paths are found (default: defaultSocketDirectory()).
	/// @param directory The directory.
	void setSocketDirectory(std::filesystem::path directory);

	/// @brief Lets the file's "channels" connect, or not: a channel refused doesn't connect, and runs "C:closed" with
	/// why. On by default; a host that asks the person (the stock host's "channels" capability) turns it off until
	/// they allow it. Call it before startChannels().
	/// @param allowed Whether they may.
	void allowChannels(bool allowed);

	/// @brief Keeps the file's "channels" sockets inside the socket directory (setSocketDirectory()), or not: a
	/// confined socket path is relative, with no "..", "\\" or ":", and still inside the directory once links are
	/// followed; any other is refused, as allowChannels() refuses. Off by default (a library screen is its embedder's
	/// own); the stock host confines every screen it opens to its own socket folder, so that a screen reaches its own
	/// host only, never another app's. Peer addresses (`wss://`) and connectChannel() aren't affected. Call it before
	/// startChannels().
	/// @param confined Whether they are.
	void confineChannels(bool confined);

	/// @brief Connects a declared channel somewhere else for this run, closing its connection first: a socket (as in
	/// "channels"), or a remote peer's `wss://` address (`ws://` on the loopback) with the token its server's verifier
	/// accepts. The token stays in memory, sent once as the connection's credentials: never in the document, the state
	/// or a log. In a browser this is how a page's program reaches a remote peer (web/README.md).
	/// @param name The channel's name in "channels" (after startChannels()).
	/// @param address The socket or the peer address.
	/// @param token The token for a peer address ("" for a socket).
	/// @return false if the screen declares no such channel.
	bool connectChannel(const std::string& name, std::string address, std::string token = {});

	/// @brief Connects the file's "channels" (createWindow() does it; call it after build()). Each message of type T
	/// on channel C runs the action "C:T" if the file has one; "C:open" and "C:closed" run on connect and disconnect.
	/// @param executor Delivers messages on the main thread (the Application).
	void startChannels(Executor& executor);

	/// @brief Keeps the screen's version-control history in a folder, and loads it from there if it has one, so that
	/// main, the open proposals, the grants and the mode survive a restart (VERSION_CONTROL.md, "History on disk").
	/// fromFile() does it with `<data folder>/<screen file name>.history` (defaultDataDirectory()); a screen from
	/// fromString() stays in memory.
	/// Call it before build() and before any change. If another program writes that history, this one only reads it
	/// (a warning says so); if the screen file changed since it was written, it is moved aside and a new one starts.
	/// @param directory The folder; created if needed.
	/// @throws ScreenError If the history there is damaged (what() names the file) or its last revision can't be
	/// shown: then nothing is loaded.
	void setHistoryDirectory(const std::filesystem::path& directory);

	/// @brief Sets the file the "persist" paths are kept in, and loads them from it if it exists (default:
	/// `<data folder>/<screen file name>.state.json`, see defaultDataDirectory()).
	/// @param file The file.
	void setStateFile(std::filesystem::path file);

	/* Records (SCREEN_FORMAT.md, "Data: models and records") */
	/// @brief Keeps the screen's records in a SQLite database file (WAL mode), which then follows the document's
	/// "models" (default: `<data folder>/<screen file name>.data.sqlite`; a screen from fromString() keeps them in
	/// memory). A model's "importFrom" brings the records an earlier version of the app kept in the state file, once,
	/// after copying that file to `<state file>.before-data.json`. Call it after setStateFile().
	/// @param file The database file; its folder is created if needed.
	/// @throws ScreenError If it can't be opened, or its records don't fit the models.
	void setDataFile(const std::filesystem::path& file);
	/// @brief The store the screen's records live in: queries, writes, the journal, the archive.
	/// @return It, owned by the Screen.
	[[nodiscard]] data::Store& data() noexcept;
	/// @brief What the screen's expressions read as `data` (each collection's records, each query's results).
	/// @return It, as JSON text.
	[[nodiscard]] std::string dataJson();
	/// @brief Sets who the write steps write as: "app" (the person using the app) by default. Around steps the host
	/// runs for an agent (its reviewed proposal), the agent's name: the journal names it, and the screen's data rules
	/// (`/data/<collection>`) decide what it may write.
	/// @param author The author ("" for "app").
	void setDataAuthor(std::string author);
	/// @brief Lets importCsv steps read CSV files in a folder besides the screen's own.
	/// @param directory The folder.
	void allowDataDirectory(const std::filesystem::path& directory);

	/* Usage summaries (VERSION_CONTROL.md, "Usage"): off by default; only node ids, action names, counts and times */
	/// @brief Sets the file the usage summaries and the person's consent are kept in, and loads it (default:
	/// `<data folder>/<screen file name>.usage.json`, beside the "persist" state).
	/// @param file The file.
	void setUsageFile(std::filesystem::path file);
	/// @brief The host's setting: usage summaries count only while it is on and the person consented.
	/// @param on Whether the setting is on.
	void setUsageEnabled(bool on);
	/// @brief Whether the person consented to usage summaries on this device, once.
	/// @return true if so.
	[[nodiscard]] bool usageConsented() const;
	/// @brief Records the person's consent (the host asks once, then never again).
	void giveUsageConsent();
	/// @brief The usage summaries, as JSON text: actions run and failed per node and action, screens visited, and the
	/// controls never touched.
	/// @return The summary.
	[[nodiscard]] std::string usageJson();
	/// @brief Forgets the usage counts (the consent stays).
	void clearUsage();

	/// @brief Runs an action as if a panel triggered it, for the application's own code: what it starts counts as the
	/// person's, as their press does (filling the display is one of them).
	/// @param name The action (from the file, else native).
	/// @param sender The panel it comes from.
	/// @param eventJson Available to the action as "event".
	void runAction(std::string_view name, Panel& sender, std::string_view eventJson = "{}");

	/* Live document: revisions (see VERSION_CONTROL.md) */

	/// @brief Applies, proposes or rejects a revision, following the file's "permissions". On error nothing changes.
	/// @param revisionJson `{"author", "message", "parent"?, "proposal"?, "patch": [JSON Patch ops], "assets"?: [...],
	/// "run"?: [steps]}`: assets and run steps make it a proposal only the person accepts (VERSION_CONTROL.md,
	/// "Assets").
	/// @return What happened.
	RevisionResult submitRevision(std::string_view revisionJson);

	/// @brief Applies a revision from the application itself: not reviewed, but deny rules still apply.
	/// @param revisionJson The revision.
	/// @param error Receives the error, if any (may be nullptr).
	/// @return true if it was applied.
	bool applyRevision(std::string_view revisionJson, std::string* error = nullptr);

	/* Review before apply */

	/// @brief The open proposals.
	[[nodiscard]] std::vector<ProposalInfo> proposals() const;

	/// @brief A change as a reviewer reads it, file by file (VERSION_CONTROL.md, "The changes, file by file").
	/// @param request `proposal` (what accepting it would change), or `to` and optionally `from` (default: `to`'s
	/// parent; `to` defaults to HEAD); `notation` ("json" or "lisp", default the screen file's); `file`, a file to give
	/// the texts of.
	/// @return Who and what (`author`, `message`, `head`, ...), `files` (`[{"file", "added", "removed", "before"?,
	/// "after"?}]`, or `"tooLarge": true`), `assets`, `run`, `impact`, `behaviour` and `person`.
	/// @throws DocumentError If there is no such proposal or commit, or a proposal no longer applies.
	[[nodiscard]] json fileChanges(const json& request) const;

	/// @brief Merges a proposal, approved by "user" (VersionControl::accept takes more).
	/// @param id The proposal.
	/// @param head Its head commit as the user reviewed it (ProposalInfo::head, or 7+ characters of it); refused if
	/// the proposal moved since.
	/// @param error Receives the error, if any (may be nullptr).
	/// @return true if it was merged.
	bool acceptProposal(int id, std::string_view head, std::string* error = nullptr);

	/// @brief Rejects a proposal.
	/// @param id The proposal.
	/// @param reason Told to its author.
	/// @return false if there is no such proposal.
	bool rejectProposal(int id, std::string_view reason = {});

	/// @brief Shows a proposal live, without committing it.
	/// @param id The proposal.
	/// @param error Receives the error, if any (may be nullptr).
	/// @return true if it is shown.
	bool previewProposal(int id, std::string* error = nullptr);

	/// @brief Goes back to the committed screen after previewProposal().
	void endPreview();
	/// @brief The proposal being previewed, if any.
	[[nodiscard]] std::optional<int> previewedProposal() const noexcept;
	/// @brief Sets how a proposal's evidence ("getEvidence", VERSION_CONTROL.md) plays the screen's scenarios: given
	/// the proposal's document as JSON text, returns `[{"name", "passed", "message"}]` as JSON text. Without one, the
	/// evidence has no scenarios. affrescoui_launcher plays the screen's `.scenarios.json` (modules/automation).
	/// @param check The check, or an empty function.
	void setScenarioCheck(std::function<std::string(const std::string& documentJson)> check);
	/// @brief Turns the inspector on or off: an overlay showing the node, bounds, bindings as written and last commit
	/// of the panel under the pointer, the frame and update statistics, and the latest events. Off by default; a
	/// reviewer can also turn it on with an "inspect" message (VERSION_CONTROL.md). It never shows a value from the
	/// state, and pictures drawn for peers leave it out.
	/// @param on true to show it.
	/// @param statistics false to leave out times and counts, so that a picture of it is always the same.
	/// @param error Receives why it can't, if not null.
	/// @return false if it can't (no window yet).
	bool setInspector(bool on, bool statistics = true, std::string* error = nullptr);
	/// @brief The inspector's latest events, oldest first (none while it has been off).
	[[nodiscard]] std::vector<std::string> inspectorTrace() const;

	/* History */

	/// @brief main's commits, oldest first.
	[[nodiscard]] std::vector<CommitInfo> log() const;

	/// @brief Brings the screen back to an earlier revision, as a new revision (the history keeps growing).
	/// @param revision The revision to go back to.
	/// @param error Receives the error, if any (may be nullptr).
	/// @return true if it was restored.
	bool restore(int revision, std::string* error = nullptr);

	/// @brief Same as restore(), following an author's permissions.
	/// @param revision The revision to go back to.
	/// @param author The author.
	/// @return What happened.
	RevisionResult submitRestore(int revision, std::string_view author);

	/// @brief Undoes one revision and keeps the ones after it, as a new revision (like `git revert`).
	/// @param revision The revision to undo.
	/// @param error Receives the error, if any (may be nullptr).
	/// @return true if it was reverted.
	bool revert(int revision, std::string* error = nullptr);

	/// @brief Reverts the latest revision.
	/// @param error Receives the error, if any (may be nullptr).
	/// @return true if it was reverted.
	bool undo(std::string* error = nullptr);

	/// @brief The current revision; 0 is the file as loaded.
	[[nodiscard]] int revision() const noexcept;

	/// @brief Exports a change set ("plugin"), portable to other copies of the same screen.
	/// @param fromRevision Revisions after this one are included.
	/// @param name The change set's name.
	/// @return The change set, as JSON text.
	[[nodiscard]] std::string exportChanges(int fromRevision = 0, std::string_view name = {}) const;

	/// @brief Applies a change set, all or nothing.
	/// @param changesJson A change set from exportChanges().
	/// @param error Receives the error, if any (may be nullptr).
	/// @return true if it was applied.
	bool importChanges(std::string_view changesJson, std::string* error = nullptr);
	/// @brief Tells whether a change set was already applied to this screen's history (importChanges() keeps them,
	/// on disk too for a screen from fromFile()), so that a host can apply a plugin once.
	/// @param changesJson A change set from exportChanges().
	/// @return true if it was; false for one that wasn't, or isn't valid JSON.
	[[nodiscard]] bool hasImported(std::string_view changesJson) const;
	/// @brief Installs a change set that may carry its request, base and proof (VERSION_CONTROL.md, "Self-repairing
	/// change sets"): applied if its patch applies and its proof passes, otherwise left off, marked "needsRepair", and
	/// a repair asked of the peers (a "request" of kind "repair"), which only a reviewer lands.
	/// @param changesJson A change set from exportChanges() (with its signature, if any, already checked).
	/// @param error Receives the error, if it is malformed (may be nullptr).
	/// @return "applied", "needsRepair", "repairing", "repaired" or "gaveUp"; empty on an error.
	std::string installChanges(std::string_view changesJson, std::string* error = nullptr);
	/// @brief Sets how installChanges() checks a change set's proof: given the document it makes (with its state) and
	/// the proof, as JSON text, returns why it fails, or "". affrescoui_launcher plays the proof's scenarios.
	/// @param check The check, or an empty function (then a change set with a proof needs repair).
	void setProofCheck(std::function<std::string(const std::string& documentJson, const std::string& proofJson)> check);
	/// @brief Where the installed change sets stand.
	/// @return `{"type": "repairs", "changeSets": [{name, status, reason, attempts, proposal, request,
	/// repairedFrom}]}`.
	[[nodiscard]] std::string repairs() const;

	/// @brief The current document, as JSON text.
	[[nodiscard]] std::string documentJson() const;
	/// @brief The live state, as JSON text.
	[[nodiscard]] std::string stateJson() const;

	/// @brief Listens to "emit" events.
	/// @param listener Receives each event, as JSON text.
	/// @return The connection.
	Connection onEvent(std::function<void(const std::string& eventJson)> listener);
	/// @brief Listens to the errors of handlers and deferred changes: a step that failed, which stops its action (they
	/// are also logged).
	/// @param listener Receives each error's message.
	/// @return The connection.
	Connection onError(std::function<void(const std::string& message)> listener);

	/* Peers: programs that read and change the screen over the live protocol (SCREEN_FORMAT.md, "Live screens").
	   A peer's changes are authored by the name it was connected with, whatever name it gives itself. Every peer hears
	   about commits, proposals and "emit" events; "request" and "reply" messages are passed on to the other peers. */

	/// @brief Adds a peer at the other end of a channel.
	/// @param channel The channel; must outlive the screen.
	/// @param author The name "permissions" knows the peer by.
	void connect(Channel& channel, std::string author = "channel");

	/// @brief Accepts any number of peers on a local socket (POSIX). Give each kind of peer its own socket.
	/// @param executor Delivers messages on the main thread (the Application).
	/// @param path The socket file, e.g. ".../agent.sock".
	/// @param author The name its peers are authored as, e.g. "agent".
	/// @throws std::runtime_error If the socket can't be created.
	void listen(Executor& executor, const std::filesystem::path& path, std::string author);

	/// @brief Accepts remote peers over WebSocket (RFC 0003): each brings a token, and the verifier names the author
	/// its changes are made as. On the loopback by default, behind a TLS proxy (see net::PeerServer and SECURITY.md).
	/// @param executor Delivers messages on the main thread (the Application).
	/// @param address "host:port", e.g. "127.0.0.1:7420" (port 0 picks a free one).
	/// @param verifier Checks tokens; must not be null.
	/// @param options Origins and limits.
	/// @return The port listened on.
	/// @throws std::runtime_error If the address is malformed, not on the loopback without allowRemote, or taken.
	std::uint16_t listenWebSocket(Executor& executor, std::string_view address,
	                              std::shared_ptr<net::TokenVerifier> verifier, net::PeerServerOptions options = {});

	/// @brief listenWebSocket() with a verifier given by value, e.g. `listenWebSocket(app, "127.0.0.1:7420",
	/// MyTokenVerifier{...})`.
	/// @param executor Delivers messages on the main thread.
	/// @param address "host:port".
	/// @param verifier The verifier (shared by the server's connections from then on).
	/// @param options Origins and limits.
	/// @return The port listened on.
	/// @throws std::runtime_error See above.
	template <std::derived_from<net::TokenVerifier> Verifier>
	std::uint16_t listenWebSocket(Executor& executor, std::string_view address, Verifier verifier,
	                              net::PeerServerOptions options = {}) {
		return listenWebSocket(executor, address, std::make_shared<Verifier>(std::move(verifier)), std::move(options));
	}

	/// @brief Sets how much reviewers are asked (see VersionControl::setMode).
	/// @param mode Default, AcceptEdits (apply what no rule allows, like auto-accept) or Plan (peers only propose).
	void setMode(Mode mode);
	/// @brief Sets how much reviewers are asked until the program quits, without keeping it in the screen's history:
	/// for a host's command line (`--mode`, `--auto-accept`). The next start uses the mode a reviewer last chose.
	/// @param mode Default, AcceptEdits or Plan (see setMode()).
	void setModeForThisRun(Mode mode);

	/// @brief The version control module, for code that needs more than this class offers.
	[[nodiscard]] VersionControl& versions() noexcept;

	/* Lookup (after build) */

	/// @brief Finds a panel by id.
	/// @param id The id.
	/// @return The panel, or nullptr.
	[[nodiscard]] Panel* find(std::string_view id) noexcept;

	/// @brief Finds a panel by id and type.
	/// @tparam T The expected panel type.
	/// @param id The id.
	/// @return The panel.
	/// @throws ScreenError If there is no panel with that id and type.
	template <std::derived_from<Panel> T = Panel> [[nodiscard]] T& get(std::string_view id) {
		T* panel = dynamic_cast<T*>(find(id));
		if(!panel)
			throw ScreenError("AffrescoUI screen: no panel with id '" + std::string(id) + "' of the requested type");
		return *panel;
	}

	/// @brief Looks a colour up in "colors".
	/// @param name The name.
	/// @return The colour.
	/// @throws ScreenError If there is no such colour.
	[[nodiscard]] Color color(std::string_view name) const;

	/// @brief Reads a colour as the file writes it, e.g. in a custom type's properties.
	/// @param value "#rgb", "#rrggbb", "#rrggbbaa", "@name" (from "colors", in the current theme) or "transparent".
	/// @return The colour.
	/// @throws ScreenError If it is not a colour.
	[[nodiscard]] Color resolveColor(std::string_view value) const;

	/// @brief Looks a font up in "fonts".
	/// @param name The name.
	/// @return The font.
	/// @throws ScreenError If there is no such font.
	[[nodiscard]] std::shared_ptr<const Font> font(std::string_view name) const;

	/* Images */

	/// @brief Sets, replaces or removes an image the screen shows by name: an image node's `"source"`. For content the
	/// application makes (a chart, a camera frame, a picture from the network); the nodes showing it are updated.
	/// @param name The name.
	/// @param bitmap The image, or nullptr to remove it.
	void setImage(const std::string& name, const std::shared_ptr<const Bitmap>& bitmap);
	/// @brief Sets or replaces a named image from pixels.
	/// @param name The name.
	/// @param width Its width.
	/// @param height Its height.
	/// @param rgba Red, green, blue and alpha per pixel, top row first, not premultiplied: width * height * 4 bytes.
	/// @throws std::invalid_argument If the size is empty, larger than 8,192 on a side or 4,096 by 4,096 in all, or
	/// doesn't match @p rgba.
	void setImage(const std::string& name, int width, int height, std::span<const std::uint8_t> rgba);
	/// @brief Allows the screen's image files to come from a folder too. By default only the screen file's own folder
	/// (and the folders inside it) is allowed, so that a revision can't show any picture on the disk.
	/// @param directory The folder.
	void allowImageDirectory(const std::filesystem::path& directory);
	/// @brief Waits until every image file asked for is decoded and shown (a picture of the screen does it itself).
	void waitForImages();

	/* Audio */

	/// @brief The audio engine the file's "audio" section plays on (affresco/audio/Engine.hpp), made at the first
	/// call: until then every "audio" step says "this host plays no audio". It plays to the system's output, or to no
	/// hardware when $AFFRESCOUI_AUDIO_OUTPUT is "null" (CI, a host under test); its events go to the Screen's
	/// executor. Calling it is the host's choice to let the screen make sound.
	/// @return The engine, owned by the Screen; nullptr if the library was built without audio (AFFRESCOUI_AUDIO=OFF).
	audio::Engine* audio();
	/// @brief Sets the audio engine the screen plays on instead (e.g. one shared by several screens, or a Manual one
	/// in a test); its events go to the executor it was made with.
	/// @param engine The engine, or null for none.
	void setAudio(std::shared_ptr<audio::Engine> engine);
	/// @brief Names a source the host makes, for sounds that say `"source": name`: a live audio::PcmStream or
	/// audio::EncodedStream the application feeds from its own thread, bytes it holds, a generator.
	/// @param name The name.
	/// @param source The source, or nullptr to remove it.
	void setAudioSource(const std::string& name, std::shared_ptr<audio::Source> source);
	/// @brief Allows the screen's sound files to come from a folder too. By default only the screen file's own folder
	/// (and the folders inside it) is allowed.
	/// @param directory The folder.
	void allowAudioDirectory(const std::filesystem::path& directory);

	/* Assets and attachments (VERSION_CONTROL.md, "Assets") */

	/// @brief The assets of the version shown (main's head, or the proposal previewed), sorted by name.
	[[nodiscard]] std::vector<AssetInfo> assets() const;
	/// @brief An asset's bytes, as the version shown names it ("asset:<name>" in the document).
	/// @param name The asset's name.
	/// @return Its bytes, or nullptr if there is no such asset.
	[[nodiscard]] std::shared_ptr<const std::string> assetBytes(std::string_view name) const;
	/// @brief Keeps a file the person gave the host as an attachment, for agents to name in a revision's assets (what
	/// an `attach` step does). Its type comes from its name, checked against its bytes; pictures and sounds are
	/// measured
	/// (`width`, `height`, `duration`).
	/// @param name The file's own name, e.g. "Beat.mp3" (made a valid asset name: "beat.mp3").
	/// @param bytes Its bytes, at most maxAssetBytes.
	/// @return The attachment, or why it was refused.
	Attached attach(std::string_view name, std::string bytes);

private:
	struct Impl;
	/// @brief Wraps a loaded screen.
	/// @param impl The modules.
	explicit Screen(std::unique_ptr<Impl> impl);

	std::unique_ptr<Impl> impl_; ///< The modules and the peers.
};

} // namespace affresco
