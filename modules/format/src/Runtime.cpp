#include "affresco/format/Runtime.hpp"
#include "affresco/render/Label.hpp"
#include "affresco/render/Stack.hpp"
#include "affresco/render/TextArea.hpp"

#include "Expression.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace affresco::format {

using expect::boolean;
using expect::fail;
using expect::integer;
using expect::number;
using expect::object;
using expect::string;

namespace {

/* Reading values */

/// @brief Reads a text property; numbers and booleans from bindings are shown as text.
/// @param value The value.
/// @return The text.
std::string text(const json& value) { return toText(value); }

/// @brief Reads a length: 120 or "120px" (pixels), "50%" (of the parent).
/// @param value The value.
/// @param path Its place, for errors.
/// @return The length.
/// @throws DocumentError If it is not a length.
Length length(const json& value, const std::string& path) {
	if(value.is_number()) return px(value.get<float>());

	if(value.is_string()) {
		std::string t = value.get<std::string>();
		Unit unit = Unit::Pixel;
		if(t.ends_with('%')) {
			unit = Unit::Percent;
			t.pop_back();
		} else if(t.ends_with("px")) {
			t.resize(t.size() - 2);
		}

		try {
			std::size_t used = 0;
			const float amount = std::stof(t, &used);
			if(used == t.size()) return {amount, unit};
		} catch(const std::exception&) {
		}
	}

	fail(path, "expected a length: a number of pixels, \"12px\" or \"50%\"");
}

/// @brief Reads "start", "center" or "end".
/// @param value The value.
/// @param path Its place, for errors.
/// @return The alignment.
/// @throws DocumentError If it is something else.
Align align(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "start") return Align::Start;
	if(name == "center") return Align::Center;
	if(name == "end") return Align::End;
	fail(path, "expected \"start\", \"center\" or \"end\"");
}

/// @brief Reads a cursor name, e.g. "hand".
/// @param value The value.
/// @param path Its place, for errors.
/// @return The cursor.
/// @throws DocumentError If the name is unknown.
Cursor cursor(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "arrow") return Cursor::Arrow;
	if(name == "ibeam") return Cursor::IBeam;
	if(name == "hand") return Cursor::Hand;
	if(name == "resize-horizontal") return Cursor::ResizeHorizontal;
	if(name == "resize-vertical") return Cursor::ResizeVertical;
	fail(path, "expected \"arrow\", \"ibeam\", \"hand\", \"resize-horizontal\" or \"resize-vertical\"");
}

/// @brief Reads an edge name: "top", "bottom", "left" or "right".
/// @param name The name.
/// @param path Its place, for errors.
/// @return The edge.
/// @throws DocumentError If the name is unknown.
Edge edge(const std::string& name, const std::string& path) {
	if(name == "top") return Edge::Top;
	if(name == "bottom") return Edge::Bottom;
	if(name == "left") return Edge::Left;
	if(name == "right") return Edge::Right;
	fail(path, "unknown edge '" + name + "' (expected top, bottom, left or right)");
}

/// @brief Tells whether a node key describes structure or behaviour, so it is never evaluated or applied.
/// @param key The key.
/// @return true for children, repeat, when, onClick, onSubmit and bind.
bool structural(const std::string& key) {
	return key == "children" || key == "repeat" || key == "when" || key == "onClick" || key == "onSubmit" ||
	       key == "bind";
}

/// @brief State colours: while pressed, focused or under the pointer ("pressed", "focused", "hover" in the file).
struct Interaction {
	Color base;                   ///< Colour at rest.
	std::optional<Color> hover;   ///< Under the pointer.
	std::optional<Color> pressed; ///< While pressed.
	std::optional<Color> focus;   ///< While focused.
	bool hovered = false;         ///< Under the pointer now.
	bool down = false;            ///< Pressed now.
	bool focused = false;         ///< Focused now.

	/// @brief Whether any state colour is set.
	[[nodiscard]] bool any() const { return hover || pressed || focus; }
	/// @brief The colour to show now: pressed, then focused, then hovered, then base.
	[[nodiscard]] Color current() const {
		if(down && pressed) return *pressed;
		if(focused && focus) return *focus;
		if(hovered && hover) return *hover;
		return base;
	}
};

/// @brief What the runtime knows about each panel it built.
struct Record {
	std::string type;                     ///< The node's "type".
	std::size_t firstManaged = 0;         ///< Children a factory created come first and are left alone.
	std::size_t managed = 0;              ///< Children built from the document.
	Color defaultColor;                   ///< Restored when "color" goes away.
	Cursor defaultCursor = Cursor::Arrow; ///< Restored when "cursor" goes away.
	json locals = json::object();         ///< Repeat variables visible to its bindings and handlers.
	json onClick;                         ///< Action name or inline steps (null: none).
	json onSubmit;                        ///< Same, for Enter in a text area.
	std::string path;                     ///< Place in the document, for errors.
	std::optional<std::string> bind;      ///< State path a text area mirrors.
	std::optional<std::string> lastText;  ///< "text" applied last time.
	Interaction interaction;              ///< State colours.
};

/// @brief A child to build.
struct Item {
	json node;        ///< Styles merged.
	json locals;      ///< Its variables.
	std::string path; ///< Its place in the document.
};

constexpr int maxActionDepth = 32;               ///< Nested action calls before giving up (recursion guard).
constexpr std::size_t maxLoopIterations = 10000; ///< "while" iterations before giving up.

} // namespace

