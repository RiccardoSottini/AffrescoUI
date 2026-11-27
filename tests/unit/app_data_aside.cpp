/// @file
/// @brief The data kept for a place is set aside, never erased (Screen::setDataAside()): an app deleted, or a folder
/// about to hold a new one, so that a new app of an old one's name starts empty, and another place's data stays.

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using affresco::json;

/// A screen that keeps a draft ("persist").
constexpr const char* draftScreen = R"({ "format": "affrescoui-screen", "version": 2, "state": { "draft": "" },
	"persist": ["state.draft"], "root": { "type": "label", "id": "draft", "text": "{{state.draft}}" } })";

/// @brief Writes a file, making its folder.
/// @param path The file.
/// @param text What it holds.
void write(const std::filesystem::path& path, const std::string& text) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream(path) << text;
}

/// @brief Writes an app "notes" in a folder, its entry keeping a draft.
/// @param folder The folder.
void writeNotes(const std::filesystem::path& folder) {
	write(folder / "app.json", R"({ "format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes",
		"entry": "index.screen.json" })");
	write(folder / "index.screen.json", draftScreen);
}

/// @brief The entries in a folder whose names start with a text.
/// @param folder The folder.
/// @param start The text.
/// @return How many there are.
int countStarting(const std::filesystem::path& folder, const std::string& start) {
	int count = 0;
	for(const auto& entry : std::filesystem::directory_iterator(folder))
		count += entry.path().filename().string().starts_with(start) ? 1 : 0;
	return count;
}

/// @brief The draft a screen file opens with.
/// @param screenFile The file.
/// @param data The data folder.
/// @return The draft.
std::string draftOf(const std::filesystem::path& screenFile, const std::filesystem::path& data) {
	const affresco::Screen screen = affresco::Screen::fromFile(screenFile, data);
	return json::parse(screen.stateJson())["draft"].get<std::string>();
}

} // namespace

TEST_CASE("data aside: a bare screen's data is set aside, and a new screen at its place starts empty") {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "affrescoui-data-aside-screen-test";
	std::filesystem::remove_all(root);
	const std::filesystem::path budget = root / "apps" / "budget", data = root / "data";
	write(budget / "index.screen.json", draftScreen);
	CHECK(draftOf(budget / "index.screen.json", data).empty()); // binds "budget" to it, and starts its history
	write(data / "budget.state.json", R"({"/draft": "the old chat"})");
	write(data / "budget.usage.json", "{}");
	CHECK(draftOf(budget / "index.screen.json", data) == "the old chat");

	// The app is deleted, then made again with the same name
	std::filesystem::remove_all(budget);
	const std::vector<std::filesystem::path> moved = affresco::Screen::setDataAside(data, budget);
	CHECK(moved.size() == 5); // its history, state, records (data.sqlite), usage and record
	CHECK_FALSE(std::filesystem::exists(data / "budget.location"));
	CHECK_FALSE(std::filesystem::exists(data / "budget.state.json"));
	CHECK(countStarting(data, "budget.state.json.replaced-") == 1); // nothing is erased
	CHECK(countStarting(data, "budget.history.replaced-") == 1);
	CHECK(countStarting(data, "budget.location.replaced-") == 1);
	write(budget / "index.screen.json", draftScreen);
	CHECK(draftOf(budget / "index.screen.json", data).empty());
	CHECK(std::filesystem::exists(data / "budget.location")); // the name is the new screen's
	CHECK(affresco::Screen::setDataAside(data, root / "apps" / "elsewhere").empty());
	std::filesystem::remove_all(root);
}

TEST_CASE("data aside: an app's data folder is set aside whole, and the app made again opens with none of it") {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "affrescoui-data-aside-app-test";
	std::filesystem::remove_all(root);
	const std::filesystem::path notes = root / "apps" / "notes", data = root / "data", apps = data / "apps";
	writeNotes(notes / "json"); // a copy of an example: the app is in its json folder
	{
		const affresco::Screen first = affresco::Screen::fromApp(notes / "json", data);
	}
	REQUIRE(std::filesystem::exists(apps / "notes" / "location"));
	write(apps / "notes" / "state.json", R"({"/draft": "the old chat"})");
	write(apps / "notes" / "grants.json", R"({"decisions": {"audio": true}})");

	std::filesystem::remove_all(notes);
	writeNotes(notes / "json"); // deleted in Finder, then made again: its folder is set aside before it opens
	// The folder, and the entry screen's own record ("notes.location", fromApp() looking for its old data)
	CHECK(affresco::Screen::setDataAside(data, notes).size() == 2);
	CHECK(countStarting(apps, "notes.replaced-") == 1);
	CHECK_FALSE(std::filesystem::exists(apps / "notes"));
	{
		const affresco::Screen again = affresco::Screen::fromApp(notes / "json", data);
		CHECK(json::parse(again.stateJson())["draft"] == "");
	}
	CHECK(affresco::Screen::appDataDirectory(data, "notes", notes / "json") == apps / "notes");
	CHECK_FALSE(std::filesystem::exists(apps / "notes" / "grants.json")); // the person is asked again
	// One set aside is never set aside again
	std::filesystem::remove_all(notes);
	CHECK(affresco::Screen::setDataAside(data, notes).size() == 2);
	CHECK(countStarting(apps, "notes.replaced-") == 2);
	std::filesystem::remove_all(root);
}

TEST_CASE("data aside: another place's data stays, whatever its name") {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "affrescoui-data-aside-others-test";
	std::filesystem::remove_all(root);
	const std::filesystem::path data = root / "data", apps = data / "apps";
	const std::filesystem::path other = root / "other" / "budget", mine = root / "apps" / "budget";
	write(other / "index.screen.json", draftScreen);
	write(mine / "index.screen.json", draftScreen);
	CHECK(draftOf(other / "index.screen.json", data).empty()); // "budget" is the other's
	CHECK(draftOf(mine / "index.screen.json", data).empty());  // this one's is "budget~<hash>"
	writeNotes(root / "apps" / "budget-2");                    // a name that starts the same
	{
		const affresco::Screen app = affresco::Screen::fromApp(root / "apps" / "budget-2", data);
	}

	const std::vector<std::filesystem::path> moved = affresco::Screen::setDataAside(data, mine);
	CHECK_FALSE(moved.empty());
	for(const std::filesystem::path& path : moved) CHECK(path.filename().string().starts_with("budget~"));
	CHECK(countStarting(data, "budget~") == static_cast<int>(moved.size())); // every one set aside
	CHECK(std::filesystem::exists(data / "budget.location"));
	CHECK(std::filesystem::exists(data / "budget.history"));
	CHECK(std::filesystem::exists(apps / "notes" / "location"));
	CHECK(countStarting(data, "budget.") == 3); // the other's record, history and records: none set aside

	// Never a file system's root, which would name every place
	CHECK_THROWS_AS(static_cast<void>(affresco::Screen::setDataAside(data, root.root_path())), affresco::ScreenError);
	std::filesystem::remove_all(root);
}
