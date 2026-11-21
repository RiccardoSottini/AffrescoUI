/// @file
/// @brief format::Runtime's commands as tools: the run control, main's commands, and running them as a peer.

#include "Engine.hpp"

namespace affresco::format {

void Runtime::setRunControl(RunControl* control) {
	Engine& engine = this->engine();
	if(engine.runControl) engine.runControl->onChanged.disconnect(engine.runsChanged);
	engine.runsChanged = 0;
	engine.runControl = control;
	if(control) engine.runsChanged = control->onChanged.connect([&engine] { engine.proposalsChanged(); });
	engine.document.globalsChanged(); // the "runs" and "runGrants" variables
	engine.reconciler.requestReconcile();
}

json Runtime::commandList() const { return engine().commandCatalog.list(); }

std::optional<json> Runtime::command(std::string_view name) const { return engine().commandCatalog.find(name); }

CommandReach Runtime::commandReach(std::string_view name) const { return engine().commandCatalog.reach(name); }

std::optional<std::string> Runtime::checkCommandArguments(std::string_view name, json& arguments) const {
	return engine().commandCatalog.checkArguments(name, arguments);
}

CommandOutcome Runtime::runCommandAs(const CommandRun& run) { return engine().commandRuns.run(run); }

CommandOutcome Runtime::undoCommandRun(int run) { return engine().commandRuns.undo(run); }

} // namespace affresco::format
