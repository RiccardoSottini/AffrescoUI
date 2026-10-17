#include "affresco/format/Runtime.hpp"
#include "affresco/render/Controls.hpp"
#include "affresco/render/Label.hpp"
#include "affresco/render/Stack.hpp"
#include "affresco/render/TextArea.hpp"

#include "Expression.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
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

/// @brief Reads an overlay placement, e.g. "below".
/// @param value The value.
/// @param path Its place, for errors.
/// @return The placement.
/// @throws DocumentError If the name is unknown.
Placement placement(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "below") return Placement::Below;
	if(name == "below-end") return Placement::BelowEnd;
	if(name == "above") return Placement::Above;
	if(name == "above-end") return Placement::AboveEnd;
	if(name == "right") return Placement::Right;
	if(name == "left") return Placement::Left;
	if(name == "center") return Placement::Center;
	if(name == "window") return Placement::Window;
	fail(path, "expected below, below-end, above, above-end, right, left, center or window");
}

/// @brief Tells whether a node key is an event handler: "on" and a capital letter ("onClick", "onDrop").
/// @param key The key.
/// @return true for handlers.
bool handlerKey(const std::string& key) {
	return key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]));
}

/// Event handlers a node may have.
constexpr std::string_view handlerNames[] = {
    "onClick", "onDoubleClick", "onContextMenu", "onPress", "onRelease", "onDrag", "onDrop",  "onHover", "onLeave",
    "onFocus", "onBlur",        "onScroll",      "onKey",   "onChange",  "onSubmit", "onShow", "onHide"};

/// Handlers that bubble: when a node has none, its nearest ancestor with one handles the event.
constexpr std::string_view bubbling[] = {"onClick", "onDoubleClick", "onContextMenu", "onPress",
                                         "onRelease", "onDrag",      "onScroll",      "onKey"};

/// Visual states, in order: later ones win when several apply.
constexpr std::string_view stateBlocks[] = {"hover", "focused", "dragging", "dragOver", "pressed"};

/// @brief Tells whether a node key describes structure or behaviour, so it is never evaluated or applied as a property.
/// @param key The key.
/// @return true for children, repeat, when, let, handlers, ...
bool structural(const std::string& key) {
	return key == "children" || key == "repeat" || key == "when" || key == "let" || key == "check" || key == "use" ||
	       key == "with" || key == "bind" || key == "keys" || key == "every" || key == "drag" || key == "note" ||
	       key == "slot" || handlerKey(key);
}

/// @brief Tells whether a node sets its own position or size (otherwise a screen fills its outlet).
/// @param node The node.
/// @return true if it has layout properties.
bool hasLayout(const json& node) {
	for(const char* key : {"x", "y", "width", "height", "anchors", "fill"})
		if(node.contains(key)) return true;
	return false;
}

/// @brief A key's name for shortcuts: "a", "7", "enter", "escape", "up", "f5", ...
/// @param key The key.
/// @return Its name, or "" for keys without one.
std::string keyName(Key key) {
	const int code = static_cast<int>(key);
	if(key >= Key::A && key <= Key::Z) return std::string(1, static_cast<char>('a' + code - static_cast<int>(Key::A)));
	if(key >= Key::Num0 && key <= Key::Num9)
		return std::string(1, static_cast<char>('0' + code - static_cast<int>(Key::Num0)));
	if(key >= Key::F1 && key <= Key::F25) return "f" + std::to_string(code - static_cast<int>(Key::F1) + 1);
	switch(key) {
	case Key::Enter:
	case Key::KeypadEnter:
		return "enter";
	case Key::Escape:
		return "escape";
	case Key::Tab:
		return "tab";
	case Key::Backspace:
		return "backspace";
	case Key::Delete:
		return "delete";
	case Key::Space:
		return "space";
	case Key::Up:
		return "up";
	case Key::Down:
		return "down";
	case Key::Left:
		return "left";
	case Key::Right:
		return "right";
	case Key::Home:
		return "home";
	case Key::End:
		return "end";
	case Key::PageUp:
		return "pageup";
	case Key::PageDown:
		return "pagedown";
	case Key::Minus:
		return "minus";
	case Key::Equal:
		return "equal";
	case Key::Comma:
		return "comma";
	case Key::Period:
		return "period";
	case Key::Slash:
		return "slash";
	default:
		return "";
	}
}

/// @brief A key press as a shortcut, modifiers in a fixed order: "cmd+shift+k".
/// @param event The key event.
/// @return The shortcut, or "" for keys without a name (and lone modifiers).
std::string shortcut(const KeyEvent& event) {
	const std::string name = keyName(event.key);
	if(name.empty()) return "";
	std::string result;
	if(event.mods.super()) result += "cmd+";
	if(event.mods.control()) result += "ctrl+";
	if(event.mods.alt()) result += "alt+";
	if(event.mods.shift()) result += "shift+";
	return result + name;
}

