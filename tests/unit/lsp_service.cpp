/// @file
/// @brief The `lsp` service: a language server's results turned into bounded shapes counted from 0, URIs kept inside
/// the granted folder, malformed answers from an untrusted server (many of them random) that must neither throw past
/// the refusals nor leave the limits; and a screen driving the fake server (tests/lsp/fake_server.py) through
/// initialization, document sync, requests, settings and the diagnostics stream.

#include "lsp/LspProtocol.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
namespace lsp = affresco::services::lsp;

namespace {

const std::filesystem::path root = "/folder"; ///< The folder of the normaliser's tests: only names are compared.

/// @brief The fake server (tests/lsp), by an absolute path, as the build gives it.
const std::filesystem::path fakeServer = AFFRESCOUI_FAKE_LANGUAGE_SERVER;

/// @brief A range from 0, as servers send it.
/// @param line The line.
/// @param start The first character.
/// @param end The character after the last.
/// @return The range.
json range(int line, int start, int end) {
	return {{"start", {{"line", line}, {"character", start}}}, {"end", {{"line", line}, {"character", end}}}};
}

/// @brief A location of a file of the folder, or of any URI.
/// @param uri The URI.
/// @param line The line, from 0.
/// @param character The character, from 0.
/// @return The location.
json location(const std::string& uri, int line, int character) {
	return {{"uri", uri}, {"range", range(line, character, character + 4)}};
}

/// @brief A random JSON value, nested at most `depth` deep: what a broken or hostile server might send.
/// @param random The generator.
/// @param depth How deep it may nest.
/// @return The value.
json randomValue(std::mt19937& random, int depth) {
	static const char* const keys[] = {"uri", "range", "start", "end", "line", "character", "contents", "items", "label",
	                                   "changes", "documentChanges", "newText", "data", "legend", "diagnostics",
	                                   "children", "location", "severity", "kind", "edits", "value", "signatures"};
	switch(std::uniform_int_distribution<int>(0, depth > 0 ? 7 : 4)(random)) {
	case 0: return nullptr;
	case 1: return std::uniform_int_distribution<int>(-3, 1 << 20)(random);
	case 2: return std::uniform_int_distribution<int>(0, 1)(random) == 1;
	case 3: return "file:///folder/" + std::to_string(random() % 7);
	case 4: return std::string(random() % 40, static_cast<char>('a' + random() % 26));
	case 5:
	case 6: {
		json object = json::object();
		for(auto n = static_cast<unsigned>(random() % 6); n > 0; --n)
			object[keys[random() % std::size(keys)]] = randomValue(random, depth - 1);
		return object;
	}
	default: {
		json list = json::array();
		for(unsigned n = random() % 8; n > 0; --n) list.push_back(randomValue(random, depth - 1));
		return list;
	}
	}
}

/// @brief An executor whose posted tasks the test runs by hand, on the main thread.
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
	std::condition_variable posted_;           ///< Signalled when a task is posted.
	std::vector<std::function<void()>> tasks_; ///< Tasks not yet run.
};

/// @brief A folder of its own under the system's temporary folder, with a file to open, removed at the end.
struct TemporaryFolder {
	std::filesystem::path path = std::filesystem::temp_directory_path() / "affrescoui-lsp-test"; ///< The folder.
	TemporaryFolder() {
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path / "work");
		std::ofstream(path / "work" / "main.cpp") << "int main() {\n\treturn FIXME;\n}\n";
		std::ofstream(path / "secret.cpp") << "int secret;\n";
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

/// @brief A screen that asks the fake server, one action per request.
constexpr const char* languageScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"services": { "lang": { "provider": "lsp", "process": "server", "root": "work", "language": "cpp",
	                        "settings": { "fake": { "style": "tabs" } } } },
	"state": { "published": [], "hover": null, "items": null, "version": null, "same": null, "rename": null,
	           "format": null, "symbols": null, "found": null, "tokens": null, "code": "" },
	"actions": {
		"listen": [{ "subscribe": "lang.diagnostics", "append": "state.published", "id": "published" }],
		"hover": [{ "invoke": "lang.hover", "with": { "path": "main.cpp", "line": 0, "column": 4 }, "into": "state.hover" }],
		"complete": [{ "invoke": "lang.completion",
		               "with": { "path": "main.cpp", "text": "int main() {\n\tpu\n}\n", "line": 1, "column": 3 },
		               "into": "state.items" }],
		"sync": [{ "invoke": "lang.sync", "with": { "path": "main.cpp", "text": "x FIXME\n" }, "into": "state.version" }],
		"again": [{ "invoke": "lang.sync", "with": { "path": "main.cpp", "text": "x FIXME\n" }, "into": "state.same" }],
		"rename": [{ "invoke": "lang.rename", "with": { "path": "main.cpp", "line": 0, "column": 4, "newName": "start" },
		             "into": "state.rename" }],
		"format": [{ "invoke": "lang.format", "with": { "path": "main.cpp" }, "into": "state.format" }],
		"symbols": [{ "invoke": "lang.symbols", "with": { "path": "main.cpp" }, "into": "state.symbols" }],
		"find": [{ "invoke": "lang.workspaceSymbols", "with": { "query": "main" }, "into": "state.found" }],
		"tokens": [{ "invoke": "lang.tokens", "with": { "path": "main.cpp" }, "into": "state.tokens" }],
		"escape": [{ "invoke": "lang.hover", "with": { "path": "../secret.cpp", "line": 0, "column": 0 },
		             "onError": [{ "set": "state.code", "to": "{{error.code}}" }] }]
	},
	"root": { "type": "label", "text": "language" }
})json";

