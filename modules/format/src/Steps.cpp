#include "RuntimeImpl.hpp"

namespace affresco::format {

/* Behaviour */

void Runtime::Impl::report(const std::string& message) {
	log(LogLevel::Error, message);
	owner.onError(message);
}

// Runs an event handler: an action name or inline steps. Errors are reported, never thrown into the event loop.
void Runtime::Impl::handle(const json& handler, Panel& sender, const json& event, const Scope& locals,
                           const std::string& where) {
	try {
		if(handler.is_string()) runNamed(handler.get<std::string>(), sender, event, locals, where);
		else runSteps(handler, sender, event, locals, where);
	} catch(const DocumentError& error) {
		report(error.what());
	}
	if(stateChanged) requestReconcile();
}

std::function<void()> Runtime::Impl::later(json steps, Panel& sender, json event, SharedScope locals, std::string where) {
	return [this, steps = std::move(steps), sender = &sender, event = std::move(event), locals = std::move(locals),
	        where = std::move(where)] {
		if(!window) return;
		Panel& from = records.contains(sender) ? *sender : window->root();
		handle(steps, from, event, *locals, where);
	};
}

void Runtime::Impl::setOnceTimer(double delayMs, std::function<void()> task) {
	const auto id = std::make_shared<std::uint64_t>(); // known once set, before it can run
	*id = window->setTimer(delayMs, [this, id, task = std::move(task)] {
		onceTimers.erase(*id);
		task();
	});
	onceTimers.insert(*id);
}

void Runtime::Impl::defer(std::function<void()> task) {
	window->defer([alive = alive, task = std::move(task)] {
		if(*alive) task();
	});
}

void Runtime::Impl::runNamed(const std::string& name, Panel& sender, const json& event, const Scope& locals,
                             const std::string& where) {
	const json& document = versions.document();
	if(document.contains("actions") && document["actions"].contains(name)) {
		const json steps = document["actions"][name]; // a copy: the steps may revise the document
		runSteps(steps, sender, event, locals, "actions." + name);
	} else if(const auto it = nativeActions.find(name); it != nativeActions.end()) {
		it->second(sender);
	} else if(unhandled) {
		unhandled(name, sender);
	} else {
		fail(where, "unknown action '" + name + "'");
	}
}

// Returns false when a "stop" step ends the action
bool Runtime::Impl::runSteps(const json& steps, Panel& sender, const json& event, const Scope& locals,
                             const std::string& where) {
	if(++actionDepth > maxActionDepth) {
		--actionDepth;
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
		--actionDepth;
		throw;
	}

	--actionDepth;
	return keepGoing;
}

json Runtime::Impl::Running::value(const json& v, const std::string& at) const {
	return format::evaluate(v, impl.context(locals, event), at);
}

json::json_pointer Runtime::Impl::Running::target(const char* key) const {
	return format::statePointer(string(step[key], where + "." + key), impl.context(locals, event), where + "." + key);
}

json Runtime::Impl::Running::list(const json& source, const std::string& at) const {
	if(source.is_string() && source.get<std::string>().find("{{") == std::string::npos)
		return format::lookup(source.get<std::string>(), impl.context(locals, event), at);
	return value(source, at);
}

double Runtime::Impl::Running::milliseconds(const char* key) const {
	return std::max(static_cast<double>(number(value(key), where + "." + key)), 0.0);
}

bool Runtime::Impl::runStep(const json& step, Panel& sender, const json& event, const Scope& locals,
                            const std::string& where) {
	object(step, where);
	const Running running{*this, step, sender, event, locals, where};
	for(auto group : {&Impl::stateStep, &Impl::flowStep, &Impl::outsideStep, &Impl::reviewStep, &Impl::navigationStep,
	                  &Impl::timeStep, &Impl::otherStep})
		if(const std::optional<bool> keepGoing = (this->*group)(running)) return *keepGoing;

	fail(where, "unknown step (expected set, push, remove, toggle, increment, if, forEach, while, run, call, emit, "
	            "send, connect, patch, focus, scrollTo, navigate, after, debounce, animate, copy, log, stop, note, "
	            "accept, reject, preview, endPreview, restore, revert or undo)");
}

std::optional<bool> Runtime::Impl::stateStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;
	json& state = versions.state();

