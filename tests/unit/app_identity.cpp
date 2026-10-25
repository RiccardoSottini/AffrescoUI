/// @file
/// @brief An app's id, and a bare screen's file name, are bound to the place that first used them
/// (Screen::appDataDirectory()): another app that names itself the same gets a data folder of its own, so it never
/// opens with the first's data, or with the permissions the person gave the first (the host keeps them there).

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

/// @brief Writes a file, making its folder.
/// @param path The file.
/// @param text What it holds.
void write(const std::filesystem::path& path, const std::string& text) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream(path) << text;
}

/// @brief Writes an app "notes" in a folder.
/// @param folder The folder.
void writeNotes(const std::filesystem::path& folder) {
	write(folder / "app.json", R"({ "format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes",
		"entry": "index.screen.json" })");
	write(folder / "index.screen.json", R"({ "format": "affrescoui-screen", "version": 2,
		"root": { "type": "label", "id": "hello", "text": "hello" } })");
}

/// @brief The folders in a folder whose names start with a text.
/// @param folder The folder.
/// @param start The text.
/// @return How many there are.
int countStarting(const std::filesystem::path& folder, const std::string& start) {
	int count = 0;
	for(const auto& entry : std::filesystem::directory_iterator(folder))
		count += entry.path().filename().string().starts_with(start) ? 1 : 0;
	return count;
}

} // namespace

TEST_CASE("app identity: an app elsewhere with the same id gets a data folder of its own") {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "affrescoui-app-identity-test";
	std::filesystem::remove_all(root);
	writeNotes(root / "mine" / "notes");
	writeNotes(root / "copied" / "notes");
	const std::filesystem::path data = root / "data", apps = data / "apps";

	{
		const affresco::Screen first = affresco::Screen::fromApp(root / "mine" / "notes", data);
	}
	const std::filesystem::path own = affresco::Screen::appDataDirectory(data, "notes", root / "mine" / "notes");
	CHECK(own == apps / "notes");
	CHECK(std::filesystem::exists(apps / "notes" / "location"));

	const std::filesystem::path other = affresco::Screen::appDataDirectory(data, "notes", root / "copied" / "notes");
	CHECK(other != own);
	CHECK(other.filename().string().starts_with("notes~"));
	{
		const affresco::Screen second = affresco::Screen::fromApp(root / "copied" / "notes" / "app.json", data);
	}
	CHECK(std::filesystem::exists(other / "location"));
	// Each opens its own again, through a link too
	std::filesystem::create_directory_symlink(root / "mine" / "notes", root / "link");
	CHECK(affresco::Screen::appDataDirectory(data, "notes", root / "link") == own);
	{
		const affresco::Screen again = affresco::Screen::fromApp(root / "copied" / "notes", data);
	}
	CHECK(countStarting(apps, "notes") == 2);

	// Data kept before ids were bound (no record) is the first place's to open it
	std::filesystem::create_directories(apps / "older");
	CHECK(affresco::Screen::appDataDirectory(data, "older", root / "mine" / "notes") == apps / "older");
	CHECK(affresco::Screen::appDataDirectory(data, "older", root / "copied" / "notes") != apps / "older");
	std::filesystem::remove_all(root);
}

TEST_CASE("app identity: a bare screen elsewhere with the same file name keeps its own data, and adopts none") {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "affrescoui-screen-identity-test";
	std::filesystem::remove_all(root);
	const std::string screen = R"({ "format": "affrescoui-screen", "version": 2,
		"root": { "type": "label", "id": "hello", "text": "hello" } })";
	write(root / "mine" / "habits.screen.json", screen);
	write(root / "copied" / "habits.screen.json", screen);
	const std::filesystem::path data = root / "data";
	{
		const affresco::Screen first = affresco::Screen::fromFile(root / "mine" / "habits.screen.json", data);
	}
	CHECK(std::filesystem::exists(data / "habits.location"));
	{
		const affresco::Screen second = affresco::Screen::fromFile(root / "copied" / "habits.screen.json", data);
	}
	CHECK(countStarting(data, "habits~") >= 1); // its own history, state, ... and record

	// The second becomes an app "habits": it takes only its own old data, never the first screen's
	write(data / "habits.state.json", R"({"/draft": "the first's"})");
	write(root / "copied" / "app.json", R"({ "format": "affrescoui-app", "version": 1, "id": "habits",
		"name": "Habits", "entry": "habits.screen.json" })");
	{
		const affresco::Screen app = affresco::Screen::fromApp(root / "copied", data);
	}
	CHECK(std::filesystem::exists(data / "habits.state.json"));
	std::filesystem::remove_all(root);
}