/// @brief A screen with the fake server granted, the folder added and an executor set.
struct Session {
	TemporaryFolder folder;                                                           ///< The granted folder.
	PumpedExecutor executor;                                                          ///< Runs what the service posts.
	ManualClock clock;                                                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(200, 100, clock); ///< A window with no screen.
	affresco::Screen screen = affresco::Screen::fromString(languageScreen);           ///< The screen.

	Session() {
		screen.services().allowProcess("server", "/usr/bin/env", {"python3", fakeServer.string()});
		screen.services().addRoot("work", folder.path / "work");
		screen.setExecutor(&executor);
		screen.build(*window);
	}
	/// @brief Runs an action and pumps until a condition on the state holds.
	/// @param action The action.
	/// @param done The condition.
	/// @return Whether it held in time.
	bool run(const char* action, const std::function<bool()>& done) {
		screen.runAction(action, window->root());
		return executor.pumpUntil(done);
	}
	/// @brief The value at a path of the state.
	/// @param pointer The path.
	/// @return The value.
	json at(const char* pointer) const { return json::parse(screen.stateJson()).at(json::json_pointer(pointer)); }
	/// @brief Whether the state has a value at a path that isn't null.
	/// @param pointer The path.
	/// @return Whether it has.
	bool has(const char* pointer) const { return !at(pointer).is_null(); }
	/// @brief The last diagnostics published for a file.
	/// @param path The file.
	/// @return Its event, or null.
	json published(const std::string& path) const {
		json last;
		for(const json& event : at("/published"))
			if(event.value("path", "") == path) last = event;
		return last;
	}
};

} // namespace

