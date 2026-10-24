/// @file
/// @brief What the Launcher (examples/launcher) stands on, headless: an `fs` copy from another granted root (binary
/// files, left-out names, a merge, read-only roots), the app-programs grant's rule (Grants.hpp) and detached runs; then
/// the Launcher itself on temporary folders: an app created blank and from an example, listed, duplicated and deleted.

#include "HistoryFiles.hpp" // TemporaryFolder

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/services/Grants.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using affresco::json;
using affresco::test::TemporaryFolder;
namespace automation = affresco::automation;
namespace services = affresco::services;
namespace fs = std::filesystem;

namespace {

/// @brief Writes a file, making its folder.
/// @param file The file.
/// @param bytes What it holds.
void writeFile(const fs::path& file, const std::string& bytes) {
	fs::create_directories(file.parent_path());
	std::ofstream(file, std::ios::binary) << bytes;
}

/// @brief Reads a whole file.
/// @param file The file.
/// @return Its bytes.
std::string readFile(const fs::path& file) {
	std::ifstream in(file, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), {}};
}

/// @brief Plays scenarios on a document, headless, with a host's grants.
/// @param scenarios The scenarios (a JSON list).
/// @param screen The screen file they play (its folder resolves the document's relative files).
/// @param document The document, or empty for the screen file's own.
/// @param grant Grants what the screen uses, before it is built.
/// @return The report.
automation::Report play(const json& scenarios, const fs::path& screen, const std::string& document,
                        const std::function<void(affresco::Screen&)>& grant) {
	const json file = {{"version", 2}, {"screen", screen.filename().string()}, {"scenarios", scenarios}};
	automation::Options options;
	options.pictures = false;
	if(!document.empty()) options.document = document;
	options.prepare = grant;
	return automation::run(automation::ScenarioFile::parse(file.dump(), screen.parent_path() / "test.scenarios.json"),
	                       options);
}

/// @brief A screen that copies from a read-only root into another.
constexpr const char* copyScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"services": {
		"source": { "provider": "fs", "root": "source", "readOnly": true },
		"target": { "provider": "fs", "root": "target", "copyFrom": ["source"] },
		"writable": { "provider": "fs", "root": "source" }
	},
	"state": { "copied": "", "errors": [] },
	"actions": {
		"copy": [{ "invoke": "target.copy", "with": { "from": "source", "path": "app", "to": "app", "exclude": ["*.cpp", ".*"] },
		           "onDone": [{ "set": "state.copied", "to": "{{result.path}}" }] }],
		"merge": [{ "invoke": "target.copy", "with": { "from": "source", "path": "shared", "to": "shared", "merge": true } }],
		"refused": [
			{ "invoke": "target.copy", "with": { "from": "source", "path": "app", "to": "app" },
			  "onError": [{ "set": "state.errors", "to": "{{concat(state.errors, [error.code])}}" }] },
			{ "invoke": "target.copy", "with": { "from": "source", "path": "../outside.txt", "to": "x.txt" },
			  "onError": [{ "set": "state.errors", "to": "{{concat(state.errors, [error.code])}}" }] },
			{ "invoke": "target.copy", "with": { "from": "elsewhere", "path": "app", "to": "y" },
			  "onError": [{ "set": "state.errors", "to": "{{concat(state.errors, [error.code])}}" }] },
			{ "invoke": "source.write", "with": { "path": "new.txt", "text": "x" },
			  "onError": [{ "set": "state.errors", "to": "{{concat(state.errors, [error.code])}}" }] },
			{ "invoke": "writable.write", "with": { "path": "new.txt", "text": "x" },
			  "onError": [{ "set": "state.errors", "to": "{{concat(state.errors, [error.code])}}" }] }
		]
	},
	"root": { "type": "label", "text": "" }
})json";

} // namespace

