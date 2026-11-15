#pragma once

/// @file
/// @brief format::RunUndo: what the newest runs changed, so that a run can be undone.

#include "affresco/core/Commands.hpp"
#include "affresco/core/Json.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace affresco::data {
class Store;
} // namespace affresco::data

namespace affresco::format {

/// @brief What the newest runs changed in the state and the records (at most maxUndoableRuns, and maxBytes in all),
/// and undo: refused, changing nothing, when what a run changed was changed since.
///
/// A run's state changes are kept as places: each state path it changed, with its value before and after (an array
/// that grew or shrank is one place, the array, as its indices moved). Its record writes are kept as the store's
/// journal groups they made. Owned by CommandRuns. Main thread only.
class RunUndo {
public:
	/// The most bytes the kept values of all runs take (their JSON text); the oldest runs go first.
	static constexpr std::size_t maxBytes = std::size_t{1} << 20U;

	/// @brief One state path a run changed.
	struct Place {
		json::json_pointer path;    ///< Where, in the state (without "/state").
		std::optional<json> before; ///< Its value before the run; std::nullopt: it had none.
		std::optional<json> after;  ///< Its value the run left; std::nullopt: it removed it.
	};

	/// @brief What one run changed.
	struct Kept {
		int run = 0;                           ///< The run's number.
		std::vector<Place> places = {};        ///< The state paths it changed.
		std::vector<std::int64_t> groups = {}; ///< The store's journal groups of its record writes, oldest first.
		std::size_t changes = 0;               ///< How many state paths and records it changed.
		Undoable undoable = Undoable::None;    ///< What undo can put back.
		bool tooLarge = false;                 ///< It changed more than maxBytes: nothing of it is kept.
		bool undone = false;                   ///< It was undone.
		std::size_t bytes = 0;                 ///< What its places take.
	};

	/// @brief The places a change of the state is made of, from its JSON Patch.
	/// @param before The state before.
	/// @param after The state after.
	/// @param patch The JSON Patch from before to after (json::diff).
	/// @return The places, none inside another, with their values before and after.
	[[nodiscard]] static std::vector<Place> placesOf(const json& before, const json& after, const json& patch);

	/// @brief Keeps what a run changed, dropping the oldest runs past maxUndoableRuns or maxBytes.
	/// @param kept What it changed (its places and groups); kept as too large past maxBytes alone.
	void keep(Kept kept);

	/// @brief What is kept of a run.
	/// @param run The run's number.
	/// @return It, or nullptr if nothing is (too old, or never ran); valid until the next keep().
	[[nodiscard]] const Kept* find(int run) const;

	/// @brief Puts back what a run changed: each state path to its value before, if it still holds the value the run
	/// left, and the run's record groups, if they are the newest groups not undone. Refused, changing nothing, when
	/// one isn't so.
	/// @param run The run's number.
	/// @param state The live state.
	/// @param store The store the run wrote.
	/// @return Why it was refused, or std::nullopt.
	[[nodiscard]] std::optional<std::string> undo(int run, json& state, data::Store& store);

private:
	/// @brief Why the state can't be put back as it was before a run, if it can't.
	/// @param kept What the run changed.
	/// @param state The live state.
	/// @return The reason, naming the first path that changed since; std::nullopt if every path holds what it left.
	[[nodiscard]] static std::optional<std::string> stateRefusal(const Kept& kept, const json& state);
	/// @brief Puts the state back as it was before a run.
	/// @param kept What the run changed.
	/// @param state The live state.
	static void restore(const Kept& kept, json& state);

	std::deque<Kept> kept_; ///< The newest runs, oldest first.
	std::size_t bytes_ = 0; ///< What their places take.
	int newest_ = 0;        ///< The newest run ever kept.
};

/// @brief The journal groups of the record writes made since a sequence and not undone, and how many writes of each
/// kind each collection had.
struct RecordWrites {
	std::vector<std::int64_t> groups = {}; ///< Their groups, oldest first.
	json counts = json::object();          ///< `{collection: {"inserted", "updated", "deleted"}}`.
	std::size_t count = 0;                 ///< How many writes.
};

/// @brief The newest sequence of a store's journal.
/// @param store The store.
/// @return It, or 0 if the journal is empty.
[[nodiscard]] std::int64_t newestSequence(const data::Store& store);

/// @brief The record writes a store's journal holds after a sequence, not undone.
/// @param store The store.
/// @param after The sequence (newestSequence() before they were made).
/// @return Their groups and counts.
[[nodiscard]] RecordWrites recordWritesSince(const data::Store& store, std::int64_t after);

/// @brief The newest groups of a store's journal that aren't undone, newest first.
/// @param store The store.
/// @param count At most this many.
/// @return They.
[[nodiscard]] std::vector<std::int64_t> newestGroups(const data::Store& store, std::size_t count);

} // namespace affresco::format
