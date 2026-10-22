#pragma once

/// @file
/// @brief format::Runtime: brings a screen document to life with the rendering engine.

#include "affresco/core/CallProgress.hpp"
#include "affresco/core/DataModel.hpp"
#include "affresco/core/Executor.hpp"
#include "affresco/core/Identity.hpp"
#include "affresco/core/Network.hpp"
#include "affresco/core/Services.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/format/Usage.hpp"
#include "affresco/render/Application.hpp"
#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/Window.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief What the last update of the panels cost: read with Runtime::lastUpdate(), for an inspector.
struct UpdateStatistics {
	std::uint64_t updates = 0;     ///< Updates since the runtime started.
	double milliseconds = 0;       ///< How long the last one took.
	std::size_t panelsApplied = 0; ///< Panels whose values the last one applied (those it rebuilt among them).
	std::size_t panelsCreated = 0; ///< Panels it created.
};

/// @brief Brings a screen document to life: builds it into panels, keeps them in line with it, and runs actions.
///
/// The document and the state come from a VersionControl (which must outlive the runtime). The runtime observes it:
/// when a revision lands, panels are reconciled in place, so focus, typing and drags survive. State changes during
/// one event are batched into one update. If a new version can't be built, the runtime refuses it and the
/// VersionControl rolls it back.
class Runtime {
public:
	using Action = std::function<void(Panel& sender)>; ///< A native action.
	/// A native function for "call" steps: takes the step's "with" value, returns what "into" stores. Throwing
	/// std::exception runs the step's "onError" steps (with `error`).
	using Call = std::function<json(const json& arguments)>;
	/// A slow native function for "call" steps, run on a worker thread: like Call, and it may report progress and
	/// partial results, and should stop soon once the call is cancelled (both through @p call).
	using AsyncCall = std::function<json(const json& arguments, CallProgress& call)>;
	using Factory = std::function<std::unique_ptr<Panel>()>; ///< Creates a custom node type.
	/// Applies a custom type's own properties to one of its panels: the node's evaluated values for the names given at
	/// registration (only those it has; colours and fonts as written, see resolveColor() and font()). Called when they
	/// change, so a name that is missing means its default. Throwing std::exception reports the error at the node.
	using ApplyProperties = std::function<void(Panel& panel, const json& properties)>;
	using UnhandledAction = std::function<void(const std::string& action, Panel& sender)>; ///< Fallback handler.

	/// @brief Reads the document's colours and fonts and starts observing @p versions.
	/// @param versions The document's version control; must outlive the runtime.
	/// @param baseDirectory Where relative paths (fonts, images) are resolved from; image files must be inside it.
	/// @throws DocumentError If a colour or font is invalid (e.g. a missing font file).
	Runtime(VersionControl& versions, std::filesystem::path baseDirectory);
	~Runtime();

	Runtime(const Runtime&) = delete;
	Runtime& operator=(const Runtime&) = delete;

	/* Before building */

	/// @brief Registers a custom "type" usable in the document, through the same registry as the built-in types. It
	/// has the properties of the built-in widget its panel derives from (e.g. a TextEditor subclass has those of
	/// "editor"), plus its own. Under the name of a built-in type, it replaces that type.
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

	/// @brief Creates a window from the "window" section, then builds the document into it.
	/// @param app The application that owns the window.
	/// @return The window.
	/// @throws DocumentError If the document can't be built.
	Window& createWindow(Application& app);

	/// @brief Builds the document into a window's root panel.
	/// @param window The window.
	/// @throws DocumentError If the document can't be built.
	void build(Window& window);

	/// @brief Builds a copy of a template as a new child ("repeat" in the document is usually simpler).
	/// @param name The template's name.
	/// @param parent Where to add it.
	/// @return The new panel.
	/// @throws DocumentError If there is no such template or it can't be built.
	Panel& instantiate(std::string_view name, Panel& parent);

	/* Behaviour */

