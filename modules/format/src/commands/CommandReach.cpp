/// @file
/// @brief What a command can reach. Not built yet: the seam f4c fills in.

#include "CommandReach.hpp"

#include <string>

namespace affresco::format {

// f4c: walks the command's steps, the actions they run and the program's functions, at most so deep
CommandReach reachOf([[maybe_unused]] const json& command) { return {true, std::string(notBuiltYet), {}, false}; }

} // namespace affresco::format
