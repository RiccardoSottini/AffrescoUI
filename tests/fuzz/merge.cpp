/// @file
/// @brief Fuzzes the three-way merge of JSON documents (vcs/Merge.hpp): a proposal's rebase merges what any peer
/// wrote, and a merge proposal or an undo merges versions that come from outside the history.
///
/// The input is a JSON object `{"base": b, "main": m, "proposal": p}`. mergeDocuments(b, m, p) must give a document
/// or throw DocumentError, never anything else; every key of the merged value, at the top two levels, must be a key
/// of some side there; a document merged with itself must give itself; and where two of the three sides are equal,
/// the merge must be clean and give the side that changed.

#include <affresco/vcs/Merge.hpp>

#include "Fuzz.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

using affresco::json;
using affresco::vcs::mergeDocuments;

namespace {

/// @brief Tells whether a key is a key of some side's object.
/// @param sides The base, main and the proposal at that place (each may be any value).
/// @param key The key.
/// @return true if one of them is an object with that key.
bool onSomeSide(const std::array<const json*, 3>& sides, const std::string& key) {
	return std::ranges::any_of(sides,
	                           [&key](const json* side) { return side && side->is_object() && side->contains(key); });
}

/// @brief The member of a value, if it is an object that has it.
/// @param value The value, or nullptr.
/// @param key The key.
/// @return The member, or nullptr.
const json* memberOf(const json* value, const std::string& key) {
	if(!value || !value->is_object()) return nullptr;
	const auto it = value->find(key);
	return it == value->end() ? nullptr : &*it;
}

/// @brief Checks that the merged keys at the top two levels each come from some side.
/// @param merged The merged document.
/// @param sides The base, main and the proposal.
void expectKnownKeys(const json& merged, const std::array<const json*, 3>& sides) {
	if(!merged.is_object()) return;
	for(auto it = merged.begin(); it != merged.end(); ++it) {
		if(!onSomeSide(sides, it.key())) affresco::fuzz::broken("a merged key comes from some side");
		if(!it->is_object()) continue;
		const std::array<const json*, 3> inside = {memberOf(sides[0], it.key()), memberOf(sides[1], it.key()),
		                                           memberOf(sides[2], it.key())};
		for(auto member = it->begin(); member != it->end(); ++member)
			if(!onSomeSide(inside, member.key())) affresco::fuzz::broken("a merged key comes from some side");
	}
}

/// @brief Checks a merge where two sides are equal: clean, and giving the side that changed.
/// @param base The base.
/// @param main main's.
/// @param proposal The proposal's.
/// @param expected What it must give.
void expectClean(const json& base, const json& main, const json& proposal, const json& expected) {
	const affresco::vcs::MergeResult result = mergeDocuments(base, main, proposal);
	if(!result.clean()) affresco::fuzz::broken("a merge where two sides are equal is clean");
	if(result.merged != expected) affresco::fuzz::broken("a merge where two sides are equal gives the other side");
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	json input;
	try {
		input = affresco::parseUntrusted(affresco::fuzz::text(data, size));
	} catch(const json::parse_error&) {
		return 0;
	} catch(const affresco::DocumentError&) {
		return 0; // too deep
	}
	if(!input.is_object()) return 0;
	const json base = input.value("base", json()), main = input.value("main", json()),
	           proposal = input.value("proposal", json());
	try {
		const affresco::vcs::MergeResult result = mergeDocuments(base, main, proposal);
		expectKnownKeys(result.merged, {&base, &main, &proposal});
		if(!result.conflicts.is_array()) affresco::fuzz::broken("the conflicts are a list");

		for(const json* side : {&base, &main, &proposal})
			if(mergeDocuments(*side, *side, *side).merged != *side)
				affresco::fuzz::broken("a document merged with itself gives itself");
		expectClean(base, base, proposal, proposal);
		expectClean(base, main, base, main);
		expectClean(base, main, main, main);
	} catch(const affresco::DocumentError&) {
		return 0; // the one error a merge may report
	}
	return 0;
}
