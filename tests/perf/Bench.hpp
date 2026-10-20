#pragma once

/// @file
/// @brief What the benchmark's files share: a workload, the groups that create them, and what the groups need.

#include <affresco/affresco.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
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
};

/// @brief A screen loaded the way affrescoui_host loads one, and its window. Both live as long as the Context.
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
	/// @param screens The template screens (examples/assets/screens).
	/// @param edits The canonical agent edits (tests/perf/edits).
	Context(std::filesystem::path screens, std::filesystem::path edits);

	/// @brief The template screens' directory.
	[[nodiscard]] const std::filesystem::path& screens() const noexcept { return screens_; }
	/// @brief The canonical edits' directory.
	[[nodiscard]] const std::filesystem::path& edits() const noexcept { return edits_; }

	/// @brief The windowing system, started on first use.
	/// @return The application.
	Application& application();

	/// @brief Loads a screen into a new hidden window, as affrescoui_host does, without a state file or a host.
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
	/// @brief Sets a screen up as affrescoui_host does: placeholder types, the "code" type, no host to talk to.
	/// @param screen The screen, not yet built.
	static void configure(Screen& screen);

	std::filesystem::path screens_, edits_;       ///< See screens() and edits().
	std::optional<Application> application_;      ///< Started by application().
	std::vector<std::unique_ptr<Screen>> loaded_; ///< Built or loaded so far; they go before the Application.
};

/// @brief A group's only workload, moved into its list (a braced list would copy it).
/// @param workload The workload.
/// @return A list of one.
[[nodiscard]] inline std::vector<Workload> single(Workload workload) {
	std::vector<Workload> workloads;
	workloads.push_back(std::move(workload));
	return workloads;
}

/// @brief Creates a group's workloads.
using GroupFactory = std::function<std::vector<Workload>(Context&)>;

/// @brief A family of workloads that share their setup, e.g. every "update/..." one.
struct Group {
	std::string prefix;  ///< The first part of its workloads' names, e.g. "update".
	GroupFactory create; ///< Creates them (the setup, such as windows, happens here).
};

/// @brief The groups that need a window: load, update, revision, layout and frame (screens.cpp).
/// @return The groups.
[[nodiscard]] std::vector<Group> screenGroups();

/// @brief The groups that need no window: expressions, commits, text (engine.cpp) and history (history.cpp).
/// @return The groups.
[[nodiscard]] std::vector<Group> engineGroups();

/// @brief The "history" group's workloads (history.cpp): 10,000 small commits on the Kanban template.
/// @param context The context.
/// @return The workloads.
[[nodiscard]] std::vector<Workload> historyWorkloads(const Context& context);

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its contents.
/// @throws std::runtime_error If it can't be read.
[[nodiscard]] std::string readFile(const std::filesystem::path& path);

/// @brief The template screens, by name ("kanban" for kanban.screen.json), in name order.
/// @param directory Where they are.
/// @return Their names.
[[nodiscard]] std::vector<std::string> templateNames(const std::filesystem::path& directory);

/// @brief The Kanban template with its four columns filled to a number of cards, by copying its own cards in turn.
/// @param directory The template screens' directory.
/// @param cards The cards in total; the template's own count, 12, leaves it as it is.
/// @return The document.
[[nodiscard]] json kanbanWithCards(const std::filesystem::path& directory, int cards);

/* Allocation counting (allocations.cpp): every operator new in the process is counted */

/// @brief How many times operator new has been called since the program started.
[[nodiscard]] std::uint64_t allocationCount() noexcept;
/// @brief The bytes allocated with operator new and not yet deleted.
[[nodiscard]] std::int64_t liveBytes() noexcept;

} // namespace affresco::bench
