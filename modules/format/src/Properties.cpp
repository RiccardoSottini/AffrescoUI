#include "RuntimeImpl.hpp"

namespace affresco::format {

namespace {

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

/// @brief Anchors an edge of a panel: a length from the parent's edge, or another panel's edge ("#header.bottom + 8").
/// @param panel The panel.
/// @param name The edge, e.g. "top".
/// @param offset Its value.
/// @param where Its place, for errors.
/// @throws DocumentError If the value is malformed.
void anchor(Panel& panel, const std::string& name, const json& offset, const std::string& where) {
	if(!offset.is_string() || !offset.get<std::string>().starts_with('#')) {
		panel.setAnchor(edge(name, where), length(offset, where));
		return;
	}

	const std::string spec = offset.get<std::string>();
	const std::size_t dot = spec.find('.');
	if(dot == std::string::npos) fail(where, "expected \"#id.edge\", e.g. \"#header.bottom + 8\"");
	std::size_t end = dot + 1;
	while(end < spec.size() && std::isalpha(static_cast<unsigned char>(spec[end]))) ++end;

	std::string rest = spec.substr(end); // "+ 8", "-8" or nothing
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
	panel.setAnchorTo(edge(name, where), spec.substr(1, dot - 1), edge(spec.substr(dot + 1, end - dot - 1), where),
	                  delta);
}

/// @brief Reads a finite number: JSON allows numbers too large for a float, which would become infinite.
/// @param value The value.
/// @param path Its place, for errors.
/// @return The number.
/// @throws DocumentError If it isn't a number, or is too large.
float finiteNumber(const json& value, const std::string& path) {
	const float amount = number(value, path);
	if(!std::isfinite(amount)) fail(path, "number too large");
	return amount;
}

/// @brief Reads a number of pixels that can't be negative: a radius, a border width or a blur.
/// @param value The value.
/// @param path Its place, for errors.
/// @return The number.
/// @throws DocumentError If it isn't a finite number of 0 or more.
float pixels(const json& value, const std::string& path) {
	const float amount = finiteNumber(value, path);
	if(amount < 0.0f) fail(path, "expected a number of pixels, 0 or more");
	return amount;
}

/// @brief Reads "radius": one number for every corner, or { "topLeft", "topRight", "bottomRight", "bottomLeft" }
/// (corners left out are square).
/// @param value The value.
/// @param path Its place, for errors.
/// @return The radii.
/// @throws DocumentError If it is malformed.
CornerRadii radii(const json& value, const std::string& path) {
	if(!value.is_object()) return CornerRadii::all(pixels(value, path));

	CornerRadii result;
	for(const auto& [corner, radius] : value.items()) {
		const std::string at = path + "." + corner;
		if(corner == "topLeft") result.topLeft = pixels(radius, at);
		else if(corner == "topRight") result.topRight = pixels(radius, at);
		else if(corner == "bottomRight") result.bottomRight = pixels(radius, at);
		else if(corner == "bottomLeft") result.bottomLeft = pixels(radius, at);
		else fail(at, "unknown corner (expected topLeft, topRight, bottomRight or bottomLeft)");
	}
	return result;
}

/// @brief The colours a "transition" can animate.
constexpr std::string_view transitionable[] = {"color", "textColor", "borderColor"};

/// @brief Sets one of the colours a transition can animate.
/// @param panel The panel; a label for "textColor".
/// @param property "color", "textColor" or "borderColor".
/// @param color The colour.
void setColorOf(Panel& panel, std::string_view property, Color color) {
	if(property == "textColor") static_cast<Label&>(panel).setTextColor(color);
	else if(property == "borderColor") panel.setBorder(panel.borderWidth(), color);
	else panel.setColor(color);
}

/// @brief The transition of one of a node's colours.
/// @param record The node's record.
/// @param property "color", "textColor" or "borderColor".
/// @return Its tween.
Tween& tweenOf(Record& record, std::string_view property) {
	if(property == "textColor") return record.textTween;
	if(property == "borderColor") return record.borderTween;
	return record.colorTween;
}

/// @brief Applies a layout property: x, y, width, height, minWidth, minHeight, grow, fill or anchors.
/// @param panel The panel.
/// @param key The property.
/// @param value Its value.
/// @param at Its place, for errors.
/// @return false if it isn't one of these.
/// @throws DocumentError If the value is malformed.
bool layoutProperty(Panel& panel, const std::string& key, const json& value, const std::string& at) {
	if(key == "x") panel.setX(length(value, at));
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
		for(const auto& [name, offset] : object(value, at).items()) anchor(panel, name, offset, at + "." + name);
	} else {
		return false;
	}
	return true;
}

} // namespace