TEST_CASE(
    "launcher: fs copies from another root, without the names left out, merges, and never writes a read-only root") {
	const TemporaryFolder folder;
	const fs::path source = folder.path() / "source", target = folder.path() / "target";
	writeFile(source / "app" / "index.screen.json", "{}");
	writeFile(source / "app" / "Native.cpp", "int main() {}");
	writeFile(source / "app" / ".DS_Store", "x");
	writeFile(source / "app" / "covers" / "a.png", std::string("\x89PNG\0\x01\x02", 7)); // binary, a NUL inside
	writeFile(source / "shared" / "font.ttf", "new font");
	writeFile(source / "shared" / "same.txt", "kept");
	writeFile(folder.path() / "outside.txt", "secret");
	writeFile(target / "shared" / "font.ttf", "old font");
	writeFile(target / "shared" / "mine.txt", "mine");
	fs::last_write_time(target / "shared" / "font.ttf",
	                    fs::last_write_time(source / "shared" / "font.ttf") - std::chrono::hours(1));
	writeFile(target / "shared" / "same.txt", "newer here");
	fs::last_write_time(target / "shared" / "same.txt",
	                    fs::last_write_time(source / "shared" / "same.txt") + std::chrono::hours(1));
	const json scenarios = json::array(
	    {{{"name", "copy"},
		  {"steps",
		   {{{"action", "copy"}},
		    {{"expect", {{"state", "copied"}, {"is", "app"}}}},
		    {{"action", "merge"}},
		    {{"action", "refused"}},
		    {{"expect", {{"state", "errors"}, {"is", {"exists", "refused", "refused", "notFound", "refused"}}}}}}}}});
	const automation::Report report =
	    play(scenarios, folder.path() / "test.screen.json", copyScreen, [&](affresco::Screen& screen) {
		    screen.services().addRoot("source", source, true);
		    screen.services().addRoot("target", target);
	    });
	INFO(report.text());
	CHECK(report.failed() == 0);
	CHECK(readFile(target / "app" / "index.screen.json") == "{}");
	CHECK(readFile(target / "app" / "covers" / "a.png") == std::string("\x89PNG\0\x01\x02", 7));
	CHECK_FALSE(fs::exists(target / "app" / "Native.cpp"));
	CHECK_FALSE(fs::exists(target / "app" / ".DS_Store"));
	CHECK(readFile(target / "shared" / "font.ttf") == "new font");   // older there: replaced
	CHECK(readFile(target / "shared" / "same.txt") == "newer here"); // newer there: kept
	CHECK(readFile(target / "shared" / "mine.txt") == "mine");       // only there: kept
	CHECK_FALSE(fs::exists(target / "x.txt"));
	CHECK_FALSE(fs::exists(source / "new.txt"));
}

TEST_CASE("launcher: the app-programs grant starts only its programs, on screens inside its roots") {
	const TemporaryFolder folder;
	const fs::path programs = folder.path() / "bin", examples = folder.path() / "examples";
	writeFile(programs / "affrescoui_host", "");
	writeFile(programs / "examples" / "forge" / "affrescoui_forge", ""); // a build tree's place for it
	writeFile(examples / "flow" / "index.screen.json", "{}");
	writeFile(examples / "flow" / "notes.txt", "");
	writeFile(folder.path() / "outside.screen.json", "{}");
	services::AppPrograms grant;
	grant.folder = programs;
	grant.programs = {
	    {"affrescoui_host", true, true}, {"affrescoui_forge", false, false}, {"affrescoui_gone", false, false}};
	grant.roots = {{"examples", fs::canonical(examples)}};
	grant.dataDirectory = folder.path() / "data";
	grant.socketDirectory = folder.path() / "sockets";
	grant.reveal = true;
	const services::ProcessShaper shape = services::appProgramsShaper(grant);
	const auto shaped = [&](std::vector<std::string> arguments) {
		services::ProcessRun run{std::move(arguments), {}, "/bin/false"};
		const std::optional<std::string> refused = shape(run);
		return refused ? std::optional<services::ProcessRun>() : std::optional<services::ProcessRun>(run);
	};

	const auto host = shaped({"affrescoui_host", "examples:flow/index.screen.json", "--ask", "Make it blue"});
	REQUIRE(host);
	CHECK(host->program == programs / "affrescoui_host");
	const std::string screen = (fs::canonical(examples) / "flow" / "index.screen.json").string();
	CHECK(host->arguments == std::vector<std::string>{screen, "--ask", "Make it blue", "--data",
	                                                  (folder.path() / "data").string(), "--socket",
	                                                  (folder.path() / "sockets" / "examples-flow").string()});
	const auto forge = shaped({"affrescoui_forge"});
	REQUIRE(forge);
	CHECK(forge->program == programs / "examples" / "forge" / "affrescoui_forge");
	CHECK(forge->arguments.back() == (folder.path() / "sockets" / "forge").string());

	CHECK_FALSE(shaped({}));
	CHECK_FALSE(shaped({"sh", "-c", "true"}));                                   // not one of its programs
	CHECK_FALSE(shaped({"affrescoui_gone"}));                                    // not there
	CHECK_FALSE(shaped({"affrescoui_host"}));                                    // no screen
	CHECK_FALSE(shaped({"affrescoui_host", "examples:../outside.screen.json"})); // outside the root
	CHECK_FALSE(shaped({"affrescoui_host", "/etc/passwd"}));                     // no root named
	CHECK_FALSE(shaped({"affrescoui_host", "nowhere:flow/index.screen.json"}));  // not one of its roots
	CHECK_FALSE(shaped({"affrescoui_host", "examples:flow/notes.txt"}));         // not a screen
	CHECK_FALSE(shaped({"affrescoui_host", "examples:flow/index.screen.json", "--data", "/tmp"})); // its own flags only
	CHECK_FALSE(shaped({"affrescoui_forge", "--ask", "hello"})); // it takes no first request
	CHECK_FALSE(shaped({"affrescoui_host", "examples:flow/index.screen.json", "--ask", std::string(5000, 'a')}));
	CHECK_FALSE(shaped({"reveal", "examples:flow/index.screen.json"})); // a folder only
	CHECK_FALSE(shaped({"reveal", "examples:.."}));
#ifdef __APPLE__
	const auto reveal = shaped({"reveal", "examples:flow"});
	REQUIRE(reveal);
	CHECK(reveal->program == "/usr/bin/open");
	CHECK(reveal->arguments == std::vector<std::string>{(fs::canonical(examples) / "flow").string()});
#endif
}

