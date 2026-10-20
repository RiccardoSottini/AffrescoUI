#pragma once

/// @file
/// @brief Signal/slot: any number of callables connected to one event source.

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <functional>
#include <utility>
#include <vector>

namespace affresco {

using Connection = std::uint64_t; ///< Identifies a connection to a Signal.

/// @brief Any number of callables connected to one event source.
///
/// Emitting never allocates, and slots may connect or disconnect (even themselves) during an emission.
/// @tparam Args The arguments passed to every slot.
template <typename... Args> class Signal {
public:
	using Slot = std::function<void(Args...)>; ///< A connected callable.

	/// @brief Connects a slot. Slots connected during an emission run from the next one.
	/// @param slot The callable.
	/// @return An id for disconnect().
	Connection connect(Slot slot) {
		// During an emission a slot may be running: it waits aside, so the list never moves under it
		(emitting_ > 0 ? connected_ : slots_).push_back({nextId_, std::move(slot)});
		return nextId_++;
	}

	/// @brief Disconnects a slot.
	/// @param id What connect() returned.
	/// @return false if it was not connected.
	bool disconnect(Connection id) {
		if(const auto waiting = std::ranges::find(connected_, id, &Entry::id); waiting != connected_.end()) {
			connected_.erase(waiting); // connected during this emission: never ran
			return true;
		}
		auto it = std::ranges::find(slots_, id, &Entry::id);
		if(it == slots_.end() || !it->live) return false;

		if(emitting_ > 0) {
			it->live = false; // erased once the emission finishes: it may be the slot running
			pendingErase_ = true;
		} else {
			slots_.erase(it);
		}
		return true;
	}

	/// @brief Calls every slot, in connection order.
	/// @param args Passed to each slot.
	void emit(Args... args) {
		++emitting_;
		// Index loop + size snapshot: slots connected during emission run from the next emit
		for(std::size_t i = 0, n = slots_.size(); i < n; ++i) {
			if(slots_[i].live) slots_[i].slot(args...);
		}
		if(--emitting_ > 0) return;
		if(pendingErase_) {
			std::erase_if(slots_, [](const Entry& entry) { return !entry.live; });
			pendingErase_ = false;
		}
		if(!connected_.empty()) {
			std::ranges::move(connected_, std::back_inserter(slots_));
			connected_.clear();
		}
	}

	/// @brief Same as emit().
	/// @param args Passed to each slot.
	void operator()(Args... args) { emit(args...); }

	/// @brief Whether no slot is connected.
	[[nodiscard]] bool empty() const noexcept { return slots_.empty() && connected_.empty(); }

private:
	/// @brief A connected slot.
	struct Entry {
		Connection id;    ///< Its id.
		Slot slot;        ///< The callable.
		bool live = true; ///< false once disconnected mid-emission, until the emission ends.
	};

	/// Connected slots, in order. Only changed outside emissions (a slot disconnected during one is marked), so a
	/// running slot never moves; a vector, because most signals have one or two slots (a deque's first block is 4 KiB).
	std::vector<Entry> slots_;
	std::vector<Entry> connected_; ///< Connected during an emission: joins slots_ when the outermost one ends.
	Connection nextId_ = 1;        ///< Next id to hand out.
	int emitting_ = 0;             ///< Emissions in progress.
	bool pendingErase_ = false;    ///< Slots were disconnected mid-emission.
};

} // namespace affresco
