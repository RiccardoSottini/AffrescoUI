/// @file
/// @brief The "history" workloads: the Kanban template's version control after 10,000 small commits, in memory and
/// read back from disk (VERSION_CONTROL.md, "History in proportion to the changes"), and undoing an early commit on
/// it ("Undo that keeps what came later").

#include "Bench.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
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

/// @brief An action of the bench's own: a top-level entry of the document, which the undo's merge takes as one part.
/// @param name Its name.
/// @param text What it logs.
/// @return The revision that sets it.
json setAction(const std::string& name, const std::string& text) {
	return {{"author", "bench"}, {"message", "Set an action"}, {"set", {{"/actions/" + name, {{{"log", text}}}}}}};
}

/// @brief Commits a revision, or throws.
/// @param history The history.
/// @param revision The revision.
/// @param what The workload, for the error.
void commitOrThrow(vcs::Repository& history, const json& revision, const std::string& what) {
	if(const auto error = history.commit(revision)) throw std::runtime_error(what + ": " + *error);
}

/// @brief "history/undo/independent/<n>": undoing revision 1, which set an action no later commit touched, on a
/// history of n commits: worked out and committed. Between runs the undo is undone again, unmeasured, so each run
/// undoes the same change; the history grows by two commits a run.
/// @param document The Kanban template.
/// @param commits n.
/// @return The workload.
Workload undoIndependentWorkload(const json& document, int commits) {
	const std::string name = "history/undo/independent/" + std::to_string(commits);
	const auto history = std::make_shared<History>(document, 0);
	commitOrThrow(*history->history, setAction("benchUndo", "independent"), name);
	history->commitRenames(commits - 1);
	const auto undone = std::make_shared<bool>(false);
	Workload workload{.name = name, .runs = 9};
	workload.prepare = [history, undone] {
		if(!*undone) return;
		commitOrThrow(*history->history, history->history->revertRevision("HEAD", "bench"), "history/undo: redo");
		*undone = false;
	};
	workload.step = [history, undone] {
		commitOrThrow(*history->history, history->history->undoRevision(1, "bench", {}), "history/undo/independent");
		*undone = true;
	};
	return workload;
}

/// How many parts the conflicting undo's commit changed that later commits changed again.
constexpr int conflictingParts = 8;

/// @brief "history/undoPlan/conflicts/10000": the plan of an undo of revision 1, 9,999 commits back, which set 8
/// actions that later commits, spread over the history, set again: 8 conflicts, each traced to the commit that built
/// on it.
/// @param document The Kanban template.
/// @return The workload.
Workload undoPlanWorkload(const json& document) {
	const std::string name = "history/undoPlan/conflicts/10000";
	const auto history = std::make_shared<History>(document, 0);
	vcs::Repository& repository = *history->history;
	json first = {{"author", "bench"}, {"message", "Set the actions"}, {"set", json::object()}};
	for(int part = 0; part < conflictingParts; ++part)
		first["set"]["/actions/bench" + std::to_string(part)] = json::array({{{"log", "first"}}});
	commitOrThrow(repository, first, name);
	constexpr int spacing = largeHistory / (conflictingParts + 1);
	for(int part = 0; part < conflictingParts; ++part) {
		history->commitRenames(spacing - 1);
		commitOrThrow(repository, setAction("bench" + std::to_string(part), "again"), name);
	}
	history->commitRenames(largeHistory - repository.head().number);
	Workload workload{.name = name, .unit = "ms", .runs = 9};
	workload.step = [history] {
		const UndoPlan plan = history->history->undoPlan(1, "bench", {});
		if(plan.outcome != "choose" || plan.conflicts.size() != static_cast<std::size_t>(conflictingParts))
			throw std::logic_error("history/undoPlan: not 8 conflicts to choose for");
	};
	return workload;
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
		    {"format", "affrescoui-changes"}, {"version", 2}, {"name", "renames"}, {"revisions", revisions}};
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

/// @brief The Kanban template's JSON document (the template the Launcher shows is its program, whose nodes the
/// program builds, so a revision can't set them).
/// @param context The context.
/// @return The document.
json kanbanDocument(const Context& context) {
	return screenDocument(context.examples() / "flow/json/index.screen.json");
}

} // namespace

std::vector<Workload> historyWorkloads(const Context& context) {
	const json document = kanbanDocument(context);
	const auto large = std::make_shared<History>(document, largeHistory); // the commit runs add to it: blame reads all
	std::vector<Workload> workloads;
	workloads.push_back(commitWorkload(std::make_shared<History>(document, 100), 100));
	workloads.push_back(commitWorkload(large, largeHistory));
	readWorkloads(large, workloads);
	workloads.push_back(undoIndependentWorkload(document, 100));
	workloads.push_back(undoIndependentWorkload(document, largeHistory));
	workloads.push_back(undoPlanWorkload(document));
	workloads.push_back(loadWorkload(document));
	return workloads;
}

} // namespace affresco::bench
