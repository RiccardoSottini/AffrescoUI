/// @file
/// @brief The pick steps: "pick", which lets the person point at an element and gives back the engine's record of
/// it; and "unpick", which forgets records. A record says where something is written, never what the app's data is.

#include "Engine.hpp"
#include "Picks.hpp"

#include "affresco/render/Window.hpp"

namespace affresco::format {

namespace {

constexpr std::size_t maxHint = 200; ///< The longest hint, in bytes (untrusted input).

/// @brief The options of a pick: the panels a node built, captioned with their type, id and definition.
/// @param engine The runtime; outlives the pick (Runtime's destructor cancels it).
/// @param hint The words shown at the top.
/// @return The options.
PickOptions pickOptions(Engine& engine, std::string hint) {
	PickOptions options;
	options.hint = std::move(hint);
	options.candidate = [&engine](Panel* panel) { return pickCandidate(engine, panel); };
	options.caption = [&engine](const Panel& panel) { return pickCaption(engine, panel); };
	return options;
}

/// @brief What a pick does once the person picked a panel: makes the record, keeps it, writes it "into" a state path.
/// @param engine The runtime.
/// @param panel The panel picked.
/// @param into The state path, or std::nullopt.
/// @param where The step, for errors.
/// @return The record as kept, or null if no node built the panel.
json keepPick(Engine& engine, const Panel& panel, const std::optional<json::json_pointer>& into,
              const std::string& where) {
	json record = describePick(engine, panel);
	if(record.is_null()) return record;
	json kept = engine.picks.add(std::move(record));
	if(into) {
		try {
			statePlace(engine.versions.state(), *into, where + ".into") = kept;
			engine.document.markStateChanged(*into);
		} catch(const DocumentError& error) {
			engine.report(std::string("AffrescoUI screen: ") + error.what());
		}
	}
	return kept;
}

/// @brief "pick": after the person's own press, the window lets them point at a panel (Window::pick()). A pick keeps
/// the engine's record (Picks), writes it "into" a state path and runs "onDone" with `result`; a cancel runs
/// "onCancel". Without a window (a check, a preview) nothing happens.
/// @param s The step.
/// @return true.
bool pick(const Running& s) {
	if(s.value("pick") != "node") fail(s.where + ".pick", R"(expected "node")");
	std::string hint = s.step.contains("hint") ? string(s.value("hint"), s.where + ".hint") : std::string();
	if(hint.size() > maxHint) fail(s.where + ".hint", "is longer than " + std::to_string(maxHint) + " bytes");
	const std::optional<json::json_pointer> into =
	    s.step.contains("into") ? std::optional<json::json_pointer>(s.target("into")) : std::nullopt;
	s.steps.checkControl(s.where); // a revision can't make the person point for it
	Engine& engine = s.steps.engine();
	if(!engine.window) return true; // a check or a preview: nothing to point at
	// Only the person starts one: a timer, a reply or a peer's message can't put the window in pick mode
	if(!engine.window->routingOwnPress()) fail(s.where + ".pick", "a pick starts from the person's own input");

	const SharedScope locals = snapshot(s.locals);
	const auto outcome = std::make_shared<const Scope>(Scope{locals.get(), locals, json::object(), {}, {}});
	auto picked = s.steps.later(s.step.value("onDone", json::array()), s.sender, s.event, outcome, s.where + ".onDone");
	auto cancelled =
	    s.steps.later(s.step.value("onCancel", json::array()), s.sender, s.event, outcome, s.where + ".onCancel");
	// NOLINTNEXTLINE(bugprone-exception-escape): past its catch, only a report's text throws, if memory runs out
	auto done = [&engine, alive = std::weak_ptr<bool>(engine.alive), into, where = s.where, picked = std::move(picked),
	             cancelled = std::move(cancelled), outcome](Panel* panel) {
		if(!alive.lock()) return;
		engine.picks.ended();
		try {
			json kept = panel ? keepPick(engine, *panel, into, where) : json();
			if(kept.is_null()) {
				cancelled();
				return;
			}
			outcome->vars = json{{"result", std::move(kept)}};
			picked();
		} catch(const std::exception& error) { // the window's callback must not throw
			engine.report(std::string("AffrescoUI screen: ") + where + ": " + error.what());
		}
	};
	const std::uint64_t id = engine.window->pick(pickOptions(engine, std::move(hint)), std::move(done));
	if(id == 0) fail(s.where + ".pick", "a pick is already in progress");
	engine.picks.started(id);
	return true;
}

/// @brief "unpick": forgets the records it names (an id, or a list of them); an unknown id is ignored.
/// @param s The step.
/// @return true.
bool unpick(const Running& s) {
	s.steps.checkControl(s.where); // what the person pointed at is theirs to drop
	const json given = s.value("unpick");
	if(!given.is_string() && !given.is_array()) fail(s.where + ".unpick", "expected a pick's id, or a list of them");
	for(const json& id : given.is_array() ? given : json::array({given})) {
		if(!id.is_string()) fail(s.where + ".unpick", "expected pick ids");
		s.steps.engine().picks.remove(id.get_ref<const std::string&>());
	}
	return true;
}

/// @brief Runs a pick step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runPickStep(const Running& s) {
	if(s.step.contains("pick")) return pick(s);
	if(s.step.contains("unpick")) return unpick(s);
	return std::nullopt;
}

} // namespace

void registerPickSteps(StepRegistry& registry) {
	registry.add({"pick", {{"pick", {"hint", "into", "onDone", "onCancel"}, true}, {"unpick", {}, true}}, runPickStep});
}

} // namespace affresco::format
