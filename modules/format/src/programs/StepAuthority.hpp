#pragma once

/// @file
/// @brief format::StepAuthority: who the steps now running come from, which work that runs after them keeps
/// (a timer, a call's callbacks, a sound's actions), so that running later lends it nothing.

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::format {

/// @brief Who the steps now running come from, while code of a program or a peer's run leads to them, and later for
/// what it scheduled.
struct StepAuthority {
	/// @brief The peer's run that led here: who runs it, and whether it was refused, which ends what it scheduled.
	struct Run {
		std::string runner;   ///< The runner.
		bool refused = false; ///< Set when the run is refused: work it scheduled then runs no step.
	};
	std::vector<std::string> authors; ///< Every author whose code led here.
	std::shared_ptr<Run> run;         ///< The peer's run that led here, shared with what it scheduled; or null.
};

/// @brief The authority a piece of later work keeps: the one of the step that asked for it, or std::nullopt for the
/// screen's own steps (Programs::keptAuthority(), AuthorityScope).
using KeptAuthority = std::optional<StepAuthority>;

} // namespace affresco::format
