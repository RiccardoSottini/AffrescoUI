#pragma once

/// @file
/// @brief What the benchmark's files share: a workload, the groups that create them, and what the groups need.

#include <affresco/affresco.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::bench {

/// @brief One thing to measure. Each run calls prepare() (not measured), then step() (measured).
struct Workload {
	std::string name{};              ///< E.g. "update/400"; the first part names the group.
	std::string unit = "us";         ///< What the value is in: "ns", "us" or "ms", per operation.
	int runs = 9;                    ///< Measured runs; the value is their median.
	int operations = 1;              ///< Operations one step does: the value, allocations and memory are per operation.
	bool cpuTime = false;            ///< Measure the main thread's CPU time instead of the wall clock.
	std::function<void()> prepare{}; ///< Gets a run ready, e.g. a fresh repository; may be empty.
	std::function<void()> step{};    ///< The measured work.
	std::function<void(json&)> report{}; ///< Adds fields to the result after the runs, e.g. a percentile or memory.
};

/// @brief A screen loaded the way affrescoui_launcher loads one, and its window. Both live as long as the Context.
struct LoadedScreen {
	Screen& screen; ///< The screen.
	Window& window; ///< Its window.
};

/// @brief What the groups share: where the files are, and the windowing system once a group needs it.
///
/// Screens are kept until the end, after the workloads that use them; the Application goes last, with its windows.
class Context {
public:
	/// @brief Sets where the inputs are.
	/// @param examples The examples, with the templates' folders (examples/templates.json).
	/// @param edits The canonical agent edits (tests/perf/edits).
	Context(std::filesystem::path examples, std::filesystem::path edits);

	/// @brief The examples' directory.
	[[nodiscard]] const std::filesystem::path& examples() const noexcept { return examples_; }
	/// @brief A template's folder, where its screen is.
	/// @param name E.g. "kanban".
	/// @return E.g. examples/flow.
	[[nodiscard]] std::filesystem::path templateFolder(const std::string& name) const;
	/// @brief The canonical edits' directory.
	[[nodiscard]] const std::filesystem::path& edits() const noexcept { return edits_; }

	/// @brief The windowing system, started on first use.
	/// @return The application.
	Application& application();

	/// @brief Loads a screen into a new hidden window, as affrescoui_launcher does, without a state file or a host.
	/// @param document The screen document.
	/// @param baseDirectory Where its relative paths (fonts) are resolved from.
	/// @return The screen, kept until the Context goes.
	/// @throws ScreenError If it can't be built.
	LoadedScreen load(const json& document, const std::filesystem::path& baseDirectory);

	/// @brief Builds a screen into a window created beforehand, as the "load" workload measures it.
	/// @param text The screen document, as text.
	/// @param baseDirectory Where its relative paths are resolved from.
	/// @param window An empty window.
	/// @return The screen, kept until the Context goes.
	/// @throws ScreenError If it can't be built.
	Screen& build(const std::string& text, const std::filesystem::path& baseDirectory, Window& window);
	/// @brief Loads a screen without building it, as the "build" workload prepares it.
	/// @param text The screen document, as text.
	/// @param baseDirectory Where its relative paths are resolved from.
	/// @return The screen, kept until the Context goes.
	/// @throws ScreenError If it can't be read.
	Screen& create(const std::string& text, const std::filesystem::path& baseDirectory);

	/// @brief Opens a hidden window for a document, sized from its "window" section.
	/// @param document The screen document.
	/// @return The window.
	Window& openWindow(const json& document);

private:
	/// @brief Sets a screen up as affrescoui_launcher does: placeholder types, no host to talk to.
	/// @param screen The screen, not yet built.
	static void configure(Screen& screen);

	std::filesystem::path examples_, edits_;      ///< See examples() and edits().
	std::optional<Application> application_;      ///< Started by application().
	std::vector<std::unique_ptr<Screen>> loaded_; ///< Built or loaded so far; they go before the Application.
};

/// @brief A group's only workload, moved into its list (a braced list would copy it).
/// @param workload The workload.
/// @return A list of one.
/// @brief How many times longer a time limit is in this run: 100 under valgrind (the callgrind job, tests/perf/ci.sh),
/// which runs the code about that much slower, else 1. The limits guard against a hang; they aren't what is measured.
/// @return The factor.
[[nodiscard]] int slowdown();

