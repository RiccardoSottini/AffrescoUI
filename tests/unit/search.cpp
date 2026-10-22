/// @file
/// @brief The search engine and provider (affresco/services/Search.hpp, modules/services/src/Search*): globs and
/// .gitignore rules, a folder searched on several threads within its caps, cancelled, matches replaced, and a screen
/// that finds through the `search` service then writes a replaced file through `fs`; and Forge's Search view natives
/// (examples/forge/features/search), which search and replace in the open folder.

#include "Headless.hpp"
#include "SearchGlob.hpp"
#include "features/search/Search.hpp"

#include <affresco/affresco.hpp>
#include <affresco/services/Search.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <mutex>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace services = affresco::services;
using affresco::json;

namespace {

/// @brief A folder of its own under the system's temporary folder, removed with the object.
struct TempFolder {
	std::filesystem::path path =
	    std::filesystem::temp_directory_path() / ("aui-search-" + std::to_string(std::random_device{}()));

	/// @brief Creates the folder.
	TempFolder() { std::filesystem::create_directories(path); }
	/// @brief Removes it and everything in it.
	~TempFolder() {
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}
	TempFolder(const TempFolder&) = delete;
	TempFolder& operator=(const TempFolder&) = delete;

	/// @brief Writes a file, creating its folders.
	/// @param relative Its path, relative to the folder.
	/// @param text Its bytes.
	void write(const std::string& relative, const std::string& text) const {
		const std::filesystem::path file = path / relative;
		std::filesystem::create_directories(file.parent_path());
		std::ofstream(file, std::ios::binary) << text;
	}

	/// @brief Reads a file back.
	/// @param relative Its path, relative to the folder.
	/// @return Its bytes.
	[[nodiscard]] std::string read(const std::string& relative) const {
		std::ifstream in(path / relative, std::ios::binary);
		return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	}
};

/// @brief Whether a glob matches a path.
/// @param glob The glob.
/// @param path The path.
/// @param folder Whether the path is a folder.
/// @return Whether the path, or a folder it is in, matches.
bool globMatches(const char* glob, const char* path, bool folder = false) {
	const std::optional<services::Glob> compiled = services::Glob::compile(glob);
	REQUIRE(compiled);
	return compiled->matchesOrParent(services::splitPath(path), folder);
}

/// @brief A search's results, gathered.
struct Found {
	std::vector<services::SearchFile> files; ///< Sorted by path.
	services::SearchSummary summary;         ///< What it did.
	std::optional<std::string> error;        ///< What it returned.
};

/// @brief Searches a folder.
/// @param root The folder.
/// @param query The query.
/// @param stop Its stop token.
/// @return The results.
Found search(const std::filesystem::path& root, const services::SearchQuery& query, std::stop_token stop = {}) {
	Found found;
	found.error = services::searchFiles(
	    root, query, stop, [&found](services::SearchFile&& file) { found.files.push_back(std::move(file)); },
	    found.summary);
	std::ranges::sort(found.files, {}, &services::SearchFile::path);
	return found;
}

/// @brief The paths of some results.
/// @param found The results.
/// @return Their paths, sorted.
std::vector<std::string> paths(const Found& found) {
	std::vector<std::string> list;
	for(const services::SearchFile& file : found.files) list.push_back(file.path);
	return list;
}

/// @brief A compiled pattern.
/// @param text The pattern.
/// @param options How it reads.
/// @return It.
affresco::TextPattern compiled(const char* text, affresco::PatternOptions options = {}) {
	affresco::TextPattern pattern;
	REQUIRE_FALSE(pattern.compile(text, options));
	return pattern;
}

/// @brief An executor whose posted tasks the test runs by hand, on the main thread (as services.cpp's).
class PumpedExecutor final : public affresco::Executor {
public:
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		posted_.notify_all();
	}
	/// @brief Runs posted tasks until a condition holds.
	/// @param done The condition, checked after each batch.
	/// @return Whether it held within ten seconds.
	bool pumpUntil(const std::function<bool()>& done) {
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
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
	std::condition_variable posted_;           ///< Told when a task is posted.
	std::vector<std::function<void()>> tasks_; ///< Posted, not run yet.
};

/// @brief A screen that finds "needle" through the `search` service, then replaces it in a.txt and writes that
/// through `fs`.
constexpr const char* searchScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"services": {
		"search": { "provider": "search", "root": "work" },
		"files": { "provider": "fs", "root": "work" }
	},
	"state": { "events": [], "result": null, "error": "", "written": false },
	"actions": {
		"find": [{ "invoke": "search.find", "with": { "pattern": "needle" }, "append": "state.events",
		           "onDone": [{ "set": "state.result", "to": "{{result}}" }],
		           "onError": [{ "set": "state.error", "to": "{{error.message}}" }] }],
		"replace": [{ "invoke": "search.replace", "with": { "path": "a.txt", "pattern": "needle", "replacement": "pin" },
		              "onError": [{ "set": "state.error", "to": "{{error.message}}" }],
		              "onDone": [{ "invoke": "files.write", "with": { "path": "a.txt", "text": "{{result.text}}" },
		                           "onDone": [{ "set": "state.written", "to": true }] }] }],
		"escape": [{ "invoke": "search.replace", "with": { "path": "../x.txt", "pattern": "a", "replacement": "b" },
		             "onError": [{ "set": "state.error", "to": "{{error.message}}" }] }]
	},
	"root": { "type": "label", "text": "search" }
})json";

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).at(json::json_pointer(pointer));
}

} // namespace

