/// @file
/// @brief The steps over time: after, debounce, animate.

#include "Engine.hpp"

#include <cstdint>

namespace affresco::format {

namespace {

/// @brief "after" (always) or "debounce" (only if it isn't run again in the meantime): steps later.
/// @param s The step.
void later(const Running& s) {
	const bool debounce = s.step.contains("debounce");
	const double ms = s.milliseconds(debounce ? "debounce" : "after");
	if(!s.step.contains("do")) fail(s.where, "needs \"do\": the steps to run");
	Engine& engine = s.steps.engine();
	if(!engine.window) return;
	auto task = s.steps.later(s.step["do"], s.sender, s.event, snapshot(s.locals), s.where + ".do");
	if(debounce) {
		const std::string key = s.where + "@" + std::to_string(reinterpret_cast<std::uintptr_t>(&s.sender));
		engine.scheduler.debounce(key, ms, std::move(task));
	} else {
		engine.scheduler.once(ms, std::move(task));
	}
}

/// @brief "animate": a number in the state moves to "to" over "ms", one frame at a time.
/// @param s The step.
void animate(const Running& s) {
	const json::json_pointer pointer = s.target("animate");
	const json to = s.value(s.step.value("to", json()), s.where + ".to");
	if(!to.is_number()) fail(s.where + ".to", "expected a number");
	const double ms = s.step.contains("ms") ? s.milliseconds("ms") : 300.0;
	const std::string easing = s.step.contains("easing") ? string(s.step["easing"], s.where + ".easing") : "out";
	Engine& engine = s.steps.engine();
	const json* current = findInState(engine.versions.state(), pointer);
	const double from = current && current->is_number() ? current->get<double>() : 0.0;
	if(!engine.window || ms <= 0.0 || engine.window->reducedMotion()) {
		statePlace(engine.versions.state(), pointer, s.where) = to;
		engine.document.markStateChanged(pointer);
		return;
	}
	engine.scheduler.animate(pointer, from, to, ms, easing, s.where);
}

/// @brief Runs a step over time.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runTimeStep(const Running& s) {
	if(s.step.contains("after") || s.step.contains("debounce")) {
		later(s);
		return true;
	}
	if(s.step.contains("animate")) {
		animate(s);
		return true;
	}
	return std::nullopt;
}

} // namespace

void registerTimeSteps(StepRegistry& registry) {
	registry.add({"time", {{"after", {"do"}}, {"debounce", {"do"}}, {"animate", {"to", "ms", "easing"}}}, runTimeStep});
}

} // namespace affresco::format
