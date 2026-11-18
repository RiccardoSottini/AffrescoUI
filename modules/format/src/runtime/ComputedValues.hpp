#pragma once
/// @file
/// @brief format::ComputedValues: the document's "computed" values, each a tracked value: kept until a place it read
/// changes, evaluated again only when read, and silent when its new value equals the old one.

#include "Reads.hpp"
#include "Tracker.hpp"
#include "affresco/core/Json.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

struct Engine;

/// Where the readers of computed values come among the readers of one update (Reader::order()): the settling of every
/// value first, so each knows whether it is stale before any is evaluated; the data layer's queries next (DataLayer),
/// which computed values read; then the values themselves.
inline constexpr std::uint64_t settleOrder = 0;
inline constexpr std::uint64_t queriesOrder = 1;  ///< See settleOrder.
inline constexpr std::uint64_t computedOrder = 2; ///< See settleOrder.

/// @brief The place of `computed.<name>` among the places values read: "/~c/<name>" (places::computed).
/// @param name The computed value.
/// @return The place.
[[nodiscard]] std::string computedPlace(std::string_view name);

/// @brief `computed.<name>`: each value is a reader of the state (Tracker), with what its last evaluation read. A place
/// it read changing makes it stale, not evaluated; it is evaluated when next read, or at the start of the next update
/// if something has read it, so that its place, "/~c/<name>", is marked before the walk over the panels, and only when
/// its value differs. What reads it reads that place, not what it read.
///
/// Invariants: a value is fresh when it isn't stale and was evaluated, or found untouched by an update's changes, at
/// the state's current version (DocumentContext::stateVersion()) and, if it read always, in the current update and at
/// the built variables' current count; between updates a move of either makes the values it concerns stale, since which
/// places changed is known only to the next update. A value that reads another is stale whenever the other is. A value
/// read in place stays valid until the evaluation reading it ends: a value equal to the old one keeps the old one's
/// storage, and values are dropped only between evaluations. A value being evaluated that reads itself, or a chain
/// deeper than maxComputedDepth, is an error. Callers: the document context's lookups (value()), the tracker (the
/// readers), the data layer (placeChanged()), the engine (documentChanged()) and verify mode (checkKept()). Main thread
/// only.
class ComputedValues {
public:
	/// @brief The computed values of an engine's document.
	/// @param engine The runtime; outlives this object, and its tracker outlives it too (Engine's order).
	explicit ComputedValues(Engine& engine) noexcept : settle_(*this), engine_(engine) {}
	/// @brief Removes the readers from the tracker.
	~ComputedValues();
	ComputedValues(const ComputedValues&) = delete;
	ComputedValues& operator=(const ComputedValues&) = delete;
	ComputedValues(ComputedValues&&) = delete;
	ComputedValues& operator=(ComputedValues&&) = delete;

	/// @brief `computed.<name>`: kept, or evaluated if it is stale.
	/// @param name The computed value.
	/// @return Its value.
	/// @throws DocumentError If there is no such value, it reads itself, or it is invalid.
	[[nodiscard]] json computed(std::string_view name);
	/// @brief `computed.<name>` in place: as computed(), without a copy. What reads it reads "/~c/<name>", and reads
	/// always if the value does.
	/// @param name The computed value.
	/// @return Its value, valid until it is evaluated again to a different value or the document changes, and at least
	/// until the evaluation reading it has ended.
	/// @throws DocumentError As computed().
	[[nodiscard]] const json& value(std::string_view name);
	/// @brief A place outside the state that values may read changed (a collection of records, a query's results):
	/// the values that read it, and those that read them, are stale.
	/// @param place The place ("/~d/<collection>"...).
	void placeChanged(std::string_view place);
	/// @brief A version of the document landed: every value is dropped, since its definition may have changed.
	void documentChanged();
	/// @brief Verify mode: evaluates every value kept as fresh again, without keeping the result, and compares; not a
	/// value reading first a panel's geometry or the clock, which move within an update.
	/// @return An error naming the first value whose kept value differs from a new evaluation, or std::nullopt.
	[[nodiscard]] std::optional<std::string> checkKept();

private:
	/// @brief One computed value: what it was, what it read, and its handle as a reader.
	class Entry final : public Reader {
	public:
		/// @brief A value not evaluated yet.
		/// @param owner The values; outlive it.
		/// @param name Its name.
		Entry(ComputedValues& owner, std::string name) : owner_(owner), name_(std::move(name)) {}
		/// @brief A place it read changed: it is stale, and is evaluated now if it was evaluated before.
		/// @param changes The update's changes.
		void update(const StateChanges& changes) override { owner_.updateEntry(*this, changes); }
		/// @brief After the settling and the queries (computedOrder).
		/// @return computedOrder.
		[[nodiscard]] std::uint64_t order() const noexcept override { return computedOrder; }
		/// @brief "computed.<name>".
		/// @return The name.
		[[nodiscard]] std::string name() const override { return "computed." + name_; }

