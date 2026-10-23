/// @file
/// @brief The properties every panel has (format::Properties::commonProperty(), shapeProperty(), overlay(),
/// snapshot()), and those of the core widget types: labels, text areas, rows, columns and grids (widgetProperty()).

#include "Engine.hpp"
#include "LayoutProperties.hpp"
#include "Properties.hpp"
#include "PropertyValues.hpp"
#include "NodeKeys.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <memory>
#include <utility>

namespace affresco::format {

bool Properties::commonProperty(Panel& panel, Record& record, const KeyValue& property, Look& look) const {
	return panelOnlyProperty(panel, property, look) || recordProperty(panel, record, property, look);
}

bool Properties::panelOnlyProperty(Panel& panel, const KeyValue& property, Look& look) const {
	const auto& [slot, value, at] = property;
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
	case Slot::Role:
		panel.setAccessibleRole(text(value));
		break;
	case Slot::Label:
		panel.setAccessibleLabel(text(value));
		break;
	case Slot::Description:
		panel.setAccessibleDescription(text(value));
		break;
	case Slot::Scroll:
		look.scroll = scroll(value, at);
		break;
	default:
		return false;
	}
	return true;
}

bool Properties::recordProperty(Panel& panel, Record& record, const KeyValue& property, Look& look) const {
	const auto& [slot, value, at] = property;
	switch(slot) {
	case Slot::Disabled:
		record.disabled = boolean(value, at);
		panel.setAccessibleState(withDisabled(panel.accessibleState(), record.disabled));
		break;
	case Slot::Transition:
		transition(value, at);
		record.transition = value;
		break;
	case Slot::Overlay:
		overlay(panel, record, value, at, look);
		break;
	case Slot::Snapshot:
		snapshot(value, at, look);
		break;
	case Slot::Focusable:
		record.focusable = boolean(value, at);
		break;
	case Slot::TabIndex:
		// Clamped before the cast: a number past int's range (1e30) would be undefined behaviour
		record.tabIndex = static_cast<int>(std::clamp(number(value, at), -1.0f, 1.0e9f));
		break;
	case Slot::Arrows:
		record.arrows = string(value, at);
		if(record.arrows != "vertical" && record.arrows != "horizontal" && record.arrows != "both")
			fail(at, R"(expected "vertical", "horizontal" or "both")");
		break;
	case Slot::Mnemonic: {
		const std::string letter = text(value);
		record.mnemonic =
		    letter.empty() ? '\0' : static_cast<char>(std::tolower(static_cast<unsigned char>(letter.front())));
		break;
	}
	default:
		return false;
	}
	return true;
}

void Properties::transition(const json& value, const std::string& at) const {
	for(const auto& [property, ms] : object(value, at).items()) {
		if(!engine_.types.transitionable(property))
			fail(std::format("{}.{}", at, property), R"(only "color", "textColor" and "borderColor" can transition)");
		number(ms, concatenate(at, ".", property));
	}
}

std::pair<bool, bool> Properties::scroll(const json& value, const std::string& at) {
	const std::string mode = value.is_boolean() ? (value.get<bool>() ? "vertical" : "none") : string(value, at);
	if(mode != "vertical" && mode != "horizontal" && mode != "both" && mode != "none")
		fail(at, R"(expected "vertical", "horizontal", "both" or "none")");
	return {mode == "horizontal" || mode == "both", mode == "vertical" || mode == "both"};
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
	case Slot::Cache:
		panel.setCached(boolean(value, at));
		break;
	default:
		return false;
	}
	return true;
}

void Properties::overlay(Panel& panel, Record& record, const json& value, const std::string& at, Look& look) const {
	static constexpr std::array<std::string_view, 5> keys = {"anchor", "placement", "open", "dismiss", "modal"};
	const json& overlay = object(value, at);
	for(const auto& [k, v] : overlay.items())
		if(std::ranges::find(keys, k) == keys.end())
			fail(concatenate(at, ".", k), "unknown property (expected anchor, placement, open, dismiss or modal)");
	std::string anchor = overlay.contains("anchor") ? string(overlay["anchor"], at + ".anchor") : "";
	if(anchor.starts_with('#')) anchor.erase(0, 1);
	record.overlayAnchor = anchor;
	panel.setOverlay(anchor, overlay.contains("placement") ? placement(overlay["placement"], at + ".placement")
	                                                       : Placement::Below);
	if(overlay.contains("open")) overlayOpen(record, overlay["open"], at + ".open", look);
	if(overlay.contains("dismiss")) {
		if(string(overlay["dismiss"], at + ".dismiss") != "outside") fail(at + ".dismiss", "expected \"outside\"");
		if(!record.overlayOpen) fail(at + ".dismiss", R"(needs "open" to be a state path, e.g. "state.menuOpen")");
		record.dismissOutside = true;
	}
	record.modal = overlay.contains("modal") && boolean(overlay["modal"], at + ".modal");
	showModal(panel, record, look);
}