[[nodiscard]] inline std::vector<Workload> single(Workload workload) {
	std::vector<Workload> workloads;
	workloads.push_back(std::move(workload));
	return workloads;
}

/// @brief An executor a workload pumps by hand, on the main thread: where a model's or a screen's services post their
/// results (watch.cpp, forge.cpp, budgets.cpp).
class PumpedExecutor final : public Executor {
public:
	/// @brief Queues a task.
	/// @param task The task.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		posted_.notify_all();
	}
	/// @brief Runs posted tasks until a condition holds.
	/// @param done The condition.
	/// @param what Who should answer, for the error: "watch: the model".
	/// @throws std::runtime_error If it doesn't hold within 60 seconds (times slowdown()).
	void pumpUntil(const std::function<bool()>& done, std::string_view what) {
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(60) * slowdown();
		while(!done()) {
			std::vector<std::function<void()>> tasks;
			{
				std::unique_lock lock(mutex_);
				if(!posted_.wait_until(lock, end, [this] { return !tasks_.empty(); }))
					throw std::runtime_error(std::string(what) + " didn't answer in time");
				tasks.swap(tasks_);
			}
			for(const auto& task : tasks) task();
		}
	}

private:
	std::mutex mutex_;                         ///< Guards tasks_.
	std::condition_variable posted_;           ///< Signalled at each post.
	std::vector<std::function<void()>> tasks_; ///< Waiting to run.
};

/// @brief Creates a group's workloads.
using GroupFactory = std::function<std::vector<Workload>(Context&)>;

/// @brief A family of workloads that share their setup, e.g. every "update/..." one.
struct Group {
	std::string prefix;  ///< The first part of its workloads' names, e.g. "update".
	GroupFactory create; ///< Creates them (the setup, such as windows, happens here).
};

/// @brief The groups that need a window: load, update, revision, layout and frame (screens.cpp), editor (editor.cpp)
/// and forge (forge.cpp).
/// @return The groups.
[[nodiscard]] std::vector<Group> screenGroups();

/// @brief The groups that need no window: expressions, commits, text (engine.cpp), history (history.cpp),
/// views (views.cpp, headless windows) and search (search.cpp).
/// @return The groups.
[[nodiscard]] std::vector<Group> engineGroups();

/// @brief The "history" group's workloads (history.cpp): 10,000 small commits on the Kanban template.
/// @param context The context.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> historyWorkloads(const Context& context);

/// @brief The "forge" group's workloads (forge.cpp): Forge, the demo IDE, opened, typed into, its tabs switched, a file
/// searched for and its command palette run.
/// @param context The context.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> forgeWorkloads(Context& context);

/// @brief The "views" group's workloads (views.cpp): a tree's 100,000-item folder opened then closed (two operations),
/// and a 100,000-row list scrolled to a new place and drawn, on headless windows.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> viewWorkloads(Context&);

/// @brief The "watch" group's workloads (watch.cpp): a folder of 100,000 files listed and watched as a DirectoryModel,
/// and one file's change seen by it.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> watchWorkloads(Context&);

/// @brief The "search" group's workloads (search.cpp): the services' search engine over 2,000 generated files, a word
/// with one thread and with eight, an expression, and a capped search.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> searchWorkloads(Context&);

/// @brief The "syntax" group's workloads (syntax.cpp): a 100,000-line C++ file parsed and highlighted, and one
/// keystroke in it parsed again and its screen highlighted, with a budget checked when AFFRESCOUI_PERF_BUDGETS is set.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> syntaxWorkloads(Context&);

/// @brief The "editor" group's workloads (editor.cpp): typing in 1,000 and 100,000 lines and scrolling 10,000 and
/// 1,000,000 on hidden windows, each operation drawn, with 99th-percentile budgets checked when AFFRESCOUI_PERF_BUDGETS is set.
/// @param context Where the fonts are, and the windowing system.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> editorWorkloads(Context& context);
/// @brief The "canvas" group's workloads (canvas.cpp): a canvas of 100,000 items panned to a new place and drawn, and
/// the same with 1,000 items, on headless windows.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> canvasWorkloads(Context&);

