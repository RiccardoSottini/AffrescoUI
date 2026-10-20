#pragma once

/// @file
/// @brief What preparing one revision may cost: how many operations it has, how wide it makes an object, and how
/// much its "copy" operations copy (SECURITY.md, "Limits").

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <string>

namespace affresco::vcs::detail {

/// Most operations one revision, or one proposal with all its commits, may have: its compact edits and its JSON Patch.
/// Each costs a walk to the place it names, and some a search of the document for an "#id".
inline constexpr std::size_t maxOperations = 10'000;
/// Most members a revision may give one object. An object finds a key by looking at its members one by one, so
/// every operation inside a wider one costs more.
inline constexpr std::size_t maxMembers = 10'000;
/// Most values (each object, list, text, number, boolean and null) the "copy" operations of one revision may copy in
/// all. A copy of a list into itself doubles it: 30 of them would ask for billions of values.
inline constexpr std::size_t maxCopiedValues = 100'000;

/// @brief Refuses a revision, or a proposal with all its commits, of more than maxOperations operations.
/// @param operations How many it has.
/// @throws DocumentError If it has more.
void expectFewEnoughOperations(std::size_t operations);

/// @brief Refuses a revision that gives an object more than maxMembers members. A screen file is trusted and may hold
/// a wider object already: then only a revision that makes an object wider still is refused.
/// @param before The document with the live state in "state", before the revision.
/// @param after The same, after it.
/// @throws DocumentError If it does.
void expectNarrowEnough(const json& before, const json& after);

/// @brief Holds the preparation of one revision to maxOperations and maxCopiedValues, and the values it writes to
/// maxMembers, before each operation runs.
class RevisionBudget {
public:
	/// @brief Starts the budget of a revision.
	/// @param patch Its operations (JSON Patch, with "set").
	/// @throws DocumentError If it has more than maxOperations.
	explicit RevisionBudget(const json& patch);

	/// @brief Checks an operation before it is applied, and counts what it copies.
	/// @param view The document as rules see it, before the operation.
	/// @param op The operation, its paths resolved to JSON Pointers.
	/// @param where Its place, for errors.
	/// @throws DocumentError If it would go over a limit.
	void spend(const json& view, const json& op, const std::string& where);

private:
	std::size_t copiesLeft_ = maxCopiedValues; ///< Values the revision's "copy" operations may still copy.
};

} // namespace affresco::vcs::detail