TEST_CASE("search: globs match names at any depth, anchored paths, classes and folders") {
	CHECK(globMatches("*.cpp", "a.cpp"));
	CHECK(globMatches("*.cpp", "src/deep/a.cpp"));
	CHECK_FALSE(globMatches("*.cpp", "a.hpp"));
	CHECK(globMatches("src/**/*.cpp", "src/a.cpp"));
	CHECK(globMatches("src/**/*.cpp", "src/x/y/a.cpp"));
	CHECK_FALSE(globMatches("src/**/*.cpp", "lib/src/a.cpp"));
	CHECK(globMatches("/lib", "lib/a.txt")); // a folder it is in
	CHECK_FALSE(globMatches("/lib", "src/lib/a.txt"));
	CHECK(globMatches("?.txt", "é.txt")); // one code point
	CHECK(globMatches("[a-c]x", "bx"));
	CHECK_FALSE(globMatches("[!a-c]x", "bx"));
	CHECK(globMatches("[^a-c]x", "dx"));
	CHECK(globMatches("\\*.md", "*.md"));
	CHECK_FALSE(globMatches("\\*.md", "a.md"));
	CHECK(globMatches("[ab", "[ab")); // no ']': a literal
	CHECK(globMatches("build/", "build", true));
	CHECK(globMatches("build/", "build/out.o"));
	CHECK_FALSE(globMatches("build/", "build")); // a file named build
	CHECK_FALSE(services::Glob::compile(""));
	CHECK_FALSE(services::Glob::compile("///"));
	CHECK_FALSE(services::Glob::compile(std::string(services::Glob::maxBytes + 1, 'a')));
}

TEST_CASE("search: a pathological glob takes linear time") {
	const std::string name(200, 'a');
	const std::string glob = "*a*a*a*a*a*a*a*a*a*a*a*a*b";
	const std::string path = std::string(60, 'x').append("/").append(name);
	std::string deep;
	for(int level = 0; level < 64; ++level) deep += "a/";
	const auto started = std::chrono::steady_clock::now();
	CHECK_FALSE(globMatches(glob.c_str(), path.c_str()));
	CHECK_FALSE(globMatches("**/a/**/a/**/a/**/b", deep.c_str()));
	CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(1));
}