/// @brief The runtime's state and logic; observes the VersionControl.
struct Runtime::Impl final : VersionObserver {
	Runtime& owner;                      ///< For onEvent and onError.
	VersionControl& versions;            ///< The document and state.
	std::filesystem::path baseDirectory; ///< Where font paths are resolved from.

	std::map<std::string, Color, std::less<>> colors;                      ///< From "colors".
	std::map<std::string, std::shared_ptr<const Font>, std::less<>> fonts; ///< From "fonts".
	std::map<std::string, std::shared_ptr<const Font>> fontCache; ///< By "file@size": revisions don't reload fonts.

	std::map<std::string, Factory, std::less<>> types;        ///< Custom node types.
	std::map<std::string, Action, std::less<>> nativeActions; ///< Native actions.
	UnhandledAction unhandled = [](const std::string& action, Panel& sender) {
		std::cerr << "AffrescoUI screen: no handler for action '" << action << "' (from '" << sender.id() << "')\n";
	};
	bool placeholderTypes = false; ///< Unknown types become plain panels.

	Window* window = nullptr;                         ///< Set by build().
	std::unordered_map<const Panel*, Record> records; ///< Every panel built from the document.

	bool stateChanged = false;    ///< An action changed the state.
	bool reconcileQueued = false; ///< A reconcile is deferred to the end of the event.
	int actionDepth = 0;          ///< Nested action calls in progress.

	Impl(Runtime& runtime, VersionControl& vcs, std::filesystem::path base)
	    : owner(runtime), versions(vcs), baseDirectory(std::move(base)) {}

	/* VersionObserver */
	/// @brief Re-reads colours and fonts, then reconciles.
	void documentChanged() override {
		readDerived();
		reconcile();
	}
	/// @brief Refreshes the globals and schedules a reconcile (a review UI may list the proposals).
	void proposalsChanged() override {
		globalsChanged();
		requestReconcile(); // a review UI may list the proposals
	}

	/* Document */
	/// @brief Reads "colors" and "fonts", changing nothing on error.
	/// @throws DocumentError If a colour or font is invalid.
	void readDerived();

	/* Values */
	/// @brief Screen-wide variables for expressions.
	struct Globals {
		json window, revision, other; ///< Window size, current revision, proposals/history/preview.
	};
	/// @brief The globals, built once and reused until the history, proposals, preview or window size change.
	const Globals& globals() const;
	/// @brief Drops the cached globals.
	void globalsChanged() const { globalsCache.reset(); }
	mutable std::optional<Globals> globalsCache; ///< See globals().
	/// @brief Evaluates a value with the state, the globals and the given variables.
	/// @param value The value.
	/// @param locals "repeat" / "forEach" variables.
	/// @param event The event being handled.
	/// @param where Its place, for errors.
	/// @return The evaluated value.
	/// @throws DocumentError If it is invalid.
	json evaluate(const json& value, const json& locals, const json& event, const std::string& where) const;
	/// @brief Reads a colour: "#rrggbb", "@name" (from "colors") or "transparent".
	/// @param value The value.
	/// @param path Its place, for errors.
	/// @return The colour.
	/// @throws DocumentError If it is not a colour.
	Color color(const json& value, const std::string& path) const;
	/// @brief Merges a node's properties on top of its styles' (later styles override earlier ones).
	/// @param node The node.
	/// @param path Its place, for errors.
	/// @return The merged node.
	/// @throws DocumentError If a style is unknown.
	json withStyles(const json& node, const std::string& path) const;

	/* Building */
	/// @brief Creates the panel for a node type (built-in, registered, or a placeholder).
	/// @param type The "type".
	/// @param path Its place, for errors.
	/// @return The panel.
	/// @throws DocumentError If the type is unknown.
	std::unique_ptr<Panel> create(const std::string& type, const std::string& path) const;
	/// @brief Starts tracking a new panel, remembering its defaults.
	/// @param panel The panel.
	/// @param type Its "type".
	/// @return Its record.
	Record& adopt(Panel& panel, const std::string& type);
	/// @brief Connects a panel's signals once; handlers read its current record.
	/// @param panel The panel.
	void hookUp(Panel& panel);
	/// @brief Evaluates a node's properties (structural keys are left as they are).
	/// @param merged The node, styles merged.
	/// @param locals Its variables.
	/// @param path Its place, for errors.
	/// @return The evaluated properties.
	json evaluateNode(const json& merged, const json& locals, const std::string& path) const;
	/// @brief Expands "repeat" and "when" into the children to build.
	/// @param children The "children" array.
	/// @param locals The parent's variables.
	/// @param path Its place, for errors.
	/// @return The children.
	std::vector<Item> expand(const json& children, const json& locals, const std::string& path) const;
	/// @brief Restores a panel's defaults, so properties no longer in the document disappear.
	/// @param panel The panel.
	/// @param record Its record.
	void reset(Panel& panel, Record& record);
	/// @brief Applies evaluated properties to a panel.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param properties The evaluated node.
	/// @param path Its place, for errors.
	/// @param isRoot Whether it is the window's root.
	/// @param created Whether it was just created.
	void apply(Panel& panel, Record& record, const json& properties, const std::string& path, bool isRoot,
	           bool created);
	/// @brief Updates a panel's children in place, creating, replacing or removing only what changed.
	/// @param properties The evaluated node.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param path Its place, for errors.
	void reconcileChildren(const json& properties, Panel& panel, Record& record, const std::string& path);
	/// @brief Drops the records of a subtree about to be destroyed.
	/// @param panel The subtree's root.
	void forgetSubtree(const Panel& panel);
	/// @brief Brings the panels in line with the document and the state.
	/// @throws DocumentError If the document can't be built (the version is then refused).
	void reconcile();
	/// @brief Schedules one reconcile after the current event (batches state changes).
	void requestReconcile();
	/// @brief Builds a node outside reconciliation (Runtime::instantiate).
	/// @param node The node.
	/// @param parent Where to add it.
	/// @param path Its place, for errors.
	/// @return The panel.
	Panel& buildDetached(const json& node, Panel& parent, const std::string& path);