	/// @brief Registers a native action, used when the document has no action of that name.
	/// @param name The action name.
	/// @param action The handler.
	void onAction(std::string name, Action action);

	/// @brief Registers a native function for `{"call": name, "with": ..., "into": "state.x"}` steps.
	/// @param name The function's name.
	/// @param call The function.
	void onCall(std::string name, Call call);

	/// @brief Registers a slow native function for "call" steps: it runs on a worker thread, so no frame waits for it.
	/// The step may add "loading" (a state path, true while it runs), "progress" (a state path, from 0 to 1),
	/// "timeout" (milliseconds) and "then" (steps run with `result`); `{"cancel": name}` stops its calls. Results
	/// arrive through the Executor (the Application, from createWindow()); without one, it runs at once. Destroying the
	/// runtime waits for the functions running, so they should check CallProgress::cancelled().
	/// @param name The function's name (taken over a Call of the same name).
	/// @param call The function; called on a worker thread, so it must not touch panels or the runtime.
	void onAsyncCall(std::string name, AsyncCall call);

	/// @brief Sets where the results of asynchronous calls are posted, for a window the runtime didn't create.
	/// @param executor The executor (outlives the runtime), or nullptr to run them at once.
	void setExecutor(Executor* executor) noexcept;

	/// @brief Sets the services the document's "services" section declares (RFC 0002): it loads them now and again
	/// whenever the section changes, reporting each one refused through onError. "invoke", "subscribe" and "respond"
	/// steps talk to them; results arrive through the Executor, like those of asynchronous calls.
	/// @param services The services (outlive the runtime, or are replaced first), or nullptr for none.
	void setServices(services::Services* services);

	/// @brief Sets the network the document's "connections" section uses (RFC 0003): "request" and "subscribe" steps
	/// reach a connection only through it, and only once the host allowed the connection's origin. The connections
	/// load again.
	/// @param network The network (outlives the runtime, or is replaced first), or nullptr: every connection is
	/// refused.
	void setNetwork(network::Network* network);
	/// @brief Sets the sign-in the connections' "auth" use (net::SignIn): it is told the providers the document
	/// declares, adds the bearer token to their requests, and shows its states as the "auth" variable.
	/// @param identity The sign-in (outlives the runtime, or is replaced first), or nullptr: no connection with "auth"
	/// is signed in.
	void setIdentity(identity::Identity* identity);

	/// @brief Registers a data model the document shows with `"repeat": {"in": "models.<name>"}`: its rows are read
	/// one at a time, and a change of a row updates only what reads that row. Agents see its count, shape and a sample.
	/// @param name Its name: letters, digits and "_", not starting with a digit.
	/// @param model The model, or nullptr to remove it.
	/// @throws std::invalid_argument If the name isn't one.
	void setModel(std::string name, std::shared_ptr<DataModel> model);

	/// @brief Sets what happens with actions nobody handles (default: a message on stderr).
	/// @param fallback The handler.
	void onUnhandledAction(UnhandledAction fallback);

	/// @brief Runs an action as if a panel triggered it.
	/// @param name The action (from the document, else native).
	/// @param sender The panel it comes from.
	/// @param event Available to the action as "event".
	void runAction(std::string_view name, Panel& sender, const json& event);

	/// @brief Keeps the panels in line with the document without running what the document runs because they
	/// changed: while suspended, no "onShow", "onHide" or "watch" handler is scheduled, "autofocus" takes no focus,
	/// and colours change without their transitions. For a document shown only for a moment, e.g. a proposal drawn
	/// for a picture, which must change nothing the person sees or what runs next.
	/// @param suspended true to suspend them, false to go back to normal.
	void setHandlersSuspended(bool suspended) noexcept;

	/// @brief Tells whether the document has an action of that name.
	/// @param name The action.
	/// @return true if "actions" has it.
	[[nodiscard]] bool hasAction(std::string_view name) const;