/// @brief The "tempera" group's workloads (tempera.cpp): a 20,000 by 20,000 image opened, panned and painted, with
/// budgets when AFFRESCOUI_PERF_BUDGETS is set.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> temperaWorkloads(Context&);
/// @brief The "graph" group's workloads (graph.cpp): a 1,000-node graph set and drawn, and one of its nodes dragged
/// (a frame per pointer move), on a headless window.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> graphWorkloads(const Context&);

/// @brief The "budgets" group's workloads (budgets.cpp): the budgets no other workload measures, an idle second, a
/// 50 MB file opened, Forge started cold and Forge with 10 tabs and a terminal (BENCHMARKS.md).
/// @param context The context.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> budgetWorkloads(Context& context);

/* Scaling (scaling_screens.cpp, scaling_engine.cpp): "scaling/<area>/<n>", the same work at sizes that double */

/// @brief The sizes an area is measured at: 5 that double from the smallest, 7 at night.
/// @param smallest The first size.
/// @param large The nightly run's sizes.
/// @return The sizes, smallest first.
[[nodiscard]] inline std::vector<int> scalingSizes(int smallest, bool large) {
	std::vector<int> sizes;
	for(int i = 0, size = smallest; i < (large ? 7 : 5); ++i, size *= 2) sizes.push_back(size);
	return sizes;
}

/// @brief A scaling workload's name.
/// @param area The area, e.g. "update" or "text/shape".
/// @param size n.
/// @return "scaling/<area>/<n>".
[[nodiscard]] inline std::string scalingName(std::string_view area, int size) {
	return "scaling/" + std::string(area) + "/" + std::to_string(size);
}

/// @brief One workload per size.
/// @param sizes The sizes.
/// @param create Makes the workload of one size.
/// @return The workloads, in the order of the sizes.
template <class Create> [[nodiscard]] std::vector<Workload> perSize(const std::vector<int>& sizes, Create create) {
	std::vector<Workload> workloads;
	workloads.reserve(sizes.size());
	for(const int size : sizes) workloads.push_back(create(size));
	return workloads;
}

/// @brief The scaling groups that need a window: the Kanban template by its cards, and adversarial screens.
/// @param large The nightly run's sizes.
/// @return The groups.
[[nodiscard]] std::vector<Group> scalingScreenGroups(bool large);

/// @brief The scaling groups that need no window: the checker, history, expressions, text, the editor and peers.
/// @param large The nightly run's sizes.
/// @return The groups.
[[nodiscard]] std::vector<Group> scalingEngineGroups(bool large);

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its contents.
/// @throws std::runtime_error If it can't be read.
[[nodiscard]] std::string readFile(const std::filesystem::path& path);

/// @brief A screen file as a history holds it: its "include" files merged in, and its state.
/// @param file The screen file.
/// @return The document.
/// @throws std::exception If it can't be read or isn't a valid screen.
[[nodiscard]] json screenDocument(const std::filesystem::path& file);

/// @brief A screen document as a history holds it: its "include" files merged in, and its state.
/// @param document The document, as read from its file.
/// @param directory The folder its includes are relative to.
/// @return The document.
/// @throws std::exception If it isn't a valid screen or an include can't be read.
[[nodiscard]] json screenDocument(const json& document, const std::filesystem::path& directory);

/// @brief The Kanban template with its four columns filled to a number of cards, by copying its own cards in turn.
/// @param directory The Kanban template's folder.
/// @param cards The cards in total; the template's own count, 12, leaves it as it is.
/// @return The document.
[[nodiscard]] json kanbanWithCards(const std::filesystem::path& directory, int cards);

/* Allocation counting (allocations.cpp): every operator new in the process is counted */

/// @brief How many times operator new has been called since the program started.
[[nodiscard]] std::uint64_t allocationCount() noexcept;
/// @brief The bytes allocated with operator new and not yet deleted.
[[nodiscard]] std::int64_t liveBytes() noexcept;

} // namespace affresco::bench
