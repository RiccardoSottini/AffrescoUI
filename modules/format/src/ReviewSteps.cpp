/// @file
/// @brief The review and history steps: accept, reject, preview, endPreview, restore, revert, undo.

#include "Engine.hpp"

#include <algorithm>
#include <array>

namespace affresco::format {

namespace {

/// @brief Evaluates a review or history step's values, and checks their types.
/// @param s The step.
/// @return The step with its values evaluated.
/// @throws DocumentError If a value has the wrong type, or an accept names no head.
json evaluateControl(const Running& s) {
	const std::string& where = s.where;
	json control = s.step;
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
	return control;
}

/// @brief Carries out an evaluated review or history step on the version control.
/// @param versions The version control.
/// @param control The step, its values evaluated.
/// @return An error, or std::nullopt.
std::optional<std::string> carryOut(VersionControl& versions, const json& control) {
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
		if(control.contains("restore")) return versions.commit(versions.restoreRevision(control["restore"], "user"));
		if(control.contains("revert")) return versions.commit(versions.revertRevision(control["revert"], "user"));
		return versions.commit(versions.revertRevision("HEAD", "user")); // undo
	} catch(const DocumentError& failure) {
		return failure.what();
	}
}

/// @brief Runs a review or history step: they change the document, so they are done after the event, like revisions.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runReviewStep(const Running& s) {
	if(std::ranges::none_of(std::array{"accept", "reject", "preview", "restore", "revert", "undo", "endPreview"},
	                        [&s](const char* key) { return s.step.contains(key); }))
		return std::nullopt;
	s.steps.checkControl(s.where);

	json control = evaluateControl(s);
	Engine& engine = s.steps.engine();
	if(!engine.window) return true;

	engine.scheduler.defer([&engine, control = std::move(control)] {
		if(const std::optional<std::string> error = carryOut(engine.versions, control)) engine.report(*error);
		engine.reconciler.requestReconcile();
	});
	return true;
}

} // namespace

void registerReviewSteps(StepRegistry& registry) {
	// Listed under "controlSteps" only, as the catalog always has
	registry.add({"review",
	              {{"accept", {"head"}, true, false},
	               {"reject", {"reason"}, true, false},
	               {"preview", {}, true, false},
	               {"endPreview", {}, true, false},
	               {"restore", {}, true, false},
	               {"revert", {}, true, false},
	               {"undo", {}, true, false}},
	              runReviewStep});
}

} // namespace affresco::format
