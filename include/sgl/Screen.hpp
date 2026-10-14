#pragma once

#include "sgl/Color.hpp"
#include "sgl/Font.hpp"
#include "sgl/Panel.hpp"

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sgl {

class Application;
class Window;

// A malformed screen file. what() names the place in the JSON, e.g. "root.children[1].anchors: unknown property
// 'lefft'"
class ScreenError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

// A UI described in the sGLGUI Screen Format (JSON, see SCREEN_FORMAT.md) and built into a Window.
// Code adds behaviour: it handles named actions ("onClick": "send"), finds panels by id, registers custom panel types
// and instantiates templates (e.g. one per chat message). Keep the Screen alive while its window is in use.
class Screen {
public:
	using Action = std::function<void(Panel& sender)>;
	using Factory = std::function<std::unique_ptr<Panel>()>;
	using UnhandledAction = std::function<void(const std::string& action, Panel& sender)>;

	[[nodiscard]] static Screen fromFile(const std::filesystem::path& path); // throws ScreenError
	// `baseDirectory` is where relative paths (fonts) are resolved from
	[[nodiscard]] static Screen fromString(std::string_view json, std::filesystem::path baseDirectory = {});

	Screen(Screen&&) noexcept;
	Screen& operator=(Screen&&) noexcept;
	~Screen();

	/* Before building */
	void registerType(std::string name, Factory factory); // a custom "type" usable in the file
	void setPlaceholderTypes(bool enabled);               // unknown types become plain panels instead of an error

	/* Building */
	Window& createWindow(Application& app); // window from the "window" section, then build()
	void build(Window& window);             // fonts, then "root" into the window's root panel

	// Builds a copy of the template `name` as a new child of `parent`
	Panel& instantiate(std::string_view name, Panel& parent);

	/* Behaviour */
	void onAction(std::string name, Action action);
	void onUnhandledAction(UnhandledAction fallback); // for actions nobody handles (default: report on stderr)

	/* Lookup (after build) */
	[[nodiscard]] Panel* find(std::string_view id) noexcept;

	template <std::derived_from<Panel> T = Panel> [[nodiscard]] T& get(std::string_view id) {
		T* panel = dynamic_cast<T*>(find(id));
		if(!panel) throw ScreenError("sGLGUI screen: no panel with id '" + std::string(id) + "' of the requested type");
		return *panel;
	}

	[[nodiscard]] Color color(std::string_view name) const;                      // from "colors"
	[[nodiscard]] std::shared_ptr<const Font> font(std::string_view name) const; // from "fonts"

private:
	struct Impl;
	explicit Screen(std::unique_ptr<Impl> impl);

	std::unique_ptr<Impl> impl_;
};

} // namespace sgl
