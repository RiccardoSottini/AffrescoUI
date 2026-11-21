#pragma once

/// @file
/// @brief format::Watches: the document's "watch" handlers, run after the update in which their value changed.

#include "Tracker.hpp"

#include "affresco/core/Json.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace affresco {
class Panel;
} // namespace affresco

namespace affresco::format {

struct Engine;

/// @brief The values the document's "watch" handlers saw last, and running the handlers whose value changed.
///
/// Each watched path is a reader of the state (Tracker) with what its lookup read: it is looked up, compared and
/// copied again only after an update whose changes reach what it read, or a full update.
/// Invariants: a value is compared only with one seen after an earlier update; while handlers are suspended nothing
/// is seen (UpdateFrame::end() doesn't call watch()), so a document shown for a moment changes nothing; handlers run
/// in the order of the document's "watch". Callers: the update frame. Main thread only.
class Watches {
public:
	/// @brief The watches of an engine's document.
	/// @param engine The runtime; outlives this object, and its tracker outlives it too (Engine's order).
	explicit Watches(Engine& engine) : engine_(engine) {}
	/// @brief Removes the readers from the tracker.
	~Watches();
	Watches(const Watches&) = delete;
	Watches& operator=(const Watches&) = delete;
	Watches(Watches&&) = delete;
	Watches& operator=(Watches&&) = delete;

	/// @brief Runs the "watch" handlers whose values changed since the last update, and remembers the values.
	/// @param root The window's root, which sends them.
	void watch(Panel& root);

private:
	/// @brief One watched path, a reader: due once a place its lookup read changed.
	class Entry final : public Reader {
	public:
		/// @brief A path not looked up yet.
		/// @param watched The path.
		explicit Entry(std::string watched) : path(std::move(watched)) {}
		/// @brief What it read changed: it is looked up again after the update.
		/// @param changes The update's changes.
		void update(const StateChanges& changes) override {
			static_cast<void>(changes);
			due = true;
		}
		/// @brief Last of all: it only notes that it is due.
		/// @return The highest order.
		[[nodiscard]] std::uint64_t order() const noexcept override {
			return std::numeric_limits<std::uint64_t>::max();
		}
		/// @brief "watch.<path>".
		/// @return The name.
		[[nodiscard]] std::string name() const override { return "watch." + path; }

		std::string path;  ///< The watched path.
		json value;        ///< The value seen last.
		ReaderId id = 0;   ///< Its handle in the tracker.
		bool seen = false; ///< A value was seen after an update.
		bool due = false;  ///< What it read changed since.
	};

	/// @brief Follows the document's "watch": an entry per path, in its order, keeping what was seen of the others.
	/// @param section The "watch" section.
	void follow(const json& section);
	/// @brief Looks a path up again, with what it reads, and runs its handler if its value changed.
	/// @param entry The path.
	/// @param handler Its handler.
	/// @param root The window's root, which sends it.
	void look(Entry& entry, const json& handler, Panel& root);

	Engine& engine_;                              ///< The runtime.
	std::vector<std::unique_ptr<Entry>> entries_; ///< The watched paths, in the document's order.
	json section_;                                ///< The "watch" section entries_ follow.
};

} // namespace affresco::format
