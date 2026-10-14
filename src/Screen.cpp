#include "sgl/Screen.hpp"
#include "sgl/Application.hpp"
#include "sgl/Label.hpp"
#include "sgl/Stack.hpp"
#include "sgl/TextArea.hpp"
#include "sgl/Window.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

namespace sgl {

using json = nlohmann::json;

namespace {

constexpr int formatVersion = 1;

[[noreturn]] void fail(const std::string& path, const std::string& message) {
	throw ScreenError("sGLGUI screen: " + (path.empty() ? message : path + ": " + message));
}

const json& object(const json& value, const std::string& path) {
	if(!value.is_object()) fail(path, "expected an object");
	return value;
}

float number(const json& value, const std::string& path) {
	if(!value.is_number()) fail(path, "expected a number");
	return value.get<float>();
}

int integer(const json& value, const std::string& path) {
	if(!value.is_number_integer()) fail(path, "expected an integer");
	return value.get<int>();
}

bool boolean(const json& value, const std::string& path) {
	if(!value.is_boolean()) fail(path, "expected true or false");
	return value.get<bool>();
}

std::string string(const json& value, const std::string& path) {
	if(!value.is_string()) fail(path, "expected a string");
	return value.get<std::string>();
}

// 120 or "120px" -> pixels, "50%" -> percentage of the parent
Length length(const json& value, const std::string& path) {
	if(value.is_number()) return px(value.get<float>());

	if(value.is_string()) {
		std::string text = value.get<std::string>();
		Unit unit = Unit::Pixel;
		if(text.ends_with('%')) {
			unit = Unit::Percent;
			text.pop_back();
		} else if(text.ends_with("px")) {
			text.resize(text.size() - 2);
		}

		try {
			std::size_t used = 0;
			const float amount = std::stof(text, &used);
			if(used == text.size()) return {amount, unit};
		} catch(const std::exception&) {
		}
	}

	fail(path, "expected a length: a number of pixels, \"12px\" or \"50%\"");
}

Align align(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "start") return Align::Start;
	if(name == "center") return Align::Center;
	if(name == "end") return Align::End;
	fail(path, "expected \"start\", \"center\" or \"end\"");
}

Cursor cursor(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "arrow") return Cursor::Arrow;
	if(name == "ibeam") return Cursor::IBeam;
	if(name == "hand") return Cursor::Hand;
	if(name == "resize-horizontal") return Cursor::ResizeHorizontal;
	if(name == "resize-vertical") return Cursor::ResizeVertical;
	fail(path, "expected \"arrow\", \"ibeam\", \"hand\", \"resize-horizontal\" or \"resize-vertical\"");
}

Edge edge(const std::string& name, const std::string& path) {
	if(name == "top") return Edge::Top;
	if(name == "bottom") return Edge::Bottom;
	if(name == "left") return Edge::Left;
	if(name == "right") return Edge::Right;
	fail(path, "unknown edge '" + name + "' (expected top, bottom, left or right)");
}

// State colors: while pressed, focused or under the pointer ("pressed", "focused", "hover" in the file)
struct Interaction {
	Color base;
	std::optional<Color> hover;
	std::optional<Color> pressed;
	std::optional<Color> focus;
	bool hovered = false;
	bool down = false;
	bool focused = false;

	[[nodiscard]] Color current() const {
		if(down && pressed) return *pressed;
		if(focused && focus) return *focus;
		if(hovered && hover) return *hover;
		return base;
	}
};

} // namespace

struct Screen::Impl {
	json document;
	std::filesystem::path baseDirectory;

	std::map<std::string, Color, std::less<>> colors;
	std::map<std::string, std::shared_ptr<const Font>, std::less<>> fonts;
	std::map<std::string, Factory, std::less<>> types;
	std::map<std::string, Action, std::less<>> actions;
	UnhandledAction unhandled = [](const std::string& action, Panel& sender) {
		std::cerr << "sGLGUI screen: no handler for action '" << action << "' (from '" << sender.id() << "')\n";
	};
	bool placeholderTypes = false;
	Window* window = nullptr;

	void readHeader();
	void readColors();
	void readFonts();

	Color color(const json& value, const std::string& path) const;
	json withStyles(const json& node, const std::string& path) const;
	std::unique_ptr<Panel> create(const std::string& type, const std::string& path) const;
	Panel& build(const json& node, Panel& parent, const std::string& path);
	void apply(Panel& panel, const json& properties, const std::string& path, bool isRoot);
	void buildChildren(const json& properties, Panel& panel, const std::string& path);
	void runAction(const std::string& name, Panel& sender);
};