/* Properties */

// Back to the defaults, so properties removed from the document (or no longer bound) disappear
void Runtime::Impl::reset(Panel& panel, Record& record) {
	panel.setId({});
	panel.setPosition(px(0.0f), px(0.0f));
	panel.setSize(px(0.0f), px(0.0f));
	panel.setMinSize(px(0.0f), px(0.0f));
	for(Edge e : {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right}) panel.clearAnchor(e);
	panel.setVisible(true);
	panel.setColor(record.defaultColor);
	panel.setCornerRadii({});
	panel.setBorder(0.0f, colors::black);
	panel.setShadow({});
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
		textArea->setPlaceholderColor(colors::muted);
	}
	if(auto* editor = dynamic_cast<TextEditor*>(&panel)) {
		editor->setFont(nullptr);
		editor->setLineNumbers(false);
		editor->resetColors();
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
		toggle->setOnColor(colors::accent);
		toggle->setOffColor(colors::idle);
		toggle->setKnobColor(colors::white);
	}
	if(auto* slider = dynamic_cast<Slider*>(&panel)) {
		slider->setRange(0.0, 100.0, 0.0);
		slider->setColors(colors::track, colors::accent, colors::white);
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
	record.caret.reset();
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
			if(!dynamic_cast<TextArea*>(&panel) && !dynamic_cast<TextEditor*>(&panel) && !dynamic_cast<Toggle*>(&panel) &&
			   !dynamic_cast<Slider*>(&panel))
				fail(at, "only text areas, text editors, toggles, checkboxes and sliders can \"bind\"");
			// A path, or "{{...}}" giving one (e.g. a component's parameter)
			record.bind = string(isDynamic(value) ? evaluate(value, *record.locals, json::object(), at) : value, at);
		} else if(key == "caret") {
			if(!dynamic_cast<TextEditor*>(&panel)) fail(at, "only text editors have \"caret\"");
			record.caret = string(isDynamic(value) ? evaluate(value, *record.locals, json::object(), at) : value, at);
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
			// The timer owns the task, which holds itself weakly: a strong reference to itself would never be freed
			auto tick = std::make_shared<std::function<void()>>();
			*tick = [this, panel = &panel, weak = std::weak_ptr(tick)] {
				Record* r = recordOf(panel);
				if(!r || !r->properties.contains("every")) return;
				r->everyTimer = window->setTimer(r->everyMs, [self = weak.lock()] { (*self)(); });
				const SharedScope locals = r->locals;
				handle(r->properties["every"]["do"], *panel, {{"type", "tick"}}, *locals, r->path + ".every.do");
			};
			record.everyTimer = window->setTimer(everyMs, [tick] { (*tick)(); });
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
	if(auto* editor = dynamic_cast<TextEditor*>(&panel)) {
		if(record.bind) editor->setText(text(bound));
		else {
			const std::optional<std::string> newText =
			    properties.contains("text") ? std::optional<std::string>(text(properties["text"])) : std::nullopt;
			if(newText != record.lastText) editor->setText(newText.value_or(""));
			record.lastText = newText;
		}
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
	Look look{.label = dynamic_cast<Label*>(&panel),
	          .textArea = dynamic_cast<TextArea*>(&panel),
	          .editor = dynamic_cast<TextEditor*>(&panel),
	          .stack = dynamic_cast<Stack*>(&panel),
	          .grid = dynamic_cast<Grid*>(&panel),
	          .toggle = dynamic_cast<Toggle*>(&panel),
	          .slider = dynamic_cast<Slider*>(&panel)};
	const Color shownColor = panel.color();
	const Color shownText = look.label ? look.label->textColor() : Color{};
	const Color shownBorder = panel.borderColor();
	reset(panel, record);
	const auto customIt = types.find(record.type);
	const CustomType* custom = customIt != types.end() ? &customIt->second : nullptr;
	json own = json::object();

	for(const auto& [key, value] : properties.items()) {
		const std::string at = record.path + "." + key;
		if(key == "type" || key == "autofocus" || key == "locked" || key == "checked" || key == "value" || key == "text")
			continue; // handled in apply()
		if(commonProperty(panel, record, key, value, at, look) || shapeProperty(panel, key, value, at, look)) continue;
		if(isRoot) fail(at, "not allowed on the root, which always fills the window");
		if(layoutProperty(panel, key, value, at) || widgetProperty(look, key, value, at)) continue;
		if(custom && std::ranges::find(custom->properties, key) != custom->properties.end()) {
			own[key] = value;
			continue;
		}
		fail(at, "unknown property '" + key + "' for this type");
	}

	// A custom type's own properties go to the application when they change
	if(custom && custom->apply && own != record.ownProperties) {
		try {
			custom->apply(panel, own);
		} catch(const std::exception& error) {
			fail(record.path, error.what());
		}
		record.ownProperties = own;
	}

	if(look.background) transitionColor(panel, record, "color", shownColor, *look.background);
	if(look.label && look.foreground) transitionColor(panel, record, "textColor", shownText, *look.foreground);
	// Always, so a border colour that goes away (a state ends, a revision removes it) also ends a running transition
	transitionColor(panel, record, "borderColor", shownBorder, look.border.value_or(colors::black));
	if(look.scroll && (panel.isScrollable() || look.scroll->first || look.scroll->second))
		panel.setScrollable(look.scroll->first, look.scroll->second);
	else if(!look.scroll && panel.isScrollable()) panel.setScrollable(false, false);
	if(look.overlayOpen && !*look.overlayOpen) panel.setVisible(false);
	if(look.toggle && look.knobColor) look.toggle->setKnobColor(*look.knobColor);
	if(look.slider) {
		if(look.sliderRange) look.slider->setRange(look.sliderMin, look.sliderMax, look.sliderStep);
		look.slider->setColors(look.trackColor.value_or(colors::track), look.fillColor.value_or(colors::accent),
		                       look.knobColor.value_or(colors::white));
	}
}

bool Runtime::Impl::commonProperty(Panel& panel, Record& record, const std::string& key, const json& value,
                                   const std::string& at, Look& look) const {
	if(key == "id") panel.setId(string(value, at));
	else if(key == "color") look.background = color(value, at);
	else if(key == "visible") panel.setVisible(boolean(value, at));
	else if(key == "cursor") panel.setCursor(cursor(value, at));
	else if(key == "tooltip") panel.setTooltip(text(value));
	else if(key == "disabled") record.disabled = boolean(value, at);
	else if(key == "transition") {
		for(const auto& [property, ms] : object(value, at).items()) {
			if(std::ranges::find(transitionable, property) == std::end(transitionable))
				fail(at + "." + property, "only \"color\", \"textColor\" and \"borderColor\" can transition");
			number(ms, at + "." + property);
		}
		record.transition = value;
	} else if(key == "scroll") {
		const std::string mode = value.is_boolean() ? (value.get<bool>() ? "vertical" : "none") : string(value, at);
		if(mode != "vertical" && mode != "horizontal" && mode != "both" && mode != "none")
			fail(at, "expected \"vertical\", \"horizontal\", \"both\" or \"none\"");
		look.scroll = {mode == "horizontal" || mode == "both", mode == "vertical" || mode == "both"};
	} else if(key == "overlay") {
		overlay(panel, record, value, at, look);
	} else {
		return false;
	}
	return true;
}

bool Runtime::Impl::shapeProperty(Panel& panel, const std::string& key, const json& value, const std::string& at,
                                  Look& look) const {
	if(key == "radius") panel.setCornerRadii(radii(value, at));
	else if(key == "borderWidth") panel.setBorder(pixels(value, at), panel.borderColor());
	else if(key == "borderColor") look.border = color(value, at); // set last, as it may transition
	else if(key == "shadow") panel.setShadow(shadow(value, at));
	else return false;
	return true;
}

// Like CSS box-shadow: "x" and "y" move it and "blur" softens it
Shadow Runtime::Impl::shadow(const json& value, const std::string& path) const {
	Shadow result{.color = colors::black.withAlpha(0.25f)};
	for(const auto& [key, item] : object(value, path).items()) {
		const std::string at = path + "." + key;
		if(key == "x") result.offset.x = finiteNumber(item, at);
		else if(key == "y") result.offset.y = finiteNumber(item, at);
		else if(key == "blur") result.blur = pixels(item, at);
		else if(key == "color") result.color = color(item, at);
		else fail(at, "unknown property (expected x, y, blur or color)");
	}
	return result;
}

void Runtime::Impl::overlay(Panel& panel, Record& record, const json& value, const std::string& at, Look& look) const {
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
			look.overlayOpen =
			    truthy(format::lookup(open.get<std::string>(), context(*record.locals, json::object()), at + ".open"));
		} else {
			look.overlayOpen = truthy(open);
		}
	}
	if(overlay.contains("dismiss")) {
		if(string(overlay["dismiss"], at + ".dismiss") != "outside") fail(at + ".dismiss", "expected \"outside\"");
		if(!record.overlayOpen) fail(at + ".dismiss", "needs \"open\" to be a state path, e.g. \"state.menuOpen\"");
		record.dismissOutside = true;
	}
}

