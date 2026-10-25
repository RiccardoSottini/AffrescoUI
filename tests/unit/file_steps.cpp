/// @file
/// @brief The file steps (saveFile, openFile) and the export step: files the person chooses in the dialog, after the
/// event; what the document learns (the file's own name, never where it is); the bounds on what is read; and that a
/// revision can't add them. The dialogs are answered by the test, in a folder of its own.

#include "Headless.hpp"
#include "HistoryFiles.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with a deck in its state, two pages to export and the actions that save, open and export.
/// @return The document.
json fileScreen() {
	return {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"window", {{"title", "files"}, {"width", 300}, {"height", 200}}},
	    {"state",
		 {{"deck", {{"title", "Plan: Q3/Q4"}, {"slides", json::array({"one", "two"})}}},
		  {"opened", nullptr},
		  {"saved", nullptr},
		  {"exported", nullptr},
		  {"error", nullptr}}},
	    {"actions",
		 {{"save", json::array({{{"saveFile", "{{state.deck}}"},
		                         {"name", "{{state.deck.title}}.json"},
		                         {"onDone", json::array({{{"set", "state.saved"}, {"to", "{{result}}"}}})}}})},
		  {"open", json::array({{{"openFile", "state.opened"},
		                         {"onDone", json::array({{{"set", "state.saved"}, {"to", "{{result}}"}}})},
		                         {"onError", json::array({{{"set", "state.error"}, {"to", "{{error.message}}"}}})}}})},
		  {"openSettings",
		   json::array({{{"openFile", "state.opened"},
		                 {"comments", true},
		                 {"onError", json::array({{{"set", "state.error"}, {"to", "{{error.message}}"}}})}}})},
		  {"pdf", json::array({{{"export", "pdf"},
		                        {"pages", json::array({"page-1", "page-2"})},
		                        {"size", json::array({960, 540})},
		                        {"title", "{{state.deck.title}}"},
		                        {"onDone", json::array({{{"set", "state.exported"}, {"to", "{{result}}"}}})},
		                        {"onError", json::array({{{"set", "state.error"}, {"to", "{{error.message}}"}}})}}})},
		  {"png", json::array({{{"export", "png"},
		                        {"pages", json::array({"page-1"})},
		                        {"onError", json::array({{{"set", "state.error"}, {"to", "{{error.code}}"}}})}}})}}},
	    {"root",
		 {{"type", "column"},
		  {"children",
		   json::array(
	           {{{"type", "panel"},
			     {"id", "page-1"},
			     {"width", 96},
			     {"height", 54},
			     {"color", "#ffffff"},
			     {"children", json::array({{{"type", "label"}, {"text", "One"}}})}},
			    {{"type", "panel"}, {"id", "page-2"}, {"width", 96}, {"height", 54}, {"color", "#336699"}}})}}}};
}

/// @brief A file's text.
/// @param path The file.
/// @return Its text.
std::string textOf(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

} // namespace

TEST_CASE("file steps: saveFile writes the value where the person chooses; openFile reads a JSON file back") {
	const affresco::test::TemporaryFolder folder;
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(fileScreen().dump());
	screen.build(*window);
	std::vector<affresco::FileDialog> asked;
	std::vector<std::string> chosen = {(folder.path() / "deck.json").string()};
	window->setFileDialogHandler([&](const affresco::FileDialog& dialog) {
		asked.push_back(dialog);
		return chosen;
	});

	screen.runAction("save", window->root());
	CHECK(asked.empty()); // after the event
	window->runDeferredTasks();
	REQUIRE(asked.size() == 1);
	CHECK(asked[0].kind == affresco::FileDialogKind::Save);
	CHECK(asked[0].name == "Plan Q3Q4.json"); // no separators in the name it suggests
	CHECK(json::parse(textOf(folder.path() / "deck.json"))["slides"] == json::array({"one", "two"}));
	json state = json::parse(screen.stateJson());
	CHECK(state["saved"]["name"] == "deck.json"); // never where it is
	CHECK(state["saved"]["bytes"].get<std::size_t>() == textOf(folder.path() / "deck.json").size());

	screen.runAction("open", window->root());
	window->runDeferredTasks();
	REQUIRE(asked.size() == 2);
	CHECK(asked[1].kind == affresco::FileDialogKind::Open);
	CHECK(asked[1].filters.at(0).extensions == std::vector<std::string>{"json"});
	state = json::parse(screen.stateJson());
	CHECK(state["opened"]["title"] == "Plan: Q3/Q4");

	// A cancel runs nothing
	chosen.clear();
	screen.runAction("save", window->root());
	window->runDeferredTasks();
	CHECK(asked.size() == 3);
}

