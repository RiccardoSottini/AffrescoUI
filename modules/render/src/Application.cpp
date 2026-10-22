/// @file
/// @brief Application: the windowing system, the windows and the event loop. The windowing system is a
/// platform::Platform (GLFW) and drawing a detail::Renderer (OpenGL): nothing here calls either directly.

#include "affresco/render/Application.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/core/Portable.hpp"

#include "paint/Renderer.hpp"
#include "gl/OpenGlRenderer.hpp"
#include "platform/Platform.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace affresco {

namespace {

bool instanceAlive = false; ///< Only one Application may exist.

/// The windows an Application owns.
using Windows = std::vector<std::unique_ptr<Window>>;

/// @brief Renders every window that is still open (if something changed in it), and hides those asked to close.
/// @param windows The windows.
/// @return Whether any is still open.
bool renderOpenWindows(const Windows& windows) {
	bool anyOpen = false;
	for(const auto& window : windows) {
		if(window->shouldClose()) {
			window->setVisible(false);
			continue;
		}
		anyOpen = true;
		window->render(); // no-op unless something changed
	}
	return anyOpen;
}

#ifndef __EMSCRIPTEN__ // the browser runs the loop, and takes no screenshots
/// @brief How long until the first timer of any window is due.
/// @param windows The windows.
/// @return Seconds, or nothing if no window has a timer.
std::optional<double> secondsToNextTimer(const Windows& windows) {
	std::optional<double> wait;
	for(const auto& window : windows)
		if(const auto next = window->secondsToNextTimer()) wait = wait ? std::min(*wait, *next) : *next;
	return wait;
}

/// @brief Saves a frame of each window: the first to @p path, the others to "<path>.1", "<path>.2" and so on.
/// @param windows The windows.
/// @param path The first file.
/// @return Whether every frame was saved.
bool saveScreenshots(const Windows& windows, const std::string& path) {
	bool saved = true;
	for(std::size_t i = 0; i < windows.size(); ++i) {
		const std::string file = i == 0 ? path : path + "." + std::to_string(i);
		const bool ok = windows[i]->saveScreenshot(file);
		log(LogLevel::Info, "AffrescoUI: screenshot " + std::string(ok ? "saved: " : "FAILED: ") + file);
		saved = saved && ok;
	}
	return saved;
}

#endif

} // namespace

Application::Application() : Application(ApplicationOptions{}) {}

Application::Application(ApplicationOptions options) {
	if(instanceAlive) throw std::logic_error("AffrescoUI: only one affresco::Application can exist at a time");
	platform_ = platform::createGlfwPlatform(options.hiddenWindows);
	if(options.clock) clock_ = options.clock;
	instanceAlive = true;
}

Application::~Application() {
	windows_.clear(); // windows (and their GL contexts) go before the windowing system
	platform_.reset();
	instanceAlive = false;
}

Window& Application::createWindow(std::string title, int width, int height) {
	std::unique_ptr<platform::NativeWindow> native = platform_->createWindow(title, width, height);
	return adopt(std::move(native), std::move(title), width, height);
}

Window& Application::createWindow(std::string title, int width, int height, const Window& sharing) {
	void* handle = sharing.nativeHandle();
	std::unique_ptr<platform::NativeWindow> native = handle ? platform_->createSharedWindow(title, width, height, handle)
	                                                        : platform_->createWindow(title, width, height);
	Window& window = adopt(std::move(native), std::move(title), width, height);
	if(const auto& font = sharing.defaultFont_) window.setDefaultFont(font);
	return window;
}

Window& Application::adopt(std::unique_ptr<platform::NativeWindow> native, std::string title, int width, int height) {
	std::unique_ptr<detail::Renderer> renderer =
	    gl::createRenderer(native->procAddressLoader()); // its context is current

	// Window's constructor is private: only the Application creates windows
	windows_.push_back(std::unique_ptr<Window>(
	    new Window(std::move(native), std::move(renderer), *clock_, std::move(title), width, height)));
	Window& window = *windows_.back();
	window.routeDisplay({window.contentScale(), platform_->colorScheme(), platform_->reducedMotion()});
	return window;
}

int Application::run() {
#ifdef __EMSCRIPTEN__
	// In a browser the page runs the loop: each of its frames renders, follows the system and runs the posted tasks.
	// run() never returns (the browser keeps main's objects, as it keeps the page), and there are no screenshots
	running_ = true;
	emscripten_set_main_loop_arg(
	    [](void* self) {
		    auto& application = *static_cast<Application*>(self);
		    if(!application.running_ || !renderOpenWindows(application.windows_)) {
			    application.running_ = false;
			    emscripten_cancel_main_loop();
			    return;
		    }
		    application.followSystem();
		    application.runPostedTasks();
	    },
	    this, 0, true);
	return EXIT_SUCCESS;
#else
	// AFFRESCOUI_SCREENSHOT=<file.ppm>: save a frame of each window and return. AFFRESCOUI_SCREENSHOT_AFTER=<ms>: run
	// normally for that long first (e.g. so peers can connect and change the screen)
	const std::optional<std::string> screenshot = environmentVariable("AFFRESCOUI_SCREENSHOT");
	const std::optional<std::string> after = environmentVariable("AFFRESCOUI_SCREENSHOT_AFTER");
	const double deadline = clock_->seconds() + (after ? std::strtod(after->c_str(), nullptr) / 1000.0 : 0.0);

	running_ = true;
	while(running_ && (!screenshot || clock_->seconds() < deadline)) {
		if(!renderOpenWindows(windows_)) break;

		// Sleeps until there is input or a timer is due: no CPU or GPU used while idle
		std::optional<double> wait = secondsToNextTimer(windows_);
		if(screenshot) wait = std::min(wait.value_or(1e9), std::max(0.0, deadline - clock_->seconds()));
		platform_->waitEvents(wait);
		followSystem();
		runPostedTasks();
	}
	running_ = false;

	if(!screenshot) return EXIT_SUCCESS;
	return saveScreenshots(windows_, *screenshot) ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

void Application::followSystem() {
	const ColorScheme scheme = platform_->colorScheme();
	const bool reduced = platform_->reducedMotion();
	for(const auto& window : windows_)
		if(window->colorScheme() != scheme || window->reducedMotion() != reduced)
			window->routeDisplay({window->contentScale(), scheme, reduced});
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
