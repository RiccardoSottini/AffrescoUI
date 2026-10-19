/// @file
/// @brief The control flow steps: if, forEach, while, run.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief "forEach": runs "do" for each item of a list, with "as" (item) and "index".
/// @param s The step.
/// @return false if a "stop" step ended it.
bool forEach(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;
	const json items = s.list(step["forEach"], where + ".forEach"); // a copy: the steps may change the list
	if(!items.is_array()) fail(where + ".forEach", "expected a list");
	if(items.size() > maxLoopIterations) fail(where + ".forEach", "list too long");
	const std::string name = step.contains("as") ? string(step["as"], where + ".as") : "item";
	const std::string indexName = step.contains("index") ? string(step["index"], where + ".index") : "index";
	for(std::size_t i = 0; i < items.size(); ++i) {
		const Scope scope{&s.locals, nullptr, {{name, items[i]}, {indexName, i}}};
		if(!s.run(step.value("do", json::array()), scope, where + ".do")) return false;
	}
	return true;
}

/// @brief "run": runs an action by name, with "with" as its own variables.
/// @param s The step.
void runAction(const Running& s) {
	// "with": the action's own variables, e.g. {"run": "moveCard", "with": {"to": "{{c + 1}}"}}
	Scope scope{&s.locals, nullptr, json::object()};
	if(s.step.contains("with"))
		for(const auto& [name, v] : object(s.step["with"], s.where + ".with").items())
			scope.vars[name] = s.value(v, s.where + ".with." + name);
	s.steps.runNamed(text(s.value("run")), s.sender, s.event, scope, s.where + ".run");
}

/// @brief Runs a control flow step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runFlowStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;

	if(step.contains("if")) {
		const char* branch = format::truthy(s.value("if")) ? "then" : "else";
		return step.contains(branch) ? s.run(step[branch], s.locals, where + "." + branch) : true;
	}
	if(step.contains("forEach")) return forEach(s);
	if(step.contains("while")) {
		std::size_t iterations = 0;
		while(format::truthy(s.value("while"))) {
			if(++iterations > maxLoopIterations) fail(where, "\"while\" ran more than 10000 times");
			if(!s.run(step.value("do", json::array()), s.locals, where + ".do")) return false;
		}
		return true;
	}
	if(step.contains("run")) {
		runAction(s);
		return true;
	}
	return std::nullopt;
}

} // namespace

void registerFlowSteps(StepRegistry& registry) {
	registry.add({"flow",
	              {{"if", {"then", "else"}}, {"forEach", {"as", "index", "do"}}, {"while", {"do"}}, {"run", {"with"}}},
	              runFlowStep});
}

} // namespace affresco::format