TEST_CASE("file steps: openFile refuses what isn't JSON, nests too deeply or is too large, and says so") {
	const affresco::test::TemporaryFolder folder;
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(fileScreen().dump());
	screen.build(*window);
	std::string file;
	window->setFileDialogHandler([&](const affresco::FileDialog&) { return std::vector<std::string>{file}; });
	affresco::setLogSink([](affresco::LogLevel, std::string_view) {});
	const auto opened = [&](const std::string& name, const std::string& text) {
		file = (folder.path() / name).string();
		std::ofstream(file, std::ios::binary) << text;
		screen.runAction("open", window->root());
		window->runDeferredTasks();
		return json::parse(screen.stateJson());
	};

	CHECK(opened("notes.json", "not json")["error"].get<std::string>().contains("isn't JSON"));
	CHECK(opened("deep.json", std::string(100000, '[') + std::string(100000, ']'))["error"].get<std::string>().contains(
	    "nests too deeply"));
	CHECK(opened("large.json", "\"" + std::string((std::size_t{16} << 20) + 1, 'a') + "\"")["error"]
	          .get<std::string>()
	          .contains("larger than 16 MB"));
	CHECK(opened("list.json", "[]")["opened"] == json::array());
	file = (folder.path() / "gone.json").string(); // chosen, then gone before it is read
	screen.runAction("open", window->root());
	window->runDeferredTasks();
	CHECK(json::parse(screen.stateJson())["error"].get<std::string>().contains("can't be read"));
	affresco::setLogSink(nullptr);
}

TEST_CASE("file steps: openFile with comments reads a settings file's comments and trailing commas") {
	const affresco::test::TemporaryFolder folder;
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(fileScreen().dump());
	screen.build(*window);
	const std::string file = (folder.path() / "theme.json").string();
	std::ofstream(file, std::ios::binary) << "{\n\t// a comment, and a \"string // that isn't one\"\n"
	                                         "\t\"name\": \"Night\", /* block */\n\t\"colors\": [1, 2,],\n}\n";
	window->setFileDialogHandler([&](const affresco::FileDialog&) { return std::vector<std::string>{file}; });
	affresco::setLogSink([](affresco::LogLevel, std::string_view) {});

	screen.runAction("open", window->root()); // plain JSON only
	window->runDeferredTasks();
	json state = json::parse(screen.stateJson());
	CHECK(state["opened"].is_null());
	CHECK(state["error"].get<std::string>().contains("isn't JSON"));

	screen.runAction("openSettings", window->root());
	window->runDeferredTasks();
	state = json::parse(screen.stateJson());
	CHECK(state["opened"] == json{{"name", "Night"}, {"colors", {1, 2}}});
	affresco::setLogSink(nullptr);
}

TEST_CASE("export step: a PDF of one page per node, saved where the person chooses") {
	const affresco::test::TemporaryFolder folder;
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(fileScreen().dump());
	screen.build(*window);
	std::vector<affresco::FileDialog> asked;
	window->setFileDialogHandler([&](const affresco::FileDialog& dialog) {
		asked.push_back(dialog);
		return std::vector<std::string>{(folder.path() / "deck.pdf").string()};
	});

	screen.runAction("pdf", window->root());
	window->runDeferredTasks();
	REQUIRE(asked.size() == 1);
	CHECK(asked[0].name == "Plan Q3Q4.pdf");
	CHECK(asked[0].filters.at(0).extensions == std::vector<std::string>{"pdf"});
	const std::string pdf = textOf(folder.path() / "deck.pdf");
	CHECK(pdf.starts_with("%PDF-"));
	const json state = json::parse(screen.stateJson());
	CHECK(state["exported"]["pages"] == 2);
	CHECK(state["exported"]["name"] == "deck.pdf");
	CHECK(state["exported"]["bytes"].get<std::size_t>() == pdf.size());

	// A headless window draws no pixels: a PNG says so, and nothing is asked
	screen.runAction("png", window->root());
	window->runDeferredTasks();
	CHECK(json::parse(screen.stateJson())["error"] == "export");
	CHECK(asked.size() == 1);
}

TEST_CASE("file steps: a revision can add none of them, and they are listed among the control steps") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(fileScreen().dump());
	screen.build(*window);
	for(const json& step : {json{{"saveFile", "{{state}}"}}, json{{"openFile", "state.deck"}},
	                        json{{"export", "pdf"}, {"pages", json::array({"page-1"})}}}) {
		const affresco::RevisionResult added =
		    screen.submitRevision(json{{"author", "agent"}, {"set", {{"/actions/leak", json::array({step})}}}}.dump());
		CHECK(added.status == RevisionStatus::Rejected);
		CHECK(added.error.contains("permission denied"));
	}
}
