#pragma once

/// @file
/// @brief format::Runtime: brings a screen document to life with the rendering engine.

#include "sgl/core/Json.hpp"
#include "sgl/core/Signal.hpp"
#include "sgl/core/VersionControl.hpp"
#include "sgl/render/Application.hpp"
#include "sgl/render/Color.hpp"
#include "sgl/render/Font.hpp"
#include "sgl/render/Panel.hpp"
#include "sgl/render/Window.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace sgl::format {

/// @brief Brings a screen document to life: builds it into panels, keeps them in line with it, and runs actions.
///
/// The document and the state come from a VersionControl (which must outlive the runtime). The runtime observes it:
/// when a revision lands, panels are reconciled in place, so focus, typing and drags survive. State changes during
/// one event are batched into one update. If a new version can't be built, the runtime refuses it and the
/// VersionControl rolls it back.
class Runtime {
public:
	using Action = std::function<void(Panel& sender)>;     ///< A native action.
	using Factory = std::function<std::unique_ptr<Panel>()>; ///< Creates a custom node type.
	using UnhandledAction = std::function<void(const std::string& action, Panel& sender)>; ///< Fallback handler.

	/// @brief Reads the document's colours and fonts and starts observing @p versions.
	/// @param versions The document's version control; must outlive the runtime.
	/// @param baseDirectory Where relative paths (fonts) are resolved from.
	/// @throws DocumentError If a colour or font is invalid (e.g. a missing font file).
	Runtime(VersionControl& versions, std::filesystem::path baseDirectory);
	~Runtime();

	Runtime(const Runtime&) = delete;
	Runtime& operator=(const Runtime&) = delete;

	/* Before building */

	/// @brief Registers a custom "type" usable in the document.
	/// @param name The type name.
	/// @param factory Creates its panel.
	void registerType(std::string name, Factory factory);

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

	/// @brief Sets what happens with actions nobody handles (default: a message on stderr).
	/// @param fallback The handler.
	void onUnhandledAction(UnhandledAction fallback);

	/// @brief Runs an action as if a panel triggered it.
	/// @param name The action (from the document, else native).
	/// @param sender The panel it comes from.
	/// @param event Available to the action as "event".
	void runAction(std::string_view name, Panel& sender, const json& event);

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

	/// @brief Looks a font up in "fonts".
	/// @param name The name.
	/// @return The font.
	/// @throws DocumentError If there is no such font.
	[[nodiscard]] std::shared_ptr<const Font> font(std::string_view name) const;

	Signal<const json&> onEvent;        ///< "emit" steps: `{"type": "event", "name", "data", "revision"}`.
	Signal<const std::string&> onError; ///< Errors in handlers and deferred changes (also printed on stderr).

private:
	struct Impl;
	std::unique_ptr<Impl> impl_; ///< Everything else, out of the header.
};

} // namespace sgl::format