	/* Behaviour */
	/// @brief Runs an event handler (an action name or inline steps); errors are reported, never thrown.
	/// @param handler The handler.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The sender's variables.
	/// @param where Its place, for errors.
	void handle(const json& handler, Panel& sender, const json& event, const json& locals, const std::string& where);
	/// @brief Runs an action from "actions", else a native one, else the unhandled fallback.
	/// @param name The action.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The caller's variables.
	/// @param where Its place, for errors.
	/// @throws DocumentError If a step fails, or nobody handles the action.
	void runNamed(const std::string& name, Panel& sender, const json& event, const json& locals,
	              const std::string& where);
	/// @brief Runs steps in order.
	/// @param steps A step or an array of steps.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals Variables; steps may add some.
	/// @param where Its place, for errors.
	/// @return false if a "stop" step ended the action.
	/// @throws DocumentError If a step fails or actions nest more than 32 levels deep.
	bool runSteps(const json& steps, Panel& sender, const json& event, json& locals, const std::string& where);
	/// @brief Runs one step (set, push, if, forEach, while, emit, patch, call, ...).
	/// @param step The step.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals Variables; the step may add some.
	/// @param where Its place, for errors.
	/// @return false if it was "stop".
	/// @throws DocumentError If it fails.
	bool runStep(const json& step, Panel& sender, const json& event, json& locals, const std::string& where);
	/// @brief Prints an error on stderr and fires onError.
	/// @param message The error.
	void report(const std::string& message);
};

/* Document */

// Throws, changing nothing, if a color or a font is invalid
void Runtime::Impl::readDerived() {
	const json& doc = versions.document();
	decltype(colors) newColors;
	decltype(fonts) newFonts;

	if(doc.contains("colors")) {
		for(const auto& [name, value] : object(doc["colors"], "colors").items()) {
			const std::string path = "colors." + name;
			const auto parsed = Color::parseHex(string(value, path));
			if(!parsed) fail(path, "expected a hex color like \"#6366f1\"");
			newColors.emplace(name, *parsed);
		}
	}

	if(doc.contains("fonts")) {
		for(const auto& [name, value] : object(doc["fonts"], "fonts").items()) {
			const std::string path = "fonts." + name;
			object(value, path);
			if(!value.contains("file") || !value.contains("size")) fail(path, "a font needs \"file\" and \"size\"");

			const std::string file = (baseDirectory / string(value["file"], path + ".file")).string();
			const float size = number(value["size"], path + ".size");
			const std::string key = file + "@" + std::to_string(size);

			auto cached = fontCache.find(key);
			if(cached == fontCache.end()) {
				try {
					cached = fontCache.emplace(key, Font::load(file, size)).first;
				} catch(const std::runtime_error& error) {
					fail(path, error.what());
				}
			}
			newFonts.emplace(name, cached->second);
		}
	}

	colors = std::move(newColors);
	fonts = std::move(newFonts);
}

/* Values */

const Runtime::Impl::Globals& Runtime::Impl::globals() const {
	if(globalsCache) return *globalsCache;
	Globals& g = globalsCache.emplace();
	g.window = window ? json{{"width", window->width()}, {"height", window->height()}} : json::object();
	g.revision = toJson(versions.head());

	json list = json::array();
	for(const ProposalInfo& p : versions.proposals()) list.push_back(toJson(p));
	json log = json::array();
	for(const CommitInfo& c : versions.log()) log.push_back(toJson(c));

	const auto previewing = versions.previewing();
	g.other = {{"proposals", list},
	           {"history", log},
	           {"preview", previewing ? json(*previewing) : json()},
	           {"mode", toString(versions.mode())}};
	return g;
}

json Runtime::Impl::evaluate(const json& value, const json& locals, const json& event, const std::string& where) const {
	const Globals& g = globals();
	return format::evaluate(value, format::Context{versions.state(), g.window, g.revision, event, locals, g.other},
	                        where);
}

// "#rrggbb", "@name" (from "colors") or "transparent"
Color Runtime::Impl::color(const json& value, const std::string& path) const {
	const std::string name = string(value, path);
	if(name == "transparent") return colors::transparent;

	if(name.starts_with('@')) {
		const auto it = colors.find(std::string_view(name).substr(1));
		if(it == colors.end()) fail(path, "unknown color '" + name + "'");
		return it->second;
	}

	const auto parsed = Color::parseHex(name);
	if(!parsed) fail(path, "expected a color: \"#rrggbb\", \"#rrggbbaa\", \"@name\" or \"transparent\"");
	return *parsed;
}