TEST_CASE("search: glob lists are capped") {
	services::GlobList list;
	CHECK_FALSE(list.compile(" *.cpp , ,src/ "));
	CHECK(list.matches(services::splitPath("src/a.txt"), false));
	CHECK(list.matches(services::splitPath("b.cpp"), false));
	CHECK_FALSE(list.matches(services::splitPath("b.txt"), false));
	std::string many;
	for(int glob = 0; glob <= static_cast<int>(services::GlobList::maxGlobs); ++glob) many += "a,";
	CHECK(list.compile(many).has_value());
	CHECK(list.compile(std::string(300, 'a')).has_value());
}

TEST_CASE("search: .gitignore rules apply under their folder, the last one that matches wins") {
	services::IgnoreRules rules;
	rules.add("", "# comment\n*.log\n!keep.log\n/build/\n\\#hash\n  \n");
	rules.add("docs", "*.tmp\n/top.txt\n");
	const auto ignored = [&rules](const char* path, bool folder = false) {
		return rules.ignored(services::splitPath(path), folder);
	};
	CHECK(ignored("a.log"));
	CHECK(ignored("src/a.log"));
	CHECK_FALSE(ignored("keep.log"));
	CHECK(ignored("build", true));
	CHECK_FALSE(ignored("build"));           // a file
	CHECK_FALSE(ignored("src/build", true)); // anchored to the root
	CHECK(ignored("#hash"));
	CHECK(ignored("docs/a.tmp"));
	CHECK(ignored("docs/deep/a.tmp"));
	CHECK_FALSE(ignored("a.tmp")); // outside docs
	CHECK(ignored("docs/top.txt"));
	CHECK_FALSE(ignored("docs/deep/top.txt"));
	CHECK(rules.size() == 6);
}

TEST_CASE("search: a folder's matches, with lines, columns and previews, filtered") {
	const TempFolder folder;
	folder.write("a.txt", "one needle\ntwo\nneedle and needle\n");
	folder.write("src/b.cpp", "// needle\n");
	folder.write("src/c.log", "needle\n");
	folder.write(".gitignore", "*.log\n");
	folder.write(".git/HEAD", "needle\n");
	folder.write("bin.dat", std::string("needle\0", 7));
	folder.write("big.txt", std::string(100, 'x') + "needle");
	const Found found = search(folder.path, {.pattern = "needle", .maxFileBytes = 64});
	REQUIRE_FALSE(found.error);
	CHECK(paths(found) == std::vector<std::string>{"a.txt", "src/b.cpp"});
	const std::vector<services::SearchMatch>& matches = found.files[0].matches;
	REQUIRE(matches.size() == 3);
	CHECK(matches[0].line == 1);
	CHECK(matches[0].column == 4);
	CHECK(matches[0].text == "one needle");
	CHECK(matches[2].line == 3);
	CHECK(matches[2].column == 11);
	CHECK(matches[2].text.substr(matches[2].at, matches[2].length) == "needle");
	CHECK(found.summary.matches == 4);
	CHECK(found.summary.files == 2);
	CHECK(found.summary.skipped == 2); // binary, too large
	CHECK_FALSE(found.summary.truncated);

	CHECK(paths(search(folder.path, {.pattern = "needle", .include = "*.cpp"})) ==
	      std::vector<std::string>{"src/b.cpp"});
	CHECK(paths(search(folder.path, {.pattern = "needle", .exclude = "src"})) ==
	      std::vector<std::string>{"a.txt", "big.txt"});
	CHECK(paths(search(folder.path, {.pattern = "needle", .folder = "src", .gitignore = false})) ==
	      std::vector<std::string>{"src/b.cpp", "src/c.log"});
	CHECK(search(folder.path, {.pattern = "NEEDLE", .options = {.caseSensitive = true}}).files.empty());
	CHECK(search(folder.path, {.pattern = "need", .options = {.wholeWord = true}}).files.empty());
	CHECK(search(folder.path, {.pattern = "^two$", .options = {.regex = true}}).summary.matches == 1);
}

