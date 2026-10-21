/// @file
/// @brief format::Steps: running handlers, actions and steps; and Running, how a step reads its values.

#include "Steps.hpp"
#include "Engine.hpp"

#include <algorithm>

namespace affresco::format {

/* Running */

json Running::value(const json& v, const std::string& at) const {
	return format::evaluate(v, steps.engine().document.context(locals, event), at);
}

json::json_pointer Running::target(const char* key) const {
	return format::statePointer(string(step[key], where + "." + key), steps.engine().document.context(locals, event),
	                            where + "." + key);
}

json Running::list(const json& source, const std::string& at) const {
	if(source.is_string() && source.get<std::string>().find("{{") == std::string::npos)
		return format::lookup(source.get<std::string>(), steps.engine().document.context(locals, event), at);
	return value(source, at);
}

double Running::milliseconds(const char* key) const {
	return std::max(static_cast<double>(number(value(key), where + "." + key)), 0.0);
}

bool Running::run(const json& nested, const Scope& scope, const std::string& at) const {
	return steps.runSteps(nested, sender, event, scope, at);
}

/* Steps */

// Runs an event handler: an action name or inline steps. Errors are reported, never thrown into the event loop.
void Steps::handle(const json& handler, Panel& sender, const json& event, const Scope& locals,
                   const std::string& where) {
	const DocumentContext::BudgetScope budget(engine_.document); // the outermost handler's, shared by nested ones
	try {
		if(handler.is_string()) runNamed(handler.get<std::string>(), sender, event, locals, where);
		else runSteps(handler, sender, event, locals, where);
	} catch(const DocumentError& error) {
		engine_.report(error.what());
	} catch(const json::exception& error) { // a JSON error a step didn't turn into a DocumentError: never into the loop
		engine_.report(where + ": " + error.what());
	}
	if(engine_.document.stateChanged()) engine_.reconciler.requestStateUpdate();
}

std::function<void()> Steps::later(json steps, Panel& sender, json event, SharedScope locals, std::string where) {
	return [this, steps = std::move(steps), sender = &sender, event = std::move(event), locals = std::move(locals),
	        where = std::move(where)] {
		Window* window = engine_.window;
		if(!window) return;
		Panel& from = engine_.reconciler.recordOf(sender) ? *sender : window->root();
		handle(steps, from, event, *locals, where);
	};
}

void Steps::runNamed(const std::string& name, Panel& sender, const json& event, const Scope& locals,
                     const std::string& where) {
	const json& document = engine_.versions.document();
	if(document.contains("actions") && document["actions"].contains(name)) {
		const json steps = document["actions"][name]; // a copy: the steps may revise the document
		runSteps(steps, sender, event, locals, "actions." + name);
	} else if(const auto it = engine_.nativeActions.find(name); it != engine_.nativeActions.end()) {
		it->second(sender);
	} else if(engine_.unhandled) {
		engine_.unhandled(name, sender);
	} else {
		fail(where, "unknown action '" + name + "'");
	}
}

// Returns false when a "stop" step ends the action
bool Steps::runSteps(const json& steps, Panel& sender, const json& event, const Scope& locals,
                     const std::string& where) {
	if(++actionDepth_ > maxActionDepth) {
		--actionDepth_;
		fail(where, "actions nested too deeply (more than 32 levels)");
	}

	bool keepGoing = true;
	try {
		if(steps.is_array()) {
			for(std::size_t i = 0; i < steps.size() && keepGoing; ++i)
				keepGoing = runStep(steps[i], sender, event, locals, where + "[" + std::to_string(i) + "]");
		} else {
			keepGoing = runStep(steps, sender, event, locals, where);
		}
	} catch(...) {
		--actionDepth_;
		throw;
	}

	--actionDepth_;
	return keepGoing;
}

bool Steps::runStep(const json& step, Panel& sender, const json& event, const Scope& locals, const std::string& where) {
	object(step, where);
	engine_.document.spendStep(where);
	if(const std::optional<bool> keepGoing = engine_.steps->run(Running{*this, step, sender, event, locals, where}))
		return *keepGoing;

	fail(where, "unknown step (expected set, push, remove, toggle, increment, if, forEach, while, run, call, emit, "
	            "send, connect, patch, focus, scrollTo, navigate, after, debounce, animate, copy, log, stop, note, "
	            "accept, reject, preview, endPreview, restore, revert or undo)");
}

// While a proposal is previewed its document runs, timers and all: a control step then runs only from the user's own
// input, never from a timer or a handler that fires by itself (onLoad, onShow, watch), which the proposal may arrange
void Steps::checkControl(const std::string& where) const {
	if(engine_.versions.previewing() && !(engine_.window && engine_.window->routingInput()))
		fail(where, "while a proposal is previewed, control steps run only from the user's input");
}

} // namespace affresco::format
