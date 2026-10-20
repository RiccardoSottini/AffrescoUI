/// @file
/// @brief What preparing one revision may cost.

#include "Budget.hpp"

#include "Permissions.hpp"

#include <algorithm>
#include <vector>

namespace affresco::vcs::detail {

using expect::fail;

namespace {

/// @brief Counts the values in a value (itself, and everything inside it), without counting much past a limit.
/// @param value The value.
/// @param limit The most worth counting.
/// @return How many, or a number above @p limit if there are more.
std::size_t countValues(const json& value, std::size_t limit) {
	std::size_t count = 0;
	std::vector<const json*> pending{&value};
	while(!pending.empty() && count <= limit) {
		const json* item = pending.back();
		pending.pop_back();
		++count;
		if(!item->is_structured()) continue;
		for(const json& inside : *item) {
			if(count + pending.size() > limit) return limit + 1;
			pending.push_back(&inside);
		}
	}
	return count;
}

/// @brief The most members any object in a value has. Walks without recursion.
/// @param value The value.
/// @return 0 if it holds no object.
std::size_t widestObject(const json& value) {
	if(!value.is_structured()) return 0;
	std::size_t widest = 0;
	std::vector<const json*> pending{&value};
	while(!pending.empty()) {
		const json* item = pending.back();
		pending.pop_back();
		if(item->is_object()) widest = std::max(widest, item->size());
		for(const json& inside : *item)
			if(inside.is_structured()) pending.push_back(&inside);
	}
	return widest;
}

} // namespace

void expectFewEnoughOperations(std::size_t operations) {
	if(operations > maxOperations)
		fail("revision", std::to_string(operations) + " operations, more than the " + std::to_string(maxOperations) +
		                     " a revision (or a proposal, with all its commits) may have: split it into smaller ones");
}

RevisionBudget::RevisionBudget(const json& patch) {
	if(patch.is_array()) expectFewEnoughOperations(patch.size());
}

void RevisionBudget::spend(const json& view, const json& op, const std::string& where) {
	const auto text = [&](const char* key) -> const std::string* {
		const auto found = op.find(key);
		return found != op.end() && found->is_string() ? &found->get_ref<const std::string&>() : nullptr;
	};
	const std::string* kind = text("op");
	if(!kind || *kind == "test" || *kind == "remove") return;

	if(const std::string* from = text("from"); *kind == "copy" && from) {
		if(const json* copied = locate(view, *from)) {
			const std::size_t values = countValues(*copied, copiesLeft_);
			if(values > copiesLeft_)
				fail(where, "the revision's copies would copy more than " + std::to_string(maxCopiedValues) +
				                " values in all");
			copiesLeft_ -= values;
		}
	}
	// Before it is in the document, where every later operation inside it would search it
	if(const auto value = op.find("value"); value != op.end() && widestObject(*value) > maxMembers)
		fail(where, "the value holds an object of more than " + std::to_string(maxMembers) + " members");
}

void expectNarrowEnough(const json& before, const json& after) {
	const std::size_t widest = widestObject(after);
	if(widest > maxMembers && widest > widestObject(before))
		fail("revision", "the screen would hold an object of " + std::to_string(widest) + " members, more than " +
		                     std::to_string(maxMembers));
}

} // namespace affresco::vcs::detail
