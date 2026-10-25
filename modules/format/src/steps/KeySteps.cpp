/// @file
/// @brief The step that presses keys for the user, as a keyboard sends them: press.

#include "Engine.hpp"
#include "Shortcuts.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t maxTimes = 1000;    ///< The most times a press repeats.
constexpr std::size_t maxPresses = 10000; ///< The most presses one step sends, its chord times "times".

/// @brief "press": a chord ("ctrl+x ctrl+s"), each press sent to the panel as the keyboard would, "times" over.
/// Bindings don't hear these presses, so a binding can press the keys it stands for without running itself.
/// @param s The step.
void press(const Running& s) {
	const std::string& where = s.where;
	const std::string chord = normalizeChord(string(s.value("press"), where + ".press"));
	std::vector<KeyEvent> presses;
	for(std::size_t at = 0; at < chord.size();) {
		const std::size_t end = std::min(chord.find(' ', at), chord.size());
		const std::optional<KeyEvent> event = keyPress(std::string_view(chord).substr(at, end - at));
		if(!event) fail(where + ".press", "no key '" + chord.substr(at, end - at) + "'");
		presses.push_back(*event);
		at = end + 1;
	}
	std::size_t times = 1;
	if(s.step.contains("times")) {
		const json count = s.value("times");
		if(!count.is_number_integer() || count.get<long long>() < 1 ||
		   count.get<long long>() > static_cast<long long>(maxTimes))
			fail(where + ".times", "must be a whole number from 1 to " + std::to_string(maxTimes));
		times = count.get<std::size_t>();
	}
	if(presses.size() * times > maxPresses)
		fail(where + ".times", "sends more than " + std::to_string(maxPresses) + " presses");
	Panel* panel = stepTarget(s);
	if(!panel) return;
	Engine& engine = s.steps.engine();
	for(std::size_t n = 0; n < times; ++n)
		for(const KeyEvent& event : presses) engine.events.press(*panel, event, typedBy(event));
}

/// @brief Runs a key step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runKeyStep(const Running& s) {
	if(!s.step.contains("press")) return std::nullopt;
	press(s);
	return true;
}

} // namespace

Panel* stepTarget(const Running& s) {
	Engine& engine = s.steps.engine();
	if(!engine.window) return nullptr;
	if(!s.step.contains("in")) return engine.window->focusedPanel();
	std::string id = text(s.value("in"));
	if(id.starts_with('#')) id.erase(0, 1);
	return engine.window->root().findById(id);
}

void registerKeySteps(StepRegistry& registry) {
	registry.add({"keys", {{"press", {"in", "times"}}}, runKeyStep});
}

} // namespace affresco::format
