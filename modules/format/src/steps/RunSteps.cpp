/// @file
/// @brief The run steps: allowRun, denyRun and undoRun, the person's decisions on the commands peers run.

#include "Engine.hpp"

#include <algorithm>
#include <array>

namespace affresco::format {

namespace {

/// @brief Evaluates a run step's values, and checks their types.
/// @param s The step.
/// @return The step with its values evaluated.
/// @throws DocumentError If a value has the wrong type.
json evaluateRun(const Running& s) {
	const std::string& where = s.where;
	json control = s.step;
	for(const char* key : {"allowRun", "denyRun", "undoRun", "always", "for", "reason"})
		if(control.contains(key)) control[key] = s.value(control[key], where + "." + key);
	for(const char* key : {"allowRun", "denyRun", "undoRun"})
		if(control.contains(key) && !control[key].is_number_integer())
			fail(where + "." + key, "expected a run's number");
	if(control.contains("always") && !control["always"].is_boolean()) fail(where + ".always", "expected true or false");
	if(control.contains("for") && !control["for"].is_string())
		fail(where + ".for", R"(expected a duration, e.g. "30m", "8h" or "7d")");
	return control;
}

/// @brief Carries out an evaluated run step: the person ("user") decides through the run control, or undoes a run.
/// @param engine The runtime.
/// @param control The step, its values evaluated.
/// @return An error, or std::nullopt.
std::optional<std::string> carryOut(Engine& engine, const json& control) {
	if(!engine.runControl) return "this screen runs no commands for peers";
	if(control.contains("allowRun"))
		return engine.runControl->allow(control["allowRun"].get<int>(), "user", control.value("always", false),
		                                control.value("for", std::string()));
	if(control.contains("denyRun"))
		return engine.runControl->deny(control["denyRun"].get<int>(), text(control.value("reason", json(""))));
	return engine.commandRuns.undo(control["undoRun"].get<int>()).refusal;
}

/// @brief Runs a run step: like the review steps, a control step done after the event, never from an agent's reply.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runRunStep(const Running& s) {
	if(std::ranges::none_of(std::array{"allowRun", "denyRun", "undoRun"},
	                        [&s](const char* key) { return s.step.contains(key); }))
		return std::nullopt;
	s.steps.checkControl(s.where);
	Engine& engine = s.steps.engine();
	if(engine.asks.deciding())
		fail(s.where, "an agent's reply runs this action: a reply is never the person's decision");

	json control = evaluateRun(s);
	if(!engine.window) return true;

	engine.scheduler.defer([&engine, control = std::move(control)] {
		if(const std::optional<std::string> error = carryOut(engine, control)) engine.report(*error);
		engine.reconciler.requestReconcile();
	});
	return true;
}

} // namespace

void registerRunSteps(StepRegistry& registry) {
	// Listed under "controlSteps" only, like the review steps
	registry.add({"runs",
	              {{"allowRun", {"always", "for"}, true, false},
	               {"denyRun", {"reason"}, true, false},
	               {"undoRun", {}, true, false}},
	              runRunStep});
}

} // namespace affresco::format