// The node's properties on top of its styles' (in order: later styles override earlier ones)
json Runtime::Impl::withStyles(const json& node, const std::string& path) const {
	object(node, path);
	if(!node.contains("style")) return node;

	const json& style = node["style"];
	const json names = style.is_array() ? style : json::array({style});

	json merged = json::object();
	for(const json& name : names) {
		const std::string key = string(name, path + ".style");
		const json& document = versions.document();
		if(!document.contains("styles") || !document["styles"].contains(key))
			fail(path + ".style", "unknown style '" + key + "'");
		merged.update(object(document["styles"][key], "styles." + key));
	}

	json own = node;
	own.erase("style");
	merged.update(own);
	return merged;
}

/* Building */

std::unique_ptr<Panel> Runtime::Impl::create(const std::string& type, const std::string& path) const {
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
		std::cerr << "AffrescoUI screen: " << path << ": unknown type '" << type << "', shown as a plain panel\n";
		return std::make_unique<Panel>();
	}
	fail(path, "unknown type '" + type + "' (built in: panel, label, textarea, stack; others must be registered)");
}

// Starts tracking a newly created panel, remembering its defaults so they can be restored later
Record& Runtime::Impl::adopt(Panel& panel, const std::string& type) {
	Record& record = records[&panel];
	record.type = type;
	record.firstManaged = panel.children().size();
	record.defaultColor = panel.color();
	record.defaultCursor = panel.cursor();
	hookUp(panel);
	return record;
}

// Signals are connected once; handlers read the panel's current record, so later updates need no reconnecting
void Runtime::Impl::hookUp(Panel& panel) {
	auto refresh = [this](Panel& p) {
		if(auto it = records.find(&p); it != records.end() && it->second.interaction.any())
			p.setColor(it->second.interaction.current());
	};

	panel.onHover.connect([this, &panel, refresh](bool hovered) {
		if(auto it = records.find(&panel); it != records.end()) it->second.interaction.hovered = hovered;
		refresh(panel);
	});
	panel.onFocusChanged.connect([this, &panel, refresh](bool focused) {
		if(auto it = records.find(&panel); it != records.end()) it->second.interaction.focused = focused;
		refresh(panel);
	});
	panel.onMouseButton.connect([this, &panel, refresh](const MouseButtonEvent& event) {
		if(event.button != MouseButton::Left) return;
		auto it = records.find(&panel);
		if(it == records.end()) return;

		it->second.interaction.down = event.action == affresco::Action::Press;
		refresh(panel);

		// A click: left button released over the panel that was pressed
		if(event.action == affresco::Action::Release && panel.bounds().contains(event.position) &&
		   !it->second.onClick.is_null()) {
			const json clickEvent = {{"type", "click"}, {"x", event.position.x}, {"y", event.position.y}};
			handle(it->second.onClick, panel, clickEvent, it->second.locals, it->second.path + ".onClick");
		}
	});

	if(auto* textArea = dynamic_cast<TextArea*>(&panel)) {
		textArea->onTextChanged.connect([this, &panel](const std::string& value) {
			auto it = records.find(&panel);
			if(it == records.end() || !it->second.bind) return;
			try {
				const json event = json::object();
				const Globals& g = globals();
				const format::Context context{versions.state(),  g.window, g.revision, event,
				                              it->second.locals, g.other};
				versions.state()[format::statePointer(*it->second.bind, context, it->second.path + ".bind")] = value;
				requestReconcile();
			} catch(const DocumentError& error) {
				report(error.what());
			}
		});
		textArea->onKey.connect([this, &panel, textArea](const KeyEvent& event) {
			const bool enter = event.key == Key::Enter || event.key == Key::KeypadEnter;
			auto it = records.find(&panel);
			if(!enter || event.action != affresco::Action::Press || it == records.end() ||
			   it->second.onSubmit.is_null())
				return;
			const json submitEvent = {{"type", "submit"}, {"text", textArea->text()}};
			handle(it->second.onSubmit, panel, submitEvent, it->second.locals, it->second.path + ".onSubmit");
		});
	}
}

json Runtime::Impl::evaluateNode(const json& merged, const json& locals, const std::string& path) const {
	json result = json::object();
	for(const auto& [key, value] : merged.items())
		result[key] =
		    structural(key) || key == "type" ? value : evaluate(value, locals, json::object(), path + "." + key);
	return result;
}

// The children to build, after "repeat" and "when"
std::vector<Item> Runtime::Impl::expand(const json& children, const json& locals, const std::string& path) const {
	std::vector<Item> items;
	if(!children.is_array()) fail(path + ".children", "expected an array");

	for(std::size_t i = 0; i < children.size(); ++i) {
		const std::string at = path + ".children[" + std::to_string(i) + "]";
		json merged = withStyles(children[i], at);

		auto visible = [&](const json& scope) {
			return !merged.contains("when") ||
			       format::truthy(evaluate(merged["when"], scope, json::object(), at + ".when"));
		};

		if(!merged.contains("repeat")) {
			if(visible(locals)) items.push_back({merged, locals, at});
			continue;
		}

		const json& repeat = object(merged["repeat"], at + ".repeat");
		const std::string name = repeat.contains("for") ? string(repeat["for"], at + ".repeat.for") : "item";
		const std::string indexName =
		    repeat.contains("index") ? string(repeat["index"], at + ".repeat.index") : "index";
		if(!repeat.contains("in")) fail(at + ".repeat", "needs \"in\": the list to repeat over");

		// "in" is a path ("state.messages") or any value/expression
		const json& source = repeat["in"];
		const json list =
		    source.is_string() && source.get<std::string>().find("{{") == std::string::npos
		        ? evaluate("{{" + source.get<std::string>() + "}}", locals, json::object(), at + ".repeat.in")
		        : evaluate(source, locals, json::object(), at + ".repeat.in");
		if(list.is_null()) continue;
		if(!list.is_array()) fail(at + ".repeat.in", "expected a list, got " + list.dump());

		for(std::size_t k = 0; k < list.size(); ++k) {
			json scope = locals;
			scope[name] = list[k];
			scope[indexName] = k;
			if(visible(scope)) items.push_back({merged, std::move(scope), at + "[" + std::to_string(k) + "]"});
		}
	}

	return items;
}

