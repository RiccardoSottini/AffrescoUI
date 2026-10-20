/// @file
/// @brief automation::run: loads each scenario's screen fresh, picks a headless or a hidden OpenGL window, plays the
/// steps, and reports.

#include "affresco/automation/Scenarios.hpp"

#include "Headless.hpp"      // the render module's headless window and manual clock
#include "Player.hpp"
#include "ScratchFolder.hpp" // the screen module's

#include <affresco/Screen.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/render/Application.hpp>

#include <algorithm>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>

namespace affresco::automation {

namespace {

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its text.
/// @throws std::runtime_error If it can't be read.
std::string readText(const std::filesystem::path& path) {
	const std::ifstream file(path, std::ios::binary);
	if(!file) throw std::runtime_error("could not read " + path.string());
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

/// @brief A whole number of a screen's "window" section, or its default (Screen::createWindow's defaults: 800 x 600,
/// no minimum). `affrescoui check` reports a malformed section; a scenario only needs the numbers.
/// @param window The section.
/// @param key "width", "height", "minWidth" or "minHeight".
/// @param fallback Its default.
/// @return The number.
int sizeIn(const json& window, const char* key, int fallback) {
	return window.is_object() && window.contains(key) && window[key].is_number_integer() ? window[key].get<int>()
	                                                                                     : fallback;
}

/// @brief Where what a screen's channels post goes while a scenario plays: nowhere. Their sockets are looked for in a
/// folder with no host, so nothing connects, and an Application's posted tasks only run inside Application::run,
/// which a scenario never calls. Safe from any thread.
class NoPeers final : public Executor {
public:
	/// @brief Drops the task.
	void post(std::function<void()> /*task*/) override {}
};

/// @brief Loads a screen connected to nobody: its channels, its includes' too, are pointed at one socket in a folder
/// that is never created. A channel's own path can be absolute, and would then reach a running host's socket (its
/// studio.sock, a reviewer's) and send it whatever the screen's `open` handler says: a scenario can play on an
/// agent's proposal (`run_scenarios`), which must not get a reviewer's socket that way.
/// @param document The screen's document, as JSON text.
/// @param directory Where its relative paths ("include", fonts) are found.
/// @param nowhere The folder that is never created.
/// @return The screen.
Screen unconnectedScreen(const std::string& document, const std::filesystem::path& directory,
                         const std::filesystem::path& nowhere) {
	Screen screen = Screen::fromString(document, directory);
	json loaded = parseUntrusted(screen.documentJson()); // the includes in place
	if(loaded.contains("channels") && loaded["channels"].is_object()) {
		for(auto& [name, channel] : loaded["channels"].items())
			if(channel.is_object()) channel["socket"] = "none.sock";
		screen = Screen::fromString(loaded.dump(), directory);
	}
	screen.setSocketDirectory(nowhere);
	return screen;
}

/// @brief Plays one scenario on a screen loaded from the text of its document.
///
/// The screen plays alone: its channels look for their hosts in a folder where there are none (unconnectedScreen()),
/// so they show their "disconnected" state every time; a step sends what a channel would with an action named
/// `<channel>:<type>`.
/// @param scenario The scenario.
/// @param document The screen's document, as JSON text.
/// @param options How to play it.
/// @param scratch A folder of the run's own.
/// @param result Receives its failures and pictures.
void play(const Scenario& scenario, const std::string& document, const Options& options,
          const std::filesystem::path& scratch, ScenarioResult& result) {
	const json settings = parseUntrusted(document).value("window", json::object());
	const int width = sizeIn(settings, "width", 800), height = sizeIn(settings, "height", 600);
	const std::string title = settings.is_object() && settings.contains("title") && settings["title"].is_string()
	                              ? settings["title"].get<std::string>()
	                              : "AffrescoUI";

	// Declared before the screen, so they outlive it: a screen disconnects from its window when it goes
	affresco::detail::ManualClock clock;
	NoPeers noPeers;
	std::optional<Application> application;
	std::unique_ptr<Window> headless;
	Window* window = nullptr;
	if(scenario.needsPictures) {
		if(!options.pictures) throw std::runtime_error("it draws pictures, and this run has no OpenGL");
		window = &application.emplace(ApplicationOptions{&clock, true}).createWindow(title, width, height);
	} else {
		headless = affresco::detail::createHeadlessWindow(width, height, clock, title);
		window = headless.get();
	}
	window->setMinSize(sizeIn(settings, "minWidth", 0), sizeIn(settings, "minHeight", 0));

	Screen screen = unconnectedScreen(document, scenario.screen.parent_path(), scratch / "no-host");
	screen.setPlaceholderTypes(true); // as affrescoui_host does
	if(options.prepare) options.prepare(screen);
	screen.build(*window);
	screen.startChannels(noPeers);
	detail::Player({screen, *window, clock, scenario, options, scratch}, result).play();
}

/// @brief Plays one scenario, catching what keeps it from starting.
/// @param scenario The scenario.
/// @param options How to play it.
/// @param scratch A folder of the run's own.
/// @return How it went.
ScenarioResult runScenario(const Scenario& scenario, const Options& options, const std::filesystem::path& scratch) {
	ScenarioResult result{scenario.name, {}, {}, 0};
	try {
		play(scenario, options.document ? *options.document : readText(scenario.screen), options, scratch, result);
	} catch(const std::exception& error) { // the screen didn't load or build: step 0
		result.failures.push_back({0, std::string("could not start: ") + error.what()});
	}
	return result;
}

} // namespace

Report run(const ScenarioFile& file, const Options& options) {
	Report report{file.path, {}};
	const affresco::detail::ScratchFolder scratch;
	for(const Scenario& scenario : file.scenarios) {
		if(!options.only.empty() && std::ranges::find(options.only, scenario.name) == options.only.end()) continue;
		if(scratch.path().empty()) {
			report.scenarios.push_back({scenario.name, {{0, "could not create a temporary folder"}}, {}, 0});
			continue;
		}
		report.scenarios.push_back(runScenario(scenario, options, scratch.path()));
	}
	return report;
}

std::size_t Report::passed() const noexcept {
	return static_cast<std::size_t>(std::ranges::count_if(scenarios, &ScenarioResult::passed));
}

std::size_t Report::failed() const noexcept { return scenarios.size() - passed(); }

std::string Report::text() const {
	std::string out = file.filename().string() + ": " + std::to_string(passed()) + " passed, " +
	                  std::to_string(failed()) + " failed\n";
	for(const ScenarioResult& scenario : scenarios)
		for(const Failure& failure : scenario.failures)
			out += "FAILED " + scenario.name + (failure.step ? ", step " + std::to_string(failure.step) : "") + ": " +
			       failure.problem + "\n";
	return out;
}

json Report::toJson() const {
	json failures = json::array();
	for(const ScenarioResult& scenario : scenarios)
		for(const Failure& failure : scenario.failures)
			failures.push_back({{"scenario", scenario.name}, {"step", failure.step}, {"problem", failure.problem}});
	return {{"file", file.filename().string()}, {"passed", passed()}, {"failed", failed()}, {"failures", failures}};
}

} // namespace affresco::automation
