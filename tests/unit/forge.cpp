/// @file
/// @brief Forge's plain C++ (examples/forge): the file index and its confinement to the open folder, and the file icon
/// theme. None of it needs a window.

#include "features/palette/FileIndex.hpp"
#include "features/themes/IconTheme.hpp"

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

} // namespace

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

#ifndef _WIN32
TEST_CASE("forge: a write in the folder never follows a link planted at a temporary name") {
	const TempFolder folder, outside;
	folder.write("main.cpp", "old");
	outside.write("victim.txt", "keep");
	// The fixed name the language server's edits once went through, and a link in place of the file itself
	std::filesystem::create_symlink(outside.path / "victim.txt", folder.path / "main.cpp.forge-edit");
	CHECK_FALSE(forge::writeInFolder(folder.path / "main.cpp", "new"));
	CHECK(forge::readInFolder(folder.path, "main.cpp") == "new");
	CHECK(forge::readInFolder(outside.path, "victim.txt") == "keep");

	std::filesystem::create_symlink(outside.path / "victim.txt", folder.path / "linked.cpp");
	CHECK_FALSE(forge::writeInFolder(folder.path / "linked.cpp", "replaced"));
	CHECK_FALSE(std::filesystem::is_symlink(folder.path / "linked.cpp")); // the link is replaced, not written through
	CHECK(forge::readInFolder(outside.path, "victim.txt") == "keep");
}
#endif

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