#ifndef _WIN32
TEST_CASE("launcher: a detached run is started, gives its pid, and takes no input") {
	const TemporaryFolder folder;
	constexpr const char* screen = R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"services": { "start": { "provider": "process", "process": "touch" } },
		"state": { "pid": 0, "error": "" },
		"actions": {
			"start": [{ "invoke": "start.run", "onDone": [{ "set": "state.pid", "to": "{{result.pid}}" }] }],
			"input": [{ "invoke": "start.run", "with": { "input": "x" }, "onError": [{ "set": "state.error", "to": "{{error.code}}" }] }]
		},
		"root": { "type": "label", "text": "" }
	})json";
	const fs::path touched = folder.path() / "touched";
	const json scenarios = json::array(
	    {{{"name", "detached"},
		  {"steps",
		   {{{"action", "start"}}, {{"action", "input"}}, {{"expect", {{"state", "error"}, {"is", "refused"}}}}}}}});
	const automation::Report report =
	    play(scenarios, folder.path() / "test.screen.json", screen, [&](affresco::Screen& played) {
		    services::ProcessGrant grant{"/usr/bin/touch", {touched.string()}, std::nullopt, {}, true};
		    played.services().allowProcess("touch", std::move(grant));
	    });
	INFO(report.text());
	CHECK(report.failed() == 0);
	for(int i = 0; i < 200 && !fs::exists(touched); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
	CHECK(fs::exists(touched)); // it ran, though nothing waited for it
}
#endif

TEST_CASE("launcher: an app is created blank and from an example, listed, duplicated and deleted") {
	const TemporaryFolder folder;
	const fs::path apps = folder.path() / "AffrescoUI", examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	fs::create_directories(apps);
	const auto grant = [&](affresco::Screen& screen) {
		screen.services().addRoot("examples", examples, true);
		screen.services().addRoot("apps", apps);
		services::AppPrograms programs; // none there: opening fails, and the app is made all the same
		programs.folder = folder.path() / "no-programs";
		programs.programs = {{"affrescoui_host", true, true}};
		programs.roots = {{"examples", examples}, {"apps", apps}};
		programs.dataDirectory = folder.path() / "data";
		programs.socketDirectory = folder.path() / "sockets";
		REQUIRE_FALSE(services::grantAppPrograms(screen.services(), "affresco", programs));
	};
	const json scenarios = json::parse(R"json([{ "name": "create", "steps": [
		{ "key": "mod+n" },
		{ "click": "#createName" },
		{ "type": "Garden planner" },
		{ "key": "enter" },
		{ "wait": 10 },
		{ "expect": { "state": "page", "is": "mine" } },
		{ "expect": { "state": "mine[0].id", "is": "garden-planner" } },
		{ "expect": { "state": "mine[0].name", "is": "Garden planner" } },
		{ "key": "mod+n" },
		{ "click": "#createName" },
		{ "type": "Team board" },
		{ "click": { "role": "radio", "name": "Flow" } },
		{ "key": "mod+enter" },
		{ "wait": 10 },
		{ "expect": { "state": "mine.length", "is": 2 } },
		{ "click": { "role": "button", "name": "Duplicate Garden planner" } },
		{ "wait": 10 },
		{ "expect": { "state": "mine.length", "is": 3 } },
		{ "click": { "role": "button", "name": "Delete Team board" } },
		{ "wait": 10 },
		{ "click": { "role": "button", "name": "Delete" } },
		{ "wait": 10 },
		{ "expect": { "state": "mine.length", "is": 2 } }
	] }])json");
	const automation::Report report = play(scenarios, examples / "launcher" / "index.screen.json", "", grant);
	INFO(report.text());
	CHECK(report.failed() == 0);
	const std::string blank = readFile(apps / "garden-planner" / "index.screen.json");
	CHECK(blank.contains("\"title\": \"Garden planner\""));
	CHECK_FALSE(blank.contains("\"New app\""));
	CHECK(readFile(apps / "garden-planner-copy" / "index.screen.json").contains("Garden planner (copy)"));
	CHECK_FALSE(fs::exists(apps / "team-board")); // deleted
	CHECK(fs::exists(apps / "shared" / "fonts" / "Roboto-Regular.ttf"));
	CHECK(fs::exists(apps / "shared" / "addons" / "assistant" / "assistant.json"));
	CHECK_FALSE(fs::exists(apps / "shared" / "CodeEditor.cpp"));
	CHECK_FALSE(fs::exists(apps / "shared" / "Assets.hpp"));
	// The new app loads where it is: its includes and fonts are found in the shared folder beside it
	CHECK_NOTHROW((void)affresco::Screen::fromString(blank, apps / "garden-planner"));
}
