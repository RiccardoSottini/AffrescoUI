/// @file
/// @brief Unit tests for the console an editor evaluates through (Runtime::console()): what it lands lands as the
/// person's own input, in the file the rule picks, and never more than the step `evaluate` may do.

#include "LispApp.hpp"

#include <affresco/core/ProgramConsole.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

using affresco::ConsoleRequest;
using affresco::ConsoleResult;
using affresco::json;
using affresco::Key;
using affresco::RevisionStatus;
using affresco::vcs::Repository;
using lispapp::createHeadlessWindow;
using lispapp::ManualClock;
using lispapp::Runtime;
using lispapp::WithoutTheScan;
namespace lisp = affresco::lisp;

namespace {

/// The screen's own file: a key that runs what an editor may define.
constexpr const char* kMain = R"lisp(
(defn shown "Shows a value." [v] (set "state.out" v))
(defcommand run-fresh "Runs fresh." :title "F" [] (fresh))
(defkey "ctrl+f" run-fresh)
(defscreen main :state {:out nil} (panel#root))
)lisp";

/// A second file of the program, with a namespace of its own.
constexpr const char* kTools = R"lisp((defn tool "A tool." [a b] (+ a b))
(defn other "Another." [] 2)
)lisp";

/// @brief A screen on a program of two files, app.lisp and tools.lisp, on a headless window: both as the app's own.
struct Console {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	WithoutTheScan rules;                     ///< What revisions are judged by.
	Repository repository;                    ///< The history.
	std::unique_ptr<Runtime> runtime;         ///< The running screen.
	int hits = 0;                             ///< How many times the application's function ran.
	std::vector<std::string> errors;          ///< What was reported.

	/// @brief Builds it.
	/// @param main The screen's file.
	/// @param scan Whether revisions are scanned for new ways to reach control steps.
	explicit Console(const std::string& main = kMain, bool scan = true)
	    : window(createHeadlessWindow(400, 300, clock)), rules(scan), repository(rules, documentOf(main)),
	      runtime(std::make_unique<Runtime>(repository, "")) {
		runtime->onCall("hit", [this](const json&) {
			++hits;
			return json();
		});
		runtime->onError.connect([this](const std::string& message) { errors.push_back(message); });
		runtime->build(*window);
		window->render();
	}

	/// @brief The program's document: the screen's file and tools.lisp.
	/// @param main The screen's file.
	/// @return It.
	static json documentOf(const std::string& main) {
		json document = lisp::sourceDocument(main, "app.lisp");
		document["program"]["tools.lisp"] = lisp::sourceDocument(kTools, "tools.lisp")["program"]["tools.lisp"];
		return document;
	}

	/// @brief Evaluates a text through the console, as an editor does.
	/// @param text The text.
	/// @param file The editor's file.
	/// @param space The editor's namespace.
	/// @return What it gave.
	ConsoleResult evaluate(const std::string& text, const std::string& file = "", const std::string& space = "") const {
		ConsoleRequest request;
		request.text = text;
		request.file = file;
		request.space = space;
		request.origin = "editor";
		ConsoleResult result = runtime->console().evaluate(request);
		window->render();
		return result;
	}

	/// @brief The file a definition is in. @param name Its address. @return The file; "" for none.
	[[nodiscard]] std::string fileOf(const std::string& name) const {
		for(const auto& [file, entries] : repository.document()["program"].items())
			if(entries.contains(name)) return file;
		return "";
	}
};

} // namespace

