#pragma once

/// @file
/// @brief format::Tracker: the readers of the state and what each read, so an update runs the readers its changes
/// touch, found by the place they read (PlaceIndex), in their order.

#include "PlaceIndex.hpp"
#include "Reads.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace affresco::format {

struct Engine;

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
	/// @brief Where it comes among the readers one update runs, as numbers compared one by one (a shorter list that
	/// begins another comes first). By default 0 and then order(): before every panel's reader.
	/// @param out Gets the numbers, appended.
	virtual void position(std::vector<std::uint32_t>& out) const;
	/// @brief Whether it is one of the panels' readers, which a full pass of the reconciler brings up to date: when
	/// everything changed, the tracker leaves them to that pass.
	/// @return false by default.
	[[nodiscard]] virtual bool panelReader() const noexcept { return false; }
};

/// @brief The readers of the state, each with what it read, kept by place (PlaceIndex): an update runs the readers its
/// changes touch, in their order (Reader::position()), each at most once; places they mark while they run join the
/// update, and the readers of those run after, in order. Readers that read what no change announces (ReadSet::always)
/// run in every update.
///
/// Invariants: each reader is added once and removed before it is destroyed; one update runs each reader at most
/// once, and a reader that records what it read while an update runs (one built or rebuilt then) counts as run. A
/// handle removed is never given again while an update runs, so one collected and then removed is skipped. Callers:
/// the readers (add(), remove(), setReads()) and the reconciler (update()). Main thread only.
class Tracker {
public:
	/// @brief The readers of an engine's state.
	/// @param engine The runtime; outlives this object.
	explicit Tracker(Engine& engine) noexcept : engine_(engine) {}

	/// The rounds of readers marking places other readers read that one update runs before it gives up (a circle).
	static constexpr int maxRounds = 64;
	/// The places all readers may record together: past it, a reader reads always (AlwaysReason::TooMany).
	static constexpr std::size_t maxEntries = 4'000'000;

	/// @brief Adds a reader, which has read nothing yet.
	/// @param reader The reader; outlives its registration.
	/// @return Its handle.
	ReaderId add(Reader& reader);
	/// @brief Removes a reader.
	/// @param id Its handle; 0 or an unknown one does nothing.
	void remove(ReaderId id) noexcept;
	/// @brief Records what a reader read, in place of what it read before.
	/// @param id Its handle.
	/// @param reads What it read.
	void setReads(ReaderId id, ReadSet reads);
	/// @brief The readers some changes touch.
	/// @param changes The changes.
	/// @return They, in their order; every reader not a panel's when everything changed.
	[[nodiscard]] std::vector<Reader*> touched(const StateChanges& changes) const;
	/// @brief Runs the readers an update's changes touch, then those the places they mark touch, and so on: each at
	/// most once, in order; past maxRounds rounds of readers marking for one another it reports one error naming the
	/// readers still marking. A place marked that a reader already run reads, or marked after a reader that reads
	/// always ran, is marked again for the next update.
	/// @param changes The update's changes; gets the places marked while the readers ran.
	/// @param panels Whether the panels' readers run too (false: a full pass of the reconciler follows).
	/// @return false if everything changed, during it or before: the panels' readers didn't run, a full pass must.
	/// @throws DocumentError From a reader.
	bool update(StateChanges& changes, bool panels = true);
	/// @brief The readers that read always.
	/// @return Their number.
	[[nodiscard]] std::size_t alwaysReaders() const noexcept { return always_.size(); }
	/// @brief The places recorded, over every reader.
	/// @return Their number.
	[[nodiscard]] std::size_t entries() const noexcept { return index_.entries(); }

private:
	/// @brief A reader in its slot.
	struct Entry {
		Reader* reader = nullptr;         ///< The reader (nullptr: the slot is free).
		ReaderId id = 0;                  ///< Its handle.
		std::uint32_t alwaysAt = noPlace; ///< Its place in always_, if it reads always.
		std::uint32_t ran = 0;            ///< The update it last ran in (or was built in).
		std::uint16_t generation = 0;     ///< The generation of the slot's last handle.
		bool panel = false;               ///< Reader::panelReader().
	};
	/// @brief A reader due in the running update.
	struct Due {
		ReaderId id = 0;          ///< The reader.
		std::uint32_t round = 0;  ///< How many readers marked places, one for the next, before it was due.
		std::uint32_t key = 0;    ///< Where its position starts in keys_.
		std::uint32_t length = 0; ///< Its position's numbers.
	};
	/// @brief Marks the update running while it lives: handles removed meanwhile are given again only after it.
	class Running {
	public:
		/// @brief Starts an update.
		/// @param tracker The tracker.
		explicit Running(Tracker& tracker) noexcept;
		~Running();
		Running(const Running&) = delete;
		Running& operator=(const Running&) = delete;

	private:
		Tracker& tracker_; ///< The tracker.
	};

	/// No place in always_.
	static constexpr std::uint32_t noPlace = 0xffffffffu;

	/// @brief A reader's entry, if the handle is still its.
	/// @param id The handle.
	/// @return The entry, or nullptr.
	[[nodiscard]] Entry* live(ReaderId id) noexcept;
	/// @brief Stops a reader reading always.
	/// @param entry Its entry.
	void leaveAlways(Entry& entry) noexcept;
	/// @brief Makes readers due, after the one running, in order.
	/// @param ids The readers; those run already are left out.
	/// @param round Their round.
	/// @param from The first due reader not run yet.
	/// @return Whether one of them had run already.
	bool enqueue(const std::vector<ReaderId>& ids, std::uint32_t round, std::size_t from);
	/// @brief Adds to found_ the panels' readers that read always and ran in this update, so they run again: a place
	/// marked after them may change what they read (a variable built from the state).
	void rerunAlways();
	/// @brief Collects into found_ the readers some changes touch.
	/// @param changes The changes.
	/// @param panels Whether the panels' readers too.
	void collect(const StateChanges& changes, bool panels);
	/// @brief Runs every reader not a panel's that hasn't run in this update, in order (everything changed).
	/// @param changes The changes.
	void runAll(StateChanges& changes);
	/// @brief Joins the places marked while a reader ran to the update's changes.
	/// @param changes The update's changes.
	/// @param marked The places marked.
	static void join(StateChanges& changes, const StateChanges& marked);
	/// @brief Reports the readers still marking for one another after maxRounds rounds.
	/// @param from The first due reader not run.
	void reportCircle(std::size_t from);

	Engine& engine_;                      ///< The runtime.
	mutable PlaceIndex index_;            ///< The readers by place (collect() reuses its vectors).
	std::vector<Entry> entries_;          ///< The readers, by slot.
	std::vector<std::uint32_t> free_;     ///< Slots free for a new reader.
	std::vector<std::uint32_t> freed_;    ///< Slots freed while an update runs, free after it.
	std::vector<ReaderId> always_;        ///< The readers that read always.
	std::vector<Due> due_;                ///< The readers due in the running update (reused).
	std::vector<std::uint32_t> keys_;     ///< Their positions (reused).
	mutable std::vector<ReaderId> found_; ///< What the index collected (reused).
	std::uint32_t stamp_ = 0;             ///< The number of the update running, or of the last one.
	bool running_ = false;                ///< Whether an update runs.
	bool warned_ = false;                 ///< Whether maxEntries was reported.
};

} // namespace affresco::format
