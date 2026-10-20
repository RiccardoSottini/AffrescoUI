/// @file
/// @brief The state steps: set, push, remove, toggle, increment.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief "remove": a state path, or an item of a list at "at".
/// @param s The step.
/// @param state The state.
void removeFrom(const Running& s, json& state) {
	const json::json_pointer pointer = s.target("remove");
	if(!s.step.contains("at")) {
		if(!findInState(state, pointer)) return;
		json& holder = statePlace(state, pointer.parent_pointer(), s.where);
		if(!holder.is_object()) fail(s.where, R"(to remove an item of a list, name the list and the item's "at")");
		holder.erase(pointer.back());
		return;
	}
	json& array = statePlace(state, pointer, s.where);
	const json at = s.value("at");
	if(!array.is_array() || !at.is_number_integer() || at.get<std::int64_t>() < 0 ||
	   static_cast<std::size_t>(at.get<std::int64_t>()) >= array.size())
		fail(s.where, "nothing to remove at " + at.dump());
	array.erase(static_cast<std::size_t>(at.get<std::int64_t>()));
}

/// @brief "increment": adds "by" (1 by default) to a number in the state.
/// @param s The step.
/// @param state The state.
void increment(const Running& s, json& state) {
	const json::json_pointer pointer = s.target("increment");
	const json by = s.value(s.step.value("by", json(1)), s.where + ".by");
	const json* found = findInState(state, pointer);
	const json current = found && !found->is_null() ? *found : json(0);
	if(!current.is_number() || !by.is_number()) fail(s.where, "\"increment\" needs numbers");
	statePlace(state, pointer, s.where) = current.is_number_integer() && by.is_number_integer()
	                                          ? json(current.get<std::int64_t>() + by.get<std::int64_t>())
	                                          : json(current.get<double>() + by.get<double>());
}

/// @brief Runs a state step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runStateStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;
	Engine& engine = s.steps.engine();
	json& state = engine.versions.state();

	if(step.contains("set")) {
		if(!step.contains("to")) fail(where, R"("set" needs "to")");
		const json::json_pointer pointer = s.target("set");
		json value = s.value("to");
		expect::shallowEnough(value, pointerDepth(pointer), where + ".to");
		statePlace(state, pointer, where) = std::move(value);
	} else if(step.contains("push")) {
		const json::json_pointer pointer = s.target("push");
		json& array = statePlace(state, pointer, where);
		if(array.is_null()) array = json::array();
		if(!array.is_array()) fail(where + ".push", "is not a list");
		json value = s.value(step.value("value", json()), where + ".value");
		expect::shallowEnough(value, pointerDepth(pointer) + 1, where + ".value");
		array.push_back(std::move(value));
	} else if(step.contains("remove")) {
		removeFrom(s, state);
	} else if(step.contains("toggle")) {
		const json::json_pointer pointer = s.target("toggle");
		const json* found = findInState(state, pointer);
		const bool on = found && format::truthy(*found);
		statePlace(state, pointer, where) = !on;
	} else if(step.contains("increment")) {
		increment(s, state);
	} else {
		return std::nullopt;
	}
	engine.document.markStateChanged();
	return true;
}

} // namespace

void registerStateSteps(StepRegistry& registry) {
	registry.add({"state",
	              {{"set", {"to"}}, {"push", {"value"}}, {"remove", {"at"}}, {"toggle", {}}, {"increment", {"by"}}},
	              runStateStep});
}

} // namespace affresco::format
