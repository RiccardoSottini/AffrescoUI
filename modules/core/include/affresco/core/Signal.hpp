#pragma once

/// @file
/// @brief Signal/slot: any number of callables connected to one event source.

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>

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
		slots_.push_back({nextId_, std::move(slot)});
		return nextId_++;
	}

	/// @brief Disconnects a slot.
	/// @param id What connect() returned.
	/// @return false if it was not connected.
	bool disconnect(Connection id) {
		auto it = std::ranges::find(slots_, id, &Entry::id);
		if(it == slots_.end() || !it->slot) return false;

		if(emitting_ > 0) {
			it->slot = nullptr; // erased once the emission finishes, so iteration stays valid
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
			if(slots_[i].slot) slots_[i].slot(args...);
		}
		if(--emitting_ == 0 && pendingErase_) {
			std::erase_if(slots_, [](const Entry& entry) { return !entry.slot; });
			pendingErase_ = false;
		}
	}

	/// @brief Same as emit().
	/// @param args Passed to each slot.
	void operator()(Args... args) { emit(args...); }

	/// @brief Whether no slot is connected.
	[[nodiscard]] bool empty() const noexcept { return slots_.empty(); }

private:
	/// @brief A connected slot.
	struct Entry {
		Connection id; ///< Its id.
		Slot slot;     ///< Empty once disconnected mid-emission.
	};

	std::deque<Entry> slots_;   ///< push_back never moves existing slots, even one that is running.
	Connection nextId_ = 1;     ///< Next id to hand out.
	int emitting_ = 0;          ///< Emissions in progress.
	bool pendingErase_ = false; ///< Slots were disconnected mid-emission.
};

} // namespace affresco
