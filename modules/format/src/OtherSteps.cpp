/// @file
/// @brief The other steps: copy, log, stop, note.

#include "Engine.hpp"

#include "affresco/core/Log.hpp"

namespace affresco::format {

namespace {

/// @brief Runs copy (to the clipboard), log (a debug message), stop (ends the action) or note (a comment).
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runOtherStep(const Running& s) {
	const json& step = s.step;
	if(step.contains("copy")) {
		if(Window* window = s.steps.engine().window) window->setClipboardText(text(s.value("copy")));
		return true;
	}
	if(step.contains("log")) {
		log(LogLevel::Debug, "AffrescoUI screen log: " + text(s.value("log")));
		return true;
	}
	if(step.contains("stop")) return false;
	if(step.contains("note")) return true; // a comment
	return std::nullopt;
}

} // namespace

void registerOtherSteps(StepRegistry& registry) {
	registry.add({"other", {{"copy", {}}, {"log", {}}, {"stop", {}}, {"note", {}}}, runOtherStep});
}

} // namespace affresco::format