TEST_CASE("search: the wrong pattern, glob or folder is refused") {
	const TempFolder folder;
	CHECK(search(folder.path, {.pattern = "(", .options = {.regex = true}}).error.value_or("").starts_with("pattern"));
	CHECK(search(folder.path, {.pattern = "a", .include = std::string(300, 'x')})
	          .error.value_or("")
	          .starts_with("include"));
	CHECK(search(folder.path, {.pattern = "a", .folder = ".."}).error.value_or("").starts_with("folder"));
	CHECK(search(folder.path, {.pattern = "a", .folder = "/etc"}).error.value_or("").starts_with("folder"));
	CHECK(search(folder.path / "missing", {.pattern = "a"}).error.has_value());
	const Found empty = search(folder.path, {.pattern = ""});
	CHECK_FALSE(empty.error);
	CHECK(empty.files.empty());
}

TEST_CASE("search: the caps hold, on one thread or many") {
	const TempFolder folder;
	for(int file = 0; file < 40; ++file) folder.write("f" + std::to_string(file) + ".txt", "x x x\n");
	for(const unsigned threads : {1u, 8u}) {
		const Found all = search(folder.path, {.pattern = "x", .threads = threads});
		CHECK(all.summary.matches == 120);
		CHECK(all.summary.searched == 40);
		const Found capped = search(folder.path, {.pattern = "x", .maxResults = 7, .threads = threads});
		CHECK(capped.summary.matches == 7);
		CHECK(capped.summary.truncated);
	}
	const Found few = search(folder.path, {.pattern = "x", .maxFiles = 5});
	CHECK(few.summary.searched == 5);
	CHECK(few.summary.truncated);
}

TEST_CASE("search: a stop request ends it, cancelled") {
	const TempFolder folder;
	folder.write("a.txt", "x\n");
	std::stop_source stop;
	stop.request_stop();
	const Found found = search(folder.path, {.pattern = "x"}, stop.get_token());
	CHECK_FALSE(found.error);
	CHECK(found.summary.cancelled);
	CHECK(found.files.empty());
}

TEST_CASE("search: a long line's preview is the part around the match, cut at code points") {
	const TempFolder folder;
	std::string line;
	for(int index = 0; index < 400; ++index) line += "é";
	folder.write("long.txt", line + "needle" + line + "\n");
	const Found found = search(folder.path, {.pattern = "needle"});
	REQUIRE(found.files.size() == 1);
	const services::SearchMatch& match = found.files[0].matches[0];
	CHECK(match.column == 800);
	CHECK(match.text.size() <= 240);
	CHECK(match.text.substr(match.at, match.length) == "needle");
	CHECK((static_cast<unsigned char>(match.text.front()) & 0xC0) != 0x80);
	folder.write("bad.txt", "\xff needle\n");
	const Found bad = search(folder.path, {.pattern = "needle", .include = "bad.txt"});
	REQUIRE(bad.files.size() == 1);
	CHECK(bad.files[0].matches[0].text == "(not UTF-8 text)");
}

TEST_CASE("search: replaceAll replaces plain text, groups and empty matches, within its caps") {
	services::Replaced replaced;
	CHECK_FALSE(services::replaceAll("a.b.a", compiled("a"), "x", replaced));
	CHECK(replaced.text == "x.b.x");
	CHECK(replaced.count == 2);
	CHECK_FALSE(services::replaceAll("key=value", compiled("(\\w+)=(\\w+)", {.regex = true}), "$2=$1", replaced));
	CHECK(replaced.text == "value=key");
	CHECK_FALSE(services::replaceAll("a\nb", compiled("^", {.regex = true}), "> ", replaced));
	CHECK(replaced.text == "> a\n> b");
	CHECK_FALSE(services::replaceAll("éé", compiled("(?:)", {.regex = true}), "-", replaced));
	CHECK(replaced.text == "-é-é-");
	CHECK(services::replaceAll("aaaa", compiled("a"), "b", replaced, 3).has_value());
	CHECK(services::replaceAll("a", compiled("(a)", {.regex = true}), "$9", replaced).has_value());
	CHECK_FALSE(services::replaceAll("abc", compiled("z"), "y", replaced));
	CHECK(replaced.text == "abc");
	CHECK(replaced.count == 0);
}

