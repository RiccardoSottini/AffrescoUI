/// @file
/// @brief The "ask" step: a request in plain words from the screen's own person to the agents connected to it.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief Runs an "ask" step: records the request, writes its id "into" a state path, and sends it after the event.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runAskStep(const Running& s) {
	if(!s.step.contains("ask")) return std::nullopt;
	s.steps.checkControl(s.where); // a revision can't make the screen speak for the person
	Engine& engine = s.steps.engine();
	const std::string id = engine.asks.send(string(s.value("ask"), s.where + ".ask"), s.where);
	if(s.step.contains("into")) {
		const json::json_pointer pointer = s.target("into");
		statePlace(engine.versions.state(), pointer, s.where + ".into") = id;
		engine.document.markStateChanged(pointer);
	}
	return true;
}

} // namespace

void registerAssistantSteps(StepRegistry& registry) {
	registry.add({"assistant", {{"ask", {"into"}, true}}, runAskStep});
}

} // namespace affresco::format