// Back to the defaults, so properties removed from the document (or no longer bound) disappear
void Runtime::Impl::reset(Panel& panel, Record& record) {
	panel.setId({});
	panel.setPosition(px(0.0f), px(0.0f));
	panel.setSize(px(0.0f), px(0.0f));
	panel.setMinSize(px(0.0f), px(0.0f));
	for(Edge e : {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right}) panel.clearAnchor(e);
	panel.setVisible(true);
	panel.setColor(record.defaultColor);
	panel.setCursor(record.defaultCursor);

	if(auto* label = dynamic_cast<Label*>(&panel)) {
		label->setFont(nullptr);
		label->setTextColor(colors::black);
		label->setPadding(0.0f);
		label->setAlignment(Align::Start, Align::Start);
		label->setWrap(false);
		label->setFitToText(false);
	}
	if(auto* textArea = dynamic_cast<TextArea*>(&panel)) {
		textArea->setPlaceholder({});
		textArea->setPlaceholderColor(Color::hex(0x9ca3af));
	}
	if(auto* stack = dynamic_cast<Stack*>(&panel)) {
		stack->setGap(0.0f);
		stack->setAlignment(Align::Start);
	}

	record.onClick = nullptr;
	record.onSubmit = nullptr;
	record.bind.reset();
	record.interaction.hover.reset();
	record.interaction.pressed.reset();
	record.interaction.focus.reset();
}

void Runtime::Impl::apply(Panel& panel, Record& record, const json& properties, const std::string& path, bool isRoot,
                          bool created) {
	reset(panel, record);

	auto* label = dynamic_cast<Label*>(&panel);
	auto* textArea = dynamic_cast<TextArea*>(&panel);
	auto* stack = dynamic_cast<Stack*>(&panel);
	bool autofocus = false;
	std::optional<std::string> newText;

	for(const auto& [key, value] : properties.items()) {
		const std::string at = path + "." + key;

		// Structure and behaviour
		if(key == "type" || key == "children" || key == "repeat" || key == "when") continue;
		if(key == "onClick") {
			if(!value.is_string() && !value.is_array() && !value.is_object())
				fail(at, "expected an action name or steps");
			record.onClick = value;
		} else if(key == "onSubmit") {
			if(!textArea) fail(at, "only text areas have \"onSubmit\"");
			record.onSubmit = value;
		} else if(key == "bind") {
			if(!textArea) fail(at, "only text areas can \"bind\"");
			record.bind = string(value, at);
		} else if(key == "autofocus") {
			autofocus = boolean(value, at);
		} else if(key == "locked") {
			if(boolean(value, at) && !properties.contains("id")) fail(at, "a locked node needs an \"id\"");
		}

		// Common to every panel
		else if(key == "id")
			panel.setId(string(value, at));
		else if(key == "color") panel.setColor(color(value, at));
		else if(key == "visible") panel.setVisible(boolean(value, at));
		else if(key == "cursor") panel.setCursor(cursor(value, at));
		else if(key == "hover")
			record.interaction.hover = color(object(value, at).value("color", json()), at + ".color");
		else if(key == "pressed")
			record.interaction.pressed = color(object(value, at).value("color", json()), at + ".color");
		else if(key == "focused")
			record.interaction.focus = color(object(value, at).value("color", json()), at + ".color");

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
			newText = text(value);
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
		} else if(textArea && key == "placeholder") textArea->setPlaceholder(text(value));
		else if(textArea && key == "placeholderColor") textArea->setPlaceholderColor(color(value, at));

		// Stacks
		else if(stack && key == "gap") stack->setGap(number(value, at));
		else if(stack && key == "align") stack->setAlignment(align(value, at));

		else fail(at, "unknown property '" + key + "' for this type");
	}

	// Text: a bound text area mirrors its state value; otherwise "text" is applied when it changes,
	// so a text area's typing isn't overwritten by every update
	if(label && record.bind) {
		const json event = json::object();
		const Globals& g = globals();
		const format::Context context{versions.state(), g.window, g.revision, event, record.locals, g.other};
		const std::string value = text(format::lookup(*record.bind, context, path + ".bind"));
		if(label->text() != value) label->setText(value);
	} else if(label && newText != record.lastText) {
		label->setText(newText.value_or(""));
	}
	record.lastText = newText;

	record.interaction.base = panel.color();
	if(record.interaction.any()) panel.setColor(record.interaction.current());

	if(created && autofocus && window) window->setFocus(&panel);
}

