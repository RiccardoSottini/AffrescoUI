#pragma once

/// @file
/// @brief format::Runtime: brings a screen document to life with the rendering engine.

#include "affresco/core/DataModel.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/render/Application.hpp"
#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/Window.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

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

private:
	struct Impl;
	std::unique_ptr<Impl> impl_; ///< Everything else, out of the header.
};

} // namespace affresco::format
