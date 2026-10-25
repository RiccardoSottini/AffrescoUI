/// @file
/// @brief The terminal steps: runInTerminal runs a line in one of the screen's terminals (TerminalSessions.hpp),
/// clearTerminal clears it, closeTerminal closes it. A line for a terminal no node shows yet waits until one does; one
/// for a closed terminal is dropped.

#include "Engine.hpp"
#include "StepRegistry.hpp"
#include "TerminalSessions.hpp"
#include "TerminalView.hpp"

#include <optional>
#include <string>

namespace affresco::format {

namespace {

/// @brief The session a terminal step names.
/// @param s The step.
/// @param key The step's key, whose value is the session's id.
/// @return The id, of 1 to mostSessionIdBytes bytes.
/// @throws DocumentError If it isn't one.
std::string sessionOf(const Running& s, const char* key) {
	const std::string where = s.where + "." + key;
	const std::string session = string(s.value(key), where);
	if(session.empty() || session.size() > mostSessionIdBytes) fail(where, "a session's id has 1 to 256 bytes");
	return session;
}

/// @brief Runs a terminal step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runTerminalStep(const Running& s) {
	TerminalSessions& terminals = *s.steps.engine().terminals;
	if(s.step.contains("runInTerminal")) {
		const std::string session = sessionOf(s, "runInTerminal");
		if(!s.step.contains("text")) fail(s.where, "\"runInTerminal\" needs the \"text\" of the line it runs");
		const std::string line = string(s.value("text"), s.where + ".text");
		if(line.size() > mostPendingOutput) fail(s.where + ".text", "longer than 64 KiB");
		terminals.run(session, line);
	} else if(s.step.contains("clearTerminal")) {
		terminals.clear(sessionOf(s, "clearTerminal"));
	} else if(s.step.contains("closeTerminal")) {
		terminals.close(sessionOf(s, "closeTerminal"));
	} else {
		return std::nullopt;
	}
	return true;
}

} // namespace

void registerTerminalSteps(StepRegistry& registry) {
	registry.add({"terminal",
	              {{"runInTerminal", {"text"}, true}, {"clearTerminal", {}, true}, {"closeTerminal", {}, true}},
	              runTerminalStep});
}

} // namespace affresco::format
