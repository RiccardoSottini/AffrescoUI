/// @file
/// @brief An idle second is idle: each template, shown in a hidden window with nothing happening, runs no update,
/// draws nothing, and turns its event loop only for the timers its document set and for the quit (a template whose
/// own timer changes its state, as a song that plays does, updates and draws once for each). Counts, not a stopwatch,
/// so the machine's load doesn't change the verdict.

#include "Headless.hpp"
#include "Helpers.hpp"
#include "Templates.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Runtime.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace {

/// @brief A template's screen as the benchmarks load it (tests/perf/screens.cpp, appDocument()): an app's entry screen
/// with its includes, models and seed records merged in; without an app, its index.screen.json (or the screen named
/// after the folder).
/// @param folder The template's folder.
/// @return The screen, not yet built.
affresco::Screen templateScreen(const std::filesystem::path& folder) {
	using affresco::Screen;
	if(std::filesystem::exists(folder / "app.json") || std::filesystem::exists(folder / "app.lisp")) {
		const Screen app = Screen::fromApp(folder, {}, Screen::History::InMemory); // nothing kept
		return Screen::fromString(app.documentJson(), folder);
	}
	const std::filesystem::path index = folder / "index.screen.json";
	const std::ifstream file(std::filesystem::exists(index) ? index : folder / (folder.filename().string() + ".screen.json"));
	std::stringstream text;
	text << file.rdbuf();
	return Screen::fromString(text.str(), folder);
}

/// @brief Runs the event loop for one real second: a thread posts quit() at its end, as the idle benchmark does.
/// @param app The application.
void runOneSecond(affresco::Application& app) {
	const std::jthread stopper([&app] {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		app.post([&app] { app.quit(); });
	});
	app.run();
}

/// @brief What an idle second is judged by.
struct Counts {
	std::uint64_t updates = 0; ///< The screen's updates (format::UpdateStatistics::updates).
	std::uint64_t frames = 0;  ///< The window's frames drawn.
	std::uint64_t turns = 0;   ///< The event loop's turns.
	std::uint64_t timers = 0;  ///< Of those, the turns a due timer ended.
};

/// @brief The counts now.
/// @param app The application.
/// @param screen The screen.
/// @param window Its window.
/// @return They.
Counts countsOf(const affresco::Application& app, const affresco::Screen& screen, const affresco::Window& window) {
	return {screen.lastUpdate().updates, window.frameStatistics().frames, app.turns(),
	        app.turns(affresco::Application::Wake::Timer)};
}

} // namespace

TEST_CASE("idle: an idle second of every template runs no update, draws nothing and wakes only for its timers") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	// Channels find no host, so a channel that reconnects takes its retry path, as in the benchmarks
	const std::filesystem::path noHost = std::filesystem::temp_directory_path() / "affrescoui-idle-no-host";
	for(const std::string& name : templateNames(examples)) {
		CAPTURE(name);
		affresco::Application app({nullptr, true}); // hidden windows
		affresco::Screen screen = templateScreen(templateDirectory(examples, name));
		screen.setPlaceholderTypes(true); // as affrescoui_launcher does
		screen.setSocketDirectory(noHost);
		const affresco::Window& window = screen.createWindow(app);

		runOneSecond(app); // settles: onLoad, the first frame and the first timers do their work
		const bool timerPending = window.secondsToNextTimer().has_value();
		const Counts before = countsOf(app, screen, window);
		const std::string state = screen.stateJson();
		runOneSecond(app);
		const Counts after = countsOf(app, screen, window);

		// Every turn but the quit's was a timer due, and few were: a timer that re-arms every frame would show here
		const std::uint64_t timers = after.timers - before.timers;
		if(screen.stateJson() == state) { // nothing changed, so nothing was worked out again or drawn
			CHECK(after.updates == before.updates);
			CHECK(after.frames == before.frames);
		} else { // the screen's own timer moved it on (a song that plays from the start): an update and a frame each
			MESSAGE(name, ": its own timers changed its state");
			CHECK(after.updates - before.updates <= timers);
			CHECK(after.frames - before.frames <= timers);
		}
		CHECK(after.turns - before.turns == timers + 1);
		CHECK(timers <= 3);
		if(!timerPending) CHECK(after.turns - before.turns == 1);
		CHECK(app.lastWake() == affresco::Application::Wake::Posted);
	}
}

// A timer set while the loop works (a repeating timer that re-arms as it runs) is seen before the loop sleeps: it
// posted an empty event as well, which turned the loop once more for nothing, each time
TEST_CASE("idle: a repeating timer turns the event loop once per run, and the turns say why") {
	affresco::Application app({nullptr, true}); // hidden windows
	affresco::Window& window = app.createWindow("timers", 120, 80);
	int runs = 0;
	std::function<void()> every = [&] {
		if(++runs == 5) app.quit();
		else window.setTimer(40, every);
	};
	window.setTimer(200, [&app] { app.quit(); });
	app.run(); // settles: what the system sends a new window comes now

	using Wake = affresco::Application::Wake;
	const std::uint64_t turns = app.turns();
	const std::uint64_t timerTurns = app.turns(Wake::Timer);
	window.setTimer(40, every);
	app.run();
	CHECK(runs == 5);
	CHECK(app.turns() - turns == 5); // the fifth quits: no turn of its own
	CHECK(app.turns(Wake::Timer) - timerTurns == 5);
	CHECK(app.lastWake() == Wake::Timer);
}

// With no host, each retry used to run "<name>:closed" again: every 2 s in an idle app, and an update with it
TEST_CASE("idle: a channel that finds no host runs its closed action once, whatever it retries") {
	affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(200, 100, clock);
	affresco::test::QueueExecutor executor;
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"state": { "closings": 0 },
		"channels": { "nobody": { "socket": "/nonexistent-affrescoui-test/nobody.sock", "reconnect": { "after": 10 } } },
		"actions": { "nobody:closed": [{ "set": "state.closings", "to": "{{state.closings + 1}}" }] },
		"root": { "type": "panel" }
	})json");
	screen.build(*window);
	screen.startChannels(executor); // nothing listens there: the first try fails
	for(int retry = 0; retry < 5; ++retry) {
		REQUIRE(window->secondsToNextTimer()); // the document's retry timer stays, with its period
		clock.advance(11.0);
		window->render();
		executor.runPosted();
	}
	CHECK(affresco::json::parse(screen.stateJson())["closings"] == 1);
}

// A paused player's "every" runs each second and writes nothing: no update may follow, nor a frame
TEST_CASE("idle: an every timer whose action writes nothing runs no update and draws nothing") {
	affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"state": { "playing": false, "elapsed": 0 },
		"actions": { "tick": [{ "if": "{{state.playing}}", "then": [{ "increment": "state.elapsed" }] }] },
		"root": { "type": "panel", "every": { "ms": 1000, "do": "tick" } }
	})json");
	screen.build(*window);
	window->render(); // the first frame
	const std::uint64_t updates = screen.lastUpdate().updates;
	const std::uint64_t frames = window->frameStatistics().frames;
	for(int second = 0; second < 5; ++second) {
		REQUIRE(window->secondsToNextTimer());
		clock.advance(1001.0);
		window->render();
	}
	CHECK(screen.lastUpdate().updates == updates);
	CHECK(window->frameStatistics().frames == frames);
}
