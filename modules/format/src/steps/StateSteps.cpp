/// @file
/// @brief The state steps: set, push, remove, toggle, increment.

#include "Engine.hpp"
#include "Settings.hpp"

#include <cstdint>
#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>

namespace affresco::format {

namespace {

/// @brief "remove": a state path, or an item of a list at "at".
/// @param s The step.
/// @param state The state.
/// @param removedAt Gets the index of the item removed from a list, if one was.
/// @return The place changed.
json::json_pointer removeFrom(const Running& s, json& state, std::optional<std::size_t>& removedAt) {
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
	removedAt = static_cast<std::size_t>(at.get<std::int64_t>());
	array.erase(*removedAt);
	return pointer; // the items after it move: the whole list changed
}

/// @brief The place a step that writes a place changes: the place, or, if it doesn't exist yet, the deepest place
/// above it that does (a write past a list's end grows the list, filling it with nulls; a write under a missing
/// member adds the member), but never the whole state.
/// @param state The state, before the write.
/// @param pointer The place written.
/// @return The place to mark changed.
json::json_pointer createdPlace(const json& state, json::json_pointer pointer) {
	while(pointer.parent_pointer() != json::json_pointer() && !findInState(state, pointer))
		pointer = pointer.parent_pointer();
	return pointer;
}

/// The values a comparison of two values looks at before it counts them as different: an equal write of a large
/// value isn't worth comparing.
constexpr std::size_t comparedValues = 4096;

/// @brief Whether two values are the same, of the same types (1 and 1.0 differ: they are shown differently; a signed
/// and an unsigned integer don't).
/// @param a A value.
/// @param b Another.
/// @param budget The values it may still look at; spent as it looks.
/// @return true if they are the same; false if they differ, or the budget ran out first.
bool sameValue(const json& a, const json& b, std::size_t& budget) {
	if(budget == 0 || (a.type() != b.type() && !(a.is_number_integer() && b.is_number_integer()))) return false;
	--budget;
	if(a.is_object()) {
		if(a.size() != b.size()) return false;
		for(auto it = a.begin(); it != a.end(); ++it) {
			const auto other = b.find(it.key());
			if(other == b.end() || !sameValue(*it, *other, budget)) return false;
		}
		return true;
	}
	if(a.is_array()) {
		if(a.size() != b.size()) return false;
		for(std::size_t i = 0; i < a.size(); ++i)
			if(!sameValue(a[i], b[i], budget)) return false;
		return true;
	}
	return a == b;
}

/// @brief Writes a value at a place of the state, unless it holds the same value already.
/// @param s The step, for errors.
/// @param state The state.
/// @param pointer The place.
/// @param value The value.
/// @return true if it wrote; false if the place held the value already, and nothing changed.
bool writeChanged(const Running& s, json& state, const json::json_pointer& pointer, json value) {
	std::size_t budget = comparedValues;
	if(const json* found = findInState(state, pointer); found && sameValue(*found, value, budget)) return false;
	statePlace(state, pointer, s.where) = std::move(value);
	return true;
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
/// @param pointer The place.
/// @return true if it wrote; false if "by" is 0 and the number was there.
bool increment(const Running& s, json& state, const json::json_pointer& pointer) {
	const json by = s.value(s.step.value("by", json(1)), s.where + ".by");
	const json* found = findInState(state, pointer);
	const json current = found && !found->is_null() ? *found : json(0);
	if(!current.is_number() || !by.is_number()) fail(s.where, "\"increment\" needs numbers");
	const bool integers = current.is_number_integer() && by.is_number_integer() &&
	                      fitsSum(current.get<std::int64_t>(), by.get<std::int64_t>());
	return writeChanged(s, state, pointer,
	                    integers ? json(current.get<std::int64_t>() + by.get<std::int64_t>())
	                             : json(current.get<double>() + by.get<double>()));
}

/// @brief The default of the setting a state place holds, so a toggle of a setting still at its default flips that.
/// @param document The document; its "settings" are read.
/// @param pointer The place, /settings/<key> for a setting.
/// @return The default, or nullptr if the place isn't a setting's.
const json* settingDefault(const json& document, const json::json_pointer& pointer) {
	if(pointer.empty() || pointer.parent_pointer() != json::json_pointer("/settings")) return nullptr;
	const auto settings = document.find("settings");
	if(settings == document.end() || !settings->is_object()) return nullptr;
	const auto type = settings->find(pointer.back());
	if(type == settings->end() || !type->is_object()) return nullptr;
	const auto value = type->find("default");
	return value != type->end() ? &*value : nullptr;
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
			if(!before.contains(key) || before[key] != value)
				checkSettingValue(engine.versions.document(), key, &value, where);
	} catch(const DocumentError&) {
		state["settings"] = before;
		throw;
	}
}

/// @brief What a state step wrote.
struct Written {
	json::json_pointer pointer{};         ///< The place written.
	json::json_pointer changed{};         ///< What readers see changed: it, or what a write past it created.
	std::optional<ListChange::Kind> list; ///< How, if it is a list that changed shape.
	std::size_t index = 0;                ///< Where in the list.
	bool unchanged = false;               ///< It wrote the value the place held: nothing changed.
};

/// @brief "set": writes "to" at a state path.
/// @param s The step.
/// @param state The state.
/// @return What it wrote.
Written setStep(const Running& s, json& state) {
	if(!s.step.contains("to")) fail(s.where, R"("set" needs "to")");
	Written written;
	written.pointer = s.target("set");
	written.changed = createdPlace(state, written.pointer);
	json value = s.value("to");
	expect::shallowEnough(value, pointerDepth(written.pointer), s.where + ".to");
	s.steps.engine().document.spendBytes(value, s.where + ".to");
	written.unchanged = !writeChanged(s, state, written.pointer, std::move(value));
	return written;
}

/// @brief "push": adds "value" at the end of a list in the state, made if there is none.
/// @param s The step.
/// @param state The state.
/// @return What it wrote.
Written pushStep(const Running& s, json& state) {
	Written written;
	written.pointer = s.target("push");
	written.changed = createdPlace(state, written.pointer);
	json& array = statePlace(state, written.pointer, s.where);
	if(array.is_null()) array = json::array();
	if(!array.is_array()) fail(s.where + ".push", "is not a list");
	json value = s.value(s.step.value("value", json()), s.where + ".value");
	expect::shallowEnough(value, pointerDepth(written.pointer) + 1, s.where + ".value");
	s.steps.engine().document.spendBytes(value, s.where + ".value");
	array.push_back(std::move(value));
	if(written.changed == written.pointer) written.list = ListChange::Kind::Inserted; // else a list made: changed whole
	written.index = array.size() - 1;
	return written;
}

/// @brief "toggle": flips a truth value in the state (a setting still at its default flips that).
/// @param s The step.
/// @param state The state.
/// @return What it wrote.
Written toggleStep(const Running& s, json& state) {
	Written written;
	written.pointer = s.target("toggle");
	written.changed = createdPlace(state, written.pointer);
	const json* found = findInState(state, written.pointer);
	if(!found) found = settingDefault(s.steps.engine().versions.document(), written.pointer);
	const bool on = found && format::truthy(*found);
	written.unchanged = !writeChanged(s, state, written.pointer, !on);
	return written;
}

/// @brief The key a state step names its place with.
/// @param step The step.
/// @return "set", "push", "remove", "toggle" or "increment", or nullptr if it isn't a state step.
const char* stateKey(const json& step) {
	for(const char* key : {"set", "push", "remove", "toggle", "increment"})
		if(step.contains(key)) return key;
	return nullptr;
}

/// @brief Runs a state step's write.
/// @param s The step.
/// @param key Its key (stateKey()).
/// @param state The state.
/// @return What it wrote.
Written writeStep(const Running& s, const std::string_view key, json& state) {
	if(key == "set") return setStep(s, state);
	if(key == "push") return pushStep(s, state);
	if(key == "toggle") return toggleStep(s, state);
	Written written;
	if(key == "remove") {
		std::optional<std::size_t> removedAt;
		written.pointer = removeFrom(s, state, removedAt);
		if(removedAt) {
			written.list = ListChange::Kind::Removed;
			written.index = *removedAt;
		}
		return written;
	}
	written.pointer = s.target("increment");
	written.changed = createdPlace(state, written.pointer);
	written.unchanged = !increment(s, state, written.pointer);
	return written;
}

/// @brief Runs a state step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runStateStep(const Running& s) {
	const char* key = stateKey(s.step);
	if(!key) return std::nullopt;
	Engine& engine = s.steps.engine();
	json& state = engine.versions.state();
	const bool settings = engine.versions.document().contains("settings");
	const json before = settings && state.contains("settings") ? state["settings"] : json::object();

	// A write that fails may have made the places on its way first (a list grown with nulls, a member added)
	const json::json_pointer made = createdPlace(state, s.target(key));
	Written written;
	try {
		written = writeStep(s, key, state);
	} catch(const DocumentError&) {
		engine.document.markStateChanged(made);
		throw;
	}
	if(written.unchanged) return true; // an equal write: nothing to check, mark or update
	if(settings) checkSettingsWritten(engine, before, s.where);
	if(written.list) engine.document.markListChanged(written.pointer, *written.list, written.index);
	else engine.document.markStateChanged(written.changed.empty() ? written.pointer : written.changed);
	return true;
}

} // namespace

void registerStateSteps(StepRegistry& registry) {
	registry.add({"state",
	              {{"set", {"to"}}, {"push", {"value"}}, {"remove", {"at"}}, {"toggle", {}}, {"increment", {"by"}}},
	              runStateStep});
}

} // namespace affresco::format