void Runtime::Impl::reconcileChildren(const json& properties, Panel& panel, Record& record, const std::string& path) {
	std::vector<Item> items;
	if(properties.contains("children")) items = expand(properties["children"], record.locals, path);
	else if(record.managed == 0) return; // nothing built here before: leave the panel's own children alone

	std::size_t index = record.firstManaged;
	for(Item& item : items) {
		const json props = evaluateNode(item.node, item.locals, item.path);
		const std::string type = props.contains("type") ? string(props["type"], item.path + ".type") : "panel";

		// Reuse the panel already in this place if it has the same type: it keeps focus, hover and drag state
		Panel* child = nullptr;
		bool created = false;
		if(index < panel.children().size()) {
			Panel& existing = *panel.children()[index];
			const auto it = records.find(&existing);
			if(it != records.end() && it->second.type == type) {
				child = &existing;
			} else {
				forgetSubtree(existing);
				child = &panel.replaceChild(index, create(type, item.path));
				created = true;
			}
		} else {
			child = &panel.addChild(create(type, item.path));
			created = true;
		}

		Record& childRecord = created ? adopt(*child, type) : records.at(child);
		childRecord.locals = std::move(item.locals);
		childRecord.path = item.path;
		apply(*child, childRecord, props, item.path, false, created);
		reconcileChildren(props, *child, childRecord, item.path);
		++index;
	}

	for(std::size_t i = index; i < panel.children().size(); ++i) forgetSubtree(*panel.children()[i]);
	panel.removeChildren(index);
	record.managed = index - record.firstManaged;
}

void Runtime::Impl::forgetSubtree(const Panel& panel) {
	records.erase(&panel);
	for(const auto& child : panel.children()) forgetSubtree(*child);
}

// Brings the UI in line with the document and the state
void Runtime::Impl::reconcile() {
	if(!window) return;
	reconcileQueued = false;
	stateChanged = false;
	globalsChanged(); // window size, history, proposals: refreshed once per update

	// Revisions can change the fonts: the default one is the window's
	if(const auto it = fonts.find("default"); it != fonts.end() && window->defaultFont() != it->second.get())
		window->setDefaultFont(it->second);

	Panel& root = window->root();
	if(!records.contains(&root)) adopt(root, "root");
	Record& record = records.at(&root);
	record.path = "root";

	const json properties = evaluateNode(withStyles(versions.document()["root"], "root"), record.locals, "root");
	apply(root, record, properties, "root", true, false);
	reconcileChildren(properties, root, record, "root");
}

// Updates are batched: many state changes during one event become one reconcile, after the event is delivered
void Runtime::Impl::requestReconcile() {
	stateChanged = true;
	if(reconcileQueued || !window) return;
	reconcileQueued = true;
	window->defer([this] {
		if(!reconcileQueued) return;
		try {
			reconcile();
		} catch(const DocumentError& error) {
			report(error.what());
		}
	});
}

// A template built outside reconciliation (Runtime::instantiate)
Panel& Runtime::Impl::buildDetached(const json& node, Panel& parent, const std::string& path) {
	const json merged = withStyles(node, path);
	const json props = evaluateNode(merged, json::object(), path);
	const std::string type = props.contains("type") ? string(props["type"], path + ".type") : "panel";

	Panel& panel = parent.addChild(create(type, path));
	Record& record = adopt(panel, type);
	record.path = path;
	apply(panel, record, props, path, false, true);
	reconcileChildren(props, panel, record, path);
	return panel;
}

/* Behaviour */

void Runtime::Impl::report(const std::string& message) {
	std::cerr << message << '\n';
	owner.onError(message);
}

// Runs an event handler: an action name or inline steps. Errors are reported, never thrown into the event loop.
void Runtime::Impl::handle(const json& handler, Panel& sender, const json& event, const json& locals,
                           const std::string& where) {
	try {
		json scope = locals;
		if(handler.is_string()) runNamed(handler.get<std::string>(), sender, event, scope, where);
		else runSteps(handler, sender, event, scope, where);
	} catch(const DocumentError& error) {
		report(error.what());
	}
	if(stateChanged) requestReconcile();
}

void Runtime::Impl::runNamed(const std::string& name, Panel& sender, const json& event, const json& locals,
                             const std::string& where) {
	const json& document = versions.document();
	if(document.contains("actions") && document["actions"].contains(name)) {
		json scope = locals;
		runSteps(document["actions"][name], sender, event, scope, "actions." + name);
	} else if(const auto it = nativeActions.find(name); it != nativeActions.end()) {
		it->second(sender);
	} else if(unhandled) {
		unhandled(name, sender);
	} else {
		fail(where, "unknown action '" + name + "'");
	}
}

// Returns false when a "stop" step ends the action
bool Runtime::Impl::runSteps(const json& steps, Panel& sender, const json& event, json& locals,
                             const std::string& where) {
	if(++actionDepth > maxActionDepth) {
		--actionDepth;
		fail(where, "actions nested too deeply (more than 32 levels)");
	}

	bool keepGoing = true;
	if(steps.is_array()) {
		for(std::size_t i = 0; i < steps.size() && keepGoing; ++i)
			keepGoing = runStep(steps[i], sender, event, locals, where + "[" + std::to_string(i) + "]");
	} else {
		keepGoing = runStep(steps, sender, event, locals, where);
	}

	--actionDepth;
	return keepGoing;
}

