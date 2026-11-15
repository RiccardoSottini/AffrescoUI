/// @file
/// @brief format::CommandCatalog: main's commands as tools. Not built yet: the seams f4c fills in.

#include "CommandCatalog.hpp"
#include "CommandArguments.hpp"
#include "CommandReach.hpp"

namespace affresco::format {

// f4c: the actions that describe themselves, Lisp's defcommand first
// NOLINTNEXTLINE(readability-convert-member-functions-to-static): f4c reads the engine's program
json CommandCatalog::list() const { return json::array(); }

// f4c: one entry of list()
// NOLINTNEXTLINE(readability-convert-member-functions-to-static): f4c reads the engine's program
std::optional<json> CommandCatalog::find([[maybe_unused]] std::string_view name) const { return std::nullopt; }

CommandReach CommandCatalog::reach(std::string_view name) const {
	const std::optional<json> command = find(name);
	return command ? reachOf(*command) : CommandReach{true, std::string(notBuiltYet), {}, false};
}

std::optional<std::string> CommandCatalog::checkArguments(std::string_view name, json& arguments) const {
	const std::optional<json> command = find(name);
	if(!command) return "no command '" + std::string(name.substr(0, 80)) + "'";
	return checkCommandArguments(*command, arguments);
}

} // namespace affresco::format
