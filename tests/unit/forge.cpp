/// @file
/// @brief Forge's plain C++ (examples/forge): the VS Code colour theme importer, the file index and its confinement to
/// the open folder, and the file icon theme. None of it needs a window.

#include "features/palette/FileIndex.hpp"
#include "features/themes/IconTheme.hpp"
#include "features/themes/ThemeImport.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

using affresco::json;

namespace {

/// @brief A folder of its own under the system's temporary folder, removed with the object.
struct TempFolder {
	std::filesystem::path path = std::filesystem::temp_directory_path() /
	                             ("aui-forge-" + std::to_string(std::random_device{}()));

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
	/// @param text Its text.
	void write(const std::string& relative, const std::string& text) const {
		const std::filesystem::path file = path / relative;
		std::filesystem::create_directories(file.parent_path());
		std::ofstream(file, std::ios::binary) << text;
	}
};

/// @brief The value an imported theme's patch gives a token.
/// @param theme importVsCodeTheme()'s result.
/// @param token The token, e.g. "color.editor".
/// @return The colour, or an empty string if no op sets it.
std::string patched(const json& theme, const std::string& token) {
	const std::string path = "/tokens/" + token + "/" + theme["id"].get<std::string>();
	for(const json& op : theme["patch"])
		if(op["path"] == path) return op["value"].get<std::string>();
	return {};
}

} // namespace

TEST_CASE("forge: a theme's name becomes a short id") {
	CHECK(forge::themeId("Solarized Dark") == "solarized-dark");
	CHECK(forge::themeId("  One  Dark Pro!! ") == "one-dark-pro");
	CHECK(forge::themeId("***") == "imported");
	CHECK(forge::themeId(std::string(100, 'a')).size() == 40);
}

TEST_CASE("forge: a VS Code theme with comments and trailing commas becomes token variants") {
	const json theme = forge::importVsCodeTheme(R"({
		// a comment, and a "string // that isn't one"
		"name": "Night Owl", "type": "dark",
		"colors": {
			"editor.background": "#011627",
			"statusBar.background": "#011627aa", /* block */
			"focusBorder": "not a colour",
			"button.background": "#7e57c2",
		},
	})",
	                                            "night-owl.json");
	CHECK(theme["id"] == "night-owl");
	CHECK(theme["name"] == "Night Owl");
	CHECK(theme["type"] == "dark");
	CHECK(patched(theme, "color.editor") == "#011627");
	CHECK(patched(theme, "color.statusBar") == "#011627aa");
	CHECK(patched(theme, "color.accent") == "#7e57c2"); // the first key set to a colour wins
	CHECK(patched(theme, "color.sideBar") == "#011627"); // falls back to editor.background
	CHECK(patched(theme, "color.hover") == "#2a2d2e");   // the dark default
	for(const json& op : theme["patch"]) CHECK(op["op"] == "add");
}

TEST_CASE("forge: a light theme without a name takes the file's name and light defaults") {
	const json theme = forge::importVsCodeTheme(R"({ "type": "light" })", "Paper");
	CHECK(theme["id"] == "paper");
	CHECK(theme["type"] == "light");
	CHECK(patched(theme, "color.editor") == "#ffffff");
}

TEST_CASE("forge: a theme file that isn't a JSON object is refused") {
	CHECK_THROWS_AS(forge::importVsCodeTheme("[1, 2]", "x"), std::runtime_error);
	CHECK_THROWS_AS(forge::importVsCodeTheme("{ \"name\": ", "x"), std::runtime_error);
}

TEST_CASE("forge: the file index lists files, folders first, and skips hidden entries and build output") {
	const TempFolder folder;
	folder.write("src/main.cpp", "int main() {}\n");
	folder.write("README.md", "# hi\n");
	folder.write(".git/config", "");
	folder.write("build/out.o", "");
	folder.write("node_modules/x/index.js", "");
	const json index = forge::indexFolder(folder.path);
	REQUIRE(index["files"].size() == 2);
	CHECK(index["files"][0]["id"] == "README.md");
	CHECK(index["files"][1]["id"] == "src/main.cpp");
	CHECK(index["files"][1]["title"] == "main.cpp");
	CHECK(index["truncated"] == false);
	REQUIRE(index["tree"].size() == 2);
	CHECK(index["tree"][0]["id"] == "src/"); // folders first
	CHECK(index["tree"][0]["children"][0]["id"] == "src/main.cpp");
}

TEST_CASE("forge: the file index stops at its limits") {
	const TempFolder folder;
	folder.write("a.txt", "");
	folder.write("b.txt", "");
	folder.write("c.txt", "");
	const json index = forge::indexFolder(folder.path, {.files = 2});
	CHECK(index["files"].size() == 2);
	CHECK(index["truncated"] == true);
	CHECK_THROWS_AS(forge::indexFolder(folder.path / "missing"), std::runtime_error);
}

TEST_CASE("forge: files are read only inside the open folder") {
	const TempFolder folder;
	folder.write("notes/a.txt", "hello");
	CHECK(forge::readInFolder(folder.path, "notes/a.txt") == "hello");
	CHECK(forge::insideFolder(folder.path, "../secret").empty());
	CHECK(forge::insideFolder(folder.path, "notes/../../secret").empty());
	CHECK(forge::insideFolder(folder.path, "/etc/hosts").empty());
	CHECK_THROWS_AS(forge::readInFolder(folder.path, "../a.txt"), std::runtime_error);
	CHECK_THROWS_AS(forge::readInFolder(folder.path, "notes/missing.txt"), std::runtime_error);
	CHECK_THROWS_AS(forge::readInFolder(folder.path, "notes/a.txt", 2), std::runtime_error);
}

TEST_CASE("forge: the icon theme dresses tree items by name, then extension, then kind") {
	json items = json::parse(R"([
		{ "id": "src/", "label": "src", "children": [{ "id": "src/main.cpp", "label": "main.cpp" }] },
		{ "id": "CMakeLists.txt", "label": "CMakeLists.txt" },
		{ "id": "notes", "label": "notes" }
	])");
	const json theme = json::parse(R"({
		"file": { "icon": "file", "color": "#cccccc" },
		"folder": { "icon": "folder" },
		"extensions": { "cpp": { "icon": "file-code", "color": "#519aba" }, "txt": { "icon": "x", "color": "red" } },
		"names": { "CMakeLists.txt": { "icon": "settings" } }
	})");
	forge::applyIconTheme(items, theme);
	CHECK(items[0]["icon"] == "folder");
	CHECK(items[0]["children"][0]["icon"] == "file-code");
	CHECK(items[0]["children"][0]["iconColor"] == "#519aba");
	CHECK(items[1]["icon"] == "settings"); // the name wins over the extension
	CHECK(items[2]["icon"] == "file");
	CHECK(items[2]["iconColor"] == "#cccccc");
}

TEST_CASE("forge: an icon theme of the wrong shape changes nothing it can't read") {
	json items = json::parse(R"([{ "id": "a.cpp", "label": "a.cpp" }])");
	forge::applyIconTheme(items, json::parse(R"({ "extensions": { "cpp": { "icon": 3, "color": "nope" } } })"));
	CHECK_FALSE(items[0].contains("iconColor"));
	forge::applyIconTheme(items, json("not an object"));
	CHECK(items[0]["label"] == "a.cpp");
}
