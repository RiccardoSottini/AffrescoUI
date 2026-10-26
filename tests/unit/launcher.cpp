/// @file
/// @brief What the Launcher (examples/launcher) stands on, headless: an `fs` copy from another granted root (binary
/// files, left-out names, a merge, read-only roots), the app-programs grant's rule (Grants.hpp) and detached runs; then
/// the Launcher itself on temporary folders: an app created blank and from an example, listed, duplicated and deleted.

#include "HistoryFiles.hpp" // TemporaryFolder

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/services/Grants.hpp>
#include <affresco/services/ServiceRegistry.hpp>

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

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

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
	writeFile(source / "app" / "run.sh", "#!/bin/sh\n");
	fs::permissions(source / "app" / "run.sh", fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all);
#ifndef _WIN32
	fs::create_symlink("/bin/sh", source / "app" / "shell");
#endif
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
	CHECK(readFile(target / "app" / "run.sh") == "#!/bin/sh\n");
#ifndef _WIN32
	// Nothing copied from another root can be run: no execute bit, no write for the others, and no link
	const fs::perms runnable = fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec |
	                           fs::perms::group_write | fs::perms::others_write;
	CHECK((fs::status(target / "app" / "run.sh").permissions() & runnable) == fs::perms::none);
	CHECK((fs::status(target / "app" / "index.screen.json").permissions() & runnable) == fs::perms::none);
	CHECK_FALSE(fs::exists(fs::symlink_status(target / "app" / "shell")));
#endif
	CHECK(readFile(target / "shared" / "font.ttf") == "new font");   // older there: replaced
	CHECK(readFile(target / "shared" / "same.txt") == "newer here"); // newer there: kept
	CHECK(readFile(target / "shared" / "mine.txt") == "mine");       // only there: kept
	CHECK_FALSE(fs::exists(target / "x.txt"));
	CHECK_FALSE(fs::exists(source / "new.txt"));
}

TEST_CASE("launcher: the app-programs grant starts only its programs, on screens inside its roots") {
	const TemporaryFolder folder;
	const fs::path programs = folder.path() / "bin", examples = folder.path() / "examples";
#ifdef _WIN32
	const std::string exe = ".exe"; // a program's file there
#else
	const std::string exe;
#endif
	writeFile(programs / ("affrescoui_launcher" + exe), "");
	writeFile(programs / "examples" / "tool" / ("affrescoui_tool" + exe), ""); // a build tree's place for it
	writeFile(examples / "flow" / "index.screen.json", "{}");
	writeFile(examples / "flow" / "notes.txt", "");
	writeFile(folder.path() / "outside.screen.json", "{}");
	services::AppPrograms grant;
	grant.folder = programs;
	grant.programs = {
	    {"affrescoui_launcher", true, true}, {"affrescoui_tool", false, false}, {"affrescoui_gone", false, false}};
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

	const auto host = shaped({"affrescoui_launcher", "examples:flow/index.screen.json", "--ask", "Make it blue"});
	REQUIRE(host);
	CHECK(host->program == programs / ("affrescoui_launcher" + exe));
	const std::string screen = (fs::canonical(examples) / "flow" / "index.screen.json").string();
	CHECK(host->arguments == std::vector<std::string>{screen, "--ask", "Make it blue", "--data",
	                                                  (folder.path() / "data").string(), "--socket",
	                                                  (folder.path() / "sockets" / "examples-flow").string()});
	const auto tool = shaped({"affrescoui_tool"});
	REQUIRE(tool);
	CHECK(tool->program == programs / "examples" / "tool" / ("affrescoui_tool" + exe));
	CHECK(tool->arguments.back() == (folder.path() / "sockets" / "tool").string());

	CHECK_FALSE(shaped({}));
	CHECK_FALSE(shaped({"sh", "-c", "true"}));                                   // not one of its programs
	CHECK_FALSE(shaped({"affrescoui_gone"}));                                    // not there
	CHECK_FALSE(shaped({"affrescoui_launcher"}));                                    // no screen
	CHECK_FALSE(shaped({"affrescoui_launcher", "examples:../outside.screen.json"})); // outside the root
	CHECK_FALSE(shaped({"affrescoui_launcher", "/etc/passwd"}));                     // no root named
	CHECK_FALSE(shaped({"affrescoui_launcher", "nowhere:flow/index.screen.json"}));  // not one of its roots
	CHECK_FALSE(shaped({"affrescoui_launcher", "examples:flow/notes.txt"}));         // not a screen
	writeFile(examples / "flow" / "app.json", "{}");
	const auto app = shaped({"affrescoui_launcher", "examples:flow"}); // an app's folder
	REQUIRE(app);
	CHECK(app->arguments.front() == (fs::canonical(examples) / "flow").string());
	CHECK(app->arguments.back() == (folder.path() / "sockets" / "examples-flow").string());
	CHECK(shaped({"affrescoui_launcher", "examples:flow/app.json"}));
	CHECK_FALSE(shaped({"affrescoui_launcher", "examples:."})); // a folder without an app.json
	fs::remove(examples / "flow" / "app.json");
	CHECK_FALSE(shaped({"affrescoui_launcher", "examples:flow"}));
	CHECK_FALSE(shaped({"affrescoui_launcher", "examples:flow/index.screen.json", "--data", "/tmp"})); // its own flags only
	CHECK_FALSE(shaped({"affrescoui_tool", "--ask", "hello"})); // it takes no first request
	CHECK_FALSE(shaped({"affrescoui_launcher", "examples:flow/index.screen.json", "--ask", std::string(5000, 'a')}));
	CHECK_FALSE(shaped({"reveal", "examples:flow/index.screen.json"})); // a folder only
	CHECK_FALSE(shaped({"reveal", "examples:.."}));
#ifdef __APPLE__
	const auto reveal = shaped({"reveal", "examples:flow"});
	REQUIRE(reveal);
	CHECK(reveal->program == "/usr/bin/open");
	CHECK(reveal->arguments == std::vector<std::string>{"-R", (fs::canonical(examples) / "flow").string()});
	fs::create_directories(examples / "Bundle" / "Contents");
	writeFile(examples / "Bundle" / "Contents" / "Info.plist", "");
	CHECK_FALSE(shaped({"reveal", "examples:Bundle"})); // a bundle without an extension is opened, never shown
#endif
	fs::create_directories(examples / "Tool.app");
	CHECK_FALSE(shaped({"reveal", "examples:Tool.app"})); // a name with an extension: a package, perhaps
#ifndef _WIN32
	fs::create_directory_symlink(examples / "flow", examples / "link");
	CHECK_FALSE(shaped({"reveal", "examples:link"}));         // a link, though to a folder inside
	CHECK_FALSE(shaped({"reveal", "examples:link/../flow"})); // nor through one
#endif
}

