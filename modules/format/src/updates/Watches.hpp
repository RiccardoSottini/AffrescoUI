#pragma once

/// @file
/// @brief format::Watches: the document's "watch" handlers, run after the update in which their value changed.

#include "affresco/core/Json.hpp"

namespace affresco {
class Panel;
} // namespace affresco

namespace affresco::format {

struct Engine;

/// @brief The values the document's "watch" handlers saw last, and running the handlers whose value changed.
///
/// Invariants: a value is compared only with one seen after an earlier update; while handlers are suspended nothing
/// is seen (UpdateFrame::end() doesn't call watch()), so a document shown for a moment changes nothing. Callers: the
/// update frame. Main thread only.
class Watches {
public:
	/// @brief The watches of an engine's document.
	/// @param engine The runtime; outlives this object.
	explicit Watches(Engine& engine) : engine_(engine) {}

	/// @brief Runs the "watch" handlers whose values changed since the last update, and remembers the values.
	/// @param root The window's root, which sends them.
	void watch(Panel& root);

private:
	Engine& engine_;                ///< The runtime.
	json watched_ = json::object(); ///< The values seen last, by path.
};

} // namespace affresco::format