void Properties::overlayOpen(Record& record, const json& open, const std::string& at, Look& look) const {
	if(!open.is_string()) {
		look.overlayOpen = truthy(open);
		return;
	}
	record.overlayOpen = open.get<std::string>(); // a state path, so a click outside can close it
	look.overlayOpen =
	    truthy(format::lookup(*record.overlayOpen, engine_.document.context(*record.locals, json::object()), at));
}

void Properties::snapshot(const json& value, const std::string& at, Look& look) {
	if(value.is_string()) {
		look.snapshot = value.get<std::string>(); // empty: drawn in place, so a binding can turn it off
		return;
	}
	static constexpr std::array<std::string_view, 2> keys = {"image", "scale"};
	const json& snapshot = object(value, at);
	for(const auto& [k, v] : snapshot.items())
		if(std::ranges::find(keys, k) == keys.end())
			fail(concatenate(at, ".", k), "unknown property (expected image or scale)");
	if(!snapshot.contains("image")) fail(at, R"(needs "image", the name image nodes show it by)");
	look.snapshot = string(snapshot["image"], at + ".image");
	if(!snapshot.contains("scale")) return;
	look.snapshotScale = number(snapshot["scale"], at + ".scale");
	if(look.snapshotScale <= 0.0f || look.snapshotScale > 4.0f)
		fail(at + ".scale", "expected a number above 0, at most 4");
}

// The window draws a snapshot into its image before each frame in which it changed; image nodes show it by name
void Properties::showSnapshot(Panel& panel, Record& record, const Look& look, bool isRoot) const {
	if(look.snapshot.empty()) {
		if(record.snapshotImage.empty()) return;
		panel.clearSnapshot();
		images().setNamed(std::exchange(record.snapshotImage, {}), nullptr);
		return;
	}
	const std::string at = record.path + ".snapshot";
	if(isRoot) fail(at, "not allowed on the root, which always fills the window");
	if(panel.isOverlay() && !panel.isSnapshot()) fail(at, R"(not allowed with "overlay": give it to a node inside)");
	if(look.snapshot == record.snapshotImage && look.snapshotScale == record.snapshotScale) return;
	if(look.snapshot != record.snapshotImage && !record.snapshotImage.empty())
		images().setNamed(record.snapshotImage, nullptr);
	record.snapshotImage = look.snapshot;
	record.snapshotScale = look.snapshotScale;
	// The window keeps the panel, and so the handler, after the runtime is gone
	panel.setSnapshot(look.snapshotScale, [&engine = engine_, alive = std::weak_ptr<bool>(engine_.alive),
	                                       name = look.snapshot](const std::shared_ptr<const Bitmap>& image) {
		if(!alive.expired()) engine.document.images().setNamed(name, image);
	});
}

// A modal overlay traps the focus while it is open, and gives it back when it closes
void Properties::showModal(Panel& panel, Record& record, const Look& look) const {
	if((record.modal && look.overlayOpen.value_or(true)) == record.modalShown) return;
	record.modalShown = !record.modalShown;
	engine_.events.modal(panel, record.modalShown);
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
	else if(slot == Slot::Direction) {
		const std::string direction = string(value, at);
		if(direction != "row" && direction != "column") fail(at, R"(expected "row" or "column")");
		stack.setDirection(direction == "row" ? Direction::Row : Direction::Column);
	} else if(slot == Slot::Wrap) stack.setWrap(boolean(value, at));
	else return false;
	return true;
}

bool Properties::gridProperty(Grid& grid, Slot slot, const json& value, const std::string& at) {
	if(slot == Slot::Columns) grid.setColumns(gridTracks(value, at));
	else if(slot == Slot::Gap) grid.setGap(number(value, at));
	else return false;
	return true;
}

} // namespace affresco::format