		ComputedValues& owner_;    ///< The values.
		std::string name_;         ///< Its name.
		json value;                ///< Its value, if known.
		ReadSet reads;             ///< What its last evaluation read.
		std::uint64_t version = 0; ///< The state version it is fresh at.
		std::uint64_t epoch = 0;   ///< The update it was last evaluated in (epoch_).
		std::uint64_t built = 0;   ///< The built variables' count it is fresh at (DocumentContext::variablesEpoch()).
		std::uint64_t appearance = 0; ///< The appearance it was evaluated in (DocumentContext::appearance()).
		ReaderId id = 0;              ///< Its handle in the tracker.
		bool known = false;           ///< It has been evaluated since the last drop.
		bool valued = false;          ///< It has a value, kept across drops to tell whether its next one differs.
		bool stale = false;           ///< A place it read changed since.
	};
	/// @brief The reader that settles every value at the start of an update (settleOrder): it reads always.
	class Settle final : public Reader {
	public:
		/// @brief The settling of some values.
		/// @param owner The values; outlive it.
		explicit Settle(ComputedValues& owner) noexcept : owner_(owner) {}
		/// @brief Settles every value against the update's changes.
		/// @param changes The update's changes.
		void update(const StateChanges& changes) override { owner_.settle(changes); }
		/// @brief First of all (settleOrder).
		/// @return settleOrder.
		[[nodiscard]] std::uint64_t order() const noexcept override { return settleOrder; }
		/// @brief "computed values".
		/// @return The name.
		[[nodiscard]] std::string name() const override { return "computed values"; }

	private:
		ComputedValues& owner_; ///< The values.
	};

	/// @brief Whether a value's reads still hold at the state as it is: the state's version is the one it is fresh at,
	/// and, if it read always (a built variable, a panel's geometry, the clock), it was evaluated in this update and
	/// the built variables' count is the one it saw.
	/// @param entry The value.
	/// @return true if neither moved.
	[[nodiscard]] bool current(const Entry& entry) const noexcept;
	/// @brief The entry of a value, made if it is new.
	/// @param name The value.
	/// @return It.
	Entry& entryOf(std::string_view name);
	/// @brief Evaluates a value, keeps what it read, and marks its place if it differs from what it was.
	/// @param entry The value.
	/// @param definition Its definition.
	void evaluate(Entry& entry, const json& definition);
	/// @brief Makes a value stale, and every value that reads it.
	/// @param entry The value.
	void makeStale(Entry& entry);
	/// @brief At the start of an update: values that read what changed, and those reading always once the state
	/// moved, are stale; the others are fresh at the state's version.
	/// @param changes The update's changes.
	void settle(const StateChanges& changes);
	/// @brief Makes stale the values that read places marked since the last look: of those evaluated in this update,
	/// only places other than values' own.
	/// @param changes The update's changes.
	void sweepNew(const StateChanges& changes);
	/// @brief The tracker reached a value: it is stale if a place marked in this update reaches it, and is evaluated
	/// then if it was evaluated before.
	/// @param entry The value.
	/// @param changes The update's changes.
	void updateEntry(Entry& entry, const StateChanges& changes);
	/// @brief Removes every reader from the tracker.
	void drop() noexcept;

	Settle settle_;  ///< See Settle.
	Engine& engine_; ///< The runtime.
	/// The values, by name: a map, so a value read in place keeps its address.
	std::map<std::string, Entry, std::less<>> values_;
	std::vector<std::string> computing_; ///< The values being evaluated (finds circles).
	ReaderId settleId_ = 0;              ///< settle_'s handle in the tracker, while there are values.
	std::uint64_t epoch_ = 0;            ///< Counts the updates settled.
	std::size_t swept_ = 0;              ///< The places of this update's changes already looked at.
};

} // namespace affresco::format