TEST_CASE("search: a screen finds through the search service, then writes a replaced file through fs") {
	const TempFolder folder;
	folder.write("work/a.txt", "a needle\n");
	folder.write("work/b/c.txt", "needle needle\n");
	PumpedExecutor executor;
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(searchScreen);
	screen.services().addRoot("work", folder.path / "work");
	screen.setExecutor(&executor);
	screen.build(*window);

	screen.runAction("find", window->root());
	REQUIRE(executor.pumpUntil([&screen] { return !stateAt(screen, "/result").is_null(); }));
	CHECK(stateAt(screen, "/error") == "");
	CHECK(stateAt(screen, "/result/matches") == 3);
	CHECK(stateAt(screen, "/result/files") == 2);
	CHECK(stateAt(screen, "/events").size() == 2);

	screen.runAction("replace", window->root());
	REQUIRE(executor.pumpUntil([&screen] { return stateAt(screen, "/written") == true; }));
	CHECK(folder.read("work/a.txt") == "a pin\n");
	CHECK(folder.read("work/b/c.txt") == "needle needle\n");

	screen.runAction("escape", window->root());
	REQUIRE(executor.pumpUntil([&screen] { return stateAt(screen, "/error") != ""; }));
	CHECK(stateAt(screen, "/error").get<std::string>().find("inside the root") != std::string::npos);
}

TEST_CASE("search: Forge's search builds a results tree by file, skipping dependencies") {
	const TempFolder folder;
	folder.write("src/main.cpp", "int main() {\n\treturn 0;\n}\n");
	folder.write("README.md", "returns nothing\n");
	folder.write("node_modules/lib.js", "return 1;\n");
	folder.write("build/out.txt", "return 2;\n");
	std::size_t partials = 0;
	const json found = forge::searchFolder(
	    folder.path, {{"pattern", "return"}, {"wholeWord", true}}, [] { return false; },
	    [&partials](const json&) { ++partials; });
	CHECK(found["done"] == true);
	CHECK(found["files"] == 1);
	CHECK(found["matches"] == 1);
	REQUIRE(found["tree"].size() == 1);
	CHECK(found["tree"][0]["id"] == "file:src/main.cpp");
	CHECK(found["tree"][0]["label"] == "main.cpp  src  (1)");
	CHECK(found["tree"][0]["children"][0]["id"] == "hit:0");
	CHECK(found["tree"][0]["children"][0]["label"] == "2: return 0;");
	CHECK(found["hits"][0] == json{{"file", "src/main.cpp"}, {"label", "main.cpp"}, {"line", 2}, {"column", 1}});
	CHECK(partials <= 1);

	const json both = forge::searchFolder(
	    folder.path, {{"pattern", "RETURN"}, {"exclude", "src"}}, [] { return false; }, [](const json&) {});
	CHECK(both["tree"].size() == 1);
	CHECK(both["tree"][0]["label"] == "README.md  (1)");
	CHECK(forge::searchFolder(
	          folder.path, {{"pattern", "RETURN"}, {"caseSensitive", true}}, [] { return false; },
	          [](const json&) {})["files"] == 0);
}