bool Runtime::Impl::widgetProperty(Look& look, const std::string& key, const json& value, const std::string& at) const {
	auto* const label = look.label;
	auto* const stack = look.stack;
	auto* const grid = look.grid;
	auto* const toggle = look.toggle;
	auto* const slider = look.slider;

	// Labels and text areas
	if(label && key == "textColor") look.foreground = color(value, at);
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
	} else if(look.textArea && key == "placeholder") look.textArea->setPlaceholder(text(value));
	else if(look.textArea && key == "placeholderColor") look.textArea->setPlaceholderColor(color(value, at));


	// Text editors
	else if(look.editor && key == "textColor") look.editor->setTextColor(color(value, at));
	else if(look.editor && key == "font") {
		const auto it = fonts.find(string(value, at));
		if(it == fonts.end()) fail(at, "unknown font '" + value.get<std::string>() + "'");
		look.editor->setFont(it->second);
	} else if(look.editor && key == "lineNumbers") look.editor->setLineNumbers(boolean(value, at));
	else if(look.editor && key == "lineColor") look.editor->setLineColor(color(value, at));
	else if(look.editor && key == "selectionColor") look.editor->setSelectionColor(color(value, at));
	else if(look.editor && key == "caretColor") look.editor->setCaretColor(color(value, at));
	else if(look.editor && key == "lineNumberColor") look.editor->setLineNumberColor(color(value, at));
	else if(look.editor && key == "activeLineNumberColor") look.editor->setActiveLineNumberColor(color(value, at));

	// Rows, columns and grids
	else if(stack && key == "gap") stack->setGap(number(value, at));
	else if(stack && (key == "from" || key == "align")) stack->setAlignment(align(value, at)); // "align": v1 name
	else if(stack && key == "direction") {
		const std::string direction = string(value, at);
		if(direction != "row" && direction != "column") fail(at, "expected \"row\" or \"column\"");
		stack->setDirection(direction == "row" ? Direction::Row : Direction::Column);
	} else if(stack && key == "wrap") stack->setWrap(boolean(value, at));
	else if(grid && key == "columns") grid->setColumns(integer(value, at));
	else if(grid && key == "gap") grid->setGap(number(value, at));

	// Toggles and sliders
	else if(toggle && key == "activeColor") toggle->setOnColor(color(value, at));
	else if(toggle && key == "trackColor") toggle->setOffColor(color(value, at));
	else if((toggle || slider) && key == "knobColor") look.knobColor = color(value, at);
	else if(slider && (key == "min" || key == "max" || key == "step")) {
		look.sliderRange = true;
		(key == "min" ? look.sliderMin : key == "max" ? look.sliderMax : look.sliderStep) = number(value, at);
	} else if(slider && key == "trackColor") look.trackColor = color(value, at);
	else if(slider && key == "activeColor") look.fillColor = color(value, at);
	else return false;
	return true;
}

