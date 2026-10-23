/// @file
/// @brief The states every node may give assistive technologies besides its role and name: "isOpen" (expanded),
/// "selected" and "live" (Panel::setAccessibleState()). "disabled" is applied with the node's behaviour
/// (Properties.cpp).

#include "Properties.hpp"
#include "PropertyValues.hpp"

namespace affresco::format {

namespace {

/// @brief Reads a live region's politeness.
/// @param value "off", "polite" or "assertive".
/// @param path Its place, for errors.
/// @return It.
LiveRegion liveRegion(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "off") return LiveRegion::Off;
	if(name == "polite") return LiveRegion::Polite;
	if(name == "assertive") return LiveRegion::Assertive;
	fail(path, R"(expected "off", "polite" or "assertive")");
}

} // namespace

bool accessibleStateProperty(Panel& panel, Slot slot, const json& value, const std::string& at) {
	AccessibleState state = panel.accessibleState();
	switch(slot) {
	case Slot::Open:
		state.expanded = boolean(value, at);
		break;
	case Slot::Selected:
		if(value.is_null()) return true; // can't be selected: a menu's item that isn't a check
		if(!value.is_boolean()) return false; // a graph's or a timeline's own "selected": ids, not a flag
		state.selected = boolean(value, at);
		break;
	case Slot::Live:
		state.live = liveRegion(value, at);
		break;
	default:
		return false;
	}
	panel.setAccessibleState(state);
	return true;
}

} // namespace affresco::format
