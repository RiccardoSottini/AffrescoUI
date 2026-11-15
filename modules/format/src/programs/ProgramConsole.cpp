/// @file
/// @brief RuntimeConsole (ProgramConsole.hpp). A stub until the console's package (f2d) fills it: it answers
/// hasProgram() truly, and evaluates and finds nothing.

#include "ProgramConsole.hpp"

#include "Engine.hpp"

namespace affresco::format {

bool RuntimeConsole::hasProgram() const { return engine_.programs.hasProgram(); }

ConsoleResult RuntimeConsole::evaluate(const ConsoleRequest& request) {
	static_cast<void>(request);
	ConsoleResult result;
	result.error = "AffrescoUI console: not built yet";
	return result;
}

std::vector<ConsoleName> RuntimeConsole::names(std::string_view prefix, std::size_t most) {
	static_cast<void>(prefix);
	static_cast<void>(most);
	return {};
}

std::optional<ConsoleName> RuntimeConsole::describe(std::string_view name) {
	static_cast<void>(name);
	return std::nullopt;
}

std::vector<std::string> RuntimeConsole::files() const { return {}; }

std::optional<std::string> RuntimeConsole::unshowable(std::string_view text) const {
	static_cast<void>(text);
	return std::nullopt;
}

} // namespace affresco::format