/// @brief Writes a shortcut from the document the way shortcut() does: "Shift+Cmd+K" is "cmd+shift+k". "mod" is cmd
/// on macOS and ctrl elsewhere.
/// @param text The shortcut as written.
/// @return The normalised shortcut.
std::string normalizeShortcut(std::string_view text) {
	std::vector<std::string> parts;
	std::string part;
	for(char c : text) {
		if(c == '+') {
			parts.push_back(part);
			part.clear();
		} else if(c != ' ') {
			part += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
	}
	parts.push_back(part);

	bool cmd = false, ctrl = false, alt = false, shift = false;
	std::string key;
	for(const std::string& p : parts) {
#ifdef __APPLE__
		if(p == "mod") cmd = true;
#else
		if(p == "mod") ctrl = true;
#endif
		else if(p == "cmd" || p == "command" || p == "super" || p == "meta") cmd = true;
		else if(p == "ctrl" || p == "control") ctrl = true;
		else if(p == "alt" || p == "option") alt = true;
		else if(p == "shift") shift = true;
		else key = p == "return" ? "enter" : p == "esc" ? "escape" : p;
	}
	return std::string(cmd ? "cmd+" : "") + (ctrl ? "ctrl+" : "") + (alt ? "alt+" : "") + (shift ? "shift+" : "") + key;
}

/// @brief Seconds on a steady clock.
/// @return Now.
double now() {
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/// @brief Eases an animation's progress.
/// @param easing "linear", "in", "out" or "inOut".
/// @param t Progress, 0 to 1.
/// @return The eased progress.
double ease(const std::string& easing, double t) {
	if(easing == "in") return t * t * t;
	if(easing == "out") return 1.0 - std::pow(1.0 - t, 3.0);
	if(easing == "inOut") return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
	return t;
}

/// @brief The scope with no variables.
/// @return It (shared, never changed).
const SharedScope& noVariables() {
	static const SharedScope none = std::make_shared<Scope>();
	return none;
}

/// @brief A copy of a scope chain as one shared scope, for work that runs later (timers).
/// @param scope The scope.
/// @return The copy.
SharedScope snapshot(const Scope& scope) {
	std::vector<const Scope*> chain;
	for(const Scope* s = &scope; s; s = s->parent) chain.push_back(s);
	json vars = json::object();
	for(auto it = chain.rbegin(); it != chain.rend(); ++it) vars.update((*it)->vars); // inner scopes win
	return extend(nullptr, std::move(vars));
}

/// @brief A colour animating towards a target (a "transition").
struct Tween {
	Color from, to;         ///< Where it started and where it goes.
	double start = 0.0;     ///< now() when it started.
	double ms = 0.0;        ///< How long it takes.
	std::uint64_t timer = 0; ///< The next frame's timer, if running.
};

/// @brief What the runtime knows about each panel it built.
struct Record {
	std::string type;                     ///< The node's "type".
	std::size_t firstManaged = 0;         ///< Children a factory created come first and are left alone.
	std::size_t managed = 0;              ///< Children built from the document.
	Color defaultColor;                   ///< Restored when "color" goes away.
	Cursor defaultCursor = Cursor::Arrow; ///< Restored when "cursor" goes away.
	SharedScope locals = noVariables();   ///< Variables visible to its bindings and handlers (repeat, let, with).
	std::string path;                     ///< Place in the document, for errors.
	json properties = json::object();     ///< The evaluated node, state blocks included (what refresh() re-applies).
	json handlers = json::object();       ///< "onClick" and the others: an action name or steps each.
	json keys;                            ///< Shortcuts: "cmd+k" -> handler (null: none).
	json drag;                            ///< "drag", unevaluated (data is read when a drag starts).
	json key;                             ///< A keyed repeat's key for this item (null: none).
	std::optional<std::string> bind;      ///< State path the value mirrors.
	std::optional<std::string> lastText;  ///< "text" applied last time.
	bool hovered = false, down = false, focused = false, dragOver = false, dragging = false; ///< Visual states.
	bool disabled = false;                ///< No events but hover.
	bool shown = false;                   ///< Applied at least once (transitions start only after that).
	json transition = json::object();     ///< "transition": property -> milliseconds.
	Tween colorTween, textTween;          ///< Running transitions.
	double everyMs = 0.0;                 ///< "every" period, if any.
	std::uint64_t everyTimer = 0;         ///< Its timer.
	std::optional<std::string> overlayOpen; ///< An overlay's "open" state path.
	bool dismissOutside = false;          ///< The overlay closes on a click outside it (and on Escape).
	std::string overlayAnchor;            ///< The overlay's anchor id (empty: the parent).
};

/// @brief A node to build: shared, so a repeat's items don't each copy their template (and its children).
using NodeRef = std::shared_ptr<const json>;

/// @brief Refers to a node of the document without copying it (the document outlives an update).
/// @param node The node.
/// @return The reference.
NodeRef borrow(const json& node) { return NodeRef(NodeRef(), &node); }

/// @brief A child to build.
struct Item {
	NodeRef node;     ///< Styles merged; shared by a repeat's items (or pointing into the document: no copy).
	SharedScope locals; ///< Its variables.
	std::string path; ///< Its place in the document.
	json key;         ///< A keyed repeat's key (null: none).
};

constexpr int maxActionDepth = 32;               ///< Nested action calls before giving up (recursion guard).
constexpr std::size_t maxLoopIterations = 10000; ///< "while" iterations before giving up.
constexpr int maxComponentDepth = 32;            ///< Components inside components before giving up.
constexpr double frameMs = 16.0;                 ///< Animation frame period.
constexpr double doubleClickSeconds = 0.4;       ///< Two clicks closer than this are a double click.
constexpr float dragThreshold = 4.0f;            ///< Pixels the pointer moves before a press becomes a drag.

} // namespace

/// @brief The runtime's state and logic; observes the VersionControl.
struct Runtime::Impl final : VersionObserver {
	Runtime& owner;                      ///< For onEvent and onError.
	VersionControl& versions;            ///< The document and state.
	std::filesystem::path baseDirectory; ///< Where font paths are resolved from.

	/// @brief A named colour, with variants per theme.
	struct NamedColor {
		Color base;                                   ///< The first value: used without a matching theme.
		std::map<std::string, Color, std::less<>> themes; ///< "light" -> colour, "dark" -> colour.
	};
	std::map<std::string, NamedColor, std::less<>> colors;                 ///< From "colors".
	std::map<std::string, std::shared_ptr<const Font>, std::less<>> fonts; ///< From "fonts".
	std::map<std::string, std::shared_ptr<const Font>> fontCache; ///< By "file@size": revisions don't reload fonts.
	std::string theme;                                            ///< "theme", evaluated at each update.

	std::map<std::string, Factory, std::less<>> types;        ///< Custom node types.
	std::map<std::string, Action, std::less<>> nativeActions; ///< Native actions.
	std::map<std::string, Call, std::less<>> nativeCalls;     ///< Native functions for "call" with results.
	UnhandledAction unhandled = [](const std::string& action, Panel& sender) {
		std::cerr << "AffrescoUI screen: no handler for action '" << action << "' (from '" << sender.id() << "')\n";
	};
	bool placeholderTypes = false; ///< Unknown types become plain panels.

	Window* window = nullptr;                         ///< Set by build().
	std::unordered_map<const Panel*, Record> records; ///< Every panel built from the document.

	bool stateChanged = false;      ///< An action changed the state.
	std::uint64_t stateVersion = 0; ///< Counts state changes (computed values are cached per version).
	bool reconcileQueued = false;   ///< A reconcile is deferred to the end of the event.
	int actionDepth = 0;            ///< Nested action calls in progress.

	std::string screen;                      ///< The screen an "outlet" shows (from "screens").
	std::vector<std::string> screenHistory;  ///< For {"navigate": "back"}.
	json watched = json::object();           ///< "watch": the values seen last.
	std::map<std::string, std::uint64_t> debounces;  ///< "debounce" steps waiting, by place.
	std::map<std::string, std::uint64_t> animations; ///< "animate" steps running, by state pointer.

	/// @brief A press that may become a click, a double click or a drag.
	struct Press {
		Panel* panel = nullptr;  ///< The panel pressed (it holds the mouse).
		Vec2 position;           ///< Where.
		Panel* source = nullptr; ///< The nearest panel with "drag", if any.
		bool dragging = false;   ///< It became a drag.
		json data;               ///< The drag's data.
		Panel* over = nullptr;   ///< The drop target under the pointer.
	} press;
	struct {
		Panel* owner = nullptr; ///< Who handled the last click.
		double time = 0.0;      ///< When.
	} lastClick; ///< For double clicks.

	/// @brief `computed.<name>`: evaluated when first read after a state change, then cached.
	NodeLookup computedLookup = [this](std::string_view name) -> json {
		const json& document = versions.document();
		const std::string key(name);
		if(!document.contains("computed") || !document["computed"].contains(key))
			fail("computed", "no computed value '" + key + "'");
		if(computedVersion != stateVersion) {
			computedValues = json::object();
			computedVersion = stateVersion;
		}
		if(computedValues.contains(key)) return computedValues[key];
		if(std::ranges::find(computing, key) != computing.end())
			fail("computed." + key, "computed values refer to each other in a circle");
		computing.push_back(key);
		json value;
		try {
			const json none = json::object();
			value = format::evaluate(document["computed"][key], context(*noVariables(), none), "computed." + key);
		} catch(...) {
			computing.pop_back();
			throw;
		}
		computing.pop_back();
		computedValues[key] = value;
		return value;
	};

	/// @brief The `nodes` variable: a built panel's geometry by id.
	NodeLookup nodeLookup = [this](std::string_view id) -> json {
		const Panel* panel = window ? window->root().findById(id) : nullptr;
		if(!panel) return nullptr;
		const Rect& f = panel->frame();
		return {{"x", f.left}, {"y", f.top}, {"width", f.width()}, {"height", f.height()}, {"visible", panel->isShown()}};
	};

	Impl(Runtime& runtime, VersionControl& vcs, std::filesystem::path base)
	    : owner(runtime), versions(vcs), baseDirectory(std::move(base)) {}

	/* VersionObserver */
	/// @brief Re-reads colours and fonts, then reconciles.
	void documentChanged() override {
		readDerived();
		++stateVersion; // a revision may change the state too
		reconcile();
	}
	/// @brief Refreshes the globals and schedules a reconcile (a review UI may list the proposals).
	void proposalsChanged() override {
		globalsChanged();
		requestReconcile();
	}

	/* Document */
	/// @brief Reads "colors" and "fonts", changing nothing on error.
	/// @throws DocumentError If a colour or font is invalid.
	void readDerived();

	/* Values */
	/// @brief Screen-wide variables for expressions.
	struct Globals {
		json window, revision, other; ///< Window size, current revision, and the rest (proposals, computed, ...).
	};
	/// @brief The globals, built once and reused until the history, proposals, preview, window size or screen change.
	const Globals& globals() const;
	/// @brief Drops the cached globals.
	void globalsChanged() const { globalsCache.reset(); }
	mutable std::optional<Globals> globalsCache;    ///< See globals().
	mutable json computedValues = json::object();   ///< "computed" values read since the state last changed.
	mutable std::uint64_t computedVersion = ~0ull;  ///< The state version computedValues belong to.
	mutable std::vector<std::string> computing;     ///< Computed values being evaluated (finds circles).
	/// @brief A context for expressions.
	/// @param locals Variables.
	/// @param event The event being handled.
	/// @return The context (it refers to the arguments).
	Context context(const Scope& locals, const json& event) const {
		const Globals& g = globals();
		return Context{versions.state(), g.window, g.revision, event, locals, g.other, &nodeLookup, &computedLookup};
	}
	/// @brief Evaluates a value with the state, the globals and the given variables.
	/// @param value The value.
	/// @param locals Variables.
	/// @param event The event being handled.
	/// @param where Its place, for errors.
	/// @return The evaluated value.
	/// @throws DocumentError If it is invalid.
	json evaluate(const json& value, const Scope& locals, const json& event, const std::string& where) const;
	/// @brief Reads a colour: "#rrggbb", "@name" (from "colors", in the current theme) or "transparent".
	/// @param value The value.
	/// @param path Its place, for errors.
	/// @return The colour.
	/// @throws DocumentError If it is not a colour.
	Color color(const json& value, const std::string& path) const;
	/// @brief Merges a node's properties on top of its styles' (later styles override earlier ones; a style may
	/// itself have a "style").
	/// @param node The node.
	/// @param path Its place, for errors.
	/// @param depth Styles inside styles so far (recursion guard).
	/// @return The merged node.
	/// @throws DocumentError If a style is unknown.
	json withStyles(const json& node, const std::string& path, int depth = 0) const;
	/// @brief Records that the state changed: computed values are stale and an update is due.
	void changed() {
		stateChanged = true;
		++stateVersion;
	}

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
	json evaluateNode(const json& merged, const Scope& locals, const std::string& path) const;
	/// @brief Turns a "use" of a component into its node: the component's node with the use's properties on top,
	/// its params defaulted under "with", and the use's children in its slot.
	/// @param use The node with "use".
	/// @param path Its place, for errors.
	/// @return The node.
	/// @throws DocumentError If the component is unknown.
	json instantiateComponent(const json& use, const std::string& path) const;
	/// @brief Expands "use", "repeat", "with", "let", "check", dynamic styles and "when" into the children to build.
	/// @param children The "children" array.
	/// @param locals The parent's variables.
	/// @param path Its place, for errors.
	/// @return The children.
	std::vector<Item> expand(const json& children, const SharedScope& locals, const std::string& path) const;
	/// @brief The variables and the final node of one built instance: "with", "let", "check", dynamic style.
	/// @param node The node.
	/// @param scope Its variables so far; gets the new ones.
	/// @param at Its place, for errors.
	/// @return The node, with a dynamic style merged.
	NodeRef scoped(const NodeRef& node, SharedScope& scope, const std::string& at) const;
	/// @brief Restores a panel's defaults, so properties no longer in the document disappear.
	/// @param panel The panel.
	/// @param record Its record.
	void reset(Panel& panel, Record& record);
	/// @brief Applies an evaluated node to a panel: handlers, bindings, timers, then its look.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param properties The evaluated node.
	/// @param path Its place, for errors.
	/// @param isRoot Whether it is the window's root.
	/// @param created Whether it was just created.
	void apply(Panel& panel, Record& record, const json& properties, const std::string& path, bool isRoot,
	           bool created);
	/// @brief The properties to show now: the node with the active state blocks (hover, pressed, ...) on top.
	/// @param record The record.
	/// @return The properties.
	json visual(const Record& record) const;
	/// @brief Sets a panel's look and layout from properties.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param properties From visual().
	/// @param isRoot Whether it is the window's root.
	void applyVisual(Panel& panel, Record& record, const json& properties, bool isRoot);
	/// @brief Shows a panel's state blocks again after hover, press, focus or drag changed.
	/// @param panel The panel.
	/// @param record Its record.
	void refresh(Panel& panel, Record& record);
	/// @brief Sets a colour, animating it if the record has a transition for it.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param property "color" or "textColor".
	/// @param shown The colour shown before this update.
	/// @param target The new colour.
	void transitionColor(Panel& panel, Record& record, const char* property, Color shown, Color target);
	/// @brief Updates a panel's children in place, creating, replacing, moving or removing only what changed.
	/// @param node The node (its "children", unevaluated).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param path Its place, for errors.
	void reconcileChildren(const json& node, Panel& panel, Record& record, const std::string& path);
	/// @brief Drops the records of a subtree about to be destroyed, stopping its timers and running "onHide".
	/// @param panel The subtree's root.
	void forgetSubtree(const Panel& panel);
	/// @brief Brings the panels in line with the document and the state, then runs "watch" handlers.
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
	/// @brief Writes a value to a bound state path and runs the panel's "onChange".
	/// @param panel The panel.
	/// @param value The new value.
	void valueChanged(Panel& panel, const json& value);

	/* Events */
	/// @brief The record of a panel built from the document.
	/// @param panel The panel.
	/// @return The record, or nullptr.
	Record* recordOf(const Panel* panel) {
		const auto it = records.find(panel);
		return it == records.end() ? nullptr : &it->second;
	}
	/// @brief The panel that handles an event: this one or its nearest ancestor with the handler (bubbling).
	/// @param from The panel the event happened on.
	/// @param handler E.g. "onClick".
	/// @return The panel, or nullptr if none handles it.
	Panel* handlerOwner(Panel* from, std::string_view handler);
	/// @brief Runs a panel's handler for an event, if it has one (and isn't disabled).
	/// @param panel The panel.
	/// @param handler E.g. "onHover".
	/// @param event The event.
	void fire(Panel& panel, std::string_view handler, json event);
	/// @brief Runs an event's handler on the panel or its nearest ancestor with one.
	/// @param from The panel the event happened on.
	/// @param handler E.g. "onClick".
	/// @param event The event (pointer events get left/top/width/height relative to the handling panel).
	/// @param position The pointer, for pointer events (else nullptr).
	/// @return true if someone handled it.
	bool bubble(Panel& from, std::string_view handler, json event, const Vec2* position = nullptr);
	/// @brief A pointer event's fields: where, in the window and in a panel.
	/// @param type The event type.
	/// @param panel The handling panel.
	/// @param position The pointer.
	/// @return The event.
	static json pointerEvent(std::string_view type, const Panel& panel, Vec2 position);
	/// @brief Handles a mouse button on a built panel: press, release, click, double click, drop.
	/// @param panel The panel that holds the mouse.
	/// @param event The event.
	void mouseButton(Panel& panel, const MouseButtonEvent& event);
	/// @brief Handles pointer moves while a panel holds the mouse: onDrag, drag and drop.
	/// @param panel The panel.
	/// @param event The event.
	void mouseMove(Panel& panel, const MouseMoveEvent& event);
	/// @brief Runs shortcuts ("keys") and "onKey" for a key pressed on a focused panel.
	/// @param panel The panel.
	/// @param event The event.
	void key(Panel& panel, const KeyEvent& event);
	/// @brief Sets a visual flag on a panel and its ancestors (like CSS :active), refreshing them.
	/// @param from The innermost panel.
	/// @param flag Which flag.
	/// @param on Its value.
	void setFlagUpwards(Panel* from, bool Record::* flag, bool on);
	/// @brief Closes the open overlays that close on a click outside them.
	/// @param position The click, or nullptr for Escape (closes them all).
	void dismissOverlays(const Vec2* position);

	/* Behaviour */
	/// @brief Runs an event handler (an action name or inline steps); errors are reported, never thrown.
	/// @param handler The handler.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The sender's variables.
	/// @param where Its place, for errors.
	void handle(const json& handler, Panel& sender, const json& event, const Scope& locals, const std::string& where);
	/// @brief Runs an action from "actions", else a native one, else the unhandled fallback.
	/// @param name The action.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The caller's variables.
	/// @param where Its place, for errors.
	/// @throws DocumentError If a step fails, or nobody handles the action.
	void runNamed(const std::string& name, Panel& sender, const json& event, const Scope& locals,
	              const std::string& where);
	/// @brief Runs steps in order.
	/// @param steps A step or an array of steps.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals Variables.
	/// @param where Its place, for errors.
	/// @return false if a "stop" step ended the action.
	/// @throws DocumentError If a step fails or actions nest more than 32 levels deep.
	bool runSteps(const json& steps, Panel& sender, const json& event, const Scope& locals, const std::string& where);
	/// @brief Runs one step (set, push, if, forEach, while, emit, patch, call, after, animate, ...).
	/// @param step The step.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals Variables.
	/// @param where Its place, for errors.
	/// @return false if it was "stop".
	/// @throws DocumentError If it fails.
	bool runStep(const json& step, Panel& sender, const json& event, const Scope& locals, const std::string& where);
	/// @brief Runs steps later, from a timer, with a sender that may be gone by then (the root stands in).
	/// @param steps The steps.
	/// @param sender The panel they come from.
	/// @param event The event.
	/// @param locals Variables.
	/// @param where Its place, for errors.
	/// @return The task.
	std::function<void()> later(json steps, Panel& sender, json event, SharedScope locals, std::string where);
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
			NamedColor named;
			auto parse = [&](const json& hex, const std::string& at) {
				const auto parsed = Color::parseHex(string(hex, at));
				if(!parsed) fail(at, "expected a hex color like \"#6366f1\"");
				return *parsed;
			};
			if(value.is_object()) { // per theme: {"light": "#ffffff", "dark": "#111827"}
				if(value.empty()) fail(path, "expected at least one theme");
				for(const auto& [themeName, hex] : value.items())
					named.themes.emplace(themeName, parse(hex, path + "." + themeName));
				named.base = named.themes.at(value.begin().key());
			} else {
				named.base = parse(value, path);
			}
			newColors.emplace(name, std::move(named));
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
	if(!globalsCache) {
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
		           {"mode", toString(versions.mode())},
		           {"theme", theme},
		           {"screen", screen}};
	}

	return *globalsCache;
}

