/// @file
/// @brief The state steps: set, push, remove, toggle, increment.

#include "Engine.hpp"
#include "Settings.hpp"

#include <cstdint>
#include <limits>

namespace affresco::format {

namespace {

/// @brief "remove": a state path, or an item of a list at "at".
/// @param s The step.
/// @param state The state.
/// @return The place changed.
json::json_pointer removeFrom(const Running& s, json& state) {
	const json::json_pointer pointer = s.target("remove");
	if(!s.step.contains("at")) {
		if(!findInState(state, pointer)) return pointer;
		json& holder = statePlace(state, pointer.parent_pointer(), s.where);
		if(!holder.is_object()) fail(s.where, R"(to remove an item of a list, name the list and the item's "at")");
		holder.erase(pointer.back());
		return pointer;
	}
	json& array = statePlace(state, pointer, s.where);
	const json at = s.value("at");
	if(!array.is_array() || !at.is_number_integer() || at.get<std::int64_t>() < 0 ||
	   static_cast<std::size_t>(at.get<std::int64_t>()) >= array.size())
		fail(s.where, "nothing to remove at " + at.dump());
	array.erase(static_cast<std::size_t>(at.get<std::int64_t>()));
	return pointer; // the items after it move: the whole list changed
}

/// @brief Tells whether two integers add up to one: integers stay integers, unless their sum doesn't fit (an
/// overflow would be undefined behaviour), when it is a double.
/// @param a An integer.
/// @param b Another.
/// @return true if a + b fits in 64 bits.
bool fitsSum(std::int64_t a, std::int64_t b) noexcept {
	return b >= 0 ? a <= std::numeric_limits<std::int64_t>::max() - b
	              : a >= std::numeric_limits<std::int64_t>::min() - b;
}

/// @brief "increment": adds "by" (1 by default) to a number in the state.
/// @param s The step.
/// @param state The state.
/// @return The place changed.
json::json_pointer increment(const Running& s, json& state) {
	const json::json_pointer pointer = s.target("increment");
	const json by = s.value(s.step.value("by", json(1)), s.where + ".by");
	const json* found = findInState(state, pointer);
	const json current = found && !found->is_null() ? *found : json(0);
	if(!current.is_number() || !by.is_number()) fail(s.where, "\"increment\" needs numbers");
	const bool integers = current.is_number_integer() && by.is_number_integer() &&
	                      fitsSum(current.get<std::int64_t>(), by.get<std::int64_t>());
	statePlace(state, pointer, s.where) = integers ? json(current.get<std::int64_t>() + by.get<std::int64_t>())
	                                               : json(current.get<double>() + by.get<double>());
	return pointer;
}

/// @brief Checks the settings a step changed (a "settings.x" write lands in state.settings), and puts them back if
/// one doesn't match its type.
/// @param engine The runtime.
/// @param before state.settings before the step.
/// @param where The step's place.
/// @throws DocumentError If a changed setting's new value doesn't match its type.
void checkSettingsWritten(Engine& engine, const json& before, const std::string& where) {
	json& state = engine.versions.state();
	const json after = state.contains("settings") ? state["settings"] : json::object();
	if(after == before) return;
	try {
		if(!after.is_object()) expect::fail(where, "state.settings holds the settings' values by key");
		for(const auto& [key, value] : after.items())
			if(!before.contains(key) || before[key] != value) checkSettingValue(engine.versions.document(), key, &value, where);
	} catch(const DocumentError&) {
		state["settings"] = before;
		throw;
	}
}

/// @brief Runs a state step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runStateStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;
	Engine& engine = s.steps.engine();
	json& state = engine.versions.state();
	const bool settings = engine.versions.document().contains("settings");
	const json before = settings && state.contains("settings") ? state["settings"] : json::object();

	json::json_pointer pointer; // the place changed
	if(step.contains("set")) {
		if(!step.contains("to")) fail(where, R"("set" needs "to")");
		pointer = s.target("set");
		json value = s.value("to");
		expect::shallowEnough(value, pointerDepth(pointer), where + ".to");
		engine.document.spendBytes(value, where + ".to");
		statePlace(state, pointer, where) = std::move(value);
	} else if(step.contains("push")) {
		pointer = s.target("push");
		json& array = statePlace(state, pointer, where);
		if(array.is_null()) array = json::array();
		if(!array.is_array()) fail(where + ".push", "is not a list");
		json value = s.value(step.value("value", json()), where + ".value");
		expect::shallowEnough(value, pointerDepth(pointer) + 1, where + ".value");
		engine.document.spendBytes(value, where + ".value");
		array.push_back(std::move(value));
	} else if(step.contains("remove")) {
		pointer = removeFrom(s, state);
	} else if(step.contains("toggle")) {
		pointer = s.target("toggle");
		const json* found = findInState(state, pointer);
		const bool on = found && format::truthy(*found);
		statePlace(state, pointer, where) = !on;
	} else if(step.contains("increment")) {
		pointer = increment(s, state);
	} else {
		return std::nullopt;
	}
	if(settings) checkSettingsWritten(engine, before, where);
	engine.document.markStateChanged(pointer);
	return true;
}

} // namespace

void registerStateSteps(StepRegistry& registry) {
	registry.add({"state",
	              {{"set", {"to"}}, {"push", {"value"}}, {"remove", {"at"}}, {"toggle", {}}, {"increment", {"by"}}},
	              runStateStep});
}

} // namespace affresco::format
