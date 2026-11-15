/// @file
/// @brief The typed arguments of a command. Not built yet: the seam f4c fills in.

#include "CommandArguments.hpp"
#include "CommandReach.hpp"

namespace affresco::format {

// f4c: types, required arguments, defaults, at most PeerLimits::maxArgumentBytes
std::optional<std::string> checkCommandArguments([[maybe_unused]] const json& command,
                                                 // NOLINTNEXTLINE(misc-const-correctness): f4c fills in defaults
                                                 [[maybe_unused]] json& arguments) {
	return std::string(notBuiltYet);
}

} // namespace affresco::format
