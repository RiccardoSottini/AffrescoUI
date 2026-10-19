/// @file
/// @brief The "history" workloads: the Kanban template's version control after 10,000 small commits, in memory and
/// read back from disk (VERSION_CONTROL.md, "History in proportion to the changes").

#include "Bench.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

/// @brief A small commit: the logo's text, a different one each time, as the "commits" workload makes.
/// @param number Which one.
/// @return The revision.
json rename(int number) {
	return {{"author", "bench"},
	        {"message", "Rename the logo"},
	        {"set", {{"#logo/text", "Flow " + std::to_string(number)}}}};
}

/// @brief A Kanban history with many small commits, and the next commit's number.
struct History {
	format::Rules rules;                      ///< The format's rules.
	std::unique_ptr<vcs::Repository> history; ///< The history.
	int next = 0;                             ///< The next rename's number.

	/// @brief Makes a history of a number of renames.
	/// @param document The Kanban template.
	/// @param commits How many.
	/// @throws std::runtime_error If a commit is refused.
	History(const json& document, int commits) : history(std::make_unique<vcs::Repository>(rules, document)) {
		commitRenames(commits);
	}

	/// @brief Commits more renames.
	/// @param commits How many.
	/// @throws std::runtime_error If one is refused.
	void commitRenames(int commits) {
		for(int i = 0; i < commits; ++i)
			if(const auto error = history->commit(rename(next++))) throw std::runtime_error("history: " + *error);
	}
};

/// @brief A folder in the system's temporary folder, removed with what it holds when the last workload goes.
class TemporaryFolder {
public:
	/// @brief Makes the folder.
	TemporaryFolder()
	    : path_(std::filesystem::temp_directory_path() /
		        ("affrescoui-bench-history-" +
		         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
		std::filesystem::create_directories(path_);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	/// @brief Removes it.
	~TemporaryFolder() {
		try {
			std::filesystem::remove_all(path_);
		} catch(...) { // a destructor mustn't throw: say what stays behind
			std::fputs("affrescoui_bench: the history folder in the temporary folder couldn't be removed\n", stderr);
		}
	}
	/// @brief The folder.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_; ///< The folder.
};

/// How many commits the large history has.
constexpr int largeHistory = 10'000;

/// @brief "history/commit/<n>": 100 more small commits on a history of n, per commit: their cost doesn't grow with
/// the history, and retainedBytes is what each one keeps.
/// @param history The history of n commits; each run adds 100.
/// @param commits n.
/// @return The workload.
Workload commitWorkload(const std::shared_ptr<History>& history, int commits) {
	Workload workload{.name = "history/commit/" + std::to_string(commits), .runs = 5, .operations = 100};
	workload.step = [history] { history->commitRenames(100); };
	return workload;
}

/// @brief "history/blame" and "history/diff" on the large history: blame of a node no commit changed (it reads every
/// commit back to revision 0), and a diff between two commits rebuilt from their snapshot (the farthest from it).
/// @param history The large history.
/// @param workloads Receives them.
void readWorkloads(const std::shared_ptr<History>& history, std::vector<Workload>& workloads) {
	Workload blame{.name = "history/blame", .runs = 5};
	blame.step = [history] {
		if(history->history->blame("#board").number != 0) throw std::logic_error("history/blame: #board changed");
	};
	workloads.push_back(std::move(blame));

	// Revision 10 * interval - 1 is the farthest from a snapshot, as its parent nearly is
	static constexpr int farthest = 10 * vcs::Repository::snapshotInterval - 1;
	Workload diff{.name = "history/diff", .runs = 9};
	diff.step = [history] {
		if(history->history->diff(farthest - 1, farthest).size() != 1)
			throw std::logic_error("history/diff: not one change");
	};
	workloads.push_back(std::move(diff));
}

/// @brief "history/load/10000": opening a folder that holds a history of 10,000 commits (written once, unmeasured,
/// as one change set: one save instead of 10,000).
/// @param document The Kanban template.
/// @return The workload.
Workload loadWorkload(const json& document) {
	const auto folder = std::make_shared<TemporaryFolder>();
	const auto rules = std::make_shared<const format::Rules>();
	const std::filesystem::path directory = folder->path() / "kanban.history";
	{
		json revisions = json::array();
		for(int i = 0; i < largeHistory; ++i) revisions.push_back(rename(i));
		vcs::Repository writer(*rules, document);
		writer.openHistory(directory);
		const json changes = {
		    {"format", "affrescoui-changes"}, {"version", 1}, {"name", "renames"}, {"revisions", revisions}};
		if(const auto error = writer.importChanges(changes)) throw std::runtime_error("history/load: " + *error);
	} // releases the folder's lock
	const auto screen = std::make_shared<const json>(document);
	Workload workload{.name = "history/load/10000", .unit = "ms", .runs = 5};
	workload.step = [folder, rules, screen] { // the folder, not its path: a copied path could throw when moved
		vcs::Repository reader(*rules, *screen);
		if(!reader.openHistory(folder->path() / "kanban.history").loaded || reader.head().number != largeHistory)
			throw std::logic_error("history/load: not loaded whole");
	};
	return workload;
}

} // namespace

std::vector<Workload> historyWorkloads(const Context& context) {
	const json document = json::parse(readFile(context.screens() / "kanban.screen.json"));
	const auto large = std::make_shared<History>(document, largeHistory); // the commit runs add to it: blame reads all
	std::vector<Workload> workloads;
	workloads.push_back(commitWorkload(std::make_shared<History>(document, 100), 100));
	workloads.push_back(commitWorkload(large, largeHistory));
	readWorkloads(large, workloads);
	workloads.push_back(loadWorkload(document));
	return workloads;
}

} // namespace affresco::bench