bool Runtime::Impl::runStep(const json& step, Panel& sender, const json& event, json& locals,
                            const std::string& where) {
	object(step, where);
	json& state = versions.state();
	const Globals& g = globals();
	const format::Context context{state, g.window, g.revision, event, locals, g.other};
	auto value = [&](const json& v, const std::string& at) { return format::evaluate(v, context, at); };
	auto target = [&](const char* key) {
		return format::statePointer(string(step[key], where + "." + key), context, where + "." + key);
	};
	// A list given as a path ("state.items") or as any value
	auto list = [&](const json& source, const std::string& at) {
		if(source.is_string() && source.get<std::string>().find("{{") == std::string::npos)
			return format::lookup(source.get<std::string>(), context, at);
		return value(source, at);
	};

	if(step.contains("set")) {
		if(!step.contains("to")) fail(where, "\"set\" needs \"to\"");
		state[target("set")] = value(step["to"], where + ".to");
		stateChanged = true;
	} else if(step.contains("push")) {
		json& array = state[target("push")];
		if(array.is_null()) array = json::array();
		if(!array.is_array()) fail(where + ".push", "is not a list");
		array.push_back(value(step.value("value", json()), where + ".value"));
		stateChanged = true;
	} else if(step.contains("remove")) {
		const json::json_pointer pointer = target("remove");
		if(step.contains("at")) {
			json& array = state[pointer];
			const json at = value(step["at"], where + ".at");
			if(!array.is_array() || !at.is_number_integer() || at.get<std::int64_t>() < 0 ||
			   static_cast<std::size_t>(at.get<std::int64_t>()) >= array.size())
				fail(where, "nothing to remove at " + at.dump());
			array.erase(static_cast<std::size_t>(at.get<std::int64_t>()));
		} else if(state.contains(pointer)) {
			state[pointer.parent_pointer()].erase(pointer.back());
		}
		stateChanged = true;
	} else if(step.contains("toggle")) {
		const json::json_pointer pointer = target("toggle");
		state[pointer] = !format::truthy(state.value(pointer, json()));
		stateChanged = true;
	} else if(step.contains("increment")) {
		const json::json_pointer pointer = target("increment");
		const json by = value(step.value("by", json(1)), where + ".by");
		json current = state.value(pointer, json(0));
		if(current.is_null()) current = 0;
		if(!current.is_number() || !by.is_number()) fail(where, "\"increment\" needs numbers");
		state[pointer] = current.is_number_integer() && by.is_number_integer()
		                     ? json(current.get<std::int64_t>() + by.get<std::int64_t>())
		                     : json(current.get<double>() + by.get<double>());
		stateChanged = true;
	} else if(step.contains("if")) {
		const bool condition = format::truthy(value(step["if"], where + ".if"));
		const char* branch = condition ? "then" : "else";
		if(step.contains(branch)) return runSteps(step[branch], sender, event, locals, where + "." + branch);
	} else if(step.contains("forEach")) {
		const json items = list(step["forEach"], where + ".forEach"); // a copy: the steps may change the list
		if(!items.is_array()) fail(where + ".forEach", "expected a list");
		if(items.size() > maxLoopIterations) fail(where + ".forEach", "list too long");
		const std::string name = step.contains("as") ? string(step["as"], where + ".as") : "item";
		const std::string indexName = step.contains("index") ? string(step["index"], where + ".index") : "index";
		for(std::size_t i = 0; i < items.size(); ++i) {
			json scope = locals;
			scope[name] = items[i];
			scope[indexName] = i;
			if(!runSteps(step.value("do", json::array()), sender, event, scope, where + ".do")) return false;
		}
	} else if(step.contains("while")) {
		std::size_t iterations = 0;
		while(format::truthy(value(step["while"], where + ".while"))) {
			if(++iterations > maxLoopIterations) fail(where, "\"while\" ran more than 10000 times");
			if(!runSteps(step.value("do", json::array()), sender, event, locals, where + ".do")) return false;
		}
	} else if(step.contains("run")) {
		runNamed(string(step["run"], where + ".run"), sender, event, locals, where + ".run");
	} else if(step.contains("call")) {
		const std::string name = string(step["call"], where + ".call");
		const auto it = nativeActions.find(name);
		if(it == nativeActions.end()) fail(where + ".call", "no native action '" + name + "' registered");
		it->second(sender);
	} else if(step.contains("emit")) {
		const json message = {{"type", "event"},
		                      {"name", string(step["emit"], where + ".emit")},
		                      {"data", value(step.value("data", json()), where + ".data")},
		                      {"revision", versions.head().number}};
		owner.onEvent(message);
	} else if(step.contains("patch")) {
		// Applied after the event, like any other document change
		json revision = {{"author", "app"},
		                 {"message", step.contains("message") ? value(step["message"], where + ".message") : json("")},
		                 {"patch", value(step["patch"], where + ".patch")}};
		if(window) {
			window->defer([this, revision = std::move(revision)] {
				if(const auto error = versions.commit(revision)) report(*error);
			});
		}
	} else if(step.contains("accept") || step.contains("reject") || step.contains("preview") ||
	          step.contains("restore") || step.contains("revert") || step.contains("undo") ||
	          step.contains("endPreview")) {
		// Review and history controls change the document: done after the event, like revisions
		json control = step;
		for(const char* key : {"accept", "reject", "preview", "restore", "revert", "reason"})
			if(control.contains(key)) control[key] = value(control[key], where + "." + key);
		for(const char* key : {"accept", "reject", "preview"})
			if(control.contains(key) && !control[key].is_number_integer()) fail(where + "." + key, "expected a number");
		for(const char* key : {"restore", "revert"})
			if(control.contains(key) && !control[key].is_number_integer() && !control[key].is_string())
				fail(where + "." + key, "expected a revision number or a commit");
		if(window) {
			window->defer([this, control = std::move(control)] {
				std::optional<std::string> error;
				try {
					if(control.contains("accept"))
						error = versions.accept(control["accept"].get<int>(), {"user", {}, false});
					else if(control.contains("reject"))
						versions.reject(control["reject"].get<int>(), text(control.value("reason", json(""))));
					else if(control.contains("preview")) error = versions.preview(control["preview"].get<int>());
					else if(control.contains("endPreview")) versions.endPreview();
					else if(control.contains("restore"))
						error = versions.commit(versions.restoreRevision(control["restore"], "user"));
					else if(control.contains("revert"))
						error = versions.commit(versions.revertRevision(control["revert"], "user"));
					else error = versions.commit(versions.revertRevision("HEAD", "user")); // undo
				} catch(const DocumentError& failure) {
					error = failure.what();
				}
				if(error) report(*error);
				requestReconcile();
			});
		}
	} else if(step.contains("focus")) {
		const std::string id = text(value(step["focus"], where + ".focus"));
		if(window) {
			window->defer([this, id] {
				if(Panel* panel = window->root().findById(id)) window->setFocus(panel);
			});
		}
	} else if(step.contains("log")) {
		std::cerr << "AffrescoUI screen log: " << text(value(step["log"], where + ".log")) << '\n';
	} else if(step.contains("stop")) {
		return false;
	} else {
		fail(where, "unknown step (expected set, push, remove, toggle, increment, if, forEach, while, run, call, emit, "
		            "patch, focus, log, stop, accept, reject, preview, endPreview, restore, revert or undo)");
	}

	return true;
}

