/// @file
/// @brief Fuzzes partial updates in verify mode: any screen, and any series of its actions run with any event values,
/// must give after every update the panels a full pass gives (format::Verify), or the run aborts.
///
/// The input is a JSON object: "screen", a screen document, and "steps", a list of [action name, event value] pairs
/// (at most 64), run one update each, except that a step whose third element is true joins the next one's update. A
/// screen that loads is built into a headless window with AFFRESCOUI_VERIFY_UPDATES=1. Errors are DocumentErrors,
/// reported through onError; an "update check" error is a finding.

#include "Fuzz.hpp"

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	::setenv("AFFRESCOUI_VERIFY_UPDATES", "1", 1); // every screen made checks its updates
	return 0;
}

namespace {

/// @brief Runs one step, [name, event, joins?]: the action, with its event (a refusal is a document's answer).
/// @param screen The screen.
/// @param window Its window.
/// @param step The step.
/// @return Whether the window updates after it (no if it joins the next step's update).
bool runStep(affresco::Screen& screen, affresco::Window& window, const affresco::json& step) {
	if(!step.is_array() || step.empty() || !step[0].is_string()) return true;
	const std::string event = step.size() > 1 ? step[1].dump() : std::string("{}");
	try {
		screen.runAction(step[0].get<std::string>(), window.root(), event);
	} catch(const affresco::DocumentError&) {
		return true; // refused, with a message that names the place: the update still runs
	}
	return step.size() <= 2 || step[2] != true;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const affresco::json input = affresco::fuzz::untrusted(affresco::fuzz::text(data, size));
	if(!input.is_object() || !input.contains("screen") || !input["screen"].is_object()) return 0;
	const affresco::json steps = input.value("steps", affresco::json::array());
	if(!steps.is_array()) return 0;

	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(640, 480, clock);
	std::optional<affresco::Screen> screen;
	[[maybe_unused]] affresco::Connection listening; // kept for the run
	try {
		screen.emplace(affresco::Screen::fromString(input["screen"].dump()));
		listening = screen->onError([](const std::string& message) {
			if(message.contains("update check")) affresco::fuzz::broken("a partial update gives what a full one gives");
		});
		screen->build(*window);
		window->render();
	} catch(const affresco::DocumentError&) {
		return 0; // refused, with a message that names the place
	}
	std::size_t ran = 0;
	for(const affresco::json& step : steps) {
		if(++ran > 64) break;
		if(!runStep(*screen, *window, step)) continue; // joins the next step's update
		window->render();
	}
	window->render();
	return 0;
}
