/// @file
/// @brief The "ask" step: a request in plain words from the screen's own person to the agents connected to it; and
/// "cancelAsk", which stops one.

#include "Engine.hpp"

namespace affresco::format {

namespace {

constexpr std::size_t maxPicks = 4;   ///< Picks a request carries at most.
constexpr std::size_t maxPickId = 64; ///< The longest pick id a step may name, in bytes.

/// @brief Reads an "ask" step's "attachments": ids, or what an attach step wrote (objects with an "id").
/// @param s The step.
/// @return The attachments' metadata, never their bytes.
json attachmentsOf(const Running& s) {
	json attachments = json::array();
	if(!s.step.contains("attachments")) return attachments;
	const json given = s.value("attachments");
	if(!given.is_null() && !given.is_array()) fail(s.where + ".attachments", "expected a list of attachments");
	for(const json& item : given.is_array() ? given : json::array()) {
		const json& id = item.is_object() ? item.value("id", json()) : item;
		if(!id.is_string()) fail(s.where + ".attachments", "expected attachment ids");
		const auto found = s.steps.engine().versions.attachment(id.get<std::string>());
		if(!found) fail(s.where + ".attachments", "no attachment '" + id.get<std::string>().substr(0, 80) + "'");
		if(attachments.size() < maxAttachments) attachments.push_back(toJson(*found));
	}
	return attachments;
}

/// @brief Reads an "ask" step's "picks": ids, or what a pick step wrote (objects with an "id"), each one the engine
/// holds (Picks): the engine's records go with the request, never what the document wrote.
/// @param s The step.
/// @return The ids.
std::vector<std::string> pickIdsOf(const Running& s) {
	std::vector<std::string> ids;
	if(!s.step.contains("picks")) return ids;
	const json given = s.value("picks");
	if(!given.is_null() && !given.is_array()) fail(s.where + ".picks", "expected a list of picks");
	if(given.size() > maxPicks)
		fail(s.where + ".picks", "a request carries at most " + std::to_string(maxPicks) + " picks");
	for(const json& item : given.is_array() ? given : json::array()) {
		const json& id = item.is_object() ? item.value("id", json()) : item;
		if(!id.is_string() || id.get_ref<const std::string&>().size() > maxPickId)
			fail(s.where + ".picks", "expected pick ids, each at most " + std::to_string(maxPickId) + " bytes");
		if(!s.steps.engine().picks.holds(id.get_ref<const std::string&>()))
			fail(s.where + ".picks", "no pick '" + id.get<std::string>() + "'");
		ids.push_back(id.get<std::string>());
	}
	return ids;
}

/// @brief Runs an "ask" step: records the request, writes its id "into" a state path, and sends it after the event,
/// with its "options" (an object the agents read, e.g. `{"model": ...}`), "attachments" and "picks" if it has them.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runAskStep(const Running& s) {
	if(!s.step.contains("ask")) return std::nullopt;
	s.steps.checkControl(s.where); // a revision can't make the screen speak for the person
	Engine& engine = s.steps.engine();
	const json options = s.step.contains("options") ? s.value("options") : json();
	const json attachments = attachmentsOf(s);
	const std::vector<std::string> picks = pickIdsOf(s);
	const std::string id =
	    engine.asks.send(string(s.value("ask"), s.where + ".ask"), s.where, options, attachments, engine.picks.records(picks));
	for(const std::string& pick : picks) engine.picks.remove(pick); // sent once: the request has them now
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
	              {{"ask", {"into", "options", "attachments", "picks"}, true}, {"cancelAsk", {}, true}},
	              [](const Running& s) {
		              const std::optional<bool> asked = runAskStep(s);
		              return asked ? asked : runCancelAskStep(s);
	              }});
}

} // namespace affresco::format
