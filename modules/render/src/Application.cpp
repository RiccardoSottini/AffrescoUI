#include "affresco/render/Application.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/core/Portable.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <stdexcept>

namespace affresco {

namespace {

bool instanceAlive = false; ///< Only one Application may exist.

} // namespace

Application::Application() {
	if(instanceAlive) throw std::logic_error("AffrescoUI: only one affresco::Application can exist at a time");

	glfwSetErrorCallback([](int code, const char* description) {
		log(LogLevel::Error, "AffrescoUI: GLFW error " + std::to_string(code) + ": " + description);
	});
	if(!glfwInit()) throw std::runtime_error("AffrescoUI: could not initialise GLFW");

	instanceAlive = true;
}

Application::~Application() {
	windows_.clear(); // windows (and their GL contexts) go before the windowing system
	glfwTerminate();
	instanceAlive = false;
}

Window& Application::createWindow(std::string title, int width, int height) {
	// OpenGL 4.1 core: the highest version available on macOS
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

	// Window's constructor is private: only the Application creates windows
	windows_.push_back(std::unique_ptr<Window>(new Window(std::move(title), width, height)));
	return *windows_.back();
}

int Application::run() {
	// AFFRESCOUI_SCREENSHOT=<file.ppm>: save a frame of each window and return. AFFRESCOUI_SCREENSHOT_AFTER=<ms>: run
	// normally for that long first (e.g. so peers can connect and change the screen)
	const std::optional<std::string> screenshot = environmentVariable("AFFRESCOUI_SCREENSHOT");
	const std::optional<std::string> after = environmentVariable("AFFRESCOUI_SCREENSHOT_AFTER");
	const double deadline = glfwGetTime() + (after ? std::strtod(after->c_str(), nullptr) / 1000.0 : 0.0);

	running_ = true;
	while(running_ && !(screenshot && glfwGetTime() >= deadline)) {
		bool anyOpen = false;
		for(auto& window : windows_) {
			if(window->shouldClose()) {
				window->setVisible(false);
				continue;
			}
			anyOpen = true;
			window->render(); // no-op unless something changed
		}
		if(!anyOpen) break;

		// Sleeps until there is input or a timer is due: no CPU or GPU used while idle
		std::optional<double> wait;
		for(auto& window : windows_)
			if(const auto next = window->secondsToNextTimer()) wait = wait ? std::min(*wait, *next) : *next;
		if(screenshot) wait = std::min(wait.value_or(1e9), std::max(0.0, deadline - glfwGetTime()));
		if(wait) glfwWaitEventsTimeout(*wait);
		else glfwWaitEvents();
		runPostedTasks();
	}
	running_ = false;

	if(!screenshot) return EXIT_SUCCESS;
	bool saved = true;
	for(std::size_t i = 0; i < windows_.size(); ++i) {
		const std::string path = i == 0 ? *screenshot : *screenshot + "." + std::to_string(i);
		const bool ok = windows_[i]->saveScreenshot(path);
		log(LogLevel::Info, "AffrescoUI: screenshot " + std::string(ok ? "saved: " : "FAILED: ") + path);
		saved = saved && ok;
	}
	return saved ? EXIT_SUCCESS : EXIT_FAILURE;
}

void Application::post(std::function<void()> task) {
	{
		const std::lock_guard lock(tasksMutex_);
		tasks_.push_back(std::move(task));
	}
	glfwPostEmptyEvent(); // wake up glfwWaitEvents
}

void Application::runPostedTasks() {
	std::vector<std::function<void()>> tasks;
	{
		const std::lock_guard lock(tasksMutex_);
		tasks.swap(tasks_);
	}
	for(auto& task : tasks) task();
}

void Application::quit() {
	running_ = false;
	glfwPostEmptyEvent(); // wake up glfwWaitEvents
}

} // namespace affresco