/* Document */

void Screen::Impl::readHeader() {
	object(document, "");

	static constexpr std::string_view sections[] = {"$schema", "format", "version",   "window", "fonts",
	                                                "colors",  "styles", "templates", "root"};
	for(const auto& [key, value] : document.items()) {
		if(std::ranges::find(sections, key) == std::end(sections)) fail(key, "unknown section");
	}

	if(document.contains("format") && string(document["format"], "format") != "sglgui-screen")
		fail("format", "expected \"sglgui-screen\"");
	if(!document.contains("version")) fail("", "missing \"version\" (this library reads version 1)");
	if(integer(document["version"], "version") != formatVersion) fail("version", "unsupported version (expected 1)");
	if(!document.contains("root")) fail("", "missing \"root\"");
}

void Screen::Impl::readColors() {
	if(!document.contains("colors")) return;
	for(const auto& [name, value] : object(document["colors"], "colors").items()) {
		const std::string path = "colors." + name;
		const auto parsed = Color::parseHex(string(value, path));
		if(!parsed) fail(path, "expected a hex color like \"#6366f1\"");
		colors.emplace(name, *parsed);
	}
}

void Screen::Impl::readFonts() {
	if(!document.contains("fonts")) return;
	for(const auto& [name, value] : object(document["fonts"], "fonts").items()) {
		const std::string path = "fonts." + name;
		object(value, path);
		if(!value.contains("file") || !value.contains("size")) fail(path, "a font needs \"file\" and \"size\"");

		const std::filesystem::path file = baseDirectory / string(value["file"], path + ".file");
		try {
			fonts.emplace(name, Font::load(file, number(value["size"], path + ".size")));
		} catch(const std::runtime_error& error) {
			fail(path, error.what());
		}
	}
}

// "#rrggbb", "@name" (from "colors") or "transparent"
Color Screen::Impl::color(const json& value, const std::string& path) const {
	const std::string text = string(value, path);
	if(text == "transparent") return colors::transparent;

	if(text.starts_with('@')) {
		const auto it = colors.find(std::string_view(text).substr(1));
		if(it == colors.end()) fail(path, "unknown color '" + text + "'");
		return it->second;
	}

	const auto parsed = Color::parseHex(text);
	if(!parsed) fail(path, "expected a color: \"#rrggbb\", \"#rrggbbaa\", \"@name\" or \"transparent\"");
	return *parsed;
}

// The node's properties on top of its styles' (in order: later styles override earlier ones)
json Screen::Impl::withStyles(const json& node, const std::string& path) const {
	object(node, path);
	if(!node.contains("style")) return node;

	const json& style = node["style"];
	const json names = style.is_array() ? style : json::array({style});

	json merged = json::object();
	for(const json& name : names) {
		const std::string key = string(name, path + ".style");
		if(!document.contains("styles") || !document["styles"].contains(key))
			fail(path + ".style", "unknown style '" + key + "'");
		merged.update(object(document["styles"][key], "styles." + key));
	}

	json own = node;
	own.erase("style");
	merged.update(own);
	return merged;
}

std::unique_ptr<Panel> Screen::Impl::create(const std::string& type, const std::string& path) const {
	if(type == "panel") return std::make_unique<Panel>();
	if(type == "label") return std::make_unique<Label>();
	if(type == "textarea") return std::make_unique<TextArea>();
	if(type == "stack") return std::make_unique<Stack>();

	if(const auto it = types.find(type); it != types.end()) {
		auto panel = it->second();
		if(!panel) fail(path, "the factory of type '" + type + "' returned nothing");
		return panel;
	}

	if(placeholderTypes) {
		std::cerr << "sGLGUI screen: " << path << ": unknown type '" << type << "', shown as a plain panel\n";
		return std::make_unique<Panel>();
	}
	fail(path, "unknown type '" + type + "' (built in: panel, label, textarea, stack; others must be registered)");
}

/* Building */

Panel& Screen::Impl::build(const json& node, Panel& parent, const std::string& path) {
	const json properties = withStyles(node, path);
	const std::string type = properties.contains("type") ? string(properties["type"], path + ".type") : "panel";

	Panel& panel = parent.addChild(create(type, path));
	apply(panel, properties, path, false);
	buildChildren(properties, panel, path);
	return panel;
}

