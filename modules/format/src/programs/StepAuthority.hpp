#pragma once

/// @file
/// @brief format::StepAuthority: who the steps now running come from, which work that runs after them keeps
/// (a timer, a call's callbacks, a sound's actions), so that running later lends it nothing.

#include <optional>
#include <string>
#include <vector>

namespace affresco::format {

/// @brief Who the steps now running come from, while code of a program or a peer's run leads to them, and later for
/// what it scheduled.
struct StepAuthority {
	std::vector<std::string> authors; ///< Every author whose code led here.
};

/// @brief The authority a piece of later work keeps: the one of the step that asked for it, or std::nullopt for the
/// screen's own steps (Programs::keptAuthority(), AuthorityScope).
using KeptAuthority = std::optional<StepAuthority>;

} // namespace affresco::format
