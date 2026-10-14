#include "sgl/Screen.hpp"
#include "sgl/Application.hpp"
#include "sgl/Channel.hpp"
#include "sgl/Label.hpp"
#include "sgl/Stack.hpp"
#include "sgl/TextArea.hpp"
#include "sgl/Window.hpp"

#include "screen/Expression.hpp"

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <functional>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace sgl {

using screen::json;

namespace {

/* Reading values */

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

// Text properties accept any value: numbers and booleans from bindings are shown as text
std::string text(const json& value) { return screen::toText(value); }

// 120 or "120px" -> pixels, "50%" -> percentage of the parent
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

// Node keys that describe structure or behaviour: never evaluated as values, never applied as properties
bool structural(const std::string& key) {
	return key == "children" || key == "repeat" || key == "when" || key == "onClick" || key == "onSubmit" ||
	       key == "bind";
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

	[[nodiscard]] bool any() const { return hover || pressed || focus; }
	[[nodiscard]] Color current() const {
		if(down && pressed) return *pressed;
		if(focused && focus) return *focus;
		if(hovered && hover) return *hover;
		return base;
	}
};

// What the screen knows about each panel it built
struct Record {
	std::string type;
	std::size_t firstManaged = 0; // children a factory created come first and are left alone
	std::size_t managed = 0;      // children built from the document
	Color defaultColor;
	Cursor defaultCursor = Cursor::Arrow;
	json locals = json::object(); // repeat variables visible to this node's bindings and handlers
	json onClick;                 // action name or inline steps (null: none)
	json onSubmit;
	std::string path; // place in the document, for error messages
	std::optional<std::string> bind;
	std::optional<std::string> lastText; // "text" applied last time
	Interaction interaction;
};

struct Revision {
	int number;
	std::string author;
	std::string message;
	json document;
	json patch = json::array(); // as submitted (with #id paths): what a change file carries
	std::string time;           // UTC, ISO 8601
};

// A revision waiting for the user's decision (author policy "review")
struct Proposal {
	int id;
	json revision;
	std::string time;
};

// A document and state ready to commit, already checked against the permissions
struct Prepared {
	json document;
	json state;
};

// Steps that govern changes: a revision may never add or alter them (it could otherwise approve itself)
constexpr std::string_view controlSteps[] = {"accept",  "reject", "preview", "endPreview",
                                             "restore", "undo",   "patch",   "call"};

std::string utcNow() {
	const std::time_t now = std::time(nullptr);
	char buffer[32];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
	return buffer;
}

// A child to build: the node (styles merged), its variables and its place in the document
struct Item {
	json node;
	json locals;
	std::string path;
};

// Format versions this library reads. A future version gets its own entry, and its differences branch on
// Impl::version where they apply.
constexpr int supportedVersions[] = {1};

constexpr int maxActionDepth = 32;
constexpr std::size_t maxLoopIterations = 10000;

} // namespace

struct Screen::Impl {
	json document;
	json state = json::object(); // live state, starts as the document's "state"
	std::filesystem::path baseDirectory;
	std::vector<Revision> history; // history.front() is the file as loaded

	std::map<std::string, Color, std::less<>> colors;
	std::map<std::string, std::shared_ptr<const Font>, std::less<>> fonts;
	std::map<std::string, std::shared_ptr<const Font>> fontCache; // by "file@size": revisions don't reload fonts

	std::map<std::string, Factory, std::less<>> types;
	std::map<std::string, Action, std::less<>> nativeActions;
	UnhandledAction unhandled = [](const std::string& action, Panel& sender) {
		std::cerr << "sGLGUI screen: no handler for action '" << action << "' (from '" << sender.id() << "')\n";
	};
	bool placeholderTypes = false;

	Window* window = nullptr;
	Channel* channel = nullptr;
	Connection channelConnection = 0;
	std::string channelAuthor = "channel";
	std::unordered_map<const Panel*, Record> records;

	bool stateChanged = false;
	bool reconcileQueued = false;
	int actionDepth = 0;

	std::vector<Proposal> proposals;
	int nextProposal = 1;
	std::optional<int> previewing; // proposal shown in place of the committed document
	json committedDocument, committedState;
	std::map<std::string, Color, std::less<>> committedColors;
	std::map<std::string, std::shared_ptr<const Font>, std::less<>> committedFonts;

	Signal<const std::string&> events; // "emit" events

	[[nodiscard]] int version() const { return document["version"].get<int>(); }

	/* Document */
	void validateHeader(const json& doc) const;
	void readDerived(const json& doc, std::map<std::string, Color, std::less<>>& outColors,
	                 std::map<std::string, std::shared_ptr<const Font>, std::less<>>& outFonts);
	void load(json doc);

	/* Values */
	struct Globals {
		json window, revision, other;
	};
	// Built once and reused by every evaluation until the history, proposals, preview or window size change
	const Globals& globals() const;
	void globalsChanged() const { globalsCache.reset(); }
	mutable std::optional<Globals> globalsCache;
	json revisionValue() const;
	json evaluate(const json& value, const json& locals, const json& event, const std::string& where) const;
	Color color(const json& value, const std::string& path) const;
	json withStyles(const json& node, const std::string& path) const;

	/* Building */
	std::unique_ptr<Panel> create(const std::string& type, const std::string& path) const;
	Record& adopt(Panel& panel, const std::string& type);
	void hookUp(Panel& panel);
	json evaluateNode(const json& merged, const json& locals, const std::string& path) const;
	std::vector<Item> expand(const json& children, const json& locals, const std::string& path) const;
	void reset(Panel& panel, Record& record);
	void apply(Panel& panel, Record& record, const json& properties, const std::string& path, bool isRoot,
	           bool created);
	void reconcileChildren(const json& properties, Panel& panel, Record& record, const std::string& path);
	void forgetSubtree(const Panel& panel);
	void reconcile();
	void requestReconcile();
	Panel& buildDetached(const json& node, Panel& parent, const std::string& path);

	/* Behaviour */
	void handle(const json& handler, Panel& sender, const json& event, const json& locals, const std::string& where);
	void runNamed(const std::string& name, Panel& sender, const json& event, const json& locals,
	              const std::string& where);
	bool runSteps(const json& steps, Panel& sender, const json& event, json& locals, const std::string& where);
	bool runStep(const json& step, Panel& sender, const json& event, json& locals, const std::string& where);
	void report(const std::string& message);

	/* Live document */
	std::optional<std::string> nodePointer(const json& doc, const std::string& id) const;
	std::string resolvePath(const json& doc, const std::string& path, const std::string& where) const;
	std::string policyFor(const std::string& author) const;
	void checkPermissions(const json& newDocument, const json& newState) const;
	Prepared prepare(const json& revision) const;
	std::optional<std::string> commit(const json& revision);
	RevisionResult submit(const json& revision);
	json restoreRevision(int number, const std::string& author) const;
	std::optional<std::string> accept(int id);
	bool reject(int id, const std::string& reason);
	std::optional<std::string> preview(int id);
	void endPreview();
	std::optional<std::string> importChanges(const json& changes);
	void handleMessage(const std::string& line);
	void send(const json& message);
};

/* Document */

void Screen::Impl::validateHeader(const json& doc) const {
	object(doc, "");

	static constexpr std::string_view sections[] = {"$schema", "format", "version", "window",
	                                                "fonts",   "colors", "styles",  "templates",
	                                                "root",    "state",  "actions", "permissions"};
	for(const auto& [key, value] : doc.items()) {
		if(std::ranges::find(sections, key) == std::end(sections)) fail(key, "unknown section");
	}

	if(doc.contains("format") && string(doc["format"], "format") != "sglgui-screen")
		fail("format", "expected \"sglgui-screen\"");
	if(!doc.contains("version")) fail("", "missing \"version\" (this library reads version 1)");
	const int v = integer(doc["version"], "version");
	if(std::ranges::find(supportedVersions, v) == std::end(supportedVersions))
		fail("version", "unsupported version " + std::to_string(v) + " (this library reads version 1)");
	if(!doc.contains("root")) fail("", "missing \"root\"");
	if(doc.contains("actions")) object(doc["actions"], "actions");

	if(doc.contains("permissions")) {
		for(const auto& [key, value] : object(doc["permissions"], "permissions").items()) {
			const std::string at = "permissions." + key;
			if(key == "authors") {
				for(const auto& [author, policy] : object(value, at).items()) {
					const std::string name = string(policy, at + "." + author);
					if(name != "allow" && name != "review" && name != "deny")
						fail(at + "." + author, "expected \"allow\", \"review\" or \"deny\"");
				}
			} else if(key == "locked") {
				if(!value.is_array()) fail(at, "expected a list of \"#id\" or JSON Pointer paths");
				for(const json& path : value) string(path, at);
			} else {
				fail(at, "unknown property (expected authors or locked)");
			}
		}
	}
}

void Screen::Impl::readDerived(const json& doc, std::map<std::string, Color, std::less<>>& outColors,
                               std::map<std::string, std::shared_ptr<const Font>, std::less<>>& outFonts) {
	if(doc.contains("colors")) {
		for(const auto& [name, value] : object(doc["colors"], "colors").items()) {
			const std::string path = "colors." + name;
			const auto parsed = Color::parseHex(string(value, path));
			if(!parsed) fail(path, "expected a hex color like \"#6366f1\"");
			outColors.emplace(name, *parsed);
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
			outFonts.emplace(name, cached->second);
		}
	}
}

void Screen::Impl::load(json doc) {
	validateHeader(doc);
	decltype(colors) newColors;
	decltype(fonts) newFonts;
	readDerived(doc, newColors, newFonts);

	document = std::move(doc);
	colors = std::move(newColors);
	fonts = std::move(newFonts);
}

/* Values */

const Screen::Impl::Globals& Screen::Impl::globals() const {
	if(globalsCache) return *globalsCache;
	Globals& g = globalsCache.emplace();
	g.window = window ? json{{"width", window->width()}, {"height", window->height()}} : json::object();
	g.revision = revisionValue();

	json list = json::array();
	for(const Proposal& p : proposals)
		list.push_back({{"id", p.id},
		                {"author", p.revision.value("author", "unknown")},
		                {"message", text(p.revision.value("message", json("")))},
		                {"operations", p.revision["patch"].size()},
		                {"time", p.time}});
	json log = json::array();
	for(const Revision& r : history)
		log.push_back({{"number", r.number}, {"author", r.author}, {"message", r.message}, {"time", r.time}});

	g.other = {{"proposals", list}, {"history", log}, {"preview", previewing ? json(*previewing) : json()}};
	return g;
}

json Screen::Impl::revisionValue() const {
	const Revision& head = history.back();
	return {{"number", head.number}, {"author", head.author}, {"message", head.message}};
}

json Screen::Impl::evaluate(const json& value, const json& locals, const json& event, const std::string& where) const {
	const Globals& g = globals();
	return screen::evaluate(value, screen::Context{state, g.window, g.revision, event, locals, g.other}, where);
}

// "#rrggbb", "@name" (from "colors") or "transparent"
Color Screen::Impl::color(const json& value, const std::string& path) const {
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

/* Building */

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

// Starts tracking a newly created panel, remembering its defaults so they can be restored later
Record& Screen::Impl::adopt(Panel& panel, const std::string& type) {
	Record& record = records[&panel];
	record.type = type;
	record.firstManaged = panel.children().size();
	record.defaultColor = panel.color();
	record.defaultCursor = panel.cursor();
	hookUp(panel);
	return record;
}

// Signals are connected once; handlers read the panel's current record, so later updates need no reconnecting
void Screen::Impl::hookUp(Panel& panel) {
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

		it->second.interaction.down = event.action == sgl::Action::Press;
		refresh(panel);

		// A click: left button released over the panel that was pressed
		if(event.action == sgl::Action::Release && panel.bounds().contains(event.position) &&
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
				const screen::Context context{state, g.window, g.revision, event, it->second.locals, g.other};
				state[screen::statePointer(*it->second.bind, context, it->second.path + ".bind")] = value;
				requestReconcile();
			} catch(const ScreenError& error) {
				report(error.what());
			}
		});
		textArea->onKey.connect([this, &panel, textArea](const KeyEvent& event) {
			const bool enter = event.key == Key::Enter || event.key == Key::KeypadEnter;
			auto it = records.find(&panel);
			if(!enter || event.action != sgl::Action::Press || it == records.end() || it->second.onSubmit.is_null())
				return;
			const json submitEvent = {{"type", "submit"}, {"text", textArea->text()}};
			handle(it->second.onSubmit, panel, submitEvent, it->second.locals, it->second.path + ".onSubmit");
		});
	}
}

