/// @file
/// @brief The other steps: copy, paste, log, stop, note.

#include "Engine.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Window.hpp"

namespace affresco::format {

namespace {

/// The most bytes of the clipboard's text "paste" reads.
constexpr std::size_t maxPastedBytes = 1U << 20;

/// @brief "paste": the clipboard's text, only from the person's own input (a timer, a reply or a peer's message can't
/// read what they copied), written "into" a state path, and given to "onDone" as `result`. Without a window (a check,
/// a preview) nothing happens.
/// @param s The step.
/// @return true.
bool paste(const Running& s) {
	if(s.value("paste") != "text") fail(s.where + ".paste", R"(expected "text")");
	const std::optional<json::json_pointer> into =
	    s.step.contains("into") ? std::optional<json::json_pointer>(s.target("into")) : std::nullopt;
	s.steps.checkControl(s.where); // a revision can't read the person's clipboard for it
	Engine& engine = s.steps.engine();
	if(!engine.window) return true;
	if(!engine.window->routingOwnPress())
		fail(s.where + ".paste", "reads the clipboard only on the person's own input");
	std::string pasted = engine.window->clipboardText();
	if(pasted.size() > maxPastedBytes) fail(s.where + ".paste", "the clipboard holds over 1 MiB of text");
	if(into) {
		statePlace(engine.versions.state(), *into, s.where + ".into") = pasted;
		engine.document.markStateChanged(*into);
	}
	if(s.step.contains("onDone")) {
		const SharedScope locals = snapshot(s.locals);
		const auto outcome =
		    std::make_shared<const Scope>(Scope{locals.get(), locals, json{{"result", std::move(pasted)}}, {}, {}});
		s.steps.later(s.step["onDone"], s.sender, s.event, outcome, s.where + ".onDone")();
	}
	return true;
}

/// @brief Runs copy (to the clipboard), paste (from it), log (a debug message), stop (ends the action) or note (a
/// comment).
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runOtherStep(const Running& s) {
	const json& step = s.step;
	if(step.contains("copy")) {
		if(Window* window = s.steps.engine().window) window->setClipboardText(text(s.value("copy")));
		return true;
	}
	if(step.contains("paste")) return paste(s);
	if(step.contains("log")) {
		log(LogLevel::Debug, "AffrescoUI screen log: " + text(s.value("log")));
		return true;
	}
	if(step.contains("stop")) return false;
	if(step.contains("note")) return true; // a comment
	return std::nullopt;
}

} // namespace

void registerOtherSteps(StepRegistry& registry) {
	registry.add({"other",
	              {{"copy", {}}, {"paste", {"into", "onDone"}, true}, {"log", {}}, {"stop", {}}, {"note", {}}},
	              runOtherStep});
}

} // namespace affresco::format
