/// @file
/// @brief The sign-in steps: signIn and signOut, for the connections whose "auth" declares a provider. They run the
/// host's Identity (net::SignIn); the tokens stay in it and never reach the document, the state or a step.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief "signIn": signs in to a connection's provider, then runs "onDone", or "onError" with the error.
/// @param s The step.
void signIn(const Running& s) {
	const std::string name = string(s.value("signIn"), s.where + ".signIn");
	s.steps.checkControl(s.where);
	Engine& engine = s.steps.engine();
	if(!engine.identity) fail(s.where, "this host has no sign-in");
	const json onDone = s.step.value("onDone", json()), onError = s.step.value("onError", json());
	auto succeeded = s.steps.later(onDone, s.sender, s.event, snapshot(s.locals), s.where + ".onDone");
	const SharedScope locals = snapshot(s.locals);
	const std::string where = s.where + ".onError";
	engine.identity->signIn(name, [&engine, alive = std::weak_ptr<bool>(engine.alive), succeeded = std::move(succeeded),
	                               done = !onDone.is_null(), onError, name, sender = &s.sender, event = s.event,
	                               locals, where](std::optional<std::string> error) {
		if(!alive.lock()) return; // the screen went away while signing in
		if(!error) {
			if(done) succeeded();
			return;
		}
		engine.report("AffrescoUI screen: sign-in to '" + name + "': " + *error);
		if(onError.is_null()) return;
		const auto scope = std::make_shared<const Scope>(
		    Scope{locals.get(), locals, json{{"error", {{"code", "signIn"}, {"message", *error}}}}, {}, {}});
		engine.runner.later(onError, *sender, event, scope, where)();
	});
}

/// @brief Runs a sign-in step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runAuthStep(const Running& s) {
	if(s.step.contains("signIn")) {
		signIn(s);
		return true;
	}
	if(!s.step.contains("signOut")) return std::nullopt;
	const std::string name = string(s.value("signOut"), s.where + ".signOut");
	s.steps.checkControl(s.where);
	if(const Engine& engine = s.steps.engine(); engine.identity) engine.identity->signOut(name);
	return true;
}

} // namespace

void registerAuthSteps(StepRegistry& registry) {
	registry.add({"auth", {{"signIn", {"onDone", "onError"}, true}, {"signOut", {}, true}}, runAuthStep});
}

} // namespace affresco::format