json Screen::Impl::evaluateNode(const json& merged, const json& locals, const std::string& path) const {
	json result = json::object();
	for(const auto& [key, value] : merged.items())
		result[key] =
		    structural(key) || key == "type" ? value : evaluate(value, locals, json::object(), path + "." + key);
	return result;
}

// The children to build, after "repeat" and "when"
std::vector<Item> Screen::Impl::expand(const json& children, const json& locals, const std::string& path) const {
	std::vector<Item> items;
	if(!children.is_array()) fail(path + ".children", "expected an array");

	for(std::size_t i = 0; i < children.size(); ++i) {
		const std::string at = path + ".children[" + std::to_string(i) + "]";
		json merged = withStyles(children[i], at);

		auto visible = [&](const json& scope) {
			return !merged.contains("when") ||
			       screen::truthy(evaluate(merged["when"], scope, json::object(), at + ".when"));
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
void Screen::Impl::reset(Panel& panel, Record& record) {
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

void Screen::Impl::apply(Panel& panel, Record& record, const json& properties, const std::string& path, bool isRoot,
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
		const screen::Context context{state, g.window, g.revision, event, record.locals, g.other};
		const std::string value = text(screen::lookup(*record.bind, context, path + ".bind"));
		if(label->text() != value) label->setText(value);
	} else if(label && newText != record.lastText) {
		label->setText(newText.value_or(""));
	}
	record.lastText = newText;

	record.interaction.base = panel.color();
	if(record.interaction.any()) panel.setColor(record.interaction.current());

	if(created && autofocus && window) window->setFocus(&panel);
}

void Screen::Impl::reconcileChildren(const json& properties, Panel& panel, Record& record, const std::string& path) {
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

void Screen::Impl::forgetSubtree(const Panel& panel) {
	records.erase(&panel);
	for(const auto& child : panel.children()) forgetSubtree(*child);
}

// Brings the UI in line with the document and the state
void Screen::Impl::reconcile() {
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

	const json properties = evaluateNode(withStyles(document["root"], "root"), record.locals, "root");
	apply(root, record, properties, "root", true, false);
	reconcileChildren(properties, root, record, "root");
}

// Updates are batched: many state changes during one event become one reconcile, after the event is delivered
void Screen::Impl::requestReconcile() {
	stateChanged = true;
	if(reconcileQueued || !window) return;
	reconcileQueued = true;
	window->defer([this] {
		if(!reconcileQueued) return;
		try {
			reconcile();
		} catch(const ScreenError& error) {
			report(error.what());
		}
	});
}

// A template built outside reconciliation (Screen::instantiate)
Panel& Screen::Impl::buildDetached(const json& node, Panel& parent, const std::string& path) {
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

void Screen::Impl::report(const std::string& message) {
	std::cerr << message << '\n';
	send({{"type", "error"}, {"message", message}, {"revision", history.back().number}});
}

// Runs an event handler: an action name or inline steps. Errors are reported, never thrown into the event loop.
void Screen::Impl::handle(const json& handler, Panel& sender, const json& event, const json& locals,
                          const std::string& where) {
	try {
		json scope = locals;
		if(handler.is_string()) runNamed(handler.get<std::string>(), sender, event, scope, where);
		else runSteps(handler, sender, event, scope, where);
	} catch(const ScreenError& error) {
		report(error.what());
	}
	if(stateChanged) requestReconcile();
}

void Screen::Impl::runNamed(const std::string& name, Panel& sender, const json& event, const json& locals,
                            const std::string& where) {
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
bool Screen::Impl::runSteps(const json& steps, Panel& sender, const json& event, json& locals,
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

bool Screen::Impl::runStep(const json& step, Panel& sender, const json& event, json& locals, const std::string& where) {
	object(step, where);
	const Globals& g = globals();
	const screen::Context context{state, g.window, g.revision, event, locals, g.other};
	auto value = [&](const json& v, const std::string& at) { return screen::evaluate(v, context, at); };
	auto target = [&](const char* key) {
		return screen::statePointer(string(step[key], where + "." + key), context, where + "." + key);
	};
	// A list given as a path ("state.items") or as any value
	auto list = [&](const json& source, const std::string& at) {
		if(source.is_string() && source.get<std::string>().find("{{") == std::string::npos)
			return screen::lookup(source.get<std::string>(), context, at);
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
		state[pointer] = !screen::truthy(state.value(pointer, json()));
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
		const bool condition = screen::truthy(value(step["if"], where + ".if"));
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
		while(screen::truthy(value(step["while"], where + ".while"))) {
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
		                      {"revision", history.back().number}};
		send(message);
		events(message.dump());
	} else if(step.contains("patch")) {
		// Applied after the event, like any other document change
		json revision = {{"author", "app"},
		                 {"message", step.contains("message") ? value(step["message"], where + ".message") : json("")},
		                 {"patch", value(step["patch"], where + ".patch")}};
		if(window) {
			window->defer([this, revision = std::move(revision)] {
				if(const auto error = commit(revision)) report(*error);
			});
		}
	} else if(step.contains("accept") || step.contains("reject") || step.contains("preview") ||
	          step.contains("restore") || step.contains("undo") || step.contains("endPreview")) {
		// Review and history controls change the document: done after the event, like revisions
		json control = step;
		for(const char* key : {"accept", "reject", "preview", "restore", "reason"})
			if(control.contains(key)) control[key] = value(control[key], where + "." + key);
		for(const char* key : {"accept", "reject", "preview", "restore"})
			if(control.contains(key) && !control[key].is_number_integer()) fail(where + "." + key, "expected a number");
		if(window) {
			window->defer([this, control = std::move(control)] {
				std::optional<std::string> error;
				try {
					if(control.contains("accept")) error = accept(control["accept"].get<int>());
					else if(control.contains("reject"))
						reject(control["reject"].get<int>(), text(control.value("reason", json(""))));
					else if(control.contains("preview")) error = preview(control["preview"].get<int>());
					else if(control.contains("endPreview")) endPreview();
					else if(control.contains("restore"))
						error = commit(restoreRevision(control["restore"].get<int>(), "user"));
					else if(history.size() > 1)
						error = commit(restoreRevision(history[history.size() - 2].number, "user"));
				} catch(const ScreenError& failure) {
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
		std::cerr << "sGLGUI screen log: " << text(value(step["log"], where + ".log")) << '\n';
	} else if(step.contains("stop")) {
		return false;
	} else {
		fail(where, "unknown step (expected set, push, remove, toggle, increment, if, forEach, while, run, call, emit, "
		            "patch, focus, log, stop, accept, reject, preview, endPreview, restore or undo)");
	}

	return true;
}

/* Live document */

// The JSON Pointer of the node with that id, in root or templates
std::optional<std::string> Screen::Impl::nodePointer(const json& doc, const std::string& id) const {
	std::function<std::optional<std::string>(const json&, const std::string&)> search =
	    [&](const json& node, const std::string& pointer) -> std::optional<std::string> {
		if(!node.is_object()) return std::nullopt;
		if(node.contains("id") && node["id"] == id) return pointer;
		if(node.contains("children") && node["children"].is_array()) {
			for(std::size_t i = 0; i < node["children"].size(); ++i)
				if(auto found = search(node["children"][i], pointer + "/children/" + std::to_string(i))) return found;
		}
		return std::nullopt;
	};

	if(doc.contains("root"))
		if(auto found = search(doc["root"], "/root")) return found;
	if(doc.contains("templates")) {
		for(const auto& [name, node] : doc["templates"].items())
			if(auto found = search(node, "/templates/" + name)) return found;
	}
	return std::nullopt;
}

// "#id/rest" -> the JSON Pointer of the node with that id + "/rest"
std::string Screen::Impl::resolvePath(const json& doc, const std::string& path, const std::string& where) const {
	if(!path.starts_with('#')) return path;

	const std::size_t slash = path.find('/');
	const std::string id = path.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
	const auto pointer = nodePointer(doc, id);
	if(!pointer) fail(where, "no node with id '" + id + "'");
	return *pointer + (slash == std::string::npos ? "" : path.substr(slash));
}

// "allow" (apply), "review" (propose to the user) or "deny", from "permissions.authors" (exact name, else "*")
std::string Screen::Impl::policyFor(const std::string& author) const {
	if(!document.contains("permissions") || !document["permissions"].contains("authors")) return "allow";
	const json& authors = document["permissions"]["authors"];
	if(authors.contains(author)) return authors[author].get<std::string>();
	// "mcp/Claude" falls back to the policy for "mcp": the channel's identity, then everyone else
	if(const auto slash = author.find('/'); slash != std::string::npos && authors.contains(author.substr(0, slash)))
		return authors[author.substr(0, slash)].get<std::string>();
	return authors.value("*", "allow");
}

namespace {

// Every governance step in a document, as text: compared before and after a revision
std::vector<std::string> controlsIn(const json& value) {
	std::vector<std::string> found;
	std::function<void(const json&)> walk = [&](const json& v) {
		if(v.is_object()) {
			if(std::ranges::any_of(controlSteps, [&](std::string_view key) { return v.contains(std::string(key)); }))
				found.push_back(v.dump());
			for(const auto& [key, item] : v.items()) walk(item);
		} else if(v.is_array()) {
			for(const json& item : v) walk(item);
		}
	};
	walk(value);
	std::ranges::sort(found);
	return found;
}

std::optional<json> at(const json& value, const std::string& pointer) {
	const json::json_pointer p(pointer);
	if(!value.contains(p)) return std::nullopt;
	return value[p];
}

} // namespace

// The rules every revision must follow, whoever wrote it
void Screen::Impl::checkPermissions(const json& newDocument, const json& newState) const {
	if(document.value("permissions", json()) != newDocument.value("permissions", json()))
		fail("revision", "permission denied: revisions can't change \"permissions\"");

	if(controlsIn(document) != controlsIn(newDocument))
		fail("revision", "permission denied: revisions can't add or change accept, reject, preview, endPreview, "
		                 "restore, undo, patch or call steps");

	// Locked parts: listed in "permissions.locked", or nodes with "locked": true
	std::vector<std::string> locked;
	if(document.contains("permissions") && document["permissions"].contains("locked"))
		for(const json& path : document["permissions"]["locked"]) locked.push_back(path.get<std::string>());
	std::function<void(const json&)> collect = [&](const json& node) {
		if(!node.is_object()) return;
		if(node.value("locked", false) && node.contains("id")) locked.push_back("#" + node["id"].get<std::string>());
		if(node.contains("children") && node["children"].is_array())
			for(const json& child : node["children"]) collect(child);
	};
	if(document.contains("root")) collect(document["root"]);

	for(const std::string& path : locked) {
		bool same = false;
		if(path.starts_with('#')) {
			const std::string id = path.substr(1);
			const auto before = nodePointer(document, id), after = nodePointer(newDocument, id);
			same = before && after && document[json::json_pointer(*before)] == newDocument[json::json_pointer(*after)];
		} else if(path == "/state" || path.starts_with("/state/")) {
			const std::string inState = path.substr(6);
			same = at(state, inState) == at(newState, inState);
		} else {
			same = at(document, path) == at(newDocument, path);
		}
		if(!same) fail("revision", "permission denied: '" + path + "' is locked");
	}
}

// Validates a revision and computes the document and state it leads to. Changes nothing.
Prepared Screen::Impl::prepare(const json& revision) const {
	object(revision, "revision");
	const int head = history.back().number;
	if(revision.contains("parent") && integer(revision["parent"], "revision.parent") != head)
		fail("revision.parent", "conflict: based on revision " + revision["parent"].dump() +
		                            ", but the current one is " + std::to_string(head));
	if(!revision.contains("patch") || !revision["patch"].is_array())
		fail("revision.patch", "expected a JSON Patch array");

	// "/state/..." operations change the live state; everything else changes the document
	json documentOps = json::array(), stateOps = json::array();
	for(std::size_t i = 0; i < revision["patch"].size(); ++i) {
		const std::string where = "revision.patch[" + std::to_string(i) + "]";
		json op = object(revision["patch"][i], where);
		for(const char* key : {"path", "from"})
			if(op.contains(key)) op[key] = resolvePath(document, string(op[key], where + "." + key), where + "." + key);

		const std::string path = op.value("path", "");
		if(path == "/state" || path.starts_with("/state/")) {
			op["path"] = path.substr(6);
			stateOps.push_back(std::move(op));
		} else {
			documentOps.push_back(std::move(op));
		}
	}

	Prepared result;
	try {
		result.document = document.patch(documentOps);
		result.state = state.patch(stateOps);
	} catch(const json::exception& error) {
		fail("revision.patch", error.what());
	}
	if(!result.state.is_object()) fail("revision.patch", "the state must stay an object");

	checkPermissions(result.document, result.state);
	validateHeader(result.document);
	return result;
}

// Applies a revision now, recording it in the history. Either everything changes or nothing does.
// Returns an error message, or nothing on success.
std::optional<std::string> Screen::Impl::commit(const json& revision) {
	endPreview();
	try {
		Prepared next = prepare(revision);

		json oldDocument = document, oldState = state;
		auto oldColors = colors;
		auto oldFonts = fonts;

		load(std::move(next.document));
		state = std::move(next.state);
		globalsChanged();
		history.push_back({history.back().number + 1, revision.value("author", "unknown"),
		                   text(revision.value("message", json(""))), document, revision["patch"], utcNow()});

		try {
			reconcile();
		} catch(const ScreenError&) {
			// The new document can't be built: roll everything back
			history.pop_back();
			globalsChanged();
			document = std::move(oldDocument);
			state = std::move(oldState);
			colors = std::move(oldColors);
			fonts = std::move(oldFonts);
			reconcile();
			throw;
		}
	} catch(const ScreenError& error) {
		return error.what();
	}
	return std::nullopt;
}

// A revision from someone who must follow the permissions: applied, proposed to the user, or rejected
RevisionResult Screen::Impl::submit(const json& revision) {
	const int head = history.back().number;
	const std::string author = revision.is_object() ? revision.value("author", "unknown") : "unknown";
	const std::string policy = policyFor(author);

	if(policy == "deny")
		return {RevisionStatus::Rejected, head, 0, "permission denied: '" + author + "' may not change this screen"};

	if(policy == "review") {
		try {
			(void)prepare(revision); // reject what could never apply now, while its author is still listening
		} catch(const ScreenError& error) {
			return {RevisionStatus::Rejected, head, 0, error.what()};
		}
		proposals.push_back({nextProposal, revision, utcNow()});
		globalsChanged();
		requestReconcile(); // a review UI may list the proposals
		return {RevisionStatus::Proposed, head, nextProposal++, {}};
	}

	if(const auto error = commit(revision)) return {RevisionStatus::Rejected, head, 0, *error};
	return {RevisionStatus::Applied, history.back().number, 0, {}};
}

// A revision that brings the document back to how it was at revision `number` (the history keeps growing)
json Screen::Impl::restoreRevision(int number, const std::string& author) const {
	const auto it = std::ranges::find(history, number, &Revision::number);
	if(it == history.end()) fail("restore", "no revision " + std::to_string(number));
	json patch = json::diff(document, it->document);
	if(patch.empty())
		fail("restore", "nothing to restore: the screen already looks like revision " + std::to_string(number));
	return {
	    {"author", author}, {"message", "Restored revision " + std::to_string(number)}, {"patch", std::move(patch)}};
}

std::optional<std::string> Screen::Impl::accept(int id) {
	const auto it = std::ranges::find(proposals, id, &Proposal::id);
	if(it == proposals.end()) return "sGLGUI screen: no proposal " + std::to_string(id);

	const json revision = it->revision;
	proposals.erase(it);
	globalsChanged();

	const auto error = commit(revision);
	if(error) send({{"type", "rejected"}, {"proposal", id}, {"error", *error}, {"revision", history.back().number}});
	else send({{"type", "applied"}, {"proposal", id}, {"revision", history.back().number}});
	requestReconcile();
	return error;
}

bool Screen::Impl::reject(int id, const std::string& reason) {
	const auto it = std::ranges::find(proposals, id, &Proposal::id);
	if(it == proposals.end()) return false;

	if(previewing == id) endPreview();
	proposals.erase(it);
	globalsChanged();
	send({{"type", "rejected"},
	      {"proposal", id},
	      {"error", "rejected by the user" + (reason.empty() ? std::string() : ": " + reason)},
	      {"revision", history.back().number}});
	requestReconcile();
	return true;
}

// Shows a proposal in place of the committed document, without committing it
std::optional<std::string> Screen::Impl::preview(int id) {
	const auto it = std::ranges::find(proposals, id, &Proposal::id);
	if(it == proposals.end()) return "sGLGUI screen: no proposal " + std::to_string(id);

	endPreview();
	try {
		Prepared next = prepare(it->revision);
		committedDocument = document;
		committedState = state;
		committedColors = colors;
		committedFonts = fonts;

		load(std::move(next.document));
		state = std::move(next.state);
		previewing = id;
		globalsChanged();
		reconcile();
	} catch(const ScreenError& error) {
		endPreview();
		return error.what();
	}
	return std::nullopt;
}

void Screen::Impl::endPreview() {
	if(!previewing) return;
	previewing.reset();
	globalsChanged();
	document = std::move(committedDocument);
	state = std::move(committedState);
	colors = std::move(committedColors);
	fonts = std::move(committedFonts);
	reconcile();
}

// Applies a change set (a "plugin") revision by revision, all or nothing
std::optional<std::string> Screen::Impl::importChanges(const json& changes) {
	try {
		object(changes, "changes");
		if(changes.value("format", "") != "sglgui-changes") fail("changes.format", "expected \"sglgui-changes\"");
		if(changes.value("version", 0) != 1) fail("changes.version", "unsupported version (expected 1)");
		if(!changes.contains("revisions") || !changes["revisions"].is_array())
			fail("changes.revisions", "expected a list");
	} catch(const ScreenError& error) {
		return error.what();
	}

	endPreview();
	const json oldDocument = document, oldState = state;
	const auto oldColors = colors;
	const auto oldFonts = fonts;
	const std::size_t oldHistory = history.size();
	const std::string name = changes.value("name", "changes");

	for(std::size_t i = 0; i < changes["revisions"].size(); ++i) {
		json revision = changes["revisions"][i];
		if(revision.is_object()) {
			revision.erase("parent"); // a change set applies on top of whatever is there
			revision["author"] = "plugin:" + name;
		}
		if(const auto error = commit(revision)) {
			history.resize(oldHistory, history.front());
			globalsChanged();
			document = oldDocument;
			state = oldState;
			colors = oldColors;
			fonts = oldFonts;
			reconcile();
			return "changes.revisions[" + std::to_string(i) + "]: " + *error;
		}
	}
	return std::nullopt;
}

/* Channel protocol: one JSON object per line */

void Screen::Impl::send(const json& message) {
	if(channel) channel->send(message.dump());
}

void Screen::Impl::handleMessage(const std::string& line) {
	json message;
	try {
		message = json::parse(line);
	} catch(const json::parse_error& error) {
		send({{"type", "error"}, {"message", std::string("invalid JSON: ") + error.what()}});
		return;
	}

	const std::string type = message.value("type", "");
	const json id = message.value("id", json());

	// The channel decides who the author is; the name the sender gives itself is kept after a '/'
	auto authored = [&](json revision) {
		const std::string declared =
		    message.contains("author") && message["author"].is_string() ? message["author"].get<std::string>() : "";
		revision["author"] =
		    declared.empty() || declared == channelAuthor ? channelAuthor : channelAuthor + "/" + declared;
		return revision;
	};

	auto reply = [&](const RevisionResult& result) {
		if(result.status == RevisionStatus::Applied)
			send({{"type", "applied"}, {"revision", result.revision}, {"id", id}});
		else if(result.status == RevisionStatus::Proposed)
			send({{"type", "proposed"}, {"proposal", result.proposal}, {"revision", result.revision}, {"id", id}});
		else {
			std::cerr << result.error << '\n';
			send({{"type", "rejected"}, {"error", result.error}, {"revision", result.revision}, {"id", id}});
		}
	};

	try {
		if(type == "revision") {
			reply(submit(authored(message)));
		} else if(type == "restore" || type == "undo") {
			// Going back is a change like any other: it follows the author's policy too
			const std::string author = authored(json::object())["author"];
			const int target = type == "restore"    ? integer(message.value("revision", json()), "restore.revision")
			                   : history.size() > 1 ? history[history.size() - 2].number
			                                        : history.back().number;
			reply(submit(restoreRevision(target, author)));
		} else if(type == "getDocument") {
			const Globals& g = globals();
			send({{"type", "document"},
			      {"revision", history.back().number},
			      {"document", document},
			      {"state", state},
			      {"proposals", g.other["proposals"]},
			      {"you", channelAuthor},
			      {"id", id}});
		} else if(type == "getHistory") {
			send({{"type", "history"},
			      {"entries", globals().other["history"]},
			      {"revision", history.back().number},
			      {"id", id}});
		} else if(type == "exportChanges") {
			json revisions = json::array();
			const int from = message.value("from", 0);
			for(const Revision& r : history)
				if(r.number > from)
					revisions.push_back(
					    {{"author", r.author}, {"message", r.message}, {"time", r.time}, {"patch", r.patch}});
			send({{"type", "changes"},
			      {"changes",
			       {{"format", "sglgui-changes"},
			        {"version", 1},
			        {"name", message.value("name", "changes")},
			        {"from", from},
			        {"to", history.back().number},
			        {"revisions", revisions}}},
			      {"id", id}});
		} else {
			send({{"type", "error"}, {"message", "unknown message type '" + type + "'"}});
		}
	} catch(const ScreenError& error) {
		send({{"type", "rejected"}, {"error", error.what()}, {"revision", history.back().number}, {"id", id}});
	}
}

/* Screen */

Screen::Screen(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Screen::Screen(Screen&&) noexcept = default;
Screen& Screen::operator=(Screen&&) noexcept = default;

Screen::~Screen() {
	if(impl_ && impl_->channel) impl_->channel->onMessage.disconnect(impl_->channelConnection);
}

Screen Screen::fromFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	if(!file) {
		throw ScreenError(
		    "sGLGUI screen: could not open " + std::filesystem::absolute(path).string() +
		    " (relative paths start from the current folder: " + std::filesystem::current_path().string() + ")");
	}

	std::stringstream content;
	content << file.rdbuf();
	return fromString(content.str(), path.parent_path());
}

Screen Screen::fromString(std::string_view content, std::filesystem::path baseDirectory) {
	auto impl = std::make_unique<Impl>();
	impl->baseDirectory = std::move(baseDirectory);

	json document;
	try {
		document = json::parse(content);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("sGLGUI screen: invalid JSON: ") + error.what());
	}

	impl->load(std::move(document));
	if(impl->document.contains("state")) impl->state = object(impl->document["state"], "state");
	impl->history.push_back({0, "file", "original", impl->document, json::array(), utcNow()});
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
	impl_->reconcile();

	// Bindings can use the window size
	window.onResize.connect([impl = impl_.get()](const ResizeEvent&) { impl->requestReconcile(); });
}

Panel& Screen::instantiate(std::string_view name, Panel& parent) {
	const json& document = impl_->document;
	const std::string key(name);
	if(!document.contains("templates") || !document["templates"].contains(key))
		throw ScreenError("sGLGUI screen: unknown template '" + key + "'");
	return impl_->buildDetached(document["templates"][key], parent, "templates." + key);
}

void Screen::onAction(std::string name, Action action) {
	impl_->nativeActions.insert_or_assign(std::move(name), std::move(action));
}

void Screen::onUnhandledAction(UnhandledAction fallback) { impl_->unhandled = std::move(fallback); }

void Screen::runAction(std::string_view name, Panel& sender, std::string_view eventJson) {
	json event;
	try {
		event = json::parse(eventJson);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("sGLGUI screen: runAction: invalid event JSON: ") + error.what());
	}

	const auto it = impl_->records.find(&sender);
	const json locals = it != impl_->records.end() ? it->second.locals : json::object();
	impl_->handle(json(std::string(name)), sender, event, locals, "runAction");
}

namespace {

std::optional<json> parseJson(std::string_view text, const char* what, std::string* error) {
	try {
		return json::parse(text);
	} catch(const json::parse_error& parseError) {
		if(error) *error = std::string("sGLGUI screen: invalid ") + what + " JSON: " + parseError.what();
		return std::nullopt;
	}
}

bool succeeded(const std::optional<std::string>& failure, std::string* error) {
	if(failure && error) *error = *failure;
	return !failure;
}

} // namespace

bool Screen::applyRevision(std::string_view revisionJson, std::string* error) {
	const auto revision = parseJson(revisionJson, "revision", error);
	return revision && succeeded(impl_->commit(*revision), error);
}

RevisionResult Screen::submitRevision(std::string_view revisionJson) {
	std::string error;
	const auto revision = parseJson(revisionJson, "revision", &error);
	if(!revision) return {RevisionStatus::Rejected, currentRevision(), 0, error};
	return impl_->submit(*revision);
}

std::vector<Screen::ProposalInfo> Screen::proposals() const {
	std::vector<ProposalInfo> result;
	for(const Proposal& p : impl_->proposals)
		result.push_back({p.id, p.revision.value("author", "unknown"), text(p.revision.value("message", json(""))),
		                  p.revision["patch"].size(), p.time});
	return result;
}

bool Screen::acceptProposal(int id, std::string* error) { return succeeded(impl_->accept(id), error); }

bool Screen::rejectProposal(int id, std::string_view reason) { return impl_->reject(id, std::string(reason)); }

bool Screen::previewProposal(int id, std::string* error) { return succeeded(impl_->preview(id), error); }

void Screen::endPreview() { impl_->endPreview(); }

std::optional<int> Screen::previewedProposal() const noexcept { return impl_->previewing; }

std::vector<Screen::HistoryEntry> Screen::history() const {
	std::vector<HistoryEntry> result;
	for(const Revision& r : impl_->history) result.push_back({r.number, r.author, r.message, r.time});
	return result;
}

bool Screen::restore(int revision, std::string* error) {
	try {
		return succeeded(impl_->commit(impl_->restoreRevision(revision, "user")), error);
	} catch(const ScreenError& failure) {
		return succeeded(failure.what(), error);
	}
}

RevisionResult Screen::submitRestore(int revision, std::string_view author) {
	try {
		return impl_->submit(impl_->restoreRevision(revision, std::string(author)));
	} catch(const ScreenError& failure) {
		return {RevisionStatus::Rejected, currentRevision(), 0, failure.what()};
	}
}

bool Screen::undo(std::string* error) {
	if(impl_->history.size() < 2) return succeeded("sGLGUI screen: nothing to undo", error);
	return restore(impl_->history[impl_->history.size() - 2].number, error);
}

int Screen::revision() const noexcept { return currentRevision(); }

int Screen::currentRevision() const noexcept { return impl_->history.back().number; }

std::string Screen::exportChanges(int fromRevision, std::string_view name) const {
	json revisions = json::array();
	for(const Revision& r : impl_->history) {
		if(r.number <= fromRevision || r.number == 0) continue;
		revisions.push_back({{"author", r.author}, {"message", r.message}, {"time", r.time}, {"patch", r.patch}});
	}
	return json{{"format", "sglgui-changes"},
	            {"version", 1},
	            {"name", name.empty() ? "changes" : std::string(name)},
	            {"from", fromRevision},
	            {"to", currentRevision()},
	            {"revisions", revisions}}
	    .dump(2);
}

bool Screen::importChanges(std::string_view changesJson, std::string* error) {
	const auto changes = parseJson(changesJson, "changes", error);
	return changes && succeeded(impl_->importChanges(*changes), error);
}

std::string Screen::documentJson() const { return impl_->document.dump(); }

std::string Screen::stateJson() const { return impl_->state.dump(); }

Connection Screen::onEvent(std::function<void(const std::string&)> listener) {
	return impl_->events.connect(std::move(listener));
}

void Screen::connect(Channel& channel, std::string author) {
	impl_->channelAuthor = std::move(author);
	if(impl_->channel) impl_->channel->onMessage.disconnect(impl_->channelConnection);
	impl_->channel = &channel;
	impl_->channelConnection =
	    channel.onMessage.connect([impl = impl_.get()](const std::string& line) { impl->handleMessage(line); });

	// Introduce ourselves: whoever is on the other end gets the full picture to work from
	impl_->send({{"type", "hello"},
	             {"format", "sglgui-screen"},
	             {"revision", revision()},
	             {"document", impl_->document},
	             {"state", impl_->state},
	             {"you", impl_->channelAuthor}});
}

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