	if(step.contains("set")) {
		if(!step.contains("to")) fail(where, "\"set\" needs \"to\"");
		state[s.target("set")] = s.value("to");
	} else if(step.contains("push")) {
		json& array = state[s.target("push")];
		if(array.is_null()) array = json::array();
		if(!array.is_array()) fail(where + ".push", "is not a list");
		array.push_back(s.value(step.value("value", json()), where + ".value"));
	} else if(step.contains("remove")) {
		const json::json_pointer pointer = s.target("remove");
		if(step.contains("at")) {
			json& array = state[pointer];
			const json at = s.value("at");
			if(!array.is_array() || !at.is_number_integer() || at.get<std::int64_t>() < 0 ||
			   static_cast<std::size_t>(at.get<std::int64_t>()) >= array.size())
				fail(where, "nothing to remove at " + at.dump());
			array.erase(static_cast<std::size_t>(at.get<std::int64_t>()));
		} else if(state.contains(pointer)) {
			state[pointer.parent_pointer()].erase(pointer.back());
		}
	} else if(step.contains("toggle")) {
		const json::json_pointer pointer = s.target("toggle");
		state[pointer] = !format::truthy(state.value(pointer, json()));
	} else if(step.contains("increment")) {
		const json::json_pointer pointer = s.target("increment");
		const json by = s.value(step.value("by", json(1)), where + ".by");
		json current = state.value(pointer, json(0));
		if(current.is_null()) current = 0;
		if(!current.is_number() || !by.is_number()) fail(where, "\"increment\" needs numbers");
		state[pointer] = current.is_number_integer() && by.is_number_integer()
		                     ? json(current.get<std::int64_t>() + by.get<std::int64_t>())
		                     : json(current.get<double>() + by.get<double>());
	} else {
		return std::nullopt;
	}
	changed();
	return true;
}

std::optional<bool> Runtime::Impl::flowStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;

	if(step.contains("if")) {
		const char* branch = format::truthy(s.value("if")) ? "then" : "else";
		return step.contains(branch) ? s.run(step[branch], s.locals, where + "." + branch) : true;
	}
	if(step.contains("forEach")) {
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
	if(step.contains("while")) {
		std::size_t iterations = 0;
		while(format::truthy(s.value("while"))) {
			if(++iterations > maxLoopIterations) fail(where, "\"while\" ran more than 10000 times");
			if(!s.run(step.value("do", json::array()), s.locals, where + ".do")) return false;
		}
		return true;
	}
	if(step.contains("run")) {
		// "with": the action's own variables, e.g. {"run": "moveCard", "with": {"to": "{{c + 1}}"}}
		Scope scope{&s.locals, nullptr, json::object()};
		if(step.contains("with"))
			for(const auto& [name, v] : object(step["with"], where + ".with").items())
				scope.vars[name] = s.value(v, where + ".with." + name);
		runNamed(text(s.value("run")), s.sender, s.event, scope, where + ".run");
		return true;
	}
	return std::nullopt;
}

std::optional<bool> Runtime::Impl::outsideStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;

	if(step.contains("call")) {
		const std::string name = string(step["call"], where + ".call");
		if(const auto call = nativeCalls.find(name); call != nativeCalls.end()) {
			const json arguments = step.contains("with") ? s.value("with") : json::object();
			try {
				json result = call->second(arguments);
				if(step.contains("into")) {
					versions.state()[s.target("into")] = std::move(result);
					changed();
				}
			} catch(const DocumentError&) {
				throw;
			} catch(const std::exception& error) {
				if(!step.contains("onError")) fail(where + ".call", name + ": " + error.what());
				const Scope scope{&s.locals, nullptr, {{"error", error.what()}}};
				return s.run(step["onError"], scope, where + ".onError");
			}
		} else if(const auto it = nativeActions.find(name); it != nativeActions.end()) {
			it->second(s.sender);
		} else {
			fail(where + ".call", "no native action '" + name + "' registered");
		}
	} else if(step.contains("emit")) {
		const json message = {{"type", "event"},
		                      {"name", string(step["emit"], where + ".emit")},
		                      {"data", s.value(step.value("data", json()), where + ".data")},
		                      {"revision", versions.head().number}};
		owner.onEvent(message);
	} else if(step.contains("send")) {
		owner.onSend(string(step["send"], where + ".send"), s.value(step.value("data", json::object()), where + ".data"));
	} else if(step.contains("connect")) {
		owner.onConnect(string(step["connect"], where + ".connect"));
	} else if(step.contains("patch")) {
		// Applied after the event, like any other document change
		json revision = {{"author", "app"},
		                 {"message", step.contains("message") ? s.value("message") : json("")},
		                 {"patch", s.value("patch")}};
		if(window) {
			defer([this, revision = std::move(revision)] {
				if(const auto error = versions.commit(revision)) report(*error);
			});
		}
	} else {
		return std::nullopt;
	}
	return true;
}

