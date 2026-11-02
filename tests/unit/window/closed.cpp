/// @file
/// @brief A closed window's timers: they wait while it is closed, without keeping the event loop awake, and run once it
/// opens again (the host's changes window has a timer every half second, and stays open but hidden once closed).

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <ctime>
#include <functional>

TEST_CASE("window: a closed window's repeating timer neither spins the event loop nor stops for good") {
	affresco::Application app({nullptr, true}); // hidden windows
	affresco::Window& main = app.createWindow("main", 120, 80);
	affresco::Window& closed = app.createWindow("closed", 120, 80);
	int runs = 0;
	std::function<void()> every = [&] { // as a document's "every": each run sets the next
		++runs;
		closed.setTimer(20, every);
	};
	closed.setTimer(0, every);
	closed.setShouldClose(true);

	main.setTimer(500, [&app] { app.quit(); });
	const std::clock_t start = std::clock();
	app.run();
	const double cpuSeconds = static_cast<double>(std::clock() - start) / CLOCKS_PER_SEC;
	CHECK(cpuSeconds < 0.25); // half a second of sleep; spinning on the due timer burned all of it
	CHECK(runs <= 1);         // at most once, as the window was hidden

	const int before = runs;
	closed.setShouldClose(false);
	main.setTimer(100, [&app] { app.quit(); });
	app.run();
	CHECK(runs > before);
}
