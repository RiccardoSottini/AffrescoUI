#pragma once

/// @file
/// @brief The runtime's private parts, shared by its source files: Runtime.cpp (the public API, colours, fonts,
/// styles), Build.cpp (building panels and keeping them in line with the document), Properties.cpp (a node's
/// properties on its panel), Events.cpp (input) and Steps.cpp (actions and their steps).

#include "affresco/core/Log.hpp"
#include "affresco/format/Runtime.hpp"
#include "affresco/render/Controls.hpp"
#include "affresco/render/Label.hpp"
#include "affresco/render/Stack.hpp"
#include "affresco/render/TextArea.hpp"
#include "affresco/render/TextEditor.hpp"

#include "Expression.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace affresco::format {

using expect::boolean;
using expect::fail;
using expect::integer;
using expect::number;
using expect::object;
using expect::string;

/* Helpers used by more than one of the runtime's files */

/// @brief Reads a text property; numbers and booleans from bindings are shown as text.
/// @param value The value.
/// @return The text.
inline std::string text(const json& value) { return toText(value); }

/// @brief Tells whether a node key is an event handler: "on" and a capital letter ("onClick", "onDrop").
/// @param key The key.
/// @return true for handlers.
inline bool handlerKey(const std::string& key) {
	return key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]));
}

/// Event handlers a node may have.
inline constexpr std::string_view handlerNames[] = {
    "onClick", "onDoubleClick", "onContextMenu", "onPress", "onRelease", "onDrag", "onDrop",  "onHover", "onLeave",
    "onFocus", "onBlur",        "onScroll",      "onKey",   "onChange",  "onSubmit", "onShow", "onHide"};

/// Handlers that bubble: when a node has none, its nearest ancestor with one handles the event.
inline constexpr std::string_view bubbling[] = {"onClick", "onDoubleClick", "onContextMenu", "onPress",
                                         "onRelease", "onDrag",      "onScroll",      "onKey"};

/// Visual states, in order: later ones win when several apply.
inline constexpr std::string_view stateBlocks[] = {"hover", "focused", "dragging", "dragOver", "pressed"};

/// @brief Tells whether a node key describes structure or behaviour, so it is never evaluated or applied as a property.
/// @param key The key.
/// @return true for children, repeat, when, let, handlers, ...
inline bool structural(const std::string& key) {
	return key == "children" || key == "repeat" || key == "when" || key == "let" || key == "check" || key == "use" ||
	       key == "with" || key == "bind" || key == "caret" || key == "keys" || key == "every" || key == "drag" || key == "note" ||
	       key == "slot" || handlerKey(key);
}

