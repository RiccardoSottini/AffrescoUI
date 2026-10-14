#pragma once

#include "sgl/Window.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sgl {

// Owns the windowing system (GLFW) and every Window; runs the event loop. One instance at a time.
class Application {
public:
	Application();
	~Application();

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&) = delete;
	Application& operator=(Application&&) = delete;

	Window& createWindow(std::string title, int width, int height);

	// Blocks until every window is closed or quit() is called.
	// SGLGUI_SCREENSHOT=<file.ppm> saves the first frame of each window and returns instead.
	int run();
	void quit();

	// Runs `task` on the main thread, inside run(). Safe to call from any thread (e.g. a channel's reader).
	void post(std::function<void()> task);

private:
	std::vector<std::unique_ptr<Window>> windows_;
	bool running_ = false;

	void runPostedTasks();

	std::mutex tasksMutex_;
	std::vector<std::function<void()>> tasks_;
};

} // namespace sgl