void Screen::Impl::buildChildren(const json& properties, Panel& panel, const std::string& path) {
	if(!properties.contains("children")) return;

	const json& children = properties["children"];
	if(!children.is_array()) fail(path + ".children", "expected an array");
	for(std::size_t i = 0; i < children.size(); ++i)
		build(children[i], panel, path + ".children[" + std::to_string(i) + "]");
}

void Screen::Impl::apply(Panel& panel, const json& properties, const std::string& path, bool isRoot) {
	auto* label = dynamic_cast<Label*>(&panel);
	auto* textArea = dynamic_cast<TextArea*>(&panel);
	auto* stack = dynamic_cast<Stack*>(&panel);

	Interaction interaction;
	std::optional<std::string> onClick;

	for(const auto& [key, value] : properties.items()) {
		const std::string at = path + "." + key;

		// Common to every panel
		if(key == "type" || key == "children") continue;
		if(key == "id") panel.setId(string(value, at));
		else if(key == "color") panel.setColor(color(value, at));
		else if(key == "visible") panel.setVisible(boolean(value, at));
		else if(key == "cursor") panel.setCursor(cursor(value, at));
		else if(key == "hover") interaction.hover = color(object(value, at).value("color", json()), at + ".color");
		else if(key == "pressed") interaction.pressed = color(object(value, at).value("color", json()), at + ".color");
		else if(key == "focused") interaction.focus = color(object(value, at).value("color", json()), at + ".color");
		else if(key == "onClick") onClick = string(value, at);

		// Layout (the root always fills the window)
		else if(isRoot) fail(at, "not allowed on the root, which always fills the window");
		else if(key == "x") panel.setX(length(value, at));
		else if(key == "y") panel.setY(length(value, at));
		else if(key == "width") panel.setWidth(length(value, at));
		else if(key == "height") panel.setHeight(length(value, at));
		else if(key == "minWidth") panel.setMinWidth(length(value, at));
		else if(key == "minHeight") panel.setMinHeight(length(value, at));
		else if(key == "fill") {
			const Length offset = value.is_boolean() && value.get<bool>() ? px(0.0f) : length(value, at);
			for(Edge e : {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right}) panel.setAnchor(e, offset);
		} else if(key == "anchors") {
			for(const auto& [name, offset] : object(value, at).items())
				panel.setAnchor(edge(name, at), length(offset, at + "." + name));
		}

		// Labels and text areas
		else if(label && key == "text")
			label->setText(string(value, at));
		else if(label && key == "textColor") label->setTextColor(color(value, at));
		else if(label && key == "padding") label->setPadding(number(value, at));
		else if(label && key == "wrap") label->setWrap(boolean(value, at));
		else if(label && key == "font") {
			const auto it = fonts.find(string(value, at));
			if(it == fonts.end()) fail(at, "unknown font '" + value.get<std::string>() + "'");
			label->setFont(it->second);
		} else if(label && key == "align") {
			if(value.is_string()) {
				label->setAlignment(align(value, at), align(value, at));
			} else {
				object(value, at);
				label->setAlignment(align(value.value("horizontal", json("start")), at + ".horizontal"),
				                    align(value.value("vertical", json("start")), at + ".vertical"));
			}
		} else if(label && key == "fitText") {
			if(value.is_boolean()) label->setFitToText(value.get<bool>());
			else label->setFitToText(true, number(object(value, at).value("maxWidth", json()), at + ".maxWidth"));
		} else if(textArea && key == "placeholder") textArea->setPlaceholder(string(value, at));
		else if(textArea && key == "placeholderColor") textArea->setPlaceholderColor(color(value, at));

		// Stacks
		else if(stack && key == "gap") stack->setGap(number(value, at));
		else if(stack && key == "align") stack->setAlignment(align(value, at));

		else fail(at, "unknown property '" + key + "' for this type");
	}

	if(interaction.hover || interaction.pressed || interaction.focus) {
		auto state = std::make_shared<Interaction>(interaction);
		state->base = panel.color();
		panel.onHover.connect([&panel, state](bool hovered) {
			state->hovered = hovered;
			panel.setColor(state->current());
		});
		panel.onMouseButton.connect([&panel, state](const MouseButtonEvent& event) {
			if(event.button != MouseButton::Left) return;
			state->down = event.action == sgl::Action::Press;
			panel.setColor(state->current());
		});
		panel.onFocusChanged.connect([&panel, state](bool focused) {
			state->focused = focused;
			panel.setColor(state->current());
		});
	}

	if(onClick) {
		// A click: left button released over the panel that was pressed
		panel.onMouseButton.connect([this, &panel, name = *onClick](const MouseButtonEvent& event) {
			if(event.button == MouseButton::Left && event.action == sgl::Action::Release &&
			   panel.bounds().contains(event.position))
				runAction(name, panel);
		});
	}
}

