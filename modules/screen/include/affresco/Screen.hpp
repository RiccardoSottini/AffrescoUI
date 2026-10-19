#pragma once

/// @file
/// @brief Screen: a live UI described in the AffrescoUI Screen Format, with all the modules put together.

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/render/Application.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/Window.hpp"

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief A malformed screen file; what() names the place, e.g. "root.children[1].anchors: unknown edge 'lefft'".
using ScreenError = DocumentError;

/// @brief A live UI described in the AffrescoUI Screen Format (JSON, see SCREEN_FORMAT.md), built into a Window.
///
/// The composition root of the library: format::Rules (DocumentRules), vcs::Repository (VersionControl),
/// format::Runtime (panels, reconciliation, actions) and one channel::Session per peer, talking only through the
/// interfaces in affresco/core. Use the modules directly to put them together differently. Keep the Screen alive while
/// its window is used.
class Screen {
public:
	using Action = std::function<void(Panel& sender)>;                                     ///< A native action.
	/// A native function for "call" steps: gets the step's "with" as JSON text, returns what "into" stores as JSON
	/// text. Throwing std::exception runs the step's "onError" steps.
	using Call = std::function<std::string(const std::string& argumentsJson)>;
	using Factory = std::function<std::unique_ptr<Panel>()>;                               ///< Creates a custom type.
	/// Applies a custom type's own properties to one of its panels: the node's evaluated values for the names given at
	/// registration, as a JSON object in text (only those it has; colours as written, see resolveColor()). Called when
	/// they change, so a name that is missing means its default. Throwing std::exception reports the error at the node.
	using ApplyProperties = std::function<void(Panel& panel, const std::string& propertiesJson)>;
	using UnhandledAction = std::function<void(const std::string& action, Panel& sender)>; ///< Fallback handler.
	using ProposalInfo = affresco::ProposalInfo;                                                ///< An open proposal.
	using CommitInfo = affresco::CommitInfo;                                                    ///< A commit.

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
	/// (setHistoryDirectory(), setStateFile()).
	/// @param path The file; relative font and "include" paths are resolved from its directory.
	/// @return The screen, not yet built.
	/// @throws ScreenError If the file can't be read, is not a valid screen, or its history on disk is damaged.
	[[nodiscard]] static Screen fromFile(const std::filesystem::path& path);

	/// @brief Loads a screen file, with its data in a folder of the caller's choice.
	/// @param path The file; relative font and "include" paths are resolved from its directory.
	/// @param dataDirectory Where "<file name>.history/" and "<file name>.state.json" are kept.
	/// @param history Whether the history is kept there or in memory only; the "persist" state is kept there either way.
	/// @return The screen, not yet built.
	/// @throws ScreenError If the file can't be read, is not a valid screen, or its history on disk is damaged.
	[[nodiscard]] static Screen fromFile(const std::filesystem::path& path, const std::filesystem::path& dataDirectory,
	                                     History history = History::OnDisk);

	/// @brief Loads a screen from JSON text.
	/// @param content The document.
	/// @param baseDirectory Where relative paths (fonts, "include") are resolved from.
	/// @return The screen, not yet built.
	/// @throws ScreenError If it is not a valid screen.
	[[nodiscard]] static Screen fromString(std::string_view content, std::filesystem::path baseDirectory = {});

	Screen(Screen&&) noexcept;
	Screen& operator=(Screen&&) noexcept;
	~Screen();

	/* Before building */

	/// @brief Registers a custom "type" usable in the file. It has the properties of the built-in type its panel derives
	/// from (e.g. a TextEditor subclass has those of "editor"), plus its own.
	/// @param name The type name.
	/// @param factory Creates its panel.
	/// @param properties Its own property names.
	/// @param apply Applies them; may be empty if there are none.
	void registerType(std::string name, Factory factory, std::vector<std::string> properties = {},
	                  ApplyProperties apply = {});

	/// @brief Makes unknown types plain panels instead of an error.
	/// @param enabled true to allow them.
	void setPlaceholderTypes(bool enabled);

	/* Building */