/// @brief A key's name for shortcuts: "a", "7", "enter", "escape", "up", "f5", ...
/// @param key The key.
/// @return Its name, or "" for keys without one.
inline std::string keyName(Key key) {
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
inline std::string shortcut(const KeyEvent& event) {
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
inline std::string normalizeShortcut(std::string_view text) {
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

/// @brief Eases an animation's progress.
/// @param easing "linear", "in", "out" or "inOut".
/// @param t Progress, 0 to 1.
/// @return The eased progress.
inline double ease(const std::string& easing, double t) {
	if(easing == "in") return t * t * t;
	if(easing == "out") return 1.0 - std::pow(1.0 - t, 3.0);
	if(easing == "inOut") return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
	return t;
}

/// @brief The scope with no variables.
/// @return It (shared, never changed).
inline const SharedScope& noVariables() {
	static const SharedScope none = std::make_shared<Scope>();
	return none;
}

/// @brief A copy of a scope chain as one shared scope, for work that runs later (timers).
/// @param scope The scope.
/// @return The copy.
inline SharedScope snapshot(const Scope& scope) {
	std::vector<const Scope*> chain;
	for(const Scope* s = &scope; s; s = s->parent) chain.push_back(s);
	json vars = json::object();
	for(auto it = chain.rbegin(); it != chain.rend(); ++it) vars.update((*it)->vars); // inner scopes win
	return extend(nullptr, std::move(vars));
}

/* What the runtime keeps */

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
	std::optional<std::string> caret;     ///< State path a text editor writes its caret to.
	std::optional<json> ownProperties;    ///< A custom type's own properties, as last applied.
	std::optional<std::string> lastText;  ///< "text" applied last time.
	bool hovered = false, down = false, focused = false, dragOver = false, dragging = false; ///< Visual states.
	bool disabled = false;                ///< No events but hover.
	bool shown = false;                   ///< Applied at least once (transitions start only after that).
	json transition = json::object();     ///< "transition": property -> milliseconds.
	Tween colorTween, textTween, borderTween; ///< Running transitions.

	/// @brief The transitions, so their timers can be cancelled together.
	/// @return Each one.
	std::array<Tween*, 3> tweens() noexcept { return {&colorTween, &textTween, &borderTween}; }
	double everyMs = 0.0;                 ///< "every" period, if any.
	std::uint64_t everyTimer = 0;         ///< Its timer.
	std::optional<std::string> overlayOpen; ///< An overlay's "open" state path.
	bool dismissOutside = false;          ///< The overlay closes on a click outside it (and on Escape).
	std::string overlayAnchor;            ///< The overlay's anchor id (empty: the parent).

	/// @brief The connections hookUp() made to the panel's signals, which ~Runtime takes back (0: none).
	struct Hooks {
		Connection hover = 0, focus = 0, button = 0, move = 0, scroll = 0, key = 0; ///< Every panel's.
		Connection value = 0; ///< A text area's, text editor's, toggle's or slider's value.
		Connection caret = 0; ///< A text editor's caret.
	} hooks;                  ///< See Hooks.
};

/// @brief A node to build: shared, so a repeat's items don't each copy their template (and its children).
using NodeRef = std::shared_ptr<const json>;

/// @brief Refers to a node of the document without copying it (the document outlives an update).
/// @param node The node.
/// @return The reference.
inline NodeRef borrow(const json& node) { return NodeRef(NodeRef(), &node); }

/// @brief A child to build.
struct Item {
	NodeRef node;     ///< Styles merged; shared by a repeat's items (or pointing into the document: no copy).
	SharedScope locals; ///< Its variables.
	std::string path; ///< Its place in the document.
	json key;         ///< A keyed repeat's key (null: none).
};

inline constexpr int maxActionDepth = 32;               ///< Nested action calls before giving up (recursion guard).
inline constexpr std::size_t maxLoopIterations = 10000; ///< "while" iterations before giving up.
inline constexpr int maxComponentDepth = 32;            ///< Components inside components before giving up.
inline constexpr double frameMs = 16.0;                 ///< Animation frame period.
inline constexpr double doubleClickSeconds = 0.4;       ///< Two clicks closer than this are a double click.
inline constexpr float dragThreshold = 4.0f;            ///< Pixels the pointer moves before a press becomes a drag.

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
	std::map<std::string, std::shared_ptr<const Font>> fontCache; ///< By file, size and fallbacks: kept across revisions.
	std::string theme;                                            ///< "theme", evaluated at each update.

	/// @brief A type the application registered.
	struct CustomType {
		Factory factory;                     ///< Creates its panel.
		std::vector<std::string> properties; ///< Its own property names.
		ApplyProperties apply;               ///< Applies them (may be empty).
	};
	std::map<std::string, CustomType, std::less<>> types;     ///< Custom node types.
	std::map<std::string, Action, std::less<>> nativeActions; ///< Native actions.
	std::map<std::string, Call, std::less<>> nativeCalls;     ///< Native functions for "call" with results.
	UnhandledAction unhandled = [](const std::string& action, Panel& sender) {
		log(LogLevel::Warning,
		    "AffrescoUI screen: no handler for action '" + action + "' (from '" + sender.id() + "')");
	};
	bool placeholderTypes = false; ///< Unknown types become plain panels.

	Window* window = nullptr;                         ///< Set by build().
	std::unordered_map<const Panel*, Record> records; ///< Every panel built from the document.

	bool stateChanged = false;      ///< An action changed the state.
	std::uint64_t stateVersion = 0; ///< Counts state changes (computed values are cached per version).
	bool reconcileQueued = false;   ///< A reconcile is deferred to the end of the event.
	int actionDepth = 0;            ///< Nested action calls in progress.
	bool handlersSuspended = false; ///< See Runtime::setHandlersSuspended().

	std::string screen;                      ///< The screen an "outlet" shows (from "screens").
	std::vector<std::string> screenHistory;  ///< For {"navigate": "back"}.
	json watched = json::object();           ///< "watch": the values seen last.
	std::map<std::string, std::uint64_t> debounces;  ///< "debounce" steps waiting, by place.
	std::map<std::string, std::uint64_t> animations; ///< "animate" steps running, by state pointer.
	std::set<std::uint64_t> onceTimers;              ///< "after", "focus" and "scrollTo" timers waiting.
	/// Cleared by ~Runtime: the tasks deferred on the window then do nothing.
	std::shared_ptr<bool> alive = std::make_shared<bool>(true);

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

	/// @brief Seconds on the window's clock, which its timers read too (a steady clock before build()).
	/// @return Now.
	[[nodiscard]] double now() const noexcept { return window ? window->clock().seconds() : SteadyClock().seconds(); }

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
	/// @brief Reads a "shadow": { "x", "y", "blur", "color" }, each optional.
	/// @param value The value.
	/// @param path Its place, for errors.
	/// @return The shadow.
	/// @throws DocumentError If it is malformed.
	Shadow shadow(const json& value, const std::string& path) const;
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
	/// @param record Its record, which keeps the connections.
	void hookUp(Panel& panel, Record& record);
	/// @brief Disconnects what hookUp() connected, so the panel no longer calls into the runtime.
	/// @param panel The panel.
	/// @param record Its record.
	static void unhook(Panel& panel, const Record& record);
	/// @brief Disconnects everything that can call into the runtime: the window's signals and those of every panel
	/// built from the document that is still in the window (~Runtime).
	void disconnectAll();
	/// @brief The connections build() made to the window's signals (0: none).
	struct {
		Connection resized = 0, key = 0, button = 0; ///< onResize, onKey, onMouseButton.
	} windowHooks;
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
	/// @brief One applyVisual() pass: the panel as each widget type it may be, and the values set at the end (they
	/// depend on each other or animate).
	struct Look {
		Label* label = nullptr;                           ///< The panel, if it is a label (or a text area).
		TextArea* textArea = nullptr;                     ///< The panel, if it is a text area.
		TextEditor* editor = nullptr;                     ///< The panel, if it is a text editor.
		Stack* stack = nullptr;                           ///< The panel, if it is a row or a column.
		Grid* grid = nullptr;                             ///< The panel, if it is a grid.
		Toggle* toggle = nullptr;                         ///< The panel, if it is a toggle or a checkbox.
		Slider* slider = nullptr;                         ///< The panel, if it is a slider.
		std::optional<Color> background = {};             ///< "color".
		std::optional<Color> foreground = {};             ///< "textColor".
		std::optional<Color> border = {};                 ///< "borderColor".
		std::optional<std::pair<bool, bool>> scroll = {}; ///< "scroll": horizontal, vertical.
		std::optional<bool> overlayOpen = {};             ///< An overlay's "open", evaluated.
		std::optional<Color> trackColor = {}, fillColor = {}, knobColor = {}; ///< A slider's or toggle's colours.
		double sliderMin = 0.0, sliderMax = 100.0, sliderStep = 0.0;          ///< A slider's range.
		bool sliderRange = false;                                             ///< Some of the range was given.
	};
	/// @brief Applies a property every panel has (id, color, visible, cursor, tooltip, disabled, transition, scroll,
	/// overlay).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @param look Collects values set at the end.
	/// @return false if it isn't one of these.
	bool commonProperty(Panel& panel, Record& record, const std::string& key, const json& value, const std::string& at,
	                    Look& look) const;
	/// @brief Applies a property of every panel's shape: radius, borderWidth, borderColor or shadow.
	/// @param panel The panel.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @param look Collects values set at the end (the border colour, which may transition).
	/// @return false if it isn't one of these.
	/// @throws DocumentError If the value is malformed.
	bool shapeProperty(Panel& panel, const std::string& key, const json& value, const std::string& at,
	                   Look& look) const;
	/// @brief Reads "overlay": where the panel floats, and whether it is open.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param value The "overlay" object.
	/// @param at Its place, for errors.
	/// @param look Gets whether it is open.
	void overlay(Panel& panel, Record& record, const json& value, const std::string& at, Look& look) const;
	/// @brief Applies a property of labels, text areas, rows, columns, grids, toggles or sliders.
	/// @param look The panel as each type, and the values set at the end.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if the panel's type has no such property.
	bool widgetProperty(Look& look, const std::string& key, const json& value, const std::string& at) const;
	/// @brief Shows a panel's state blocks again after hover, press, focus or drag changed.
	/// @param panel The panel.
	/// @param record Its record.
	void refresh(Panel& panel, Record& record);
	/// @brief Sets a colour, animating it if the record has a transition for it.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param property "color", "textColor" or "borderColor": a string literal, since running transitions keep it.
	/// @param shown The colour shown before this update.
	/// @param target The new colour.
	void transitionColor(Panel& panel, Record& record, const char* property, Color shown, Color target);
	/// @brief Sets the colours an update gave a panel, animating those the record has a transition for.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param look The colours the update set.
	/// @param shownColor The background shown before the update.
	/// @param shownText The text colour shown before the update.
	/// @param shownBorder The border colour shown before the update.
	void transitionLook(Panel& panel, Record& record, const Look& look, Color shownColor, Color shownText,
	                    Color shownBorder);
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

	/// @brief A step being run: the step, where it comes from, and how to read its values.
	struct Running {
		Impl& impl;               ///< The runtime.
		const json& step;         ///< The step.
		Panel& sender;            ///< The panel it comes from.
		const json& event;        ///< The event being handled.
		const Scope& locals;      ///< Variables.
		const std::string& where; ///< Its place, for errors.

		/// @brief Evaluates a value of the step.
		/// @param value The value.
		/// @param at Its place, for errors.
		/// @return The evaluated value.
		json value(const json& value, const std::string& at) const;
		/// @brief Evaluates the step's value under a key.
		/// @param key The key, e.g. "to".
		/// @return The evaluated value.
		json value(const char* key) const { return value(step[key], where + "." + key); }
		/// @brief The state path the step changes, e.g. "set": "state.count" is /count.
		/// @param key The key holding the path.
		/// @return The pointer into the state.
		json::json_pointer target(const char* key) const;
		/// @brief A list given as a path ("state.items") or as any value.
		/// @param source The path or value.
		/// @param at Its place, for errors.
		/// @return The list (not checked to be one).
		json list(const json& source, const std::string& at) const;
		/// @brief A duration under a key, never negative.
		/// @param key The key.
		/// @return Milliseconds.
		double milliseconds(const char* key) const;
		/// @brief Runs nested steps with the same sender and event.
		/// @param steps The steps.
		/// @param scope Their variables.
		/// @param at Their place.
		/// @return false if a "stop" step ended them.
		bool run(const json& steps, const Scope& scope, const std::string& at) const {
			return impl.runSteps(steps, sender, event, scope, at);
		}
	};
	/* Step groups, tried in order by runStep(): each returns std::nullopt for a step that isn't one of its kinds,
	   else whether to keep going (false after "stop"). */
	/// @brief set, push, remove, toggle, increment: changes to the state.
	/// @param s The step.
	/// @return std::nullopt if it isn't one of these, else whether to keep going.
	std::optional<bool> stateStep(const Running& s);
	/// @brief if, forEach, while, run: control flow.
	/// @param s The step.
	/// @return std::nullopt if it isn't one of these, else whether to keep going.
	std::optional<bool> flowStep(const Running& s);
	/// @brief Refuses a control step (call, send, connect, patch, review and history) while a proposal is previewed,
	/// unless the user's input runs it.
	/// @param where The step's place, for the error.
	/// @throws DocumentError If it may not run now.
	void checkControl(const std::string& where) const;
	/// @brief call, emit, send, connect, patch: the app, peers and the document.
	/// @param s The step.
	/// @return std::nullopt if it isn't one of these, else whether to keep going.
	std::optional<bool> outsideStep(const Running& s);
	/// @brief call: a native function or action.
	/// @param s The step.
	/// @return Whether to keep going.
	bool callStep(const Running& s);
	/// @brief accept, reject, preview, endPreview, restore, revert, undo: review and history.
	/// @param s The step.
	/// @return std::nullopt if it isn't one of these, else whether to keep going.
	std::optional<bool> reviewStep(const Running& s);
	/// @brief Evaluates a review or history step's values, and checks their types.
	/// @param s The step.
	/// @return The step with its values evaluated.
	/// @throws DocumentError If a value has the wrong type, or an accept names no head.
	[[nodiscard]] static json evaluateControl(const Running& s);
	/// @brief focus, scrollTo, navigate: where the user is.
	/// @param s The step.
	/// @return std::nullopt if it isn't one of these, else whether to keep going.
	std::optional<bool> navigationStep(const Running& s);
	/// @brief after, debounce, animate: steps and values over time.
	/// @param s The step.
	/// @return std::nullopt if it isn't one of these, else whether to keep going.
	std::optional<bool> timeStep(const Running& s);
	/// @brief copy, log, stop, note.
	/// @param s The step.
	/// @return std::nullopt if it isn't one of these, else whether to keep going.
	std::optional<bool> otherStep(const Running& s);
	/// @brief Runs steps later, from a timer, with a sender that may be gone by then (the root stands in).
	/// @param steps The steps.
	/// @param sender The panel they come from.
	/// @param event The event.
	/// @param locals Variables.
	/// @param where Its place, for errors.
	/// @return The task.
	std::function<void()> later(json steps, Panel& sender, json event, SharedScope locals, std::string where);
	/// @brief Sets a window timer that runs once, kept in onceTimers until it runs (~Runtime cancels it).
	/// @param delayMs Milliseconds from now.
	/// @param task The work.
	void setOnceTimer(double delayMs, std::function<void()> task);
	/// @brief Runs a task after the current event (Window::defer), unless the runtime is destroyed before then: the
	/// window can't cancel what it has deferred.
	/// @param task The work.
	void defer(std::function<void()> task) const;
	/// @brief Logs an error (stderr unless the application set a log sink) and fires onError.
	/// @param message The error.
	void report(const std::string& message);
};

} // namespace affresco::format
