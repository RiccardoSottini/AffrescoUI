#pragma once

/// @file
/// @brief format::ItemIndex: which children of a long list read which item, so an update visits only the children
/// that read a changed place instead of every child.

#include "Expression.hpp"
#include "Reads.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief The children of one panel by the item of the state they read (a repeat's items, Expression.hpp,
/// Scope::origin).
///
/// A child is local when everything below it reads only places inside its item ("/items/5" and what is in it). A
/// change can then meet it only if it is that item, inside it, or around the list: an update looks the changed
/// places up instead of visiting every child. The other children are visited every time.
/// Invariants: indices are those of the panel's children when it was built (Reconciler::gatherReads()), which is
/// also the only time children move. Main thread only.
class ItemIndex {
public:
	/// The children a panel has before it keeps an index: fewer are visited faster than looked up.
	static constexpr std::size_t minChildren = 64;
	/// The lists the items may come from before the index is dropped: more are faster to visit.
	static constexpr std::size_t maxLists = 8;

	/// @brief Starts an index again, with no child.
	/// @param children The panel's children now (all of them, the factory's too).
	void start(std::size_t children) noexcept {
		local_.clear();
		others_.clear();
		lists_.clear();
		children_ = children;
		built_ = true;
	}
	/// @brief Drops the index: every child is visited.
	void drop() noexcept {
		local_.clear();
		others_.clear();
		lists_.clear();
		built_ = false;
	}
	/// @brief Adds a child.
	/// @param child Its index among the panel's children.
	/// @param locals Its variables.
	/// @param reads What it and the panels below it read.
	void add(std::size_t child, const Scope* locals, const ReadSet& reads);
	/// @brief Whether a child updated may stay where it is: it reads no more than its item, or the index has it with
	/// the others already. Call drop() if not.
	/// @param locals The child's variables.
	/// @param reads What it reads now.
	/// @return true if the index is still right.
	[[nodiscard]] bool keeps(const Scope* locals, const ReadSet& reads) const;
	/// @brief The children changes may meet.
	/// @param changes What changed.
	/// @param children The panel's children now: if they are not as many as when it was built, every child may.
	/// @return Their indices, in order; nullopt if every child may (no index, or a change around a list).
	[[nodiscard]] std::optional<std::vector<std::size_t>> visits(const StateChanges& changes,
	                                                             std::size_t children) const;

private:
	std::multimap<std::string, std::size_t, std::less<>> local_; ///< Local children by their item's place.
	std::vector<std::size_t> others_;                             ///< The children that read more than their item.
	std::vector<std::string> lists_; ///< The places of the lists the items come from.
	std::size_t children_ = 0;       ///< The panel's children when it was built.
	bool built_ = false;             ///< Whether there is an index.
};

} // namespace affresco::format
