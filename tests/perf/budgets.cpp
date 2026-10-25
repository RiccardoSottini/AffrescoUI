/// @file
/// @brief The "budgets" group: the budgets of BENCHMARKS.md that no other workload measures. An idle second of the
/// Kanban template, a 50 MB file opened, Forge started cold to its first frame, and Forge with 10 tabs and a terminal.
/// The memory budgets report "residentBytes", the whole process's memory: run.py runs each in a process of its own.
/// Every setup happens on the first run, so selecting one workload sets up only its own.

#include "Bench.hpp"
#include "Forge.hpp"
#include "Headless.hpp"
#include "features/editor/Buffers.hpp"
#include "features/editor/Editor.hpp"
#include "features/explorer/Explorer.hpp"
#include "features/search/Search.hpp"
#include "features/terminal/Terminal.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef __APPLE__
#include <mach/mach.h>
#elif !defined(_WIN32)
#include <unistd.h>
#endif
#ifndef _WIN32
#include <ctime>
#endif

namespace affresco::bench {
namespace {

/// @brief The memory the process holds now: its footprint on macOS (what Activity Monitor shows), its resident set on
/// Linux.
/// @return The bytes, or std::nullopt where it can't be read (Windows).
std::optional<std::int64_t> residentBytes() {
#ifdef __APPLE__
	task_vm_info_data_t info{};
	mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
	if(task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
		return std::nullopt;
	return static_cast<std::int64_t>(info.phys_footprint);
#elif !defined(_WIN32)
	std::ifstream statm("/proc/self/statm");
	std::int64_t size = 0, resident = 0;
	if(!(statm >> size >> resident)) return std::nullopt;
	return resident * static_cast<std::int64_t>(sysconf(_SC_PAGESIZE));
#else
	return std::nullopt;
#endif
}

/// @brief Adds "residentBytes" (residentBytes()) to a workload's result, read after its last run.
/// @param workload The workload.
void reportMemory(Workload& workload) {
	workload.report = [](json& result) {
		if(const std::optional<std::int64_t> bytes = residentBytes()) result["residentBytes"] = *bytes;
	};
}

/// @brief The CPU time of the whole process, every thread, in nanoseconds.
/// @return The time; 0 where it can't be read (Windows).
std::int64_t processTime() {
#ifndef _WIN32
	timespec time{};
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
	return static_cast<std::int64_t>(time.tv_sec) * 1'000'000'000 + time.tv_nsec;
#else
	return 0;
#endif
}

/// @brief A folder of its own under the system's temporary folder, removed with the object.
struct TempFolder {
	std::filesystem::path path =
	    std::filesystem::temp_directory_path() / ("aui-bench-budgets-" + std::to_string(std::random_device{}()));

	/// @brief Creates it.
	TempFolder() { std::filesystem::create_directories(path); }
	/// @brief Removes it.
	~TempFolder() {
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}
	TempFolder(const TempFolder&) = delete;
	TempFolder& operator=(const TempFolder&) = delete;

	/// @brief Writes a file of code-like lines, the same on every machine, until it reaches a size.
	/// @param name The file, in the folder.
	/// @param bytes Its size, at least.
	void write(const std::string& name, std::size_t bytes) const {
		std::ofstream file(path / name, std::ios::binary);
		std::string line;
		for(std::size_t written = 0, number = 0; written < bytes; written += line.size(), ++number) {
			line = number % 7 == 0 ? "function f" + std::to_string(number) + "(a, b) {\n"
			                       : "    total = total + a[b] * " + std::to_string(number % 100) + ";\n";
			file << line;
		}
	}
};

/// @brief "budgets/idle": the Kanban template shown, with nothing happening, for one second of its event loop. The
/// value is the main thread's CPU time in that second, and "processMs" every thread's (the budget: 0% CPU).
/// @param context The context.
/// @return The workload.
Workload idleWorkload(Context& context) {
	const auto loaded = std::make_shared<std::optional<LoadedScreen>>();
	const auto processMs = std::make_shared<std::vector<double>>();
	Workload workload{.name = "budgets/idle", .unit = "ms", .runs = 3, .cpuTime = true};
	workload.prepare = [&context, loaded] {
		if(!*loaded)
			loaded->emplace(context.load(json::parse(readFile(context.templateFolder("kanban") / "index.screen.json")),
			                             context.templateFolder("kanban")));
		detail::renderWithoutPresenting((*loaded)->window);
	};
	workload.step = [&context, processMs] {
		Application& app = context.application();
		const std::int64_t start = processTime();
		std::jthread stopper([&app] {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			app.post([&app] { app.quit(); });
		});
		app.run();
		stopper.join();
		processMs->push_back(static_cast<double>(processTime() - start) / 1e6);
	};
	workload.report = [processMs](json& result) {
		std::vector<double> measured(processMs->begin() + 1, processMs->end()); // the warm-up left out
		std::ranges::sort(measured);
		if(!measured.empty()) result["processMs"] = std::round(measured[measured.size() / 2] * 1e3) / 1e3;
	};
	return workload;
}

/// @brief "budgets/open-50mb": a 50 MB file opened in Forge's buffers, with the process's memory once it is open.
/// @return The workload.
Workload openWorkload() {
	const auto folder = std::make_shared<std::unique_ptr<TempFolder>>();
	const auto buffers = std::make_shared<std::unique_ptr<forge::Buffers>>();
	Workload workload{.name = "budgets/open-50mb", .unit = "ms", .runs = 5};
	workload.prepare = [folder, buffers] {
		buffers->reset(); // the last run's, before the next is made
		if(!*folder) {
			*folder = std::make_unique<TempFolder>();
			(*folder)->write("big.cpp", 50 * 1024 * 1024);
		}
		*buffers = std::make_unique<forge::Buffers>();
	};
	workload.step = [folder, buffers] {
		if(const std::optional<std::string> error = (*buffers)->open((*folder)->path, "big.cpp"))
			throw std::runtime_error("budgets/open-50mb: " + *error);
	};
	reportMemory(workload);
	return workload;
}

/// @brief Builds Forge into a new window as affrescoui_forge does, a folder open and a terminal that starts no shell,
/// and draws its first frame.
/// @param context The context.
/// @param folder The folder.
/// @return The screen and its window.
LoadedScreen startForge(Context& context, const std::filesystem::path& folder) {
	const std::filesystem::path directory = context.examples() / "forge";
	const std::string text = readFile(directory / "index.screen.json");
	Window& window = context.openWindow(json::parse(text));
	Screen& screen = context.create(text, directory);
	const auto workspace = std::make_shared<forge::Workspace>(folder, &window);
	forge::registerNatives(screen, workspace);
	forge::addExampleCatalog(screen, directory);
	forge::registerExplorer(screen, workspace, nullptr);
	forge::grantTerminal(screen, folder);
	forge::registerEditor(screen, workspace);
	forge::registerSearch(screen, workspace);
	screen.build(window);
	detail::renderWithoutPresenting(window);
	return {screen, window};
}

/// @brief "budgets/forge-cold-start": Forge from its document to its first frame, the folder of the rendering check
/// open: the window, parsing, building, indexing, layout and drawing.
/// @param context The context.
/// @return The workload.
Workload coldStartWorkload(Context& context) {
	Workload workload{.name = "budgets/forge-cold-start", .unit = "ms", .runs = 5};
	workload.step = [&context] { static_cast<void>(startForge(context, AFFRESCOUI_FORGE_FOLDER)); };
	return workload;
}

/// @brief Runs one of Forge's actions with an event, then its updates, as a routed event ends.
/// @param loaded Forge.
/// @param action The action.
/// @param event The event.
void runAction(const LoadedScreen& loaded, const char* action, const json& event) {
	loaded.screen.runAction(action, loaded.window.root(), event.dump());
	loaded.window.routeKey({Key::Unknown, 0, Action::Release, {}});
}

/// @brief "budgets/forge-10-tabs": Forge with 10 files of 64 KB open in tabs and a terminal, with the process's memory.
/// The first run opens them; each run then switches to the next tab and draws it.
/// @param context The context.
/// @return The workload.
Workload tabsWorkload(Context& context) {
	const auto folder = std::make_shared<std::unique_ptr<TempFolder>>();
	const auto loaded = std::make_shared<std::optional<LoadedScreen>>();
	const auto tab = std::make_shared<int>(0);
	Workload workload{.name = "budgets/forge-10-tabs", .unit = "ms", .runs = 5};
	workload.prepare = [&context, folder, loaded] {
		if(*loaded) return;
		*folder = std::make_unique<TempFolder>();
		for(int file = 0; file < 10; ++file) (*folder)->write("file" + std::to_string(file) + ".cpp", 64 * 1024);
		loaded->emplace(startForge(context, (*folder)->path));
		for(int file = 0; file < 10; ++file)
			runAction(**loaded, "openExplorerItem", {{"id", "file" + std::to_string(file) + ".cpp"}});
		runAction(**loaded, "terminalNew", json::object());
		if(const json state = json::parse((*loaded)->screen.stateJson()); state.at("docs").size() != 10)
			throw std::runtime_error("budgets/forge-10-tabs: " + std::to_string(state.at("docs").size()) +
			                         " files open");
	};
	workload.step = [loaded, tab] {
		*tab = (*tab + 1) % 10;
		runAction(**loaded, "openExplorerItem", {{"id", "file" + std::to_string(*tab) + ".cpp"}});
		detail::renderWithoutPresenting((*loaded)->window);
	};
	reportMemory(workload);
	return workload;
}

} // namespace

std::vector<Workload> budgetWorkloads(Context& context) {
	std::vector<Workload> workloads;
	workloads.push_back(idleWorkload(context));
	workloads.push_back(openWorkload());
	workloads.push_back(coldStartWorkload(context));
	workloads.push_back(tabsWorkload(context));
	return workloads;
}

} // namespace affresco::bench
