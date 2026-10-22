/// @file
/// @brief The file watcher: ignore globs, a DirectoryModel's listing and its change ranges as files come and go, and
/// the `watch` provider's stream, confined to its root.

#include "Providers.hpp"
#include "files/Watcher.hpp"

#include <affresco/services/DirectoryModel.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

using affresco::json;
using affresco::ModelChange;
using affresco::services::DirectoryModel;
using affresco::services::globMatches;

namespace {

/// @brief An executor whose posted tasks the test runs by hand, on the main thread.
class PumpedExecutor final : public affresco::Executor {
public:
	/// @brief Queues a task.
	/// @param task The task.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		posted_.notify_all();
	}
	/// @brief Runs posted tasks until a condition holds.
	/// @param done The condition, checked after each batch.
	/// @param most How long at most.
	/// @return Whether it held in time.
	bool pumpUntil(const std::function<bool()>& done, std::chrono::milliseconds most = std::chrono::seconds(10)) {
		const auto end = std::chrono::steady_clock::now() + most;
		while(!done()) {
			std::vector<std::function<void()>> tasks;
			{
				std::unique_lock lock(mutex_);
				if(!posted_.wait_until(lock, end, [this] { return !tasks_.empty(); })) return done();
				tasks.swap(tasks_);
			}
			for(const auto& task : tasks) task();
		}
		return true;
	}

private:
	std::mutex mutex_;                         ///< Guards tasks_.
	std::condition_variable posted_;           ///< Signalled at each post.
	std::vector<std::function<void()>> tasks_; ///< Waiting to run.
};

/// @brief A folder of its own under the system's temporary folder (canonical, as roots are), removed at the end.
struct TemporaryFolder {
	std::filesystem::path path; ///< The folder.
	/// @brief Makes it, empty.
	/// @param name Its name.
	explicit TemporaryFolder(const char* name) {
		const std::filesystem::path made = std::filesystem::temp_directory_path() / name;
		std::filesystem::remove_all(made);
		std::filesystem::create_directories(made);
		path = std::filesystem::canonical(made);
	}
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	TemporaryFolder(TemporaryFolder&&) = delete;
	TemporaryFolder& operator=(TemporaryFolder&&) = delete;
};

/// @brief Writes a text file.
/// @param path Where.
/// @param text What.
void writeText(const std::filesystem::path& path, const std::string& text) { std::ofstream(path) << text; }

/// @brief The paths of a model's rows, in order.
/// @param model The model.
/// @return The paths.
std::vector<std::string> paths(const DirectoryModel& model) {
	std::vector<std::string> all;
	for(std::size_t index = 0; index < model.size(); ++index) all.push_back(model.entry(index).path);
	return all;
}

/// @brief A subscription the test reads: its events, collected under a lock.
class StreamCall final : public affresco::services::Call {
public:
	const json& arguments() const noexcept override { return arguments_; }
	std::stop_token stopToken() const noexcept override { return stop_.get_token(); }
	void progress(json) override {}
	void event(json event) override {
		const std::scoped_lock lock(mutex_);
		events_.push_back(std::move(event));
	}
	void finish(json) override {}
	void fail(std::string, std::string, json) override {}
	/// @brief The paths the events named so far.
	/// @return The paths.
	std::vector<std::string> paths() {
		const std::scoped_lock lock(mutex_);
		std::vector<std::string> all;
		for(const json& event : events_)
			for(const json& change : event.at("changes")) all.push_back(change.at("path").get<std::string>());
		return all;
	}

private:
	json arguments_ = json::object(); ///< No arguments.
	std::stop_source stop_;           ///< Never asked.
	std::mutex mutex_;                ///< Guards events_.
	std::vector<json> events_;        ///< Those heard.
};

/// @brief Waits for a condition, polling.
/// @param done The condition.
/// @return Whether it held within 10 seconds.
bool waitFor(const std::function<bool()>& done) {
	const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while(!done())
		if(std::chrono::steady_clock::now() > end) return false;
		else std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return true;
}

} // namespace

TEST_CASE("fs watch: ignore globs match a name anywhere, or a whole path") {
	CHECK(globMatches("node_modules", "src/node_modules"));
	CHECK(globMatches("*.o", "build/main.o"));
	CHECK_FALSE(globMatches("*.o", "main.oo"));
	CHECK(globMatches("build/**", "build/a/b.o"));
	CHECK_FALSE(globMatches("build/*", "build/a/b.o"));
	CHECK(globMatches("**/*.log", "a/b/c.log"));
	CHECK(globMatches("?.txt", "a.txt"));
	CHECK_FALSE(globMatches("src/*.cpp", "lib/src/a.cpp"));
}

TEST_CASE("fs watch: globs keep their meaning at the edges") {
	CHECK(globMatches("a/**/b", "a/b"));
	CHECK(globMatches("a/**/b", "a/x/y/b"));
	CHECK_FALSE(globMatches("a/**/b", "a/xb"));
	CHECK(globMatches("a/**", "a/"));
	CHECK(globMatches("**", "a/b/c"));
	CHECK(globMatches("a*/b", "a/b"));
	CHECK_FALSE(globMatches("a*/b", "ax/y/b"));
	CHECK_FALSE(globMatches("a?b", "a/b"));
	CHECK(globMatches("*", ""));
	CHECK_FALSE(globMatches("?", ""));
	CHECK(globMatches(std::string(300, 'a') + "*", std::string(301, 'a'))); // longer than the states on the stack
	CHECK_FALSE(globMatches(std::string(300, 'a') + "?", std::string(300, 'a')));
}

