/// @file
/// @brief The "ask" step: a request in plain words from the screen's own person to the agents connected to it; and
/// "cancelAsk", which stops one.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief Runs an "ask" step: records the request, writes its id "into" a state path, and sends it after the event,
/// with its "options" (an object the agents read, e.g. `{"model": ...}`) if it has them.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runAskStep(const Running& s) {
	if(!s.step.contains("ask")) return std::nullopt;
	s.steps.checkControl(s.where); // a revision can't make the screen speak for the person
	Engine& engine = s.steps.engine();
	const json options = s.step.contains("options") ? s.value("options") : json();
	// "attachments": ids (or what an attach step wrote, objects with an "id"): agents get their metadata only
	json attachments = json::array();
	if(s.step.contains("attachments")) {
		const json given = s.value("attachments");
		if(!given.is_null() && !given.is_array()) fail(s.where + ".attachments", "expected a list of attachments");
		for(const json& item : given.is_array() ? given : json::array()) {
			const json& id = item.is_object() ? item.value("id", json()) : item;
			if(!id.is_string()) fail(s.where + ".attachments", "expected attachment ids");
			const auto found = engine.versions.attachment(id.get<std::string>());
			if(!found) fail(s.where + ".attachments", "no attachment '" + id.get<std::string>().substr(0, 80) + "'");
			if(attachments.size() < maxAttachments) attachments.push_back(toJson(*found));
		}
	}
	const std::string id = engine.asks.send(string(s.value("ask"), s.where + ".ask"), s.where, options, attachments);
	if(s.step.contains("into")) {
		const json::json_pointer pointer = s.target("into");
		statePlace(engine.versions.state(), pointer, s.where + ".into") = id;
		engine.document.markStateChanged(pointer);
	}
	return true;
}

/// @brief Runs a "cancelAsk" step: cancels the requests it names that are still in progress (Asks::cancel()).
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runCancelAskStep(const Running& s) {
	if(!s.step.contains("cancelAsk")) return std::nullopt;
	s.steps.checkControl(s.where); // a revision can't speak for the person
	const json given = s.value("cancelAsk");
	if(!given.is_string() && !given.is_array())
		fail(s.where + ".cancelAsk", "expected a request's id, or a list of them");
	for(const json& id : given.is_array() ? given : json::array({given})) {
		if(!id.is_string()) fail(s.where + ".cancelAsk", "expected request ids");
		(void)s.steps.engine().asks.cancel(id.get<std::string>()); // one already finished stays as it is
	}
	return true;
}

} // namespace

void registerAssistantSteps(StepRegistry& registry) {
	registry.add({"assistant",
	              {{"ask", {"into", "options", "attachments"}, true}, {"cancelAsk", {}, true}},
	              [](const Running& s) {
		              const std::optional<bool> asked = runAskStep(s);
		              return asked ? asked : runCancelAskStep(s);
	              }});
}

} // namespace affresco::format
