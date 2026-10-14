#include "sgl/Application.hpp"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace sgl {

namespace {

bool instanceAlive = false;

} // namespace

Application::Application() {
	if(instanceAlive) throw std::logic_error("sGLGUI: only one sgl::Application can exist at a time");

	glfwSetErrorCallback([](int code, const char* description) {
		std::cerr << "sGLGUI: GLFW error " << code << ": " << description << '\n';
	});
	if(!glfwInit()) throw std::runtime_error("sGLGUI: could not initialise GLFW");

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
	if(const char* screenshot = std::getenv("SGLGUI_SCREENSHOT")) {
		bool saved = true;
		for(std::size_t i = 0; i < windows_.size(); ++i) {
			const std::string path = i == 0 ? screenshot : std::string(screenshot) + "." + std::to_string(i);
			const bool ok = windows_[i]->saveScreenshot(path);
			std::cout << "sGLGUI: screenshot " << (ok ? "saved: " : "FAILED: ") << path << '\n';
			saved = saved && ok;
		}
		return saved ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	running_ = true;
	while(running_) {
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

		glfwWaitEvents(); // sleeps until there is input: no CPU or GPU used while idle
		runPostedTasks();
	}

	running_ = false;
	return EXIT_SUCCESS;
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

} // namespace sgl