TEST_CASE("fs watch: a pathological ignore glob takes linear time") {
	// A screen gives the globs (up to 64, of up to 200 bytes): one that backtracked would stall the watcher's thread
	std::string glob;
	for(int star = 0; star < 40; ++star) glob += "*a";
	glob += 'b';
	const std::string name(150, 'a');
	std::string deep;
	for(int level = 0; level < 64; ++level) deep += "a/";
	const auto started = std::chrono::steady_clock::now();
	CHECK_FALSE(globMatches(glob, name));
	CHECK_FALSE(globMatches(glob, "src/" + name));
	CHECK_FALSE(globMatches("**/a/**/a/**/a/**/b", deep));
	CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(1));
}

TEST_CASE("fs watch: a directory model lists folders first, by name, and leaves ignored paths out") {
	const TemporaryFolder folder("affrescoui-fs-watch-list");
	std::filesystem::create_directories(folder.path / "src" / "deep");
	std::filesystem::create_directories(folder.path / ".git");
	writeText(folder.path / "b.txt", "b");
	writeText(folder.path / "a.txt", "a");
	writeText(folder.path / "src" / "main.cpp", "int main() {}");
	writeText(folder.path / "src" / "deep" / "x.hpp", "");
	PumpedExecutor executor;
	const auto model = DirectoryModel::open(executor, folder.path, {.ignore = {".git"}, .watch = false});
	REQUIRE(executor.pumpUntil([&] { return model->ready(); }));
	CHECK(paths(*model) ==
	      std::vector<std::string>{"src", "src/deep", "src/deep/x.hpp", "src/main.cpp", "a.txt", "b.txt"});
	CHECK(model->row(3) ==
	      json{{"path", "src/main.cpp"}, {"name", "main.cpp"}, {"kind", "file"}, {"depth", 1}, {"size", 13}});
	CHECK(model->row(0).at("size").is_null());
	CHECK(model->find("src/deep") == std::size_t{1});
	CHECK(model->subtreeEnd(0) == 4);
	CHECK_FALSE(model->find(".git"));
}

TEST_CASE("fs watch: a watched model reports what changes as ranges of rows") {
	const TemporaryFolder folder("affrescoui-fs-watch-change");
	std::filesystem::create_directories(folder.path / "old" / "inner");
	writeText(folder.path / "old" / "inner" / "one.txt", "1");
	writeText(folder.path / "keep.txt", "k");
	PumpedExecutor executor;
	const auto model = DirectoryModel::open(executor, folder.path, {.debounce = std::chrono::milliseconds(20)});
	std::vector<ModelChange> changes;
	(void)model->onChange.connect([&](const ModelChange& change) { changes.push_back(change); });
	REQUIRE(executor.pumpUntil([&] { return model->ready(); }));
	CHECK(model->watching());
	REQUIRE(model->size() == 4);
	changes.clear();

	writeText(folder.path / "new.txt", "n");
	REQUIRE(executor.pumpUntil([&] { return model->find("new.txt").has_value(); }));
	CHECK(paths(*model).back() == "new.txt");
	CHECK(std::ranges::any_of(changes, [](const ModelChange& change) {
		return change.kind == ModelChange::Kind::Inserted && change.first == 4 && change.count == 1;
	}));

	changes.clear();
	std::filesystem::remove_all(folder.path / "old");
	REQUIRE(executor.pumpUntil([&] { return !model->find("old").has_value(); }));
	CHECK(paths(*model) == std::vector<std::string>{"keep.txt", "new.txt"});
	CHECK(std::ranges::any_of(changes, [](const ModelChange& change) {
		return change.kind == ModelChange::Kind::Removed && change.first == 0 && change.count == 3;
	}));

	std::filesystem::create_directories(folder.path / "made" / "sub");
	writeText(folder.path / "made" / "sub" / "f.txt", "f");
	REQUIRE(executor.pumpUntil([&] { return model->find("made/sub/f.txt").has_value(); }));
	CHECK(paths(*model) == std::vector<std::string>{"made", "made/sub", "made/sub/f.txt", "keep.txt", "new.txt"});
}

TEST_CASE("fs watch: the watch provider streams changes under its root, without ignored ones, at most 4 at once") {
	const TemporaryFolder folder("affrescoui-fs-watch-provider");
	PumpedExecutor executor;
	const auto provider = affresco::services::makeWatchProvider();
	const json configuration = {{"root", "work"}, {"ignore", {"*.tmp"}}, {"debounce", 20}};
	auto service = provider->create(configuration, {executor, {folder.path, "work", std::nullopt, ""}, {}, {}});
	const auto call = std::make_shared<StreamCall>();
	REQUIRE_FALSE(service->subscribe("changes", call));
	CHECK(service->subscribe("other", call));
	writeText(folder.path / "scratch.tmp", "");
	writeText(folder.path / "seen.txt", "");
	REQUIRE(waitFor([&] { return std::ranges::contains(call->paths(), std::string("seen.txt")); }));
	CHECK_FALSE(std::ranges::contains(call->paths(), std::string("scratch.tmp")));
	for(const std::string& path : call->paths()) CHECK_FALSE(path.starts_with(".."));
	for(int more = 0; more < 3; ++more) CHECK_FALSE(service->subscribe("changes", std::make_shared<StreamCall>()));
	CHECK(service->subscribe("changes", std::make_shared<StreamCall>()));
}