TEST_CASE("lisp_console: a definition lands as the person's own input, and runs as theirs") {
	Console app;
	const int head = app.repository.head().number;
	const std::string text = R"lisp((defn fresh "Reaches the host." [] (call "hit")))lisp";
	const ConsoleResult result = app.evaluate(text);
	INFO(result.error.value_or(""));
	REQUIRE_FALSE(result.error.has_value());
	CHECK(result.values == std::vector<std::string>{"fresh"});
	CHECK(app.repository.head().number == head + 1);
	CHECK(app.repository.head().author == "user");
	CHECK(app.repository.head().message == "Evaluate fresh (editor)");
	CHECK(app.repository.programAuthors()["evaluated.lisp"]["fresh"] == "user");
	// Own input, not a commit of the person's that the scan judges: an agent can't land the same text
	const auto agents =
	    app.repository.submit({{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/fresh2", text}}}});
	CHECK(agents.status == RevisionStatus::Rejected);
	// The person's key reaches it, and so does the console
	app.window->routeKey({Key::F, 0, affresco::Action::Press, affresco::Mods{lispapp::controlKey}});
	app.window->render();
	app.window->render();
	CHECK(app.hits == 1);
	CHECK(app.evaluate("(fresh)").error == std::nullopt);
	CHECK(app.hits == 2);
}

TEST_CASE("lisp_console: an expression gives its value and leaves the history where it is") {
	const Console app;
	const int head = app.repository.head().number;
	const ConsoleResult result = app.evaluate("(tool 1 2)");
	CHECK(result.values == std::vector<std::string>{"3"});
	CHECK_FALSE(result.error.has_value());
	CHECK(app.repository.head().number == head);
	// One value per form, in order: a definition gives its name
	const ConsoleResult three = app.evaluate("(defn three \"\" [] 3)\n(+ 1 1)\n(three)");
	CHECK(three.values == std::vector<std::string>{"three", "2", "3"});
	CHECK(app.repository.head().number == head + 1);
}

TEST_CASE("lisp_console: a definition goes in the file that has it, else the editor's, else its namespace's") {
	const Console app;
	REQUIRE_FALSE(app.evaluate(R"lisp((defn other "Changed." [] 3))lisp", "app.lisp").error.has_value());
	CHECK(app.fileOf("other") == "tools.lisp");
	REQUIRE_FALSE(app.evaluate(R"lisp((defn by-file "" [] 1))lisp", "tools.lisp").error.has_value());
	CHECK(app.fileOf("by-file") == "tools.lisp");
	REQUIRE_FALSE(app.evaluate(R"lisp((defn by-space "" [] 1))lisp", "", "tools").error.has_value());
	CHECK(app.fileOf("by-space") == "tools.lisp");
	REQUIRE_FALSE(app.evaluate(R"lisp((defn by-none "" [] 1))lisp", "nowhere.lisp", "nowhere").error.has_value());
	CHECK(app.fileOf("by-none") == "evaluated.lisp");
}

TEST_CASE("lisp_console: an error's place is in the editor's buffer") {
	Console app;
	ConsoleRequest request;
	request.text = "(+ 1 1)\n  (+ 1";
	request.line = 40;
	request.column = 3;
	const ConsoleResult unfinished = app.runtime->console().evaluate(request);
	REQUIRE(unfinished.error.has_value());
	CHECK(unfinished.line == 41);
	request.text = "(tool 1 \"a\" 2 3 4)";
	const ConsoleResult wrong = app.runtime->console().evaluate(request);
	REQUIRE(wrong.error.has_value());
	CHECK(wrong.line == 40);
	CHECK(wrong.column >= 3);
	request.text = "(+ 1 1)\n(tool 1 \"a\" 2 3 4)";
	const ConsoleResult second = app.runtime->console().evaluate(request);
	REQUIRE(second.error.has_value());
	CHECK(second.line == 41);
}

TEST_CASE("lisp_console: a whole file lands what changed in one revision, and only that") {
	Console app;
	const int head = app.repository.head().number;
	ConsoleRequest request;
	request.file = "tools.lisp";
	request.wholeFile = true;
	request.text = "(ns tools)\n(defn tool \"A tool.\" [a b] (+ a b))\n(defn other \"Changed.\" [] 5)\n"
	               "(defn added \"\" [] 6)\n";
	ConsoleResult result = app.runtime->console().evaluate(request);
	INFO(result.error.value_or(""));
	REQUIRE_FALSE(result.error.has_value());
	CHECK(result.values == std::vector<std::string>{"tool", "other", "added"});
	CHECK(app.repository.head().number == head + 1);
	CHECK(app.repository.programAuthors()["tools.lisp"]["tool"] == "file"); // unchanged: still the file's
	CHECK(app.repository.programAuthors()["tools.lisp"]["other"] == "user");
	CHECK(app.fileOf("added") == "tools.lisp");
	// The same again changes nothing, and makes no revision
	result = app.runtime->console().evaluate(request);
	CHECK_FALSE(result.error.has_value());
	CHECK(result.values == std::vector<std::string>{"tool", "other", "added"});
	CHECK(app.repository.head().number == head + 1);
	// A form that defines nothing is no file of definitions
	request.text = "(defn quiet \"\" [] 1)\n(call \"hit\")\n";
	result = app.runtime->console().evaluate(request);
	REQUIRE(result.error.has_value());
	CHECK(result.line == 2);
	CHECK(app.fileOf("quiet").empty());
	CHECK(app.hits == 0);
}

TEST_CASE("lisp_console attack: what the step evaluate refuses, the console refuses") {
	// A screen with no program
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	const WithoutTheScan rules(true);
	Repository plain(rules, json::parse(R"({"format": "affrescoui-screen", "version": 2, "root": {"id": "root"}})"));
	Runtime runtime(plain, "");
	runtime.build(*window);
	ConsoleRequest request;
	request.text = "(+ 1 1)";
	CHECK_FALSE(runtime.console().hasProgram());
	CHECK(runtime.console().evaluate(request).error.value_or("").contains("no program"));
	// A proposal previewed
	Console app;
	const auto proposed = app.repository.propose(
	    {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/tools.lisp/other", "(defn other \"\" [] 9)"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	REQUIRE_FALSE(app.repository.preview(proposed.proposal).has_value());
	const int head = app.repository.head().number;
	CHECK(app.evaluate("(defn other \"\" [] 3)").error.value_or("").contains("previewed"));
	app.repository.endPreview();
	CHECK(app.repository.head().number == head);
	// Over the limit
	CHECK(app.evaluate(std::string((std::size_t{4} << 20U) + 1U, ' ')).error.value_or("").contains("longer"));
	// A definition named like a review step
	CHECK(app.evaluate(R"lisp((defn accept "" [] 1))lisp").error.has_value());
	CHECK(app.fileOf("accept").empty());
	CHECK(app.repository.head().number == head);
}

TEST_CASE("lisp_console attack: a deny rule on the person holds for the editor") {
	const Console app(
	    R"lisp((defscreen main :permissions {:allow ["agent"] :deny ["user(/program)"] :reviewers ["studio"]}
  :state {:out nil} (panel#root)))lisp");
	const int head = app.repository.head().number;
	const ConsoleResult result = app.evaluate(R"lisp((defn fresh "" [] 1))lisp");
	CHECK(result.error.has_value());
	CHECK(app.repository.head().number == head);
	CHECK(app.fileOf("fresh").empty());
}

TEST_CASE("lisp_console attack: exactly the text an agent wrote stays the agent's") {
	Console app(kMain, false);
	const std::string text = R"lisp((defn other "An agent's." [] (call "hit")))lisp";
	REQUIRE_FALSE(
	    app.repository.commit({{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/tools.lisp/other", text}}}})
	        .has_value());
	REQUIRE_FALSE(app.repository
	                  .commit({{"author", "user"},
	                           {"message", "m"},
	                           {"set", {{"/program/tools.lisp/other", R"lisp((defn other "Mine." [] 1))lisp"}}}})
	                  .has_value());
	static_cast<void>(app.evaluate(text));
	CHECK(app.repository.programAuthors()["tools.lisp"]["other"] == "agent/x");
}

TEST_CASE("lisp_console: names and what is known of one") {
	Console app;
	const std::vector<affresco::ConsoleName> names = app.runtime->console().names("ma", 50);
	CHECK(std::ranges::any_of(names, [](const affresco::ConsoleName& name) { return name.name == "map"; }));
	CHECK(std::ranges::all_of(names, [](const affresco::ConsoleName& name) { return name.name.starts_with("ma"); }));
	CHECK(app.runtime->console().names("", 3).size() == 3);
	const std::optional<affresco::ConsoleName> found = app.runtime->console().describe("tool");
	REQUIRE(found.has_value());
	if(!found) return;
	const affresco::ConsoleName& tool = *found;
	CHECK(tool.kind == "defn");
	CHECK(tool.doc == "A tool.");
	CHECK(tool.file == "tools.lisp");
	CHECK(tool.line >= 1);
	CHECK(tool.arguments == std::vector<std::string>{"[a b]"});
	const std::optional<affresco::ConsoleName> command = app.runtime->console().describe("run-fresh");
	CHECK((command && command->kind == "defcommand"));
	CHECK_FALSE(app.runtime->console().describe("no-such-name").has_value());
	// A revision's text is in no file: no line
	REQUIRE_FALSE(app.evaluate(R"lisp((defn tool "Changed." [a b] (- a b)))lisp").error.has_value());
	const std::optional<affresco::ConsoleName> changed = app.runtime->console().describe("tool");
	CHECK((changed && changed->line == 0));
	const std::vector<std::string> files = app.runtime->console().files();
	CHECK(std::ranges::find(files, "tools.lisp") != files.end());
	CHECK(app.runtime->console().unshowable("a​b").has_value());
	CHECK_FALSE(app.runtime->console().unshowable("plain").has_value());
	// Too long to evaluate is too long to ask about: the question never opens on it
	const std::string longest(std::size_t{4} << 20U, 'a');
	CHECK_FALSE(app.runtime->console().unshowable(longest).has_value());
	CHECK(app.runtime->console().unshowable(longest + "a").has_value());
	ConsoleRequest tooLong;
	tooLong.text = longest + "a";
	CHECK(app.runtime->console().evaluate(tooLong).error.has_value());
}
