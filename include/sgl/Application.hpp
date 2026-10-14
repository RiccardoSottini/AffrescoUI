#pragma once

#include "sgl/Window.hpp"

#include <memory>
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

private:
	std::vector<std::unique_ptr<Window>> windows_;
	bool running_ = false;
};

} // namespace sgl