TEST_SUITE("lsp service") {
	TEST_CASE("URIs stay inside the folder") {
		CHECK(lsp::uriOf(root, "src/main.cpp") == "file:///folder/src/main.cpp");
		CHECK(lsp::uriOf(root, "") == "file:///folder");
		CHECK(lsp::uriOf(root, "a b/ü.py") == "file:///folder/a%20b/%C3%BC.py");
		CHECK(lsp::pathOfUri(root, "file:///folder/src/main.cpp") == "src/main.cpp");
		CHECK(lsp::pathOfUri(root, "file:///folder/a%20b/%C3%BC.py") == "a b/ü.py");
		CHECK(lsp::pathOfUri(root, "file://localhost/folder/x.cpp") == "x.cpp");
		CHECK_FALSE(lsp::pathOfUri(root, "file:///folder/../etc/passwd"));
		CHECK_FALSE(lsp::pathOfUri(root, "file:///folder%2F..%2Fetc"));
		CHECK_FALSE(lsp::pathOfUri(root, "file:///elsewhere/x.cpp"));
		CHECK_FALSE(lsp::pathOfUri(root, "file:///folderx/x.cpp"));
		CHECK_FALSE(lsp::pathOfUri(root, "file://host/folder/x.cpp"));
		CHECK_FALSE(lsp::pathOfUri(root, "https://example.com/folder/x.cpp"));
		CHECK_FALSE(lsp::pathOfUri(root, "file:///folder"));
	}

	TEST_CASE("diagnostics: from 0, an empty range kept, codes as text, outside files refused") {
		const json params = {
		    {"uri", "file:///folder/src/main.cpp"},
		    {"diagnostics",
		     {{{"range", range(2, 4, 9)}, {"severity", 1}, {"message", "bad"}, {"source", "fake"}, {"code", 42}},
		      {{"range", range(0, 3, 3)}, {"severity", 3}, {"message", "note"}}}}};
		const json shown = lsp::normalize("diagnostics", params, root);
		CHECK(shown["path"] == "src/main.cpp");
		REQUIRE(shown["diagnostics"].size() == 2);
		const json& first = shown["diagnostics"][0];
		CHECK(first["severity"] == "error");
		CHECK(first["line"] == 2);
		CHECK(first["column"] == 4);
		CHECK(first["endColumn"] == 9);
		CHECK(first["message"] == "bad");
		CHECK(first["source"] == "fake");
		CHECK(first["code"] == "42");
		CHECK(shown["diagnostics"][1]["severity"] == "info");
		CHECK(shown["diagnostics"][1]["endColumn"] == 3);
		CHECK(shown["truncated"] == false);
		CHECK_THROWS_AS((void)lsp::normalize("diagnostics", {{"uri", "file:///etc/x"}, {"diagnostics", json::array()}}, root),
		                std::runtime_error);
	}

	TEST_CASE("completion: snippets as plain text, cut to the limit") {
		const json result = {{"isIncomplete", true},
		                     {"items",
		                      {{{"label", "puts"}, {"insertTextFormat", 2}, {"insertText", "puts(${1:text}, $2)$0"}},
		                       {{"label", ""}},
		                       {{"label", "printf"}, {"textEdit", {{"newText", "printf"}, {"range", range(0, 0, 2)}}}}}}};
		const json shown = lsp::normalize("completion", result, root);
		REQUIRE(shown["items"].size() == 2); // the one with no label is left out
		CHECK(shown["items"][0]["insert"] == "puts(text, )");
		CHECK(shown["items"][1]["insert"] == "printf");
		CHECK(shown["incomplete"] == true);
		lsp::Limits limits;
		limits.completions = 1;
		CHECK(lsp::normalize("completion", result, root, {}, limits)["truncated"] == true);
		CHECK(lsp::plainSnippet("a\\$b ${2:x${3:y}}") == "a$b xy");
	}

	TEST_CASE("definitions and references: from 0, outside the folder dropped") {
		const json result = json::array({location("file:///folder/a.cpp", 0, 4), location("file:///elsewhere/b.cpp", 1, 1),
		                                  {{"targetUri", "file:///folder/c.cpp"}, {"targetSelectionRange", range(5, 2, 3)}}});
		const json shown = lsp::normalize("references", result, root);
		REQUIRE(shown["locations"].size() == 2);
		CHECK(shown["locations"][0]["path"] == "a.cpp");
		CHECK(shown["locations"][0]["column"] == 4);
		CHECK(shown["locations"][1]["path"] == "c.cpp");
		CHECK(shown["locations"][1]["line"] == 5);
		CHECK(lsp::normalize("definition", location("file:///folder/a.cpp", 3, 0), root)["locations"].size() == 1);
	}

	TEST_CASE("rename: every edit inside the folder, or none") {
		const json inside = {{"changes", {{"file:///folder/a.cpp", {{{"range", range(0, 4, 8)}, {"newText", "start"}}}},
		                                  {"file:///folder/b.cpp", {{{"range", range(1, 0, 4)}, {"newText", "start"}}}}}}};
		const json shown = lsp::normalize("rename", inside, root);
		CHECK(shown["files"] == 2);
		REQUIRE(shown["edits"].size() == 2);
		CHECK(shown["edits"][0]["path"] == "a.cpp");
		CHECK(shown["edits"][0]["column"] == 4);
		CHECK(shown["edits"][0]["text"] == "start");
		json outside = inside;
		outside["changes"]["file:///elsewhere/c.cpp"] = json::array({{{"range", range(0, 0, 1)}, {"newText", "x"}}});
		CHECK_THROWS_AS((void)lsp::normalize("rename", outside, root), std::runtime_error);
		const json creates = {{"documentChanges", {{{"kind", "create"}, {"uri", "file:///folder/new.cpp"}}}}};
		CHECK_THROWS_AS((void)lsp::normalize("rename", creates, root), std::runtime_error);
	}

	TEST_CASE("symbols nest, workspace symbols name their files") {
		const json document = json::array(
		    {{{"name", "main"}, {"kind", 12}, {"range", range(0, 0, 9)}, {"selectionRange", range(0, 4, 8)},
		      {"children", {{{"name", "count"}, {"kind", 13}, {"range", range(1, 1, 9)}, {"selectionRange", range(1, 5, 9)}}}}}});
		const json shown = lsp::normalize("symbols", document, root, "a.cpp");
		REQUIRE(shown["symbols"].size() == 2);
		CHECK(shown["symbols"][1]["depth"] == 1);
		CHECK(shown["symbols"][1]["path"] == "a.cpp");
		const json workspace = json::array({{{"name", "main"}, {"kind", 12}, {"location", location("file:///folder/b.cpp", 2, 0)}},
		                                    {{"name", "hidden"}, {"kind", 12}, {"location", location("file:///etc/x", 0, 0)}}});
		const json found = lsp::normalize("workspaceSymbols", workspace, root);
		REQUIRE(found["symbols"].size() == 1);
		CHECK(found["symbols"][0]["path"] == "b.cpp");
		CHECK(found["symbols"][0]["line"] == 2);
	}

	TEST_CASE("semantic tokens: relative positions, the legend's types as the grammar's styles") {
		const json legend = {{"tokenTypes", {"type", "function", "macro"}}, {"tokenModifiers", json::array()}};
		const json shown = lsp::normalize(
		    "tokens", {{"data", {0, 0, 3, 0, 0, 0, 4, 4, 1, 0, 2, 1, 5, 2, 0}}, {"legend", legend}}, root, "a.cpp");
		const json& tokens = shown["tokens"];
		REQUIRE(tokens.size() == 3);
		CHECK(tokens[1]["column"] == 4);
		CHECK(tokens[2]["line"] == 2);
		CHECK(tokens[2]["column"] == 1);
		CHECK(tokens[0]["style"] == lsp::styleOfToken("type"));
		CHECK(lsp::normalize("tokens", {{"data", {0, 0, 3, 99, 0}}, {"legend", legend}}, root, "a.cpp")["tokens"].empty());
	}

	TEST_CASE("columns: UTF-16 units and code points, past the end too") {
		const lsp::ColumnMapper halve = [](const std::string&, std::size_t, std::size_t column) { return column / 2; };
		const json shown = lsp::normalize("definition", location("file:///folder/a.cpp", 0, 8), root, {}, {}, halve);
		CHECK(shown["locations"][0]["column"] == 4);
		const std::string line = "a\xC3\xA9\xF0\x9F\x98\x80" "b"; // a, é, an emoji (two units), b
		CHECK(lsp::utf16Units(line, 2) == 2);
		CHECK(lsp::utf16Units(line, 3) == 4);
		CHECK(lsp::utf16Units(line, 6) == 7);
		CHECK(lsp::codePointsOf(line, 4) == 3);
		CHECK(lsp::codePointsOf(line, 5) == 4);
		CHECK(lsp::codePointsOf(line, 7) == 6);
	}

	TEST_CASE("texts are cut on a character boundary") {
		CHECK(lsp::cut("abc", 5) == "abc");
		const std::string cut = lsp::cut("ééééé", 5);
		CHECK(cut.size() <= 5 + 3);
		CHECK(cut.find("é") == 0);
		CHECK((static_cast<unsigned char>(cut[4]) & 0xC0) != 0x80);
	}

	TEST_CASE("an unknown kind is refused") {
		CHECK_THROWS_AS((void)lsp::normalize("everything", json::object(), root), std::invalid_argument);
	}

	TEST_CASE("random answers are refused or kept within the limits") {
		std::mt19937 random(8);
		lsp::Limits limits;
		limits.diagnostics = limits.completions = limits.edits = limits.locations = limits.tokens = 5;
		limits.text = limits.documentation = 16;
		static const char* const kinds[] = {"hover",  "completion", "signature", "definition",       "references", "rename",
		                                    "format", "symbols",    "workspaceSymbols", "tokens", "diagnostics"};
		for(std::size_t n = 0; n < 4000; ++n) {
			const char* kind = kinds[n % std::size(kinds)];
			const json result = randomValue(random, 5);
			json shown;
			try {
				shown = lsp::normalize(kind, result, root, "a.cpp", limits);
			} catch(const std::runtime_error&) {
				continue; // a refused edit, or diagnostics of no file of the folder
			}
			for(const char* list : {"items", "locations", "edits", "symbols", "tokens", "diagnostics"})
				if(shown.is_object() && shown.contains(list)) CHECK(shown[list].size() <= 5);
			if(shown.is_object() && shown.contains("text")) CHECK(shown["text"].get<std::string>().size() <= 16 + 3);
		}
	}

	TEST_CASE("symbols nested past the depth are cut") {
		json symbol = {{"name", "leaf"}, {"kind", 12}, {"range", range(0, 0, 1)}, {"selectionRange", range(0, 0, 1)}};
		for(int n = 0; n < 200; ++n)
			symbol = {{"name", "s"}, {"kind", 12}, {"range", range(0, 0, 1)}, {"selectionRange", range(0, 0, 1)},
			          {"children", json::array({symbol})}};
		const json shown = lsp::normalize("symbols", json::array({symbol}), root, "a.cpp");
		for(const json& item : shown["symbols"]) CHECK(item["depth"].get<int>() < lsp::Limits{}.depth + 1);
	}

#ifndef _WIN32
	TEST_CASE("a screen asks the fake server") {
		Session session;
		REQUIRE(session.run("listen", [] { return true; }));
		// The first call initializes the server, opens the file from the disk and gets its diagnostics
		REQUIRE(session.run("hover", [&] { return session.has("/hover") && !session.published("main.cpp").is_null(); }));
		CHECK(session.at("/hover/text").get<std::string>().find("The program starts here.") != std::string::npos);
		const json fixme = session.published("main.cpp")["diagnostics"];
		REQUIRE(fixme.size() == 1);
		CHECK(fixme[0]["line"] == 1);
		CHECK(fixme[0]["column"] == 8);
		CHECK(fixme[0]["severity"] == "error");
		// The server asked for its settings: the `fake` section of the configuration
		REQUIRE(session.executor.pumpUntil([&] { return !session.published("settings.json").is_null(); }));
		CHECK(json::parse(session.published("settings.json")["diagnostics"][0]["message"].get<std::string>()) ==
		      json::array({{{"style", "tabs"}}}));

		// A text the screen gives is synced first; the same text again keeps its version
		REQUIRE(session.run("complete", [&] { return session.has("/items"); }));
		CHECK(session.at("/items/items/1/insert") == "puts(text)");
		REQUIRE(session.run("sync", [&] { return session.has("/version"); }));
		CHECK(session.at("/version/version") == 3);
		REQUIRE(session.executor.pumpUntil([&] {
			const json diagnostics = session.published("main.cpp")["diagnostics"];
			return diagnostics.size() == 1 && diagnostics[0]["column"] == 2;
		}));
		REQUIRE(session.run("again", [&] { return session.has("/same"); }));
		CHECK(session.at("/same/version") == 3);

		REQUIRE(session.run("rename", [&] { return session.has("/rename"); }));
		CHECK(session.at("/rename/files") == 1);
		CHECK(session.at("/rename/edits").size() == 1); // the fake names one file twice: its last edit
		CHECK(session.at("/rename/edits/0/text") == "start");
		REQUIRE(session.run("format", [&] { return session.has("/format"); }));
		CHECK(session.at("/format/edits/0/text") == "// formatted\n");
		REQUIRE(session.run("symbols", [&] { return session.has("/symbols"); }));
		CHECK(session.at("/symbols/symbols").size() == 2);
		REQUIRE(session.run("find", [&] { return session.has("/found"); }));
		CHECK(session.at("/found/symbols/0/path") == "src/main.cpp");
		REQUIRE(session.run("tokens", [&] { return session.has("/tokens"); }));
		REQUIRE(session.at("/tokens/tokens").size() == 2);
		CHECK(session.at("/tokens/tokens/1/column") == 4);
		CHECK(session.at("/tokens/tokens/1/style") == lsp::styleOfToken("function"));

		// A file outside the folder is refused before the server hears of it
		REQUIRE(session.run("escape", [&] { return !session.at("/code").get<std::string>().empty(); }));
		CHECK(session.at("/code") == "refused");
	}
#endif
}
