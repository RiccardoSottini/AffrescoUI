/// @file
/// @brief Application: the windowing system, the windows and the event loop. The windowing system is a
/// platform::Platform (GLFW) and drawing a detail::Renderer (OpenGL): nothing here calls either directly.

#include "affresco/render/Application.hpp"
#include "affresco/core/Log.hpp"

#include "Renderer.hpp"
#include "gl/OpenGlRenderer.hpp"
#include "platform/Platform.hpp"

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
	platform_ = platform::createGlfwPlatform();
	instanceAlive = true;
}

Application::~Application() {
	windows_.clear(); // windows (and their GL contexts) go before the windowing system
	platform_.reset();
	instanceAlive = false;
}

Window& Application::createWindow(std::string title, int width, int height) {
	std::unique_ptr<platform::NativeWindow> native = platform_->createWindow(title, width, height);
	std::unique_ptr<detail::Renderer> renderer =
	    gl::createRenderer(native->procAddressLoader()); // its context is current

	// Window's constructor is private: only the Application creates windows
	windows_.push_back(std::unique_ptr<Window>(
	    new Window(std::move(native), std::move(renderer), clock_, std::move(title), width, height)));
	return *windows_.back();
}

int Application::run() {
	// AFFRESCOUI_SCREENSHOT=<file.ppm>: save a frame of each window and return. AFFRESCOUI_SCREENSHOT_AFTER=<ms>: run
	// normally for that long first (e.g. so peers can connect and change the screen)
	const char* screenshot = std::getenv("AFFRESCOUI_SCREENSHOT");
	const char* after = std::getenv("AFFRESCOUI_SCREENSHOT_AFTER");
	const double deadline = clock_.seconds() + (after ? std::atof(after) / 1000.0 : 0.0);

	running_ = true;
	while(running_ && !(screenshot && clock_.seconds() >= deadline)) {
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
		if(screenshot) wait = std::min(wait.value_or(1e9), std::max(0.0, deadline - clock_.seconds()));
		platform_->waitEvents(wait);
		runPostedTasks();
	}
	running_ = false;

	if(!screenshot) return EXIT_SUCCESS;
	bool saved = true;
	for(std::size_t i = 0; i < windows_.size(); ++i) {
		const std::string path = i == 0 ? screenshot : std::string(screenshot) + "." + std::to_string(i);
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
	platform_->wake(); // wake up the event loop's wait
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
	platform_->wake(); // wake up the event loop's wait
}

} // namespace affresco