// With a transition, the colour moves from what was shown to the target over that many milliseconds
void Runtime::Impl::transitionColor(Panel& panel, Record& record, const char* property, Color shown, Color target) {
	Tween& tween = tweenOf(record, property);
	const double ms = record.transition.contains(property) ? record.transition[property].get<double>() : 0.0;

	if(!window || ms <= 0.0 || shown == target || !record.shown) {
		if(tween.timer && window) window->cancelTimer(std::exchange(tween.timer, 0));
		setColorOf(panel, property, target);
		return;
	}
	if(tween.timer && tween.to == target) { // already on its way there
		setColorOf(panel, property, shown);
		return;
	}
	if(tween.timer) window->cancelTimer(tween.timer);
	tween = {shown, target, now(), ms, 0};
	setColorOf(panel, property, shown);

	auto step = std::make_shared<std::function<void()>>(); // owned by the timer, holds itself weakly (see "every")
	*step = [this, panel = &panel, name = std::string_view(property), weak = std::weak_ptr(step)] {
		Record* r = recordOf(panel);
		if(!r) return;
		Tween& t = tweenOf(*r, name);
		const double progress = std::min((now() - t.start) * 1000.0 / t.ms, 1.0);
		setColorOf(*panel, name, Color::mix(t.from, t.to, static_cast<float>(ease("out", progress))));
		t.timer = progress < 1.0 ? window->setTimer(frameMs, [self = weak.lock()] { (*self)(); }) : 0;
	};
	tween.timer = window->setTimer(frameMs, [step] { (*step)(); });
}

} // namespace affresco::format
