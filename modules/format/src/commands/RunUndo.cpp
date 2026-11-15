/// @file
/// @brief format::RunUndo: what the newest runs changed. Not built yet: the seam f4d fills in.

#include "RunUndo.hpp"
#include "CommandReach.hpp"

namespace affresco::format {

// f4d: the state paths and records a run changed, compared with what they are now
// NOLINTNEXTLINE(readability-convert-member-functions-to-static): f4d keeps what the runs changed
std::optional<std::string> RunUndo::undo([[maybe_unused]] int run) { return std::string(notBuiltYet); }

} // namespace affresco::format