void Screen::Impl::runAction(const std::string& name, Panel& sender) {
	if(const auto it = actions.find(name); it != actions.end()) it->second(sender);
	else if(unhandled) unhandled(name, sender);
}

/* Screen */

Screen::Screen(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Screen::Screen(Screen&&) noexcept = default;
Screen& Screen::operator=(Screen&&) noexcept = default;
Screen::~Screen() = default;

Screen Screen::fromFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	if(!file) {
		throw ScreenError(
		    "sGLGUI screen: could not open " + std::filesystem::absolute(path).string() +
		    " (relative paths start from the current folder: " + std::filesystem::current_path().string() + ")");
	}

	std::stringstream text;
	text << file.rdbuf();
	return fromString(text.str(), path.parent_path());
}

Screen Screen::fromString(std::string_view text, std::filesystem::path baseDirectory) {
	auto impl = std::make_unique<Impl>();
	impl->baseDirectory = std::move(baseDirectory);

	try {
		impl->document = json::parse(text);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("sGLGUI screen: invalid JSON: ") + error.what());
	}

	impl->readHeader();
	impl->readColors();
	impl->readFonts();
	return Screen(std::move(impl));
}

void Screen::registerType(std::string name, Factory factory) {
	impl_->types.insert_or_assign(std::move(name), std::move(factory));
}

void Screen::setPlaceholderTypes(bool enabled) { impl_->placeholderTypes = enabled; }

Window& Screen::createWindow(Application& app) {
	const json& document = impl_->document;
	const json settings = document.contains("window") ? object(document["window"], "window") : json::object();

	for(const auto& [key, value] : settings.items()) {
		if(key != "title" && key != "width" && key != "height" && key != "minWidth" && key != "minHeight")
			fail("window." + key, "unknown property");
	}

	Window& window = app.createWindow(settings.contains("title") ? string(settings["title"], "window.title") : "sGLGUI",
	                                  settings.contains("width") ? integer(settings["width"], "window.width") : 800,
	                                  settings.contains("height") ? integer(settings["height"], "window.height") : 600);
	window.setMinSize(settings.contains("minWidth") ? integer(settings["minWidth"], "window.minWidth") : 0,
	                  settings.contains("minHeight") ? integer(settings["minHeight"], "window.minHeight") : 0);

	build(window);
	return window;
}

void Screen::build(Window& window) {
	impl_->window = &window;
	if(const auto it = impl_->fonts.find("default"); it != impl_->fonts.end()) window.setDefaultFont(it->second);

	const json root = impl_->withStyles(impl_->document["root"], "root");
	impl_->apply(window.root(), root, "root", true);
	impl_->buildChildren(root, window.root(), "root");
}

Panel& Screen::instantiate(std::string_view name, Panel& parent) {
	const json& document = impl_->document;
	const std::string key(name);
	if(!document.contains("templates") || !document["templates"].contains(key))
		throw ScreenError("sGLGUI screen: unknown template '" + key + "'");
	return impl_->build(document["templates"][key], parent, "templates." + key);
}

void Screen::onAction(std::string name, Action action) {
	impl_->actions.insert_or_assign(std::move(name), std::move(action));
}

void Screen::onUnhandledAction(UnhandledAction fallback) { impl_->unhandled = std::move(fallback); }

Panel* Screen::find(std::string_view id) noexcept {
	return impl_->window ? impl_->window->root().findById(id) : nullptr;
}

Color Screen::color(std::string_view name) const {
	const auto it = impl_->colors.find(name);
	if(it == impl_->colors.end()) throw ScreenError("sGLGUI screen: unknown color '" + std::string(name) + "'");
	return it->second;
}

std::shared_ptr<const Font> Screen::font(std::string_view name) const {
	const auto it = impl_->fonts.find(name);
	if(it == impl_->fonts.end()) throw ScreenError("sGLGUI screen: unknown font '" + std::string(name) + "'");
	return it->second;
}

} // namespace sgl
