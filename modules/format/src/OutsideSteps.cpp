/// @file
/// @brief The steps that reach the application, peers and the document: call, cancel, emit, send, connect, patch.

#include "Engine.hpp"

#include <algorithm>
#include <array>

namespace affresco::format {

namespace {

/// @brief "call": a native function (its result "into" a state path; "onError" if it throws) or a native action.
/// @param s The step.
/// @return Whether to keep going.
bool call(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;
	Engine& engine = s.steps.engine();
	const std::string name = string(step["call"], where + ".call");
	if(const auto slow = engine.asyncCalls.find(name); slow != engine.asyncCalls.end())
		return engine.calls.start(s, name, slow->second);
	if(const auto native = engine.nativeCalls.find(name); native != engine.nativeCalls.end()) {
		const json arguments = step.contains("with") ? s.value("with") : json::object();
		try {
			json result = native->second(arguments);
			if(step.contains("into")) {
				const json::json_pointer pointer = s.target("into");
				expect::shallowEnough(result, pointerDepth(pointer), where + ".into");
				statePlace(engine.versions.state(), pointer, where + ".into") = std::move(result);
				engine.document.markStateChanged(pointer);
			}
		} catch(const DocumentError&) {
			throw;
		} catch(const std::exception& error) {
			if(!step.contains("onError")) fail(where + ".call", name + ": " + error.what());
			const Scope scope{&s.locals, nullptr, {{"error", error.what()}}, {}, {}};
			return s.run(step["onError"], scope, where + ".onError");
		}
	} else if(const auto it = engine.nativeActions.find(name); it != engine.nativeActions.end()) {
		it->second(s.sender);
	} else {
		fail(where + ".call", "no native action '" + name + "' registered");
	}
	return true;
}

/// @brief "patch": a revision of the document by the app itself, committed after the event.
/// @param s The step.
void patch(const Running& s) {
	Engine& engine = s.steps.engine();
	json revision = {{"author", "app"},
	                 {"message", s.step.contains("message") ? s.value("message") : json("")},
	                 {"patch", s.value("patch")}};
	if(!engine.window) return;
	engine.scheduler.defer([&engine, revision = std::move(revision)] {
		if(const auto error = engine.versions.commit(revision)) engine.report(*error);
	});
}

/// @brief Runs a step that reaches outside the screen.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runOutsideStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;
	if(std::ranges::any_of(std::array{"call", "send", "connect", "patch"},
	                       [&step](const char* key) { return step.contains(key); }))
		s.steps.checkControl(where);
	Runtime& owner = s.steps.engine().owner;

	if(step.contains("call")) return call(s);
	if(step.contains("cancel")) {
		s.steps.engine().calls.cancel(string(step["cancel"], where + ".cancel"));
		return true;
	}
	if(step.contains("emit")) {
		const json message = {{"type", "event"},
		                      {"name", string(step["emit"], where + ".emit")},
		                      {"data", s.value(step.value("data", json()), where + ".data")},
		                      {"revision", s.steps.engine().versions.head().number}};
		owner.onEvent(message);
	} else if(step.contains("send")) {
		owner.onSend(string(step["send"], where + ".send"),
		             s.value(step.value("data", json::object()), where + ".data"));
	} else if(step.contains("connect")) {
		owner.onConnect(string(step["connect"], where + ".connect"));
	} else if(step.contains("patch")) {
		patch(s); // applied after the event, like any other document change
	} else {
		return std::nullopt;
	}
	return true;
}

} // namespace

void registerOutsideSteps(StepRegistry& registry) {
	registry.add({"outside",
	              {{"call", {"with", "into", "onError", "then", "loading", "progress", "timeout"}, true},
	               {"cancel", {}},
	               {"emit", {"data"}},
	               {"send", {"data"}, true},
	               {"connect", {}, true},
	               {"patch", {"message"}, true}},
	              runOutsideStep});
}

} // namespace affresco::format