	/// @brief Creates a window from the "window" section, then builds the screen into it.
	/// @param app The application that owns the window.
	/// @return The window.
	/// @throws ScreenError If the screen can't be built.
	Window& createWindow(Application& app);

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

	/// @brief Sets what happens with actions nobody handles (default: a message on stderr).
	/// @param fallback The handler.
	void onUnhandledAction(UnhandledAction fallback);

	/// @brief What the format offers (node types, properties, events, steps, operators, registered types and natives),
	/// as agents get it over the live protocol ("getCatalog").
	/// @return JSON text.
	[[nodiscard]] std::string catalogJson() const;

	/* Capabilities the file declares: "channels" (programs to talk to) and "persist" (state kept on disk) */

	/// @brief Where programs on this machine meet by default, and where relative "channels" socket paths are found:
	/// $AFFRESCOUI_SOCKET_DIR if it is set, else a folder of this user's: %LOCALAPPDATA%\\affrescoui\\sockets on
	/// Windows, /tmp/affrescoui-<uid> elsewhere. mcp/affrescoui_mcp.py is to follow the same rule (roadmap 1.1d); until
	/// then it ignores the variable and defaults to /tmp/affrescoui-<uid>, so pass it --socket.
	/// @return The directory; it may not exist yet.
	[[nodiscard]] static std::filesystem::path defaultSocketDirectory();

	/// @brief Sets where relative "channels" socket paths are found (default: defaultSocketDirectory()).
	/// @param directory The directory.
	void setSocketDirectory(std::filesystem::path directory);

	/// @brief Connects the file's "channels" (createWindow() does it; call it after build()). Each message of type T
	/// on channel C runs the action "C:T" if the file has one; "C:open" and "C:closed" run on connect and disconnect.
	/// @param executor Delivers messages on the main thread (the Application).
	void startChannels(Executor& executor);

	/// @brief Keeps the screen's version-control history in a folder, and loads it from there if it has one, so that
	/// main, the open proposals, the grants and the mode survive a restart (VERSION_CONTROL.md, "History on disk").
	/// fromFile() does it with <data folder>/<screen file name>.history (defaultDataDirectory()); a screen from
	/// fromString() stays in memory.
	/// Call it before build() and before any change. If another program writes that history, this one only reads it
	/// (a warning says so); if the screen file changed since it was written, it is moved aside and a new one starts.
	/// @param directory The folder; created if needed.
	/// @throws ScreenError If the history there is damaged (what() names the file) or its last revision can't be
	/// shown: then nothing is loaded.
	void setHistoryDirectory(const std::filesystem::path& directory);

	/// @brief Sets the file the "persist" paths are kept in, and loads them from it if it exists (default:
	/// <data folder>/<screen file name>.state.json, see defaultDataDirectory()).
	/// @param file The file.
	void setStateFile(std::filesystem::path file);

	/// @brief Runs an action as if a panel triggered it.
	/// @param name The action (from the file, else native).
	/// @param sender The panel it comes from.
	/// @param eventJson Available to the action as "event".
	void runAction(std::string_view name, Panel& sender, std::string_view eventJson = "{}");

	/* Live document: revisions (see VERSION_CONTROL.md) */

	/// @brief Applies, proposes or rejects a revision, following the file's "permissions". On error nothing changes.
	/// @param revisionJson `{"author", "message", "parent"?, "proposal"?, "patch": [JSON Patch ops]}`.
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

	/// @brief The current document, as JSON text.
	[[nodiscard]] std::string documentJson() const;
	/// @brief The live state, as JSON text.
	[[nodiscard]] std::string stateJson() const;

	/// @brief Listens to "emit" events.
	/// @param listener Receives each event, as JSON text.
	/// @return The connection.
	Connection onEvent(std::function<void(const std::string& eventJson)> listener);

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

private:
	struct Impl;
	/// @brief Wraps a loaded screen.
	/// @param impl The modules.
	explicit Screen(std::unique_ptr<Impl> impl);

	std::unique_ptr<Impl> impl_; ///< The modules and the peers.
};

} // namespace affresco
