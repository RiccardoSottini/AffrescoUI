/// @file
/// @brief The Launcher (examples/launcher) on temporary folders, headless: an app made shows at the top of My apps,
/// the apps are listed last changed first and once each, its assistant's changes wait for review, a name whose folder
/// something else holds is refused with a clear reason (and that folder kept), Settings save the provider the form
/// shows, with no pick first, and the window that asks for a new app's permissions shows no empty reason for a screen
/// without an app.json.

#include "Headless.hpp"
#include "HistoryFiles.hpp" // TemporaryFolder

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/intelligence/Intelligence.hpp>
#include <affresco/services/Grants.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>

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

/// @brief Whether a subtree holds a label whose text starts with some text.
/// @param panel The subtree's root.
/// @param start The text.
/// @return Whether it does.
bool showsLabelStarting(const affresco::Panel& panel, const std::string& start) {
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel); label && label->text().starts_with(start))
		return true;
	return std::ranges::any_of(panel.children(), [&](const auto& child) { return showsLabelStarting(*child, start); });
}

/// @brief Plays scenarios on the Launcher, headless, with a host's grants.
/// @param scenarios The scenarios (a JSON list).
/// @param grant Grants what the screen uses, before it is built.
/// @return The report.
automation::Report playLauncher(const json& scenarios, const std::function<void(affresco::Screen&)>& grant) {
	const fs::path screen = fs::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "launcher" / "index.screen.json";
	const json file = {{"version", 2}, {"screen", screen.filename().string()}, {"scenarios", scenarios}};
	automation::Options options;
	options.pictures = false;
	options.prepare = grant;
	return automation::run(automation::ScenarioFile::parse(file.dump(), screen.parent_path() / "test.scenarios.json"),
	                       options);
}

/// @brief The Launcher's folders, as affrescoui_launcher grants them, on a temporary folder: the examples to read,
/// the apps folder, and app programs that aren't there (opening an app fails, and it is made all the same).
struct LauncherFolders {
	TemporaryFolder folder;                            ///< Everything is in it.
	fs::path apps = folder.path() / "AffrescoUI";      ///< The person's apps.
	fs::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY; ///< The examples.

	/// @brief Makes the apps folder.
	LauncherFolders() { fs::create_directories(apps); }

	/// @brief Grants them to a screen.
	/// @param screen The Launcher's screen.
	void grant(affresco::Screen& screen) const {
		screen.services().addRoot("examples", examples, true);
		screen.services().addRoot("apps", apps);
		services::AppPrograms programs;
		programs.folder = folder.path() / "no-programs";
		programs.programs = {{"affrescoui_launcher", true, true}};
		programs.roots = {{"examples", examples}, {"apps", apps}};
		programs.dataDirectory = folder.path() / "data";
		programs.socketDirectory = folder.path() / "sockets";
		REQUIRE_FALSE(services::grantAppPrograms(screen.services(), "affresco", programs));
	}

	/// @brief Puts an app in the apps folder, last changed some days ago.
	/// @param id Its folder.
	/// @param title Its window's title.
	/// @param daysAgo When its screen was last changed.
	void app(const std::string& id, const std::string& title, int daysAgo) const {
		const fs::path screen = apps / id / "index.screen.json";
		writeFile(screen, json({{"format", "affrescoui-screen"},
		                        {"version", 2},
		                        {"window", {{"title", title}}},
		                        {"root", json::object()}})
		                      .dump());
		fs::last_write_time(screen, fs::file_time_type::clock::now() - std::chrono::hours(24 * daysAgo));
	}
};

} // namespace

TEST_CASE("launcher: the apps are listed last changed first, once each, and an app made goes to the top") {
	const LauncherFolders folders;
	folders.app("apple-list", "Apple list", 2);
	folders.app("zebra-notes", "Zebra notes", 1); // after the other by name, changed later
	const json scenarios = json::parse(R"json([{ "name": "listed", "steps": [
		{ "wait": 10 },
		{ "expect": { "state": "mine.length", "is": 2 } },
		{ "expect": { "state": "mine[0].name", "is": "Zebra notes" } },
		{ "expect": { "state": "mine[1].name", "is": "Apple list" } },
		{ "action": "launcher.refresh" },
		{ "action": "launcher.refresh" },
		{ "key": "mod+r" },
		{ "wait": 10 },
		{ "expect": { "state": "mine.length", "is": 2 } },
		{ "key": "mod+n" },
		{ "click": "#createName" },
		{ "type": "Mango" },
		{ "key": "enter" },
		{ "wait": 10 },
		{ "expect": { "state": "page", "is": "mine" } },
		{ "expect": { "state": "mine.length", "is": 3 } },
		{ "expect": { "state": "mine[0].id", "is": "mango" } },
		{ "expect": { "node": "#mineName", "text": "Mango" } },
		{ "key": "mod+1" },
		{ "expect": { "node": "#cardName", "text": "Mango" } },
		{ "expect": { "node": "#cardKind", "text": "Yours" } }
	] }])json");
	const automation::Report report = playLauncher(scenarios, [&](affresco::Screen& screen) { folders.grant(screen); });
	INFO(report.text());
	CHECK(report.failed() == 0);

	// The new app's assistant proposes: its change waits for the person's review, as a card to accept
	const fs::path made = folders.apps / "mango" / "index.screen.json";
	const json reviewed = {{"version", 2},
	                       {"screen", made.filename().string()},
	                       {"scenarios", json::parse(R"json([{ "name": "review", "steps": [
		{ "propose": { "author": "agent", "message": "A subtitle", "set": { "#welcomeTitle/text": "Mango, with a subtitle" } } },
		{ "wait": 0 },
		{ "expect": { "node": "#welcomeTitle", "text": "Mango" } },
		{ "expect": { "node": "#assistantWaiting1", "exists": true } },
		{ "click": { "role": "button", "name": "Accept" } },
		{ "wait": 0 },
		{ "expect": { "node": "#welcomeTitle", "text": "Mango, with a subtitle" } }
	] }])json")}};
	automation::Options options;
	options.pictures = false;
	const automation::Report review = automation::run(
	    automation::ScenarioFile::parse(reviewed.dump(), made.parent_path() / "test.scenarios.json"), options);
	INFO(review.text());
	CHECK(review.failed() == 0);
}

