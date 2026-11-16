/// @file
/// @brief The review and history steps: accept, reject, preview, endPreview, restore, revert, undo; and undoPlan,
/// which reads what undoing a commit would do and changes nothing.

#include "Engine.hpp"

#include <algorithm>
#include <array>

namespace affresco::format {

namespace {

/// The keys of an undo's choice that a step carries: what "revert" and "undoPlan" read with undoOptionsFrom(). A
/// step's "then" is a list of steps, never the protocol's patch, and a step has no "message".
constexpr std::array undoChoiceKeys = {"drop", "keep", "head"};

/// @brief Reads the undo's choice a step carries, its values evaluated: only its "drop", "keep" and "head".
/// @param control The step, its values evaluated.
/// @param where Where it is, for errors ("actions.undo.steps[0]").
/// @return The options; empty without those keys.
/// @throws DocumentError Naming the key, if one is malformed.
UndoOptions undoChoice(const json& control, const std::string& where) {
	json choice = json::object();
	for(const char* key : undoChoiceKeys)
		if(control.contains(key)) choice[key] = control[key];
	return undoOptionsFrom(choice, where);
}

/// @brief Evaluates a review or history step's values, and checks their types.
/// @param s The step.
/// @return The step with its values evaluated.
/// @throws DocumentError If a value has the wrong type, or an accept names no head.
json evaluateControl(const Running& s) {
	const std::string& where = s.where;
	json control = s.step;
	for(const char* key : {"accept", "head", "reject", "preview", "restore", "revert", "reason", "drop", "keep"})
		if(control.contains(key)) control[key] = s.value(control[key], where + "." + key);
	for(const char* key : {"accept", "reject", "preview"})
		if(control.contains(key) && !control[key].is_number_integer()) fail(where + "." + key, "expected a number");
	for(const char* key : {"restore", "revert"})
		if(control.contains(key) && !control[key].is_number_integer() && !control[key].is_string())
			fail(where + "." + key, "expected a revision number or a commit");
	if(control.contains("accept") && !control.value("head", json()).is_string())
		fail(where + ".head", "\"accept\" needs the proposal's head commit, the version the user reviewed, e.g. "
		                      "\"{{proposals[0].head}}\"");
	if(control.contains("revert")) static_cast<void>(undoChoice(control, where)); // a malformed choice is the step's
	return control;
}

/// @brief Commits what the person's undo gives: when it is the person's input and only touches the program, as
/// evaluating did, so that what an evaluation added to what reaches a control step can be undone by the same hand.
/// @param versions The version control.
/// @param revision The revision.
/// @param ownInput Whether the person's own input started it.
/// @return An error, or std::nullopt.
std::optional<std::string> commitOwn(VersionControl& versions, const json& revision, bool ownInput) {
	const auto patch = revision.find("patch");
	const bool onlyProgram =
	    patch != revision.end() && patch->is_array() && std::ranges::all_of(*patch, [](const json& op) {
		    return op.value("path", std::string()).starts_with("/program/") && !op.contains("from");
	    });
	return ownInput && onlyProgram ? versions.commitOwnInput(revision) : versions.commit(revision);
}

/// @brief Carries out an evaluated review or history step on the version control.
/// @param versions The version control.
/// @param control The step, its values evaluated.
/// @param ownInput Whether the person's own input started it.
/// @return An error, or std::nullopt.
std::optional<std::string> carryOut(VersionControl& versions, const json& control, bool ownInput) {
	try {
		if(control.contains("accept"))
			return versions.accept(control["accept"].get<int>(), {"user", control["head"].get<std::string>(), false});
		if(control.contains("reject")) {
			versions.reject(control["reject"].get<int>(), text(control.value("reason", json(""))));
			return std::nullopt;
		}
		if(control.contains("preview")) return versions.preview(control["preview"].get<int>());
		if(control.contains("endPreview")) {
			versions.endPreview();
			return std::nullopt;
		}
		if(control.contains("restore"))
			return commitOwn(versions, versions.restoreRevision(control["restore"], "user"), ownInput);
		if(control.contains("revert")) {
			// Without a choice, exactly the plain undo it always was; with one, the undo that keeps later changes
			const UndoOptions options = undoChoice(control, "revert");
			return commitOwn(versions,
			                 options.empty() ? versions.revertRevision(control["revert"], "user")
			                                 : versions.undoRevision(control["revert"], "user", options),
			                 ownInput);
		}
		return commitOwn(versions, versions.revertRevision("HEAD", "user"), ownInput); // undo
	} catch(const DocumentError& failure) {
		return failure.what();
	}
}

/// @brief "undoPlan": what undoing a commit would do, worked out without changing anything, "into" a state path and
/// as "then"'s result; "onError" if the commit or the choice is wrong. Not a control step: it only reads.
/// @param s The step.
/// @return Whether to keep going.
bool undoPlanStep(const Running& s) {
	const json& step = s.step;
	const std::string& where = s.where;
	Engine& engine = s.steps.engine();
	json control = json::object();
	for(const char* key : undoChoiceKeys)
		if(step.contains(key)) control[key] = s.value(step[key], where + "." + key);
	const json revision = s.value(step["undoPlan"], where + ".undoPlan");
	json plan;
	try {
		if(!revision.is_number_integer() && !revision.is_string())
			fail(where + ".undoPlan", "expected a revision number or a commit");
		plan = toJson(engine.versions.undoPlan(revision, "user", undoChoice(control, where)));
	} catch(const DocumentError& error) {
		if(!step.contains("onError")) throw;
		const Scope scope{&s.locals, nullptr, {{"error", error.what()}}, {}, {}};
		return s.run(step["onError"], scope, where + ".onError");
	}
	if(step.contains("into")) {
		const json::json_pointer pointer = s.target("into");
		expect::shallowEnough(plan, pointerDepth(pointer), where + ".into");
		statePlace(engine.versions.state(), pointer, where + ".into") = plan;
		engine.document.markStateChanged(pointer);
	}
	if(step.contains("then")) {
		const Scope scope{&s.locals, nullptr, {{"result", std::move(plan)}}, {}, {}};
		return s.run(step["then"], scope, where + ".then");
	}
	return true;
}

/// @brief Runs a review or history step: they change the document, so they are done after the event, like revisions.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runReviewStep(const Running& s) {
	if(s.step.contains("undoPlan")) return undoPlanStep(s); // it only reads: from any handler, before checkControl
	if(std::ranges::none_of(std::array{"accept", "reject", "preview", "restore", "revert", "undo", "endPreview"},
	                        [&s](const char* key) { return s.step.contains(key); }))
		return std::nullopt;
	s.steps.checkControl(s.where);
	Engine& engine = s.steps.engine();
	if(engine.asks.deciding() && !s.step.contains("preview") && !s.step.contains("endPreview"))
		fail(s.where, "an agent's reply runs this action: a reply is never the person's decision");

	json control = evaluateControl(s);
	if(!engine.window) return true;

	engine.scheduler.defer([&engine, control = std::move(control), own = engine.window->routingOwnPress()] {
		if(const std::optional<std::string> error = carryOut(engine.versions, control, own)) engine.report(*error);
		engine.reconciler.requestReconcile();
	});
	return true;
}

} // namespace

void registerReviewSteps(StepRegistry& registry) {
	// Listed under "controlSteps" only, as the catalog always has; undoPlan, which only reads, under "steps"
	registry.add({"review",
	              {{"accept", {"head"}, true, false},
	               {"reject", {"reason"}, true, false},
	               {"preview", {}, true, false},
	               {"endPreview", {}, true, false},
	               {"restore", {}, true, false},
	               {"revert", {"drop", "keep", "head"}, true, false},
	               {"undo", {}, true, false},
	               {"undoPlan", {"into", "drop", "keep", "head", "then", "onError"}, false, true}},
	              runReviewStep});
}

} // namespace affresco::format
