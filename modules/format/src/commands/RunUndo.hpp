#pragma once

/// @file
/// @brief format::RunUndo: what the newest runs changed, so that a run can be undone.

#include "affresco/core/Json.hpp"

#include <optional>
#include <string>

namespace affresco::format {

/// @brief What the newest runs changed in the state and the records (at most maxUndoableRuns), and undo: refused when
/// what a run changed was changed since.
///
/// Not built yet (f4d fills it in): it keeps nothing, so every undo is refused. Owned by CommandRuns. Main thread
/// only.
class RunUndo {
public:
	/// @brief Puts back what a run changed.
	/// @param run The run's number.
	/// @return Why it was refused, or std::nullopt; not built yet: "not built yet".
	std::optional<std::string> undo(int run);
};

} // namespace affresco::format