TEST_CASE("launcher: a name whose folder holds something else is refused, saying so, and the folder is kept") {
	const LauncherFolders folders;
	writeFile(folders.apps / "notes" / "keep.txt", "mine"); // a folder of the person's, not an app
	const json scenarios = json::parse(R"json([{ "name": "taken", "steps": [
		{ "wait": 10 },
		{ "key": "mod+n" },
		{ "click": "#createName" },
		{ "type": "Notes" },
		{ "key": "enter" },
		{ "wait": 10 },
		{ "expect": { "state": "page", "is": "create" } },
		{ "expect": { "state": "create.busy", "is": false } },
		{ "expect": { "node": "#createProblem", "text": "The app couldn't be made: a folder named AffrescoUI/notes is there already: choose another name" } },
		{ "expect": { "state": "mine.length", "is": 0 } }
	] }])json");
	const automation::Report report = playLauncher(scenarios, [&](affresco::Screen& screen) { folders.grant(screen); });
	INFO(report.text());
	CHECK(report.failed() == 0);
	CHECK(fs::exists(folders.apps / "notes" / "keep.txt"));
	CHECK_FALSE(fs::exists(folders.apps / "notes" / "index.screen.json"));
}

TEST_CASE("launcher: Settings save the provider the form shows first, with no pick before") {
	const LauncherFolders folders;
	const fs::path data = folders.folder.path() / "data";
	const auto grant = [&](affresco::Screen& screen) {
		folders.grant(screen);
		affresco::intelligence::CallsOptions options;
		options.keys = std::make_shared<affresco::MemorySecretStore>();
		options.claudeInstalled = [] { return true; };
		affresco::intelligence::registerCalls(screen, data, std::move(options)); // the shipped providers only
	};
	const json scenarios = json::parse(R"json([{ "name": "first-save", "steps": [
		{ "key": "mod+comma" },
		{ "wait": 10 },
		{ "expect": { "state": "intelligence.native", "is": true } },
		{ "expect": { "state": "intelligence.selected", "is": "claude-code" } },
		{ "action": "intelligence.apply" },
		{ "wait": 10 },
		{ "expect": { "state": "intelligence.error", "is": "" } },
		{ "expect": { "state": "intelligence.data.current.provider", "is": "claude-code" } }
	] }])json");
	const automation::Report report = playLauncher(scenarios, grant);
	INFO(report.text());
	CHECK(report.failed() == 0);
	CHECK(fs::exists(affresco::intelligence::Settings::file(data)));
}

TEST_CASE("launcher: a new blank app's permissions show what it asks for, with no empty reason") {
	// A blank app is a screen without an app.json: the host asks for what it uses, each with no reason
	const fs::path file = fs::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "shared" / "host" / "permissions.screen.json";
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(640, 480, clock);
	std::ifstream in(file, std::ios::binary);
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	affresco::Screen screen = affresco::Screen::fromString(text, file.parent_path());
	screen.setLocale("en");
	const json app = {{"name", "Garden planner"},
	                  {"capabilities",
	                   {{{"key", "assistant"}, {"kind", "assistant"}, {"name", ""}, {"why", ""}, {"allowed", true}},
	                    {{"key", "files"}, {"kind", "files"}, {"name", ""}, {"why", "Backs up."}, {"allowed", true}}}}};
	screen.onCall("permissions.app", [&](const std::string&) { return app.dump(); });
	screen.build(*window);
	window->render();
	CHECK(showsLabelStarting(window->root(), "Garden planner asks to use"));
	CHECK(showsLabelStarting(window->root(), "Ask the assistant you set up in Settings"));
	CHECK(showsLabelStarting(window->root(), "Garden planner says: “Backs up.”"));
	CHECK_FALSE(showsLabelStarting(window->root(), "Garden planner says: “”"));
}
