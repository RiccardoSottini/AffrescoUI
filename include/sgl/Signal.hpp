#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>

namespace sgl {

using Connection = std::uint64_t;

// Signal/slot: any number of callables connected to one event source.
// Emitting never allocates, and slots may connect/disconnect while the signal is being emitted.
template <typename... Args> class Signal {
public:
	using Slot = std::function<void(Args...)>;

	Connection connect(Slot slot) {
		slots_.push_back({nextId_, std::move(slot)});
		return nextId_++;
	}

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

	void operator()(Args... args) { emit(args...); }

	[[nodiscard]] bool empty() const noexcept { return slots_.empty(); }

private:
	struct Entry {
		Connection id;
		Slot slot;
	};

	std::deque<Entry> slots_; // push_back never moves existing slots, even one that is running
	Connection nextId_ = 1;
	int emitting_ = 0;
	bool pendingErase_ = false;
};

} // namespace sgl