	/// @brief What the format offers, for agents: node types and their properties, events, steps, operators, the
	/// registered types, native actions and calls.
	/// @return The catalog (see SCREEN_FORMAT.md, "Catalog").
	[[nodiscard]] json catalog() const;

	/// @brief The JSON Schema of the format, with the types, sections and steps registered here (the application's
	/// types among them).
	/// @return The schema; schema/affrescoui-screen.schema.json is the one of the built-in types.
	[[nodiscard]] json schema() const;

	/// @brief Writes SCREEN_FORMAT.md's generated tables from the registries: the text between each
	/// "<!-- generated: name -->" and "<!-- end generated -->".
	/// @param markdown The document.
	/// @return The document with its tables generated.
	/// @throws std::invalid_argument If a marker has no end, or names a table that doesn't exist.
	[[nodiscard]] std::string referenceTables(std::string_view markdown) const;

	/// @brief The window built into, or nullptr before build().
	[[nodiscard]] Window* window() const noexcept;

	/* Inspection */

	/// @brief What the last update of the panels cost.
	[[nodiscard]] const UpdateStatistics& lastUpdate() const noexcept;

	/// @brief What the document says about a panel, for an inspector: the node it was built from (the nearest
	/// ancestor built from one, for a panel a type creates itself) and its bindings as written. Never a value: what
	/// a binding gives now stays out, so the state never shows.
	/// @param panel A panel of the window.
	/// @return `{"node": "#id" or the node's place, "type", "path", "bind"?, "bindings": {key: written}, "handlers":
	/// [key]}`, or null if no node built it.
	[[nodiscard]] json inspect(const Panel& panel);

	/* Lookup */

	/// @brief Finds a built panel by id.
	/// @param id The id.
	/// @return The panel, or nullptr.
	[[nodiscard]] Panel* find(std::string_view id) noexcept;

	/// @brief Looks a colour up in "colors".
	/// @param name The name.
	/// @return The colour.
	/// @throws DocumentError If there is no such colour.
	[[nodiscard]] Color color(std::string_view name) const;

	/// @brief Reads a colour as the document writes it, e.g. in a custom type's properties.
	/// @param value "#rgb", "#rrggbb", "#rrggbbaa", "@name" (from "colors", in the current theme) or "transparent".
	/// @return The colour.
	/// @throws DocumentError If it is not a colour.
	[[nodiscard]] Color resolveColor(const json& value) const;

	/* Images */

	/// @brief Sets, replaces or removes an image the document can show by name (`"source"` of an image node), e.g. a
	/// picture the application made. Nodes showing it are updated.
	/// @param name The name.
	/// @param bitmap The image, or nullptr to remove it.
	void setImage(const std::string& name, const std::shared_ptr<const Bitmap>& bitmap);

	/// @brief Allows image files from a folder too: by default only the screen's own folder (and those inside it).
	/// @param directory The folder.
	void allowImageDirectory(const std::filesystem::path& directory);

	/// @brief Waits until every image file asked for is decoded, and shows them: before a picture of the screen.
	void waitForImages();

	/// @brief Looks a font up in "fonts".
	/// @param name The name.
	/// @return The font.
	/// @throws DocumentError If there is no such font.
	[[nodiscard]] std::shared_ptr<const Font> font(std::string_view name) const;

	Signal<const json&> onEvent;        ///< "emit" steps: `{"type": "event", "name", "data", "revision"}`.
	Signal<const std::string&> onError; ///< Errors in handlers and deferred changes (also printed on stderr).
	Signal<const std::string&, const json&> onSend; ///< "send" steps: a channel's name and the message.
	Signal<const std::string&> onConnect;           ///< "connect" steps: a channel's name.
	Signal<> onUpdated;                             ///< The panels were brought in line with the document and state.
	Usage usage; ///< Usage summaries (VERSION_CONTROL.md, "Usage"): off until the host switches them on, with consent.

private:
	struct Impl;
	std::unique_ptr<Impl> impl_; ///< Everything else, out of the header.
};

} // namespace affresco::format
