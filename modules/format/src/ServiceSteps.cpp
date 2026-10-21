/// @file
/// @brief The steps that talk to the screen's services (RFC 0002) and connections (RFC 0003): invoke, request,
/// subscribe, and respond to a service's request.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief Runs a service step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runServiceStep(const Running& s) {
	ServiceCalls& calls = s.steps.engine().serviceCalls;
	if(s.step.contains("invoke") || s.step.contains("request")) return calls.start(s, false);
	if(s.step.contains("subscribe")) return calls.start(s, true);
	if(s.step.contains("respond")) return calls.respond(s);
	return std::nullopt;
}

} // namespace

void registerServiceSteps(StepRegistry& registry) {
	registry.add({"services",
	              {{"invoke",
	                {"with", "into", "append", "keep", "onEvent", "loading", "progress", "timeout", "id", "onDone",
	                 "onError"}},
	               {"request",
	                {"with", "into", "append", "keep", "onEvent", "loading", "progress", "timeout", "id", "onDone",
	                 "onError"}},
	               {"subscribe", {"with", "do", "append", "keep", "into", "id", "onError"}},
	               {"respond", {"error"}, true}},
	              runServiceStep});
}

} // namespace affresco::format