TEST_CASE("launcher: the app-programs grant refuses roots that overlap, resolved") {
	const TemporaryFolder folder;
	const fs::path examples = folder.path() / "examples", apps = folder.path() / "apps";
	fs::create_directories(examples / "inner");
	fs::create_directories(apps);
	CHECK(services::rootsOverlap(examples, examples / "inner"));
	CHECK(services::rootsOverlap(examples / "inner", examples));
	CHECK(services::rootsOverlap(examples / "inner" / "..", examples));
	CHECK_FALSE(services::rootsOverlap(examples, apps));
	CHECK_FALSE(services::rootsOverlap(folder.path() / "ex", examples)); // a common prefix isn't a parent
#ifndef _WIN32
	fs::create_directory_symlink(examples / "inner", folder.path() / "alias");
	CHECK(services::rootsOverlap(folder.path() / "alias", examples)); // through a link
#endif
	services::ServiceRegistry registry;
	services::AppPrograms programs;
	programs.folder = folder.path();
	programs.dataDirectory = folder.path() / "data";
	programs.socketDirectory = folder.path() / "sockets";
	programs.roots = {{"examples", examples}, {"apps", examples / "inner"}};
	const std::optional<std::string> refused = services::grantAppPrograms(registry, "affresco", programs);
	REQUIRE(refused);
	CHECK(refused->find("overlap") != std::string::npos);
	programs.roots = {{"examples", examples}, {"apps", apps}};
	CHECK_FALSE(services::grantAppPrograms(registry, "affresco", programs));
}

TEST_CASE("launcher: a read-only root is refused to the providers that run programs in it") {
	const TemporaryFolder folder;
	services::ServiceRegistry registry;
	registry.addRoot("examples", folder.path(), true);
	registry.allowProcess("true", "/usr/bin/true");
	const std::vector<std::string> refused =
	    registry.load({{"files", {{"provider", "fs"}, {"root", "examples"}, {"readOnly", true}}},
		               {"writes", {{"provider", "fs"}, {"root", "examples"}}},
		               {"search", {{"provider", "search"}, {"root", "examples"}}},
		               {"run", {{"provider", "process"}, {"process", "true"}, {"root", "examples"}}}});
	const json status = registry.status();
	INFO(status.dump());
	CHECK(refused.size() == 2);
	CHECK(status["files"]["state"] != "refused");
	CHECK(status["search"]["state"] != "refused");
	CHECK(status["writes"]["state"] == "refused");
	CHECK(status["run"]["state"] == "refused");
	CHECK(status["run"].dump().find("only fs, watch and search may use it") != std::string::npos);
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
		programs.programs = {{"affrescoui_launcher", true, true}};
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
	// ...and builds in a window, where the properties are checked (a root with "dir" failed only there)
	const json opened = json::parse(R"json([{ "name": "open", "steps": [
		{ "expect": { "node": "#welcomeTitle", "text": "Garden planner" } }
	] }])json");
	const automation::Report built = play(opened, apps / "garden-planner" / "index.screen.json", "", nullptr);
	INFO(built.text());
	CHECK(built.failed() == 0);
}

// NOLINTEND(bugprone-unchecked-optional-access)
