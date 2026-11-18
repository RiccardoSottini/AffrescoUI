#pragma once

/// @file
/// @brief format::Tracker: the readers of the state and what each read, so an update runs the readers its changes
/// touch before the walk over the panels.

#include "Reads.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace affresco::format {

struct Engine;

/// A reader's handle in a Tracker (0: none).
using ReaderId = std::uint32_t;

/// @brief Something that reads the state and brings itself up to date when what it read changes.
class Reader {
public:
	/// @brief A reader.
	Reader() = default;
	virtual ~Reader() = default;
	Reader(const Reader&) = delete;
	Reader& operator=(const Reader&) = delete;

	/// @brief Brings it up to date: what it read changed.
	/// @param changes The changes of the update running (places it marks while it runs join them).
	/// @throws DocumentError If it can't be brought up to date.
	virtual void update(const StateChanges& changes) = 0;
	/// @brief Where it comes among the readers one update runs: lower first.
	/// @return Its order.
	[[nodiscard]] virtual std::uint64_t order() const noexcept = 0;
	/// @brief A name for it, in an error that names readers marking each other's places: never a place of the state.
	/// @return The name.
	[[nodiscard]] virtual std::string name() const = 0;
};

/// @brief The readers of the state, each with what it read: an update runs the readers its changes touch, in their
/// order, before the walk over the panels; places they mark while they run join the update.
///
/// Invariants: each reader is added once and removed before it is destroyed; one update runs each reader at most
/// once. Callers: the readers (add(), remove(), setReads()) and the reconciler (update()). Main thread only.
class Tracker {
public:
	/// @brief The readers of an engine's state.
	/// @param engine The runtime; outlives this object.
	explicit Tracker(Engine& engine) noexcept : engine_(engine) {}

	/// The rounds of readers marking places other readers read that one update runs before it gives up (a circle).
	static constexpr int maxRounds = 64;

	/// @brief Adds a reader, which has read nothing yet.
	/// @param reader The reader; outlives its registration.
	/// @return Its handle.
	ReaderId add(Reader& reader);
	/// @brief Removes a reader.
	/// @param id Its handle; 0 or an unknown one does nothing.
	void remove(ReaderId id) noexcept;
	/// @brief Records what a reader read.
	/// @param id Its handle.
	/// @param reads What it read.
	void setReads(ReaderId id, ReadSet reads);
	/// @brief The readers some changes touch.
	/// @param changes The changes.
	/// @return They, by order().
	[[nodiscard]] std::vector<Reader*> touched(const StateChanges& changes) const;
	/// @brief Runs the readers an update's changes touch, then those the places they marked touch, and so on: each at
	/// most once; after maxRounds rounds it reports one error naming the readers still marking.
	/// @param changes The update's changes; gets the places marked while the readers ran.
	/// @throws DocumentError From a reader.
	void update(StateChanges& changes);

private:
	/// @brief A reader and what it read.
	struct Entry {
		Reader* reader = nullptr; ///< The reader.
		ReadSet reads;            ///< What it read.
	};

	Engine& engine_;                              ///< The runtime.
	std::unordered_map<ReaderId, Entry> readers_; ///< The readers, by handle.
	ReaderId next_ = 1;                           ///< The next handle.
};

} // namespace affresco::format