// Review and history controls change the document: done after the event, like revisions
std::optional<bool> Runtime::Impl::reviewStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;
	if(!step.contains("accept") && !step.contains("reject") && !step.contains("preview") && !step.contains("restore") &&
	   !step.contains("revert") && !step.contains("undo") && !step.contains("endPreview"))
		return std::nullopt;

	json control = step;
	for(const char* key : {"accept", "head", "reject", "preview", "restore", "revert", "reason"})
		if(control.contains(key)) control[key] = s.value(control[key], where + "." + key);
	for(const char* key : {"accept", "reject", "preview"})
		if(control.contains(key) && !control[key].is_number_integer()) fail(where + "." + key, "expected a number");
	for(const char* key : {"restore", "revert"})
		if(control.contains(key) && !control[key].is_number_integer() && !control[key].is_string())
			fail(where + "." + key, "expected a revision number or a commit");
	if(control.contains("accept") && !control.value("head", json()).is_string())
		fail(where + ".head", "\"accept\" needs the proposal's head commit, the version the user reviewed, e.g. "
		                      "\"{{proposals[0].head}}\"");
	if(!window) return true;

	defer([this, control = std::move(control)] {
		std::optional<std::string> error;
		try {
			if(control.contains("accept"))
				error =
				    versions.accept(control["accept"].get<int>(), {"user", control["head"].get<std::string>(), false});
			else if(control.contains("reject"))
				versions.reject(control["reject"].get<int>(), text(control.value("reason", json(""))));
			else if(control.contains("preview")) error = versions.preview(control["preview"].get<int>());
			else if(control.contains("endPreview")) versions.endPreview();
			else if(control.contains("restore"))
				error = versions.commit(versions.restoreRevision(control["restore"], "user"));
			else if(control.contains("revert")) error = versions.commit(versions.revertRevision(control["revert"], "user"));
			else error = versions.commit(versions.revertRevision("HEAD", "user")); // undo
		} catch(const DocumentError& failure) {
			error = failure.what();
		}
		if(error) report(*error);
		requestReconcile();
	});
	return true;
}

std::optional<bool> Runtime::Impl::navigationStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;

	if(step.contains("focus") || step.contains("scrollTo")) {
		const bool focus = step.contains("focus");
		std::string id = text(s.value(step[focus ? "focus" : "scrollTo"], where));
		if(id.starts_with('#')) id.erase(0, 1);
		if(window) {
			// After the update and its layout, so the panel exists and has its place
			setOnceTimer(0.0, [this, id, focus] {
				if(Panel* panel = window->root().findById(id)) {
					if(focus) window->setFocus(panel);
					else panel->scrollIntoView();
				}
			});
		}
		return true;
	}
	if(step.contains("navigate")) {
		const std::string name = text(s.value("navigate"));
		if(name == "back") {
			if(screenHistory.empty()) return true;
			screen = screenHistory.back();
			screenHistory.pop_back();
		} else {
			const json& document = versions.document();
			if(!document.contains("screens") || !document["screens"].contains(name))
				fail(where + ".navigate", "unknown screen '" + name + "'");
			if(name != screen) screenHistory.push_back(screen);
			screen = name;
		}
		globalsChanged();
		changed();
		return true;
	}
	return std::nullopt;
}

std::optional<bool> Runtime::Impl::timeStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;

	if(step.contains("after") || step.contains("debounce")) {
		// Later: "after" always, "debounce" only if it isn't run again in the meantime
		const bool debounce = step.contains("debounce");
		const double ms = s.milliseconds(debounce ? "debounce" : "after");
		if(!step.contains("do")) fail(where, "needs \"do\": the steps to run");
		if(!window) return true;
		auto task = later(step["do"], s.sender, s.event, snapshot(s.locals), where + ".do");
		if(debounce) {
			const std::string key = where + "@" + std::to_string(reinterpret_cast<std::uintptr_t>(&s.sender));
			if(const auto it = debounces.find(key); it != debounces.end()) window->cancelTimer(it->second);
			debounces[key] = window->setTimer(ms, [this, key, task] {
				debounces.erase(key);
				task();
			});
		} else {
			setOnceTimer(ms, std::move(task));
		}
		return true;
	}
	if(step.contains("animate")) {
		// A number in the state moves to "to" over "ms", one frame at a time
		const json::json_pointer pointer = s.target("animate");
		const json to = s.value(step.value("to", json()), where + ".to");
		if(!to.is_number()) fail(where + ".to", "expected a number");
		const double ms = step.contains("ms") ? s.milliseconds("ms") : 300.0;
		const std::string easing = step.contains("easing") ? string(step["easing"], where + ".easing") : "out";
		const json current = versions.state().value(pointer, json(0));
		const double from = current.is_number() ? current.get<double>() : 0.0;
		if(!window || ms <= 0.0) {
			versions.state()[pointer] = to;
			changed();
			return true;
		}
		const std::string key = pointer.to_string();
		if(const auto it = animations.find(key); it != animations.end()) window->cancelTimer(it->second);
		// The timer owns the task, which holds itself weakly: a strong reference to itself would never be freed
		auto frame = std::make_shared<std::function<void()>>();
		*frame = [this, pointer, key, from, to, ms, easing, start = now(), weak = std::weak_ptr(frame)] {
			const double progress = std::min((now() - start) * 1000.0 / ms, 1.0);
			versions.state()[pointer] = progress >= 1.0 ? to : json(from + (to.get<double>() - from) * ease(easing, progress));
			changed();
			requestReconcile();
			if(progress < 1.0) animations[key] = window->setTimer(frameMs, [self = weak.lock()] { (*self)(); });
			else animations.erase(key);
		};
		animations[key] = window->setTimer(frameMs, [frame] { (*frame)(); });
		return true;
	}
	return std::nullopt;
}

std::optional<bool> Runtime::Impl::otherStep(const Running& s) {
	const json& step = s.step;
	if(step.contains("copy")) {
		if(window) window->setClipboardText(text(s.value("copy")));
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

} // namespace affresco::format