json Runtime::Impl::evaluate(const json& value, const Scope& locals, const json& event, const std::string& where) const {
	return format::evaluate(value, context(locals, event), where);
}

// "#rrggbb", "@name" (from "colors", in the current theme) or "transparent"
Color Runtime::Impl::color(const json& value, const std::string& path) const {
	const std::string name = string(value, path);
	if(name == "transparent") return colors::transparent;

	if(name.starts_with('@')) {
		const auto it = colors.find(std::string_view(name).substr(1));
		if(it == colors.end()) fail(path, "unknown color '" + name + "'");
		const auto variant = it->second.themes.find(theme);
		return variant != it->second.themes.end() ? variant->second : it->second.base;
	}

	const auto parsed = Color::parseHex(name);
	if(!parsed) fail(path, "expected a color: \"#rrggbb\", \"#rrggbbaa\", \"@name\" or \"transparent\"");
	return *parsed;
}

// The node's properties on top of its styles' (in order: later styles override earlier ones)
json Runtime::Impl::withStyles(const json& node, const std::string& path, int depth) const {
	object(node, path);
	if(!node.contains("style")) return node;
	if(depth > 16) fail(path + ".style", "styles nested too deeply (a style uses itself?)");

	const json& style = node["style"];
	if(isDynamic(style)) return node; // chosen per instance, once its variables are known (scoped())
	const json names = style.is_array() ? style : json::array({style});

	json merged = json::object();
	for(const json& name : names) {
		const std::string key = string(name, path + ".style");
		const json& document = versions.document();
		if(!document.contains("styles") || !document["styles"].contains(key))
			fail(path + ".style", "unknown style '" + key + "'");
		merged.update(withStyles(object(document["styles"][key], "styles." + key), "styles." + key, depth + 1));
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
	if(type == "stack" || type == "column") return std::make_unique<Stack>();
	if(type == "row") {
		auto row = std::make_unique<Stack>();
		row->setDirection(Direction::Row);
		return row;
	}
	if(type == "grid") return std::make_unique<Grid>();
	if(type == "toggle") return std::make_unique<Toggle>(Toggle::Look::Switch);
	if(type == "checkbox") return std::make_unique<Toggle>(Toggle::Look::Checkbox);
	if(type == "slider") return std::make_unique<Slider>();
	if(type == "outlet") {
		auto outlet = std::make_unique<Panel>();
		outlet->setColor(colors::transparent);
		return outlet;
	}

	if(const auto it = types.find(type); it != types.end()) {
		auto panel = it->second();
		if(!panel) fail(path, "the factory of type '" + type + "' returned nothing");
		return panel;
	}

	if(placeholderTypes) {
		std::cerr << "AffrescoUI screen: " << path << ": unknown type '" << type << "', shown as a plain panel\n";
		return std::make_unique<Panel>();
	}
	fail(path, "unknown type '" + type +
	               "' (built in: panel, label, textarea, stack, row, column, grid, toggle, checkbox, slider, outlet; "
	               "others must be registered)");
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
	panel.onHover.connect([this, &panel](bool hovered) {
		Record* record = recordOf(&panel);
		if(!record) return;
		record->hovered = hovered;
		refresh(panel, *record);
		fire(panel, hovered ? "onHover" : "onLeave", {{"type", hovered ? "hover" : "leave"}});
	});
	panel.onFocusChanged.connect([this, &panel](bool focused) {
		Record* record = recordOf(&panel);
		if(!record) return;
		record->focused = focused;
		refresh(panel, *record);
		fire(panel, focused ? "onFocus" : "onBlur", {{"type", focused ? "focus" : "blur"}});
	});
	panel.onMouseButton.connect([this, &panel](const MouseButtonEvent& event) { mouseButton(panel, event); });
	panel.onMouseMove.connect([this, &panel](const MouseMoveEvent& event) { mouseMove(panel, event); });
	panel.onScroll.connect([this, &panel](const ScrollEvent& event) {
		bubble(panel, "onScroll", {{"type", "scroll"}, {"dx", event.offset.x}, {"dy", event.offset.y}});
	});
	panel.onKey.connect([this, &panel](const KeyEvent& event) { key(panel, event); });

	if(auto* textArea = dynamic_cast<TextArea*>(&panel))
		textArea->onTextChanged.connect([this, &panel](const std::string& value) { valueChanged(panel, value); });
	if(auto* toggle = dynamic_cast<Toggle*>(&panel))
		toggle->onToggled.connect([this, &panel](bool checked) { valueChanged(panel, checked); });
	if(auto* slider = dynamic_cast<Slider*>(&panel))
		slider->onValueChanged.connect([this, &panel](double value) {
			valueChanged(panel, std::trunc(value) == value ? json(static_cast<std::int64_t>(value)) : json(value));
		});
}

void Runtime::Impl::valueChanged(Panel& panel, const json& value) {
	Record* record = recordOf(&panel);
	if(!record) return;
	if(record->bind) {
		try {
			const json none = json::object();
			versions.state()[format::statePointer(*record->bind, context(*record->locals, none), record->path + ".bind")] =
			    value;
			changed();
			requestReconcile();
		} catch(const DocumentError& error) {
			report(error.what());
		}
	}
	fire(panel, "onChange", {{"type", "change"}, {"value", value}});
}

json Runtime::Impl::evaluateNode(const json& merged, const Scope& locals, const std::string& path) const {
	json result = json::object();
	for(const auto& [key, value] : merged.items()) {
		if(key == "children") continue; // built from the node itself (reconcileChildren), never copied
		result[key] =
		    structural(key) || key == "type" ? value : evaluate(value, locals, json::object(), path + "." + key);
	}
	return result;
}

json Runtime::Impl::instantiateComponent(const json& use, const std::string& path) const {
	const std::string name = string(use["use"], path + ".use");
	const json& document = versions.document();
	if(!document.contains("components") || !document["components"].contains(name))
		fail(path + ".use", "unknown component '" + name + "'");
	const json& component = object(document["components"][name], "components." + name);
	if(!component.contains("node")) fail("components." + name, "a component needs a \"node\"");

	json result = object(component["node"], "components." + name + ".node");
	for(const auto& [key, value] : use.items())
		if(key != "use" && key != "with" && key != "children") result[key] = value;

	// Parameters: the component's defaults, then what the use passes (evaluated in the caller's scope)
	json with = component.contains("params") ? object(component["params"], "components." + name + ".params")
	                                         : json::object();
	if(use.contains("with")) with.update(object(use["with"], path + ".with"));
	result["with"] = std::move(with);

	// The use's children go where the component has {"slot": true}
	if(use.contains("children")) {
		std::function<bool(json&)> fill = [&](json& node) {
			if(!node.is_object() || !node.contains("children") || !node["children"].is_array()) return false;
			json& children = node["children"];
			for(std::size_t i = 0; i < children.size(); ++i) {
				if(children[i].is_object() && children[i].value("slot", json(false)) == true) {
					json replaced = json::array();
					for(std::size_t j = 0; j < children.size(); ++j) {
						if(j == i) replaced.insert(replaced.end(), use["children"].begin(), use["children"].end());
						else replaced.push_back(children[j]);
					}
					children = std::move(replaced);
					return true;
				}
				if(!children[i].contains("use") && fill(children[i])) return true;
			}
			return false;
		};
		if(!fill(result)) result["children"] = use["children"];
	}
	return result;
}

// The new variables form one layer over the parent's scope; with and let see the ones before them
NodeRef Runtime::Impl::scoped(const NodeRef& shared, SharedScope& scope, const std::string& at) const {
	const json& node = *shared;
	const json none = json::object();
	Scope layer{scope.get(), nullptr, json::object()};
	if(node.contains("with"))
		for(const auto& [name, value] : object(node["with"], at + ".with").items())
			layer.vars[name] = evaluate(value, layer, none, at + ".with." + name);
	if(node.contains("let")) // in order: each sees the ones before it
		for(const auto& [name, value] : object(node["let"], at + ".let").items())
			layer.vars[name] = evaluate(value, layer, none, at + ".let." + name);
	if(node.contains("check")) {
		const json& check = object(node["check"], at + ".check");
		// The value checked: "value", else the node's bound value
		const json value = check.contains("value") ? evaluate(check["value"], layer, none, at + ".check.value")
		                   : node.contains("bind") && node["bind"].is_string()
		                       ? format::lookup(text(evaluate(node["bind"], layer, none, at + ".bind")),
		                                        context(layer, none), at + ".bind")
		                       : json();
		layer.vars["value"] = value;
		const bool valid = !check.contains("rule") || truthy(evaluate(check["rule"], layer, none, at + ".check.rule"));
		layer.vars["valid"] = valid;
		layer.vars["error"] =
		    valid ? json("") : evaluate(check.value("message", json("Invalid")), layer, none, at + ".check");
	}
	NodeRef result = shared;
	if(node.contains("style") && isDynamic(node["style"])) { // this instance's own copy, with its style
		json chosen = node;
		chosen["style"] = evaluate(node["style"], layer, none, at + ".style");
		if(chosen["style"].is_null() || chosen["style"] == "") chosen.erase("style");
		result = std::make_shared<const json>(withStyles(chosen, at));
	}
	if(!layer.vars.empty()) scope = extend(std::move(scope), std::move(layer.vars));
	return result;
}

// The children to build, after "use", "repeat", "with", "let", "check" and "when"
std::vector<Item> Runtime::Impl::expand(const json& children, const SharedScope& locals, const std::string& path) const {
	std::vector<Item> items;
	if(!children.is_array()) fail(path + ".children", "expected an array");

	for(std::size_t i = 0; i < children.size(); ++i) {
		const std::string at = path + ".children[" + std::to_string(i) + "]";
		const json& child = object(children[i], at);
		NodeRef shared;
		if(child.contains("use")) {
			json raw = child;
			for(int depth = 0; raw.contains("use"); ++depth) {
				if(depth >= maxComponentDepth) fail(at, "components nested too deeply (a component uses itself?)");
				raw = instantiateComponent(raw, at);
			}
			shared = std::make_shared<const json>(withStyles(raw, at));
		} else if(child.contains("style") && !isDynamic(child["style"])) {
			shared = std::make_shared<const json>(withStyles(child, at));
		} else {
			shared = borrow(child);
		}
		const json& merged = *shared;

		auto add = [&](SharedScope scope, const std::string& where, json itemKey) {
			NodeRef node = scoped(shared, scope, where);
			if(node->contains("when") &&
			   !format::truthy(evaluate((*node)["when"], *scope, json::object(), where + ".when")))
				return;
			items.push_back({std::move(node), std::move(scope), where, std::move(itemKey)});
		};

		if(!merged.contains("repeat")) {
			add(locals, at, nullptr);
			continue;
		}

		const json& repeat = object(merged["repeat"], at + ".repeat");
		const std::string name = repeat.contains("for") ? string(repeat["for"], at + ".repeat.for") : "item";
		const std::string indexName =
		    repeat.contains("index") ? string(repeat["index"], at + ".repeat.index") : "index";
		if(!repeat.contains("in")) fail(at + ".repeat", "needs \"in\": the list to repeat over");

		// "in" is a path ("state.messages") or any value/expression
		const json& source = repeat["in"];
		const json list = source.is_string() && source.get<std::string>().find("{{") == std::string::npos
		                      ? evaluate("{{" + source.get<std::string>() + "}}", *locals, json::object(), at + ".repeat.in")
		                      : evaluate(source, *locals, json::object(), at + ".repeat.in");
		if(list.is_null()) continue;
		if(!list.is_array()) fail(at + ".repeat.in", "expected a list, got " + list.dump());

		for(std::size_t k = 0; k < list.size(); ++k) {
			SharedScope scope = extend(locals, {{name, list[k]}, {indexName, k}});
			const std::string where = at + "[" + std::to_string(k) + "]";
			json itemKey = repeat.contains("key") ? evaluate(repeat["key"], *scope, json::object(), where + ".repeat.key")
			                                      : json();
			add(std::move(scope), where, std::move(itemKey));
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
	panel.setGrow(0.0f);
	panel.setTooltip({});
	panel.clearOverlay();

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
		stack->setDirection(record.type == "row" ? Direction::Row : Direction::Column);
		stack->setWrap(false);
	}
	if(auto* grid = dynamic_cast<Grid*>(&panel)) {
		grid->setColumns(2);
		grid->setGap(0.0f);
	}
	if(auto* toggle = dynamic_cast<Toggle*>(&panel)) {
		toggle->setOnColor(Color::hex(0x4f46e5));
		toggle->setOffColor(Color::hex(0xd1d5db));
		toggle->setKnobColor(colors::white);
	}
	if(auto* slider = dynamic_cast<Slider*>(&panel)) {
		slider->setRange(0.0, 100.0, 0.0);
		slider->setColors(Color::hex(0xe5e7eb), Color::hex(0x4f46e5), colors::white);
	}

	record.disabled = false;
	record.transition = json::object();
	record.overlayOpen.reset();
	record.dismissOutside = false;
	record.overlayAnchor.clear();
}

void Runtime::Impl::apply(Panel& panel, Record& record, const json& properties, const std::string& path, bool isRoot,
                          bool created) {
	record.properties = properties;
	record.path = path;

	// Behaviour: handlers, bindings, shortcuts, drag, timers
	record.handlers = json::object();
	record.keys = nullptr;
	record.drag = nullptr;
	record.bind.reset();
	bool autofocus = false;
	double everyMs = 0.0;
	for(const auto& [key, value] : properties.items()) {
		const std::string at = path + "." + key;
		if(handlerKey(key)) {
			if(std::ranges::find(handlerNames, key) == std::end(handlerNames))
				fail(at, "unknown event (expected onClick, onDoubleClick, onContextMenu, onPress, onRelease, onDrag, "
				         "onDrop, onHover, onLeave, onFocus, onBlur, onScroll, onKey, onChange, onSubmit, onShow or "
				         "onHide)");
			if(!value.is_string() && !value.is_array() && !value.is_object())
				fail(at, "expected an action name or steps");
			if(key == "onSubmit" && !dynamic_cast<TextArea*>(&panel)) fail(at, "only text areas have \"onSubmit\"");
			record.handlers[key] = value;
		} else if(key == "bind") {
			if(!dynamic_cast<TextArea*>(&panel) && !dynamic_cast<Toggle*>(&panel) && !dynamic_cast<Slider*>(&panel))
				fail(at, "only text areas, toggles, checkboxes and sliders can \"bind\"");
			// A path, or "{{...}}" giving one (e.g. a component's parameter)
			record.bind = string(isDynamic(value) ? evaluate(value, *record.locals, json::object(), at) : value, at);
		} else if(key == "keys") {
			record.keys = json::object();
			for(const auto& [name, handler] : object(value, at).items())
				record.keys[normalizeShortcut(name)] = handler;
		} else if(key == "drag") {
			record.drag = object(value, at);
		} else if(key == "every") {
			const json& every = object(value, at);
			if(!every.contains("ms") || !every.contains("do")) fail(at, "expected {\"ms\": milliseconds, \"do\": steps}");
			everyMs = std::max(number(evaluate(every["ms"], *record.locals, json::object(), at + ".ms"), at + ".ms"), 16.0f);
		} else if(key == "autofocus") {
			autofocus = boolean(value, at);
		} else if(key == "locked") {
			if(boolean(value, at) && !properties.contains("id")) fail(at, "a locked node needs an \"id\"");
		}
	}

	// "every": a repeating timer while the node exists
	if(window && everyMs != record.everyMs) {
		if(record.everyTimer) window->cancelTimer(std::exchange(record.everyTimer, 0));
		record.everyMs = everyMs;
		if(everyMs > 0.0) {
			auto tick = std::make_shared<std::function<void()>>();
			*tick = [this, panel = &panel, tick] {
				Record* r = recordOf(panel);
				if(!r || !r->properties.contains("every")) return;
				r->everyTimer = window->setTimer(r->everyMs, *tick);
				const SharedScope locals = r->locals;
				handle(r->properties["every"]["do"], *panel, {{"type", "tick"}}, *locals, r->path + ".every.do");
			};
			record.everyTimer = window->setTimer(everyMs, *tick);
		}
	}

	applyVisual(panel, record, visual(record), isRoot);

	// Values: a bound widget mirrors its state value; otherwise "text" is applied when it changes, so a text area's
	// typing isn't overwritten by every update
	auto* label = dynamic_cast<Label*>(&panel);
	const json none = json::object();
	const json bound = record.bind ? format::lookup(*record.bind, context(*record.locals, none), path + ".bind") : json();
	if(label && record.bind) {
		const std::string value = text(bound);
		if(label->text() != value) label->setText(value);
	} else if(label) {
		const std::optional<std::string> newText =
		    properties.contains("text") ? std::optional<std::string>(text(properties["text"])) : std::nullopt;
		if(newText != record.lastText) label->setText(newText.value_or(""));
		record.lastText = newText;
	}
	if(auto* toggle = dynamic_cast<Toggle*>(&panel))
		toggle->setChecked(record.bind ? truthy(bound) : truthy(properties.value("checked", json(false))));
	if(auto* slider = dynamic_cast<Slider*>(&panel)) {
		const json value = record.bind ? bound : properties.value("value", json(0));
		slider->setValue(value.is_number() ? value.get<double>() : 0.0);
	}

	if(created && autofocus && window) window->setFocus(&panel);
	record.shown = true;
}

// The node at rest, then each active state block on top (with its own styles)
json Runtime::Impl::visual(const Record& record) const {
	json result = json::object();
	for(const auto& [key, value] : record.properties.items())
		if(!structural(key) && std::ranges::find(stateBlocks, key) == std::end(stateBlocks)) result[key] = value;

	const bool active[] = {record.hovered, record.focused, record.dragging, record.dragOver, record.down};
	for(std::size_t i = 0; i < std::size(stateBlocks); ++i) {
		const std::string name(stateBlocks[i]);
		if(!active[i] || !record.properties.contains(name)) continue;
		json block = withStyles(object(record.properties[name], record.path + "." + name), record.path + "." + name);
		for(const auto& [key, value] : block.items())
			if(!structural(key) && key != "type") result[key] = value;
	}
	return result;
}

void Runtime::Impl::refresh(Panel& panel, Record& record) {
	if(std::ranges::none_of(stateBlocks, [&](std::string_view name) { return record.properties.contains(name); }))
		return;
	try {
		applyVisual(panel, record, visual(record), &panel == &window->root());
	} catch(const DocumentError& error) {
		report(error.what());
	}
}

void Runtime::Impl::applyVisual(Panel& panel, Record& record, const json& properties, bool isRoot) {
	const std::string& path = record.path;
	auto* label = dynamic_cast<Label*>(&panel);
	auto* textArea = dynamic_cast<TextArea*>(&panel);
	auto* stack = dynamic_cast<Stack*>(&panel);
	auto* grid = dynamic_cast<Grid*>(&panel);
	auto* toggle = dynamic_cast<Toggle*>(&panel);
	auto* slider = dynamic_cast<Slider*>(&panel);

	const Color shownColor = panel.color();
	const Color shownText = label ? label->textColor() : Color{};
	reset(panel, record);

	std::optional<Color> background, foreground;
	std::optional<std::pair<bool, bool>> scroll;
	std::optional<bool> overlayOpen;
	double sliderMin = 0.0, sliderMax = 100.0, sliderStep = 0.0;
	bool sliderRange = false;
	std::optional<Color> trackColor, fillColor, knobColor;

	for(const auto& [key, value] : properties.items()) {
		const std::string at = path + "." + key;

		// Handled in apply()
		if(key == "type" || key == "autofocus" || key == "locked" || key == "checked" || key == "value" ||
		   key == "text")
			continue;

		// Common to every panel
		else if(key == "id")
			panel.setId(string(value, at));
		else if(key == "color") background = color(value, at);
		else if(key == "visible") panel.setVisible(boolean(value, at));
		else if(key == "cursor") panel.setCursor(cursor(value, at));
		else if(key == "tooltip") panel.setTooltip(text(value));
		else if(key == "disabled") record.disabled = boolean(value, at);
		else if(key == "transition") {
			for(const auto& [property, ms] : object(value, at).items()) {
				if(property != "color" && property != "textColor")
					fail(at + "." + property, "only \"color\" and \"textColor\" can transition");
				number(ms, at + "." + property);
			}
			record.transition = value;
		} else if(key == "scroll") {
			const std::string mode = value.is_boolean() ? (value.get<bool>() ? "vertical" : "none") : string(value, at);
			if(mode != "vertical" && mode != "horizontal" && mode != "both" && mode != "none")
				fail(at, "expected \"vertical\", \"horizontal\", \"both\" or \"none\"");
			scroll = {mode == "horizontal" || mode == "both", mode == "vertical" || mode == "both"};
		} else if(key == "overlay") {
			const json& overlay = object(value, at);
			for(const auto& [k, v] : overlay.items())
				if(k != "anchor" && k != "placement" && k != "open" && k != "dismiss")
					fail(at + "." + k, "unknown property (expected anchor, placement, open or dismiss)");
			std::string anchor = overlay.contains("anchor") ? string(overlay["anchor"], at + ".anchor") : "";
			if(anchor.starts_with('#')) anchor.erase(0, 1);
			record.overlayAnchor = anchor;
			panel.setOverlay(anchor, overlay.contains("placement") ? placement(overlay["placement"], at + ".placement")
			                                                      : Placement::Below);
			if(overlay.contains("open")) {
				const json& open = overlay["open"];
				if(open.is_string()) { // a state path, so a click outside can close it
					record.overlayOpen = open.get<std::string>();
					overlayOpen = truthy(format::lookup(open.get<std::string>(),
					                                    context(*record.locals, json::object()), at + ".open"));
				} else {
					overlayOpen = truthy(open);
				}
			}
			if(overlay.contains("dismiss")) {
				if(string(overlay["dismiss"], at + ".dismiss") != "outside") fail(at + ".dismiss", "expected \"outside\"");
				if(!record.overlayOpen) fail(at + ".dismiss", "needs \"open\" to be a state path, e.g. \"state.menuOpen\"");
				record.dismissOutside = true;
			}
		}

		// Layout (the root always fills the window)
		else if(isRoot) fail(at, "not allowed on the root, which always fills the window");
		else if(key == "x") panel.setX(length(value, at));
		else if(key == "y") panel.setY(length(value, at));
		else if(key == "width") panel.setWidth(length(value, at));
		else if(key == "height") panel.setHeight(length(value, at));
		else if(key == "minWidth") panel.setMinWidth(length(value, at));
		else if(key == "minHeight") panel.setMinHeight(length(value, at));
		else if(key == "grow") panel.setGrow(number(value, at));
		else if(key == "fill") {
			const Length offset = value.is_boolean() && value.get<bool>() ? px(0.0f) : length(value, at);
			for(Edge e : {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right}) panel.setAnchor(e, offset);
		} else if(key == "anchors") {
			for(const auto& [name, offset] : object(value, at).items()) {
				const std::string where = at + "." + name;
				if(offset.is_string() && offset.get<std::string>().starts_with('#')) {
					// Another panel's edge: "#header.bottom + 8"
					const std::string spec = offset.get<std::string>();
					const std::size_t dot = spec.find('.');
					std::size_t end = dot == std::string::npos ? spec.size() : dot + 1;
					while(end < spec.size() && std::isalpha(static_cast<unsigned char>(spec[end]))) ++end;
					if(dot == std::string::npos)
						fail(where, "expected \"#id.edge\", e.g. \"#header.bottom + 8\"");
					std::string rest = spec.substr(end);
					std::erase(rest, ' ');
					float delta = 0.0f;
					if(!rest.empty()) {
						try {
							std::size_t used = 0;
							delta = std::stof(rest, &used);
							if(used != rest.size() || (rest[0] != '+' && rest[0] != '-')) throw std::invalid_argument(rest);
						} catch(const std::exception&) {
							fail(where, "expected \"#id.edge\", \"#id.edge + 8\" or \"#id.edge - 8\"");
						}
					}
					panel.setAnchorTo(edge(name, where), spec.substr(1, dot - 1),
					                  edge(spec.substr(dot + 1, end - dot - 1), where), delta);
				} else {
					panel.setAnchor(edge(name, where), length(offset, where));
				}
			}
		}

		// Labels and text areas
		else if(label && key == "textColor")
			foreground = color(value, at);
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

		// Stacks (row, column) and grids
		else if(stack && key == "gap") stack->setGap(number(value, at));
		else if(stack && (key == "from" || key == "align")) stack->setAlignment(align(value, at)); // "align": v1 name
		else if(stack && key == "direction") {
			const std::string direction = string(value, at);
			if(direction != "row" && direction != "column") fail(at, "expected \"row\" or \"column\"");
			stack->setDirection(direction == "row" ? Direction::Row : Direction::Column);
		} else if(stack && key == "wrap") stack->setWrap(boolean(value, at));
		else if(grid && key == "columns") grid->setColumns(integer(value, at));
		else if(grid && key == "gap") grid->setGap(number(value, at));

		// Value widgets
		else if(toggle && key == "activeColor") toggle->setOnColor(color(value, at));
		else if(toggle && key == "trackColor") toggle->setOffColor(color(value, at));
		else if((toggle || slider) && key == "knobColor") knobColor = color(value, at);
		else if(slider && (key == "min" || key == "max" || key == "step")) {
			sliderRange = true;
			const double n = number(value, at);
			(key == "min" ? sliderMin : key == "max" ? sliderMax : sliderStep) = n;
		} else if(slider && key == "trackColor") trackColor = color(value, at);
		else if(slider && key == "activeColor") fillColor = color(value, at);

		else fail(at, "unknown property '" + key + "' for this type");
	}

	if(background) transitionColor(panel, record, "color", shownColor, *background);
	if(label && foreground) transitionColor(panel, record, "textColor", shownText, *foreground);
	if(scroll && (panel.isScrollable() || scroll->first || scroll->second)) panel.setScrollable(scroll->first, scroll->second);
	else if(!scroll && panel.isScrollable()) panel.setScrollable(false, false);
	if(overlayOpen && !*overlayOpen) panel.setVisible(false);
	if(toggle && knobColor) toggle->setKnobColor(*knobColor);
	if(slider) {
		if(sliderRange) slider->setRange(sliderMin, sliderMax, sliderStep);
		slider->setColors(trackColor.value_or(Color::hex(0xe5e7eb)), fillColor.value_or(Color::hex(0x4f46e5)),
		                  knobColor.value_or(colors::white));
	}
}

// With a transition, the colour moves from what was shown to the target over that many milliseconds
void Runtime::Impl::transitionColor(Panel& panel, Record& record, const char* property, Color shown, Color target) {
	const bool text = std::string_view(property) == "textColor";
	auto set = [&panel, text](Color c) {
		if(text) static_cast<Label&>(panel).setTextColor(c);
		else panel.setColor(c);
	};
	Tween& tween = text ? record.textTween : record.colorTween;
	const double ms = record.transition.contains(property) ? record.transition[property].get<double>() : 0.0;

	const Color current = shown;
	if(!window || ms <= 0.0 || current == target || !record.shown) {
		if(tween.timer && window) window->cancelTimer(std::exchange(tween.timer, 0));
		set(target);
		return;
	}
	if(tween.timer && tween.to == target) { // already on its way there
		set(current);
		return;
	}
	if(tween.timer) window->cancelTimer(tween.timer);
	tween = {current, target, now(), ms, 0};
	set(current);

	auto step = std::make_shared<std::function<void()>>();
	*step = [this, panel = &panel, text, step] {
		Record* r = recordOf(panel);
		if(!r) return;
		Tween& t = text ? r->textTween : r->colorTween;
		const double progress = std::min((now() - t.start) * 1000.0 / t.ms, 1.0);
		const Color c = Color::mix(t.from, t.to, static_cast<float>(ease("out", progress)));
		if(text) static_cast<Label*>(panel)->setTextColor(c);
		else panel->setColor(c);
		t.timer = progress < 1.0 ? window->setTimer(frameMs, *step) : 0;
	};
	tween.timer = window->setTimer(frameMs, *step);
}

void Runtime::Impl::reconcileChildren(const json& node, Panel& panel, Record& record, const std::string& path) {
	std::vector<Item> items;
	if(record.type == "outlet") {
		// The current screen, filling the outlet unless it places itself
		const json& document = versions.document();
		if(document.contains("screens") && document["screens"].contains(screen)) {
			json screenNode = withStyles(document["screens"][screen], "screens." + screen);
			if(!hasLayout(screenNode)) screenNode["fill"] = 0;
			SharedScope scope = record.locals;
			NodeRef scopedNode = scoped(std::make_shared<const json>(std::move(screenNode)), scope, "screens." + screen);
			items.push_back({std::move(scopedNode), std::move(scope), "screens." + screen, nullptr});
		}
	} else if(node.contains("children")) {
		items = expand(node["children"], record.locals, path);
	} else if(record.managed == 0) {
		return; // nothing built here before: leave the panel's own children alone
	}

	std::size_t index = record.firstManaged;
	for(Item& item : items) {
		const json props = evaluateNode(*item.node, *item.locals, item.path);
		const std::string type = props.contains("type") ? string(props["type"], item.path + ".type") : "panel";

		// A keyed item finds its panel wherever it is now, so it keeps its focus, scroll and drag when lists reorder
		if(!item.key.is_null()) {
			for(std::size_t j = index; j < panel.children().size(); ++j) {
				const Record* r = recordOf(panel.children()[j].get());
				if(r && r->type == type && r->key == item.key) {
					if(j != index) panel.moveChild(j, index);
					break;
				}
			}
		}

		// Reuse the panel already in this place if it has the same type (and key): it keeps focus, hover and drag
		Panel* child = nullptr;
		bool created = false;
		if(index < panel.children().size()) {
			Panel& existing = *panel.children()[index];
			const Record* r = recordOf(&existing);
			if(r && r->type == type && r->key == item.key) {
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
		childRecord.key = std::move(item.key);
		apply(*child, childRecord, props, item.path, false, created);
		reconcileChildren(*item.node, *child, childRecord, item.path);
		if(created && childRecord.handlers.contains("onShow"))
			window->defer(later(childRecord.handlers["onShow"], *child, {{"type", "show"}}, childRecord.locals,
			                    childRecord.path + ".onShow"));
		++index;
	}

	for(std::size_t i = index; i < panel.children().size(); ++i) forgetSubtree(*panel.children()[i]);
	panel.removeChildren(index);
	record.managed = index - record.firstManaged;
}

void Runtime::Impl::forgetSubtree(const Panel& panel) {
	if(Record* record = recordOf(&panel)) {
		if(window) {
			if(record->everyTimer) window->cancelTimer(record->everyTimer);
			if(record->colorTween.timer) window->cancelTimer(record->colorTween.timer);
			if(record->textTween.timer) window->cancelTimer(record->textTween.timer);
			if(record->handlers.contains("onHide"))
				window->defer(later(record->handlers["onHide"], window->root(), {{"type", "hide"}}, record->locals,
				                    record->path + ".onHide"));
		}
		if(press.panel == &panel) press = {};
		if(press.source == &panel) press.source = nullptr;
		if(press.over == &panel) press.over = nullptr;
		if(lastClick.owner == &panel) lastClick.owner = nullptr;
		records.erase(&panel);
	}
	for(const auto& child : panel.children()) forgetSubtree(*child);
}

// Brings the UI in line with the document and the state
void Runtime::Impl::reconcile() {
	if(!window) return;
	reconcileQueued = false;
	stateChanged = false;
	const json& document = versions.document();

	// The screen an outlet shows: the first one, unless navigation chose another that still exists
	if(document.contains("screens") && document["screens"].is_object() && !document["screens"].empty() &&
	   !document["screens"].contains(screen)) {
		screen = document["screens"].begin().key();
		screenHistory.clear();
	}
	globalsChanged(); // window size, history, proposals, screen: refreshed once per update
	if(document.contains("theme")) {
		theme = text(evaluate(document["theme"], *noVariables(), json::object(), "theme"));
		globalsChanged();
	} else {
		theme.clear();
	}

	// Revisions can change the fonts: the default one is the window's
	if(const auto it = fonts.find("default"); it != fonts.end() && window->defaultFont() != it->second.get())
		window->setDefaultFont(it->second);

	Panel& root = window->root();
	if(!records.contains(&root)) adopt(root, "root");
	Record& record = records.at(&root);

	SharedScope scope = noVariables();
	const NodeRef node = scoped(std::make_shared<const json>(withStyles(document["root"], "root")), scope, "root");
	record.locals = std::move(scope);
	const json properties = evaluateNode(*node, *record.locals, "root");
	apply(root, record, properties, "root", true, false);
	reconcileChildren(*node, root, record, "root");

	// "watch": a handler runs after the update in which its value changed
	if(document.contains("watch")) {
		for(const auto& [path, handler] : object(document["watch"], "watch").items()) {
			const json value = format::lookup(path, context(*noVariables(), json::object()), "watch." + path);
			if(watched.contains(path) && watched[path] != value)
				window->defer(later(handler, root, {{"type", "change"}, {"value", value}, {"previous", watched[path]}},
				                    noVariables(), "watch." + path));
			watched[path] = value;
		}
	}
	owner.onUpdated();
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
	SharedScope scope = noVariables();
	const NodeRef scopedNode = scoped(std::make_shared<const json>(withStyles(node, path)), scope, path);
	const json props = evaluateNode(*scopedNode, *scope, path);
	const std::string type = props.contains("type") ? string(props["type"], path + ".type") : "panel";

	Panel& panel = parent.addChild(create(type, path));
	Record& record = adopt(panel, type);
	record.locals = std::move(scope);
	apply(panel, record, props, path, false, true);
	reconcileChildren(*scopedNode, panel, record, path);
	return panel;
}

/* Events */

Panel* Runtime::Impl::handlerOwner(Panel* from, std::string_view handler) {
	const bool bubbles = std::ranges::find(bubbling, handler) != std::end(bubbling);
	for(Panel* panel = from; panel; panel = bubbles ? panel->parent() : nullptr) {
		const Record* record = recordOf(panel);
		if(record && record->handlers.contains(std::string(handler))) return record->disabled ? nullptr : panel;
	}
	return nullptr;
}

void Runtime::Impl::fire(Panel& panel, std::string_view handler, json event) {
	Record* record = recordOf(&panel);
	if(!record || record->disabled || !record->handlers.contains(std::string(handler))) return;
	const json steps = record->handlers[std::string(handler)];
	const SharedScope locals = record->locals; // the handler may rebuild the panel
	handle(steps, panel, event, *locals, record->path + "." + std::string(handler));
}

json Runtime::Impl::pointerEvent(std::string_view type, const Panel& panel, Vec2 position) {
	const Rect& f = panel.frame();
	return {{"type", std::string(type)}, {"x", position.x},     {"y", position.y},       {"left", position.x - f.left},
	        {"top", position.y - f.top}, {"width", f.width()}, {"height", f.height()}};
}

bool Runtime::Impl::bubble(Panel& from, std::string_view handler, json event, const Vec2* position) {
	Panel* target = handlerOwner(&from, handler);
	if(!target) return false;
	if(position) {
		json fields = pointerEvent(event.value("type", ""), *target, *position);
		fields.update(event);
		event = std::move(fields);
	}
	fire(*target, handler, std::move(event));
	return true;
}

void Runtime::Impl::setFlagUpwards(Panel* from, bool Record::* flag, bool on) {
	for(Panel* panel = from; panel; panel = panel->parent()) {
		Record* record = recordOf(panel);
		if(!record || record->*flag == on) continue;
		record->*flag = on;
		refresh(*panel, *record);
	}
}

void Runtime::Impl::mouseButton(Panel& panel, const MouseButtonEvent& event) {
	const Vec2 at = event.position;

	if(event.button == MouseButton::Right) {
		if(event.action == affresco::Action::Release && panel.bounds().contains(at))
			bubble(panel, "onContextMenu", {{"type", "contextMenu"}}, &at);
		return;
	}
	if(event.button != MouseButton::Left) return;

	if(event.action == affresco::Action::Press) {
		press = {};
		press.panel = &panel;
		press.position = at;
		for(Panel* p = &panel; p; p = p->parent())
			if(const Record* r = recordOf(p); r && !r->drag.is_null()) {
				if(!r->disabled) press.source = p;
				break;
			}
		setFlagUpwards(&panel, &Record::down, true);
		bubble(panel, "onPress", {{"type", "press"}}, &at);
		return;
	}

	// Release
	setFlagUpwards(&panel, &Record::down, false);
	const Press ended = std::exchange(press, {});
	if(ended.dragging) {
		if(Record* source = recordOf(ended.source)) {
			source->dragging = false;
			refresh(*ended.source, *source);
		}
		if(Record* target = recordOf(ended.over)) {
			target->dragOver = false;
			refresh(*ended.over, *target);
			json drop = pointerEvent("drop", *ended.over, at);
			drop["data"] = ended.data;
			fire(*ended.over, "onDrop", std::move(drop));
		}
		return; // a drag is not a click
	}
	if(!panel.bounds().contains(at)) return;

	bubble(panel, "onRelease", {{"type", "release"}}, &at);

	// A toggle or slider keeps its click (like a native control): it changed its value
	const Record* own = recordOf(&panel);
	if((dynamic_cast<Toggle*>(&panel) || dynamic_cast<Slider*>(&panel)) && (!own || !own->handlers.contains("onClick")))
		return;
	Panel* clicked = handlerOwner(&panel, "onClick");
	if(clicked) bubble(panel, "onClick", {{"type", "click"}}, &at);

	// The second click on the same panel soon after the first
	const double time = now();
	Panel* handler = clicked ? clicked : handlerOwner(&panel, "onDoubleClick");
	if(handler && lastClick.owner == handler && time - lastClick.time < doubleClickSeconds) {
		bubble(panel, "onDoubleClick", {{"type", "doubleClick"}}, &at);
		lastClick = {};
	} else {
		lastClick = {handler, time};
	}
}

void Runtime::Impl::mouseMove(Panel& panel, const MouseMoveEvent& event) {
	if(press.panel != &panel) return;
	const Vec2 at = event.position;

	if(press.source && !press.dragging &&
	   std::hypot(at.x - press.position.x, at.y - press.position.y) > dragThreshold) {
		try {
			Record& source = *recordOf(press.source);
			press.data = evaluate(source.drag.value("data", json()), *source.locals, json::object(), source.path + ".drag");
			press.dragging = true;
			source.dragging = true;
			refresh(*press.source, source);
		} catch(const DocumentError& error) {
			report(error.what());
			press.source = nullptr;
		}
	}
	if(press.dragging && window) {
		Panel* over = nullptr;
		for(Panel* p = window->hoveredPanel(); p; p = p->parent())
			if(const Record* r = recordOf(p); r && r->handlers.contains("onDrop") && !r->disabled) {
				over = p;
				break;
			}
		if(over != press.over) {
			if(Record* old = recordOf(press.over)) {
				old->dragOver = false;
				refresh(*press.over, *old);
			}
			press.over = over;
			if(Record* now = recordOf(over)) {
				now->dragOver = true;
				refresh(*over, *now);
			}
		}
	}

	bubble(panel, "onDrag",
	       {{"type", "drag"}, {"dx", at.x - press.position.x}, {"dy", at.y - press.position.y}}, &at);
}

void Runtime::Impl::key(Panel& panel, const KeyEvent& event) {
	if(event.action == affresco::Action::Release) return;

	// Enter in a text area submits
	if(dynamic_cast<TextArea*>(&panel) && (event.key == Key::Enter || event.key == Key::KeypadEnter) &&
	   event.action == affresco::Action::Press)
		if(auto* textArea = dynamic_cast<TextArea*>(&panel))
			fire(panel, "onSubmit", {{"type", "submit"}, {"text", textArea->text()}});

	// Shortcuts on the focused panel or its ancestors
	const std::string name = shortcut(event);
	if(!name.empty()) {
		for(Panel* p = &panel; p; p = p->parent()) {
			const Record* record = recordOf(p);
			if(!record || !record->keys.is_object() || !record->keys.contains(name)) continue;
			const json handler = record->keys[name];
			const SharedScope locals = record->locals;
			handle(handler, *p, {{"type", "key"}, {"key", name}}, *locals, record->path + ".keys." + name);
			return;
		}
	}
	bubble(panel, "onKey",
	       {{"type", "key"},
	        {"key", keyName(event.key)},
	        {"shortcut", name},
	        {"shift", event.mods.shift()},
	        {"ctrl", event.mods.control()},
	        {"alt", event.mods.alt()},
	        {"cmd", event.mods.super()}});
}

void Runtime::Impl::dismissOverlays(const Vec2* position) {
	bool closed = false;
	for(auto& [panel, record] : records) {
		if(!record.dismissOutside || !record.overlayOpen || !panel->isShown()) continue;
		if(position) {
			if(panel->bounds().contains(*position)) continue;
			const Panel* anchor = record.overlayAnchor.empty() ? panel->parent()
			                                                   : window->root().findById(record.overlayAnchor);
			if(anchor && anchor->bounds().contains(*position)) continue; // the anchor toggles it itself
		}
		try {
			versions.state()[format::statePointer(*record.overlayOpen, context(*record.locals, json::object()),
			                                      record.path + ".overlay.open")] = false;
			closed = true;
		} catch(const DocumentError& error) {
			report(error.what());
		}
	}
	if(closed) {
		changed();
		requestReconcile();
	}
}

/* Behaviour */

void Runtime::Impl::report(const std::string& message) {
	std::cerr << message << '\n';
	owner.onError(message);
}

// Runs an event handler: an action name or inline steps. Errors are reported, never thrown into the event loop.
void Runtime::Impl::handle(const json& handler, Panel& sender, const json& event, const Scope& locals,
                           const std::string& where) {
	try {
		if(handler.is_string()) runNamed(handler.get<std::string>(), sender, event, locals, where);
		else runSteps(handler, sender, event, locals, where);
	} catch(const DocumentError& error) {
		report(error.what());
	}
	if(stateChanged) requestReconcile();
}

std::function<void()> Runtime::Impl::later(json steps, Panel& sender, json event, SharedScope locals, std::string where) {
	return [this, steps = std::move(steps), sender = &sender, event = std::move(event), locals = std::move(locals),
	        where = std::move(where)] {
		if(!window) return;
		Panel& from = records.contains(sender) ? *sender : window->root();
		handle(steps, from, event, *locals, where);
	};
}

void Runtime::Impl::runNamed(const std::string& name, Panel& sender, const json& event, const Scope& locals,
                             const std::string& where) {
	const json& document = versions.document();
	if(document.contains("actions") && document["actions"].contains(name)) {
		const json steps = document["actions"][name]; // a copy: the steps may revise the document
		runSteps(steps, sender, event, locals, "actions." + name);
	} else if(const auto it = nativeActions.find(name); it != nativeActions.end()) {
		it->second(sender);
	} else if(unhandled) {
		unhandled(name, sender);
	} else {
		fail(where, "unknown action '" + name + "'");
	}
}

// Returns false when a "stop" step ends the action
bool Runtime::Impl::runSteps(const json& steps, Panel& sender, const json& event, const Scope& locals,
                             const std::string& where) {
	if(++actionDepth > maxActionDepth) {
		--actionDepth;
		fail(where, "actions nested too deeply (more than 32 levels)");
	}

	bool keepGoing = true;
	try {
		if(steps.is_array()) {
			for(std::size_t i = 0; i < steps.size() && keepGoing; ++i)
				keepGoing = runStep(steps[i], sender, event, locals, where + "[" + std::to_string(i) + "]");
		} else {
			keepGoing = runStep(steps, sender, event, locals, where);
		}
	} catch(...) {
		--actionDepth;
		throw;
	}

	--actionDepth;
	return keepGoing;
}

bool Runtime::Impl::runStep(const json& step, Panel& sender, const json& event, const Scope& locals,
                            const std::string& where) {
	object(step, where);
	json& state = versions.state();
	const Context ctx = context(locals, event);
	auto value = [&](const json& v, const std::string& at) { return format::evaluate(v, context(locals, event), at); };
	auto target = [&](const char* key) {
		return format::statePointer(string(step[key], where + "." + key), ctx, where + "." + key);
	};
	// A list given as a path ("state.items") or as any value
	auto list = [&](const json& source, const std::string& at) {
		if(source.is_string() && source.get<std::string>().find("{{") == std::string::npos)
			return format::lookup(source.get<std::string>(), ctx, at);
		return value(source, at);
	};
	auto milliseconds = [&](const char* key) {
		return std::max(static_cast<double>(number(value(step[key], where + "." + key), where + "." + key)), 0.0);
	};

	if(step.contains("set")) {
		if(!step.contains("to")) fail(where, "\"set\" needs \"to\"");
		state[target("set")] = value(step["to"], where + ".to");
		changed();
	} else if(step.contains("push")) {
		json& array = state[target("push")];
		if(array.is_null()) array = json::array();
		if(!array.is_array()) fail(where + ".push", "is not a list");
		array.push_back(value(step.value("value", json()), where + ".value"));
		changed();
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
		changed();
	} else if(step.contains("toggle")) {
		const json::json_pointer pointer = target("toggle");
		state[pointer] = !format::truthy(state.value(pointer, json()));
		changed();
	} else if(step.contains("increment")) {
		const json::json_pointer pointer = target("increment");
		const json by = value(step.value("by", json(1)), where + ".by");
		json current = state.value(pointer, json(0));
		if(current.is_null()) current = 0;
		if(!current.is_number() || !by.is_number()) fail(where, "\"increment\" needs numbers");
		state[pointer] = current.is_number_integer() && by.is_number_integer()
		                     ? json(current.get<std::int64_t>() + by.get<std::int64_t>())
		                     : json(current.get<double>() + by.get<double>());
		changed();
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
			const Scope scope{&locals, nullptr, {{name, items[i]}, {indexName, i}}};
			if(!runSteps(step.value("do", json::array()), sender, event, scope, where + ".do")) return false;
		}
	} else if(step.contains("while")) {
		std::size_t iterations = 0;
		while(format::truthy(value(step["while"], where + ".while"))) {
			if(++iterations > maxLoopIterations) fail(where, "\"while\" ran more than 10000 times");
			if(!runSteps(step.value("do", json::array()), sender, event, locals, where + ".do")) return false;
		}
	} else if(step.contains("run")) {
		// "with": the action's own variables, e.g. {"run": "moveCard", "with": {"to": "{{c + 1}}"}}
		Scope scope{&locals, nullptr, json::object()};
		if(step.contains("with"))
			for(const auto& [name, v] : object(step["with"], where + ".with").items())
				scope.vars[name] = value(v, where + ".with." + name);
		runNamed(text(value(step["run"], where + ".run")), sender, event, scope, where + ".run");
	} else if(step.contains("call")) {
		const std::string name = string(step["call"], where + ".call");
		if(const auto call = nativeCalls.find(name); call != nativeCalls.end()) {
			const json arguments = step.contains("with") ? value(step["with"], where + ".with") : json::object();
			try {
				json result = call->second(arguments);
				if(step.contains("into")) {
					state[target("into")] = std::move(result);
					changed();
				}
			} catch(const DocumentError&) {
				throw;
			} catch(const std::exception& error) {
				if(!step.contains("onError")) fail(where + ".call", name + ": " + error.what());
				const Scope scope{&locals, nullptr, {{"error", error.what()}}};
				return runSteps(step["onError"], sender, event, scope, where + ".onError");
			}
		} else if(const auto it = nativeActions.find(name); it != nativeActions.end()) {
			it->second(sender);
		} else {
			fail(where + ".call", "no native action '" + name + "' registered");
		}
	} else if(step.contains("emit")) {
		const json message = {{"type", "event"},
		                      {"name", string(step["emit"], where + ".emit")},
		                      {"data", value(step.value("data", json()), where + ".data")},
		                      {"revision", versions.head().number}};
		owner.onEvent(message);
	} else if(step.contains("send")) {
		owner.onSend(string(step["send"], where + ".send"), value(step.value("data", json::object()), where + ".data"));
	} else if(step.contains("connect")) {
		owner.onConnect(string(step["connect"], where + ".connect"));
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
	} else if(step.contains("focus") || step.contains("scrollTo")) {
		const bool focus = step.contains("focus");
		std::string id = text(value(step[focus ? "focus" : "scrollTo"], where));
		if(id.starts_with('#')) id.erase(0, 1);
		if(window) {
			// After the update and its layout, so the panel exists and has its place
			window->setTimer(0.0, [this, id, focus] {
				if(Panel* panel = window->root().findById(id)) {
					if(focus) window->setFocus(panel);
					else panel->scrollIntoView();
				}
			});
		}
	} else if(step.contains("navigate")) {
		const std::string name = text(value(step["navigate"], where + ".navigate"));
		if(name == "back") {
			if(screenHistory.empty()) return true;
			screen = screenHistory.back();
			screenHistory.pop_back();
		} else {
			const json& document = versions.document();
			if(!document.contains("screens") || !document["screens"].contains(name))
				fail(where + ".navigate", "unknown screen '" + name + "'");
			if(name != screen) screenHistory.push_back(screen);
			screen = name;
		}
		globalsChanged();
		changed();
	} else if(step.contains("after") || step.contains("debounce")) {
		// Later: "after" always, "debounce" only if it isn't run again in the meantime
		const bool debounce = step.contains("debounce");
		const double ms = milliseconds(debounce ? "debounce" : "after");
		if(!step.contains("do")) fail(where, "needs \"do\": the steps to run");
		if(window) {
			auto task = later(step["do"], sender, event, snapshot(locals), where + ".do");
			if(debounce) {
				const std::string key = where + "@" + std::to_string(reinterpret_cast<std::uintptr_t>(&sender));
				if(const auto it = debounces.find(key); it != debounces.end()) window->cancelTimer(it->second);
				debounces[key] = window->setTimer(ms, [this, key, task] {
					debounces.erase(key);
					task();
				});
			} else {
				window->setTimer(ms, task);
			}
		}
	} else if(step.contains("animate")) {
		// A number in the state moves to "to" over "ms", one frame at a time
		const json::json_pointer pointer = target("animate");
		const json to = value(step.value("to", json()), where + ".to");
		if(!to.is_number()) fail(where + ".to", "expected a number");
		const double ms = step.contains("ms") ? milliseconds("ms") : 300.0;
		const std::string easing = step.contains("easing") ? string(step["easing"], where + ".easing") : "out";
		const json current = state.value(pointer, json(0));
		const double from = current.is_number() ? current.get<double>() : 0.0;
		if(!window || ms <= 0.0) {
			state[pointer] = to;
			changed();
			return true;
		}
		const std::string key = pointer.to_string();
		if(const auto it = animations.find(key); it != animations.end()) window->cancelTimer(it->second);
		auto frame = std::make_shared<std::function<void()>>();
		*frame = [this, pointer, key, from, to, ms, easing, start = now(), frame] {
			const double progress = std::min((now() - start) * 1000.0 / ms, 1.0);
			versions.state()[pointer] =
			    progress >= 1.0 ? to : json(from + (to.get<double>() - from) * ease(easing, progress));
			changed();
			requestReconcile();
			if(progress < 1.0) animations[key] = window->setTimer(frameMs, *frame);
			else animations.erase(key);
		};
		animations[key] = window->setTimer(frameMs, *frame);
	} else if(step.contains("copy")) {
		if(window) window->setClipboardText(text(value(step["copy"], where + ".copy")));
	} else if(step.contains("log")) {
		std::cerr << "AffrescoUI screen log: " << text(value(step["log"], where + ".log")) << '\n';
	} else if(step.contains("stop")) {
		return false;
	} else if(step.contains("note")) {
		// A comment
	} else {
		fail(where, "unknown step (expected set, push, remove, toggle, increment, if, forEach, while, run, call, emit, "
		            "send, connect, patch, focus, scrollTo, navigate, after, debounce, animate, copy, log, stop, note, "
		            "accept, reject, preview, endPreview, restore, revert or undo)");
	}

	return true;
}

/* Runtime */

Runtime::Runtime(VersionControl& versions, std::filesystem::path baseDirectory)
    : impl_(std::make_unique<Impl>(*this, versions, std::move(baseDirectory))) {
	impl_->readDerived();
	versions.setObserver(impl_.get());
}

Runtime::~Runtime() {
	impl_->versions.setObserver(nullptr);
	if(impl_->window) // timers and deferred tasks point at the runtime: stop them
		for(auto& [panel, record] : impl_->records) {
			if(record.everyTimer) impl_->window->cancelTimer(record.everyTimer);
			if(record.colorTween.timer) impl_->window->cancelTimer(record.colorTween.timer);
			if(record.textTween.timer) impl_->window->cancelTimer(record.textTween.timer);
		}
}

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
	Impl* impl = impl_.get();
	impl->window = &window;
	impl->reconcile();

	// Bindings can use the window size
	window.onResize.connect([impl](const ResizeEvent&) { impl->requestReconcile(); });

	// Screen-wide shortcuts ("keys"); a focused text area keeps plain keys for typing
	window.onKey.connect([impl](const KeyEvent& event) {
		if(event.action == affresco::Action::Release) return;
		if(event.key == Key::Escape) impl->dismissOverlays(nullptr);
		const json& document = impl->versions.document();
		if(!document.contains("keys") || !document["keys"].is_object()) return;
		const std::string name = shortcut(event);
		const bool typing = dynamic_cast<TextArea*>(impl->window->focusedPanel()) != nullptr;
		if(name.empty() || (typing && !event.mods.super() && !event.mods.control() && !event.mods.alt() &&
		                    name != "escape"))
			return;
		for(const auto& [written, handler] : document["keys"].items()) {
			if(normalizeShortcut(written) != name) continue;
			const json steps = handler;
			impl->handle(steps, impl->window->root(), {{"type", "key"}, {"key", name}}, *noVariables(),
			             "keys." + written);
			return;
		}
	});

	// A press outside an open overlay that closes on it
	window.onMouseButton.connect([impl](const MouseButtonEvent& event) {
		if(event.action == affresco::Action::Press) impl->dismissOverlays(&event.position);
	});

	// "onLoad": once, after the first update
	const json& document = impl->versions.document();
	if(document.contains("onLoad"))
		window.defer(impl->later(document["onLoad"], window.root(), {{"type", "load"}}, noVariables(), "onLoad"));
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

void Runtime::onCall(std::string name, Call call) { impl_->nativeCalls.insert_or_assign(std::move(name), std::move(call)); }

void Runtime::onUnhandledAction(UnhandledAction fallback) { impl_->unhandled = std::move(fallback); }

void Runtime::runAction(std::string_view name, Panel& sender, const json& event) {
	const auto it = impl_->records.find(&sender);
	const SharedScope locals = it != impl_->records.end() ? it->second.locals : noVariables();
	impl_->handle(json(std::string(name)), sender, event, *locals, "runAction");
}

bool Runtime::hasAction(std::string_view name) const {
	const json& document = impl_->versions.document();
	return document.contains("actions") && document["actions"].contains(std::string(name));
}

Window* Runtime::window() const noexcept { return impl_->window; }

json Runtime::catalog() const {
	auto names = [](const auto& map) {
		json list = json::array();
		for(const auto& [name, value] : map) list.push_back(name);
		return list;
	};
	const json common = {"id",       "style",    "children", "x",      "y",       "width",     "height",  "minWidth",
	                     "minHeight", "anchors", "fill",     "grow",   "color",   "visible",   "cursor",  "tooltip",
	                     "disabled", "scroll",   "overlay",  "transition", "hover", "pressed", "focused",
	                     "dragging", "dragOver", "repeat",   "when",   "let",     "use",       "with",    "keys",
	                     "every",    "drag",     "locked",   "note"};
	const json text = {"text", "textColor", "font", "padding", "align", "wrap", "fitText"};
	json textarea = text;
	for(const char* key : {"placeholder", "placeholderColor", "bind", "check", "autofocus"}) textarea.push_back(key);

	json events = json::array();
	for(std::string_view name : handlerNames) events.push_back(std::string(name));

	return {{"types",
	         {{"panel", {{"properties", common}, {"about", "A rectangle; children are placed by their anchors"}}},
	          {"label", {{"properties", text}, {"about", "Text"}}},
	          {"textarea", {{"properties", textarea}, {"about", "A single-line text input"}}},
	          {"stack", {{"properties", {"gap", "direction", "from", "wrap"}}, {"about", "Children in a column or a row"}}},
	          {"row", {{"properties", {"gap", "from", "wrap"}}, {"about", "A stack in a row"}}},
	          {"column", {{"properties", {"gap", "from"}}, {"about", "A stack in a column"}}},
	          {"grid", {{"properties", {"columns", "gap"}}, {"about", "Children in equal columns"}}},
	          {"toggle", {{"properties", {"bind", "checked", "activeColor", "trackColor", "knobColor"}}, {"about", "A switch"}}},
	          {"checkbox",
	           {{"properties", {"bind", "checked", "activeColor", "trackColor", "knobColor"}}, {"about", "A checkbox"}}},
	          {"slider",
	           {{"properties", {"bind", "value", "min", "max", "step", "trackColor", "activeColor", "knobColor"}},
	            {"about", "A value in a range"}}},
	          {"outlet", {{"properties", json::array()}, {"about", "Shows the current screen from \"screens\""}}}}},
	        {"common", common},
	        {"events", events},
	        {"steps",
	         {"set",    "push",     "remove", "toggle",   "increment", "if",      "forEach", "while",  "run",
	          "call",   "emit",     "send",   "connect",  "patch",     "focus",   "scrollTo", "navigate", "after",
	          "debounce", "animate", "copy",  "log",      "stop",      "note"}},
	        {"controlSteps", {"accept", "reject", "preview", "endPreview", "restore", "revert", "undo", "patch", "call",
	                          "send", "connect"}},
	        {"operators", operatorNames()},
	        {"variables",
	         {"state", "event", "window", "revision", "proposals", "history", "preview", "mode", "computed", "theme",
	          "screen", "nodes"}},
	        {"sections",
	         {"window", "fonts", "colors", "theme", "styles", "components", "templates", "screens", "state", "computed",
	          "watch", "actions", "keys", "onLoad", "channels", "persist", "include", "permissions", "root"}},
	        {"customTypes", names(impl_->types)},
	        {"nativeActions", names(impl_->nativeActions)},
	        {"nativeCalls", names(impl_->nativeCalls)}};
}

Panel* Runtime::find(std::string_view id) noexcept {
	return impl_->window ? impl_->window->root().findById(id) : nullptr;
}

Color Runtime::color(std::string_view name) const {
	const auto it = impl_->colors.find(name);
	if(it == impl_->colors.end()) throw DocumentError("AffrescoUI screen: unknown color '" + std::string(name) + "'");
	const auto variant = it->second.themes.find(impl_->theme);
	return variant != it->second.themes.end() ? variant->second : it->second.base;
}

std::shared_ptr<const Font> Runtime::font(std::string_view name) const {
	const auto it = impl_->fonts.find(name);
	if(it == impl_->fonts.end()) throw DocumentError("AffrescoUI screen: unknown font '" + std::string(name) + "'");
	return it->second;
}

} // namespace affresco::format