TEST_CASE("search: Forge's search refuses what it can't run, and stops when cancelled") {
	const TempFolder folder;
	for(int file = 0; file < 64; ++file)
		folder.write("f" + std::to_string(file) + ".txt", std::string(1000, 'a') + "\n");
	const auto never = [] { return false; };
	const auto ignore = [](const json&) {};
	CHECK_THROWS(forge::searchFolder({}, {{"pattern", "a"}}, never, ignore));
	CHECK_THROWS(forge::searchFolder(folder.path, json::array(), never, ignore));
	CHECK_THROWS(forge::searchFolder(folder.path, {{"pattern", 3}}, never, ignore));
	CHECK_THROWS(forge::searchFolder(folder.path, {{"pattern", std::string(5000, 'a')}}, never, ignore));
	CHECK_THROWS(forge::searchFolder(folder.path, {{"pattern", "("}, {"regex", true}}, never, ignore));
	const json capped = forge::searchFolder(folder.path, {{"pattern", "a"}}, never, ignore, {.results = 10});
	CHECK(capped["matches"] == 10);
	CHECK(capped["truncated"] == true);
	const json stopped = forge::searchFolder(folder.path, {{"pattern", "a"}}, [] { return true; }, ignore);
	CHECK(stopped["done"] == true);
	CHECK(stopped["matches"].get<std::size_t>() <= 2000);
}

TEST_CASE("search: Forge's replace writes the listed files, skipping unsaved and outside ones") {
	const TempFolder folder;
	folder.write("a.txt", "one two one\n");
	folder.write("b.txt", "one\n");
	folder.write("c.txt", "none here\n");
	folder.write("big.txt", std::string(100, 'o') + "one\n");
	const json replaced = forge::replaceInFolder(
	    folder.path,
	    {{"pattern", "(o)ne"},
		 {"regex", true},
		 {"wholeWord", true},
		 {"replacement", "$1NE"},
		 {"files", {"a.txt", "b.txt", "c.txt", "a.txt", "../escape.txt", "missing.txt", "big.txt"}},
		 {"skip", {"b.txt"}}},
	    {.fileBytes = 64});
	CHECK(replaced["replaced"] == 2);
	CHECK(replaced["changed"] == json{"a.txt"});
	CHECK(replaced["skipped"] == json{"b.txt"});
	CHECK(replaced["failed"].size() == 3);
	CHECK(folder.read("a.txt") == "oNE two oNE\n");
	CHECK(folder.read("b.txt") == "one\n");
	CHECK(folder.read("c.txt") == "none here\n");
	CHECK(std::distance(std::filesystem::directory_iterator(folder.path), {}) == 4); // nothing left beside them
	CHECK(!std::filesystem::exists(folder.path.parent_path() / "escape.txt"));
	CHECK_THROWS(forge::replaceInFolder(folder.path, {{"pattern", "o"}, {"files", "a.txt"}}));
	CHECK_THROWS(forge::replaceInFolder(folder.path, {{"pattern", "o"}, {"replacement", std::string(70000, 'x')}}));
	CHECK_THROWS(forge::replaceInFolder({}, {{"pattern", "o"}}));
}

#ifndef _WIN32
// Found in the security review of wave 6: replace wrote through "<file>.forge-replace", so a link left there by
// whoever made the folder (a cloned repository) had the replaced text written over the file it pointed to
TEST_CASE("search: Forge's replace writes nothing through a link planted beside the file") {
	TempFolder folder, elsewhere;
	folder.write("a.txt", "one\n");
	elsewhere.write("target", "untouched");
	std::filesystem::create_symlink(elsewhere.path / "target", folder.path / "a.txt.forge-replace");
	const json replaced =
	    forge::replaceInFolder(folder.path, {{"pattern", "one"}, {"replacement", "two"}, {"files", {"a.txt"}}});
	CHECK(replaced["changed"] == json{"a.txt"});
	CHECK(folder.read("a.txt") == "two\n");
	CHECK(elsewhere.read("target") == "untouched");
	CHECK_FALSE(std::filesystem::is_symlink(folder.path / "a.txt"));
}
#endif