/* Runtime */

Runtime::Runtime(VersionControl& versions, std::filesystem::path baseDirectory)
    : impl_(std::make_unique<Impl>(*this, versions, std::move(baseDirectory))) {
	impl_->readDerived();
	versions.setObserver(impl_.get());
}

Runtime::~Runtime() { impl_->versions.setObserver(nullptr); }

void Runtime::registerType(std::string name, Factory factory) {
	impl_->types.insert_or_assign(std::move(name), std::move(factory));
}

void Runtime::setPlaceholderTypes(bool enabled) { impl_->placeholderTypes = enabled; }

Window& Runtime::createWindow(Application& app) {
	const json& document = impl_->versions.document();
	const json settings = document.contains("window") ? object(document["window"], "window") : json::object();

	for(const auto& [key, value] : settings.items()) {
		if(key != "title" && key != "width" && key != "height" && key != "minWidth" && key != "minHeight")
			fail("window." + key, "unknown property");
	}

	Window& window =
	    app.createWindow(settings.contains("title") ? string(settings["title"], "window.title") : "AffrescoUI",
		                 settings.contains("width") ? integer(settings["width"], "window.width") : 800,
		                 settings.contains("height") ? integer(settings["height"], "window.height") : 600);
	window.setMinSize(settings.contains("minWidth") ? integer(settings["minWidth"], "window.minWidth") : 0,
	                  settings.contains("minHeight") ? integer(settings["minHeight"], "window.minHeight") : 0);

	build(window);
	return window;
}

void Runtime::build(Window& window) {
	impl_->window = &window;
	impl_->reconcile();

	// Bindings can use the window size
	window.onResize.connect([impl = impl_.get()](const ResizeEvent&) { impl->requestReconcile(); });
}

Panel& Runtime::instantiate(std::string_view name, Panel& parent) {
	const json& document = impl_->versions.document();
	const std::string key(name);
	if(!document.contains("templates") || !document["templates"].contains(key))
		throw DocumentError("AffrescoUI screen: unknown template '" + key + "'");
	return impl_->buildDetached(document["templates"][key], parent, "templates." + key);
}

void Runtime::onAction(std::string name, Action action) {
	impl_->nativeActions.insert_or_assign(std::move(name), std::move(action));
}

void Runtime::onUnhandledAction(UnhandledAction fallback) { impl_->unhandled = std::move(fallback); }

void Runtime::runAction(std::string_view name, Panel& sender, const json& event) {
	const auto it = impl_->records.find(&sender);
	const json locals = it != impl_->records.end() ? it->second.locals : json::object();
	impl_->handle(json(std::string(name)), sender, event, locals, "runAction");
}

Panel* Runtime::find(std::string_view id) noexcept {
	return impl_->window ? impl_->window->root().findById(id) : nullptr;
}

Color Runtime::color(std::string_view name) const {
	const auto it = impl_->colors.find(name);
	if(it == impl_->colors.end()) throw DocumentError("AffrescoUI screen: unknown color '" + std::string(name) + "'");
	return it->second;
}

std::shared_ptr<const Font> Runtime::font(std::string_view name) const {
	const auto it = impl_->fonts.find(name);
	if(it == impl_->fonts.end()) throw DocumentError("AffrescoUI screen: unknown font '" + std::string(name) + "'");
	return it->second;
}

} // namespace affresco::format
