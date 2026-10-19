/// @file
/// @brief format::Properties: a node's evaluated values on its panel.

#include "Properties.hpp"
#include "Engine.hpp"
#include "PropertyValues.hpp"
#include "Shortcuts.hpp"

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <array>
#include <format>

namespace affresco::format {

namespace {

/// The properties a widget's value hook reads (ValueHooks::show).
constexpr std::array<std::string_view, 3> valueKeys = {"text", "checked", "value"};

/// @brief Tells whether a slot is a behaviour key that readBehaviour() reads.
/// @param slot The slot.
/// @param key Its key ("autofocus" and "locked" share Slot::Value with keys it doesn't read).
/// @return true if it reads it.
bool behaviourSlot(Slot slot, std::string_view key) noexcept {
	return slot == Slot::Handler || slot == Slot::Bind || slot == Slot::Caret || slot == Slot::Keys ||
	       slot == Slot::Drag || slot == Slot::Every || key == "autofocus" || key == "locked";
}

} // namespace

/* PropertyReader */

Color Properties::color(const json& value, const std::string& at) const { return engine_.document.color(value, at); }

std::shared_ptr<const Font> Properties::font(const json& value, const std::string& at) const {
	const std::string name = string(value, at);
	auto found = engine_.document.findFont(name);
	if(!found) fail(at, "unknown font '" + name + "'");
	return found;
}

/* Applying a node */

// Cost follows the change: a node whose compiled form, values and colours are those applied last is left as it is
void Properties::apply(Panel& panel, Record& record, CompiledRef node, std::vector<json> values,
                       const std::string& path, bool isRoot, bool created) {
	const std::uint64_t appearance = engine_.document.appearance();
	const bool unchanged = !created && record.appearance == appearance && record.node == node && !node->alwaysApply &&
	                       record.values == values;
	if(record.path != path) record.path = path;
	std::optional<Behaviour> behaviour;
	if(!unchanged) {
		record.node = std::move(node);
		record.values = std::move(values);
		record.appearance = 0; // until it is applied without an error
		behaviour = readBehaviour(panel, record);
		if(engine_.window) engine_.scheduler.every(panel, record, behaviour->everyMs); // repeats while it exists
		applyVisual(panel, record, visual(record), isRoot);
	}
	showValue(panel, record);
	record.appearance = appearance;

	if(created && behaviour && behaviour->autofocus && engine_.window && !engine_.handlersSuspended)
		engine_.window->setFocus(&panel);
	record.shown = true;
}

bool Properties::bindable(const Panel& panel, const Record& record) {
	return dynamic_cast<const TextArea*>(&panel) ||
	       std::ranges::any_of(record.sets, [](const auto& set) { return set && set->value.show; });
}

// Behaviour: handlers, bindings, shortcuts, drag, timers
Properties::Behaviour Properties::readBehaviour(Panel& panel, Record& record) const {
	record.handlers = json::object();
	record.keys = nullptr;
	record.drag = nullptr;
	record.bind.reset();
	record.caret.reset();
	Behaviour behaviour;
	const CompiledNode& node = *record.node;
	std::string at;
	for(const CompiledProperty& property : node.properties) {
		if(!behaviourSlot(property.slot, property.key)) continue;
		at.assign(record.path).append(".").append(property.key);
		const json& value = CompiledNode::valueOf(property, record.values);
		if(property.slot == Slot::Handler) readHandler(panel, record, property.key, value, at);
		else readBehaviourKey(panel, record, property.key, {property.slot, value, at, node.hasId}, behaviour);
	}
	return behaviour;
}

void Properties::readHandler(const Panel& panel, Record& record, const std::string& key, const json& value,
                             const std::string& at) {
	if(std::ranges::find(handlerNames, key) == std::end(handlerNames))
		fail(at, "unknown event (expected onClick, onDoubleClick, onContextMenu, onPress, onRelease, onDrag, onDrop, "
		         "onHover, onLeave, onFocus, onBlur, onScroll, onKey, onChange, onSubmit, onShow or onHide)");
	if(!value.is_string() && !value.is_array() && !value.is_object()) fail(at, "expected an action name or steps");
	if(key == "onSubmit" && !dynamic_cast<const TextArea*>(&panel)) fail(at, "only text areas have \"onSubmit\"");
	record.handlers[key] = value;
}

void Properties::readBehaviourKey(const Panel& panel, Record& record, std::string_view key, const KeyValue& property,
                                  Behaviour& behaviour) const {
	const auto& [slot, value, at, hasId] = property;
	if(slot == Slot::Bind) {
		if(!bindable(panel, record))
			fail(at, "only text areas, text editors, toggles, checkboxes and sliders can \"bind\"");
		record.bind = statePath(record, value, at);
	} else if(slot == Slot::Caret) {
		if(!dynamic_cast<const TextEditor*>(&panel)) fail(at, "only text editors have \"caret\"");
		record.caret = statePath(record, value, at);
	} else if(slot == Slot::Keys) {
		record.keys = json::object();
		for(const auto& [name, handler] : object(value, at).items()) record.keys[normalizeShortcut(name)] = handler;
	} else if(slot == Slot::Drag) {
		record.drag = object(value, at);
	} else if(slot == Slot::Every) {
		behaviour.everyMs = everyPeriod(record, value, at);
	} else if(key == "autofocus") {
		behaviour.autofocus = boolean(value, at);
	} else if(key == "locked" && boolean(value, at) && !hasId) {
		fail(at, "a locked node needs an \"id\"");
	}
}

// A path, or "{{...}}" giving one (e.g. a component's parameter)
std::string Properties::statePath(const Record& record, const json& value, const std::string& at) const {
	return string(isDynamic(value) ? engine_.document.evaluate(value, *record.locals, json::object(), at) : value, at);
}

double Properties::everyPeriod(const Record& record, const json& value, const std::string& at) const {
	const json& every = object(value, at);
	if(!every.contains("ms") || !every.contains("do")) fail(at, R"(expected {"ms": milliseconds, "do": steps})");
	return std::max(
	    number(engine_.document.evaluate(every["ms"], *record.locals, json::object(), at + ".ms"), at + ".ms"), 16.0f);
}

// A bound widget mirrors its state value; otherwise "text" is applied when it changes, so a text area's typing isn't
// overwritten by every update
void Properties::showValue(Panel& panel, Record& record) const {
	const CompiledNode& node = *record.node;
	const json bound = record.bind
	                       ? format::lookup(*record.bind, engine_.document.context(*record.locals, emptyObject()),
	                                        record.path + ".bind")
	                       : json();
	if(auto* label = dynamic_cast<Label*>(&panel); label && record.bind) {
		const std::string value = text(bound);
		if(label->text() != value) label->setText(value);
	} else if(label) {
		const CompiledProperty* written = node.find("text");
		const std::optional<std::string> newText =
		    written ? std::optional<std::string>(text(CompiledNode::valueOf(*written, record.values))) : std::nullopt;
		if(newText != record.lastText) label->setText(newText.value_or(""));
		record.lastText = newText;
	}
	if(std::ranges::none_of(record.sets, [](const auto& set) { return set && set->value.show; })) return;
	json shown = json::object(); // what the value hooks read
	for(const std::string_view key : valueKeys)
		if(const CompiledProperty* property = node.find(key))
			shown[std::string(key)] = CompiledNode::valueOf(*property, record.values);
	for(const auto& set : record.sets)
		if(set && set->value.show) set->value.show(panel, record.bind ? &bound : nullptr, shown, record.lastText);
}

// The node at rest, then each active state block on top (with its own styles)
Properties::Visual Properties::visual(const Record& record) const {
	Visual result;
	const CompiledNode& node = *record.node;
	result.entries.reserve(node.properties.size());
	for(const CompiledProperty& property : node.properties) {
		if(property.slot == Slot::StateBlock || behaviourSlot(property.slot, {})) continue;
		result.entries.push_back({property.key, property.slot, &CompiledNode::valueOf(property, record.values)});
	}
	if(!node.hasStateBlocks) return result;

	const std::array<bool, 5> active = {record.hovered, record.focused, record.dragging, record.dragOver, record.down};
	for(std::size_t i = 0; i < std::size(stateBlocks); ++i)
		if(active[i]) addStateBlock(record, stateBlocks[i], result.blocks[i], result);
	return result;
}

void Properties::addStateBlock(const Record& record, std::string_view name, json& block, Visual& result) const {
	const CompiledProperty* property = record.node->find(name);
	if(!property) return;
	const std::string at = concatenate(record.path, ".", name);
	block = engine_.builder.withStyles(object(CompiledNode::valueOf(*property, record.values), at), at);
	for(const auto& [key, value] : block.items()) {
		if(structural(key) || key == "type") continue;
		for(const auto& set : record.sets)
			if(const PropertySpec* spec = set ? set->find(key) : nullptr; spec && !spec->states)
				fail(std::format("{}.{}", at, key), "can't be set in a state");
		// A key already shown keeps its place, as a later value of an object's key does
		const auto shown = std::ranges::find(result.entries, std::string_view(key), &Visual::Entry::key);
		if(shown != result.entries.end()) shown->value = &value;
		else result.entries.push_back({key, slotOf(key), &value});
	}
}

void Properties::refresh(Panel& panel, Record& record) {
	if(!record.node || !record.node->hasStateBlocks) return;
	try {
		applyVisual(panel, record, visual(record), &panel == &engine_.window->root());
	} catch(const DocumentError& error) {
		engine_.report(error.what());
	}
}

// Back to the defaults, so properties removed from the document (or no longer bound) disappear. The properties node
// types registered are reset by their own apply functions.
void Properties::reset(Panel& panel, Record& record) {
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

	record.disabled = false;
	record.transition = json::object();
	record.overlayOpen.reset();
	record.dismissOutside = false;
	record.overlayAnchor.clear();
}

void Properties::applyVisual(Panel& panel, Record& record, const Visual& properties, bool isRoot) {
	Look look{.label = dynamic_cast<Label*>(&panel),
	          .textArea = dynamic_cast<TextArea*>(&panel),
	          .stack = dynamic_cast<Stack*>(&panel),
	          .grid = dynamic_cast<Grid*>(&panel)};
	const std::array<Color, 3> shown = {panel.color(), look.label ? look.label->textColor() : Color{},
	                                    panel.borderColor()};
	reset(panel, record);

	std::string at; // reused: one allocation for the node's places
	for(const auto& [key, slot, pointer] : properties.entries) {
		if(slot == Slot::Value) continue; // applied with the values (showValue()) or the behaviour
		const json& value = *pointer;
		at.assign(record.path).append(".").append(key);
		if(commonProperty(panel, record, slot, value, at, look) || shapeProperty(panel, slot, value, at, look))
			continue;
		if(isRoot) fail(at, "not allowed on the root, which always fills the window");
		if((slot == Slot::Layout && layoutProperty(panel, key, value, at)) || widgetProperty(look, slot, value, at) ||
		   registeredProperty(record, key, value, look))
			continue;
		fail(at, "unknown property '" + std::string(key) + "' for this type");
	}

	applySets(panel, record, look);
	transitionLook(panel, record, look, shown);
	finish(panel, look);
}

void Properties::finish(Panel& panel, const Look& look) {
	if(look.scroll && (panel.isScrollable() || look.scroll->first || look.scroll->second))
		panel.setScrollable(look.scroll->first, look.scroll->second);
	else if(!look.scroll && panel.isScrollable()) panel.setScrollable(false, false);
	if(look.overlayOpen && !*look.overlayOpen) panel.setVisible(false);
}

bool Properties::registeredProperty(const Record& record, std::string_view key, const json& value, Look& look) {
	for(std::size_t i = record.sets.size(); i-- > 0;) {
		const auto& set = record.sets[i];
		if(!set || !set->apply || !set->find(key)) continue;
		look.values[i][std::string(key)] = value;
		return true;
	}
	return false;
}

void Properties::applySets(Panel& panel, Record& record, const Look& look) const {
	for(std::size_t i = record.sets.size(); i-- > 0;) {
		const auto& set = record.sets[i];
		if(!set || !set->apply) continue;
		const json& values = look.values[i].is_null() ? emptyObject() : look.values[i];
		if(set->everyUpdate) {
			set->apply(panel, values, *this, record.path);
		} else if(values != record.ownProperties) { // an application's type: only when its values change
			set->apply(panel, values, *this, record.path);
			record.ownProperties = values;
		}
	}
}

bool Properties::commonProperty(Panel& panel, Record& record, Slot slot, const json& value, const std::string& at,
                                Look& look) const {
	switch(slot) {
	case Slot::Id:
		panel.setId(string(value, at));
		break;
	case Slot::Color:
		look.background = color(value, at);
		break;
	case Slot::Visible:
		panel.setVisible(boolean(value, at));
		break;
	case Slot::Cursor:
		panel.setCursor(cursor(value, at));
		break;
	case Slot::Tooltip:
		panel.setTooltip(text(value));
		break;
	case Slot::Disabled:
		record.disabled = boolean(value, at);
		break;
	case Slot::Transition:
		record.transition = transition(value, at);
		break;
	case Slot::Scroll:
		look.scroll = scroll(value, at);
		break;
	case Slot::Overlay:
		overlay(panel, record, value, at, look);
		break;
	default:
		return false;
	}
	return true;
}

const json& Properties::transition(const json& value, const std::string& at) const {
	for(const auto& [property, ms] : object(value, at).items()) {
		if(!engine_.types.transitionable(property))
			fail(std::format("{}.{}", at, property), R"(only "color", "textColor" and "borderColor" can transition)");
		number(ms, concatenate(at, ".", property));
	}
	return value;
}

std::pair<bool, bool> Properties::scroll(const json& value, const std::string& at) {
	const std::string mode = value.is_boolean() ? (value.get<bool>() ? "vertical" : "none") : string(value, at);
	if(mode != "vertical" && mode != "horizontal" && mode != "both" && mode != "none")
		fail(at, R"(expected "vertical", "horizontal", "both" or "none")");
	return {mode == "horizontal" || mode == "both", mode == "vertical" || mode == "both"};
}

void Properties::transitionLook(Panel& panel, Record& record, const Look& look, const std::array<Color, 3>& shown) {
	Scheduler& scheduler = engine_.scheduler;
	if(look.background) scheduler.transitionColor(panel, record, "color", shown[0], *look.background);
	if(look.label && look.foreground) scheduler.transitionColor(panel, record, "textColor", shown[1], *look.foreground);
	// Always, so a border colour that goes away (a state ends, a revision removes it) also ends a running transition
	scheduler.transitionColor(panel, record, "borderColor", shown[2], look.border.value_or(colors::black));
}

bool Properties::shapeProperty(Panel& panel, Slot slot, const json& value, const std::string& at, Look& look) const {
	switch(slot) {
	case Slot::Radius:
		panel.setCornerRadii(radii(value, at));
		break;
	case Slot::BorderWidth:
		panel.setBorder(pixels(value, at), panel.borderColor());
		break;
	case Slot::BorderColor:
		look.border = color(value, at);
		break; // set last, as it may transition
	case Slot::Shadow:
		panel.setShadow(engine_.document.shadow(value, at));
		break;
	default:
		return false;
	}
	return true;
}

void Properties::overlay(Panel& panel, Record& record, const json& value, const std::string& at, Look& look) const {
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
			look.overlayOpen = truthy(format::lookup(
			    open.get<std::string>(), engine_.document.context(*record.locals, json::object()), at + ".open"));
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

bool Properties::widgetProperty(Look& look, Slot slot, const json& value, const std::string& at) const {
	return (look.label && textProperty(look, slot, value, at)) ||
	       (look.stack && stackProperty(*look.stack, slot, value, at)) ||
	       (look.grid && gridProperty(*look.grid, slot, value, at));
}

// Labels and text areas
bool Properties::textProperty(Look& look, Slot slot, const json& value, const std::string& at) const {
	Label& label = *look.label;
	if(slot == Slot::TextColor) look.foreground = color(value, at);
	else if(slot == Slot::Padding) label.setPadding(number(value, at));
	else if(slot == Slot::Wrap) label.setWrap(boolean(value, at));
	else if(slot == Slot::Font) label.setFont(font(value, at));
	else if(slot == Slot::Align && value.is_string()) label.setAlignment(align(value, at), align(value, at));
	else if(slot == Slot::Align) {
		object(value, at);
		label.setAlignment(align(value.value("horizontal", json("start")), at + ".horizontal"),
		                   align(value.value("vertical", json("start")), at + ".vertical"));
	} else if(slot == Slot::FitText && value.is_boolean()) label.setFitToText(value.get<bool>());
	else if(slot == Slot::FitText)
		label.setFitToText(true, number(object(value, at).value("maxWidth", json()), at + ".maxWidth"));
	else if(look.textArea && slot == Slot::Placeholder) look.textArea->setPlaceholder(text(value));
	else if(look.textArea && slot == Slot::PlaceholderColor) look.textArea->setPlaceholderColor(color(value, at));
	else return false;
	return true;
}

// Rows and columns
bool Properties::stackProperty(Stack& stack, Slot slot, const json& value, const std::string& at) {
	if(slot == Slot::Gap) stack.setGap(number(value, at));
	else if(slot == Slot::From || slot == Slot::Align) stack.setAlignment(align(value, at)); // "align": v1 name
	else if(slot == Slot::Direction) {
		const std::string direction = string(value, at);
		if(direction != "row" && direction != "column") fail(at, "expected \"row\" or \"column\"");
		stack.setDirection(direction == "row" ? Direction::Row : Direction::Column);
	} else if(slot == Slot::Wrap) stack.setWrap(boolean(value, at));
	else return false;
	return true;
}

bool Properties::gridProperty(Grid& grid, Slot slot, const json& value, const std::string& at) {
	if(slot == Slot::Columns) grid.setColumns(integer(value, at));
	else if(slot == Slot::Gap) grid.setGap(number(value, at));
	else return false;
	return true;
}

} // namespace affresco::format
