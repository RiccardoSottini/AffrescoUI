/// @file
/// @brief format::TypedText: the runs of text the person typed themselves.

#include "TypedText.hpp"

#include <affresco/lisp/Document.hpp>

#include <algorithm>
#include <utility>

namespace affresco::format {

namespace {

constexpr std::size_t maxValues = 16; ///< The most values remembered: the oldest goes first.
/// The longest value remembered: as much as `evaluate` takes, so that nothing is lost, and little to compare per key.
constexpr std::size_t maxTextBytes = std::size_t{4} * lisp::maxEntryBytes;

/// @brief What differs between two texts, as one replacement.
struct Change {
	std::size_t at = 0;     ///< Where it starts in both.
	std::size_t oldEnd = 0; ///< Where it ends in the old one.
	std::size_t newEnd = 0; ///< Where it ends in the new one.
};

/// @brief The smallest replacement that makes one text the other.
/// @param before The old one.
/// @param after The new one.
/// @return It.
Change changeBetween(std::string_view before, std::string_view after) {
	const std::size_t shorter = std::min(before.size(), after.size());
	std::size_t at = 0;
	while(at < shorter && before[at] == after[at]) ++at;
	std::size_t tail = 0;
	while(tail < shorter - at && before[before.size() - 1 - tail] == after[after.size() - 1 - tail]) ++tail;
	return {at, before.size() - tail, after.size() - tail};
}

/// @brief A state value by its place: anything may have changed the state since.
/// @param state The state.
/// @param place Its JSON Pointer.
/// @return It, or nullptr if nothing is there now.
const json* valueAt(const json& state, const std::string& place) {
	try {
		const json::json_pointer pointer(place);
		return state.contains(pointer) ? &state[pointer] : nullptr;
	} catch(const json::exception&) { // a key of digits that is no index, if what holds it is a list now
		return nullptr;
	}
}

} // namespace

void TypedText::follow(Held& held, std::string_view now) {
	if(held.text == now) return;
	const Change change = changeBetween(held.text, now);
	std::vector<Run> kept;
	for(const Run& run : held.runs) {
		if(run.start < change.at) kept.push_back({run.start, std::min(run.end, change.at)});
		if(run.end > change.oldEnd)
			kept.push_back({std::max(run.start, change.oldEnd) - change.oldEnd + change.newEnd,
			                run.end - change.oldEnd + change.newEnd});
	}
	held.runs = std::move(kept);
	held.text = now;
}

void TypedText::typed(const std::string& place, std::string_view before, std::string_view after) {
	const auto found = std::ranges::find(held_, place, &Held::place);
	if(after.size() > maxTextBytes) {
		if(found != held_.end()) held_.erase(found);
		return;
	}
	const bool known = found != held_.end();
	if(!known) {
		if(held_.size() >= maxValues) held_.erase(held_.begin());
		held_.push_back({place, std::string(before), {}});
	}
	Held& held = known ? *found : held_.back();
	follow(held, before);
	const Change change = changeBetween(before, after);
	// Within a run it stays that run: what they type, and what they take out of what they typed
	const auto within = std::ranges::find_if(
	    held.runs, [&](const Run& run) { return run.start <= change.at && change.oldEnd <= run.end; });
	if(within == held.runs.end()) {
		follow(held, after); // over what wasn't theirs: that goes, and what they typed is a run of its own
		if(change.newEnd > change.at) held.runs.push_back({change.at, change.newEnd});
		return;
	}
	const Run grown = {within->start, within->end - change.oldEnd + change.newEnd};
	for(Run& run : held.runs) {
		if(run.start < change.oldEnd) continue;
		run.start = run.start - change.oldEnd + change.newEnd;
		run.end = run.end - change.oldEnd + change.newEnd;
	}
	*within = grown;
	std::erase_if(held.runs, [](const Run& run) { return run.start == run.end; });
	held.text = after;
}

bool TypedText::holds(std::string_view form, const json& state) {
	if(form.empty()) return true;
	bool found = false;
	std::erase_if(held_, [&](Held& held) {
		const json* value = valueAt(state, held.place);
		if(!value || !value->is_string()) return true; // it went: so did what it held
		follow(held, value->get_ref<const std::string&>());
		found = found || std::ranges::any_of(held.runs, [&](const Run& run) {
			        return std::string_view(held.text).substr(run.start, run.end - run.start).contains(form);
		        });
		return false;
	});
	return found;
}

} // namespace affresco::format
