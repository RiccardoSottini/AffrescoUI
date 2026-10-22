/// @file
/// @brief Forge's file buffers (examples/forge/features/editor/Buffers.hpp): open, dirty and clean again, save,
/// revert, close, the limits, the folder they are confined to, and the TODO lines for Problems. No window.

#include "features/editor/Buffers.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace {

/// @brief A folder of its own under the system's temporary folder, removed with the object.
struct TempFolder {
	std::filesystem::path path = std::filesystem::temp_directory_path() /
	                             ("aui-forge-editor-" + std::to_string(std::random_device{}()));

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

	/// @brief Reads a file back.
	/// @param relative Its path, relative to the folder.
	/// @return Its text.
	[[nodiscard]] std::string read(const std::string& relative) const {
		std::ifstream in(path / relative, std::ios::binary);
		return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	}
};

/// @brief Records the dirty handler's calls.
struct DirtyLog {
	std::vector<std::pair<std::string, bool>> calls; ///< (id, dirty), in order.

	/// @brief Connects to some buffers.
	/// @param buffers They.
	void attach(forge::Buffers& buffers) {
		buffers.onDirty([this](const std::string& id, bool dirty) { calls.emplace_back(id, dirty); });
	}
};

} // namespace

TEST_CASE("forge editor: a buffer opens clean and lists itself") {
	TempFolder folder;
	folder.write("src/a.cpp", "int a;\nint b;\n");
	forge::Buffers buffers;
	REQUIRE_FALSE(buffers.open(folder.path, "src/a.cpp"));
	REQUIRE(buffers.find("src/a.cpp"));
	CHECK(buffers.find("src/a.cpp")->text() == "int a;\nint b;\n");
	CHECK_FALSE(buffers.dirty("src/a.cpp"));
	REQUIRE(buffers.model()->size() == 1);
	CHECK(buffers.model()->row(0)["label"] == "a.cpp");
	CHECK(buffers.model()->row(0)["lines"] == 3);
	CHECK_FALSE(buffers.open(folder.path, "src/a.cpp")); // already open: left as it is
	CHECK(buffers.size() == 1);
}

TEST_CASE("forge editor: an edit makes a buffer dirty, undoing it makes it clean") {
	TempFolder folder;
	folder.write("a.txt", "hello");
	forge::Buffers buffers;
	DirtyLog log;
	log.attach(buffers);
	REQUIRE_FALSE(buffers.open(folder.path, "a.txt"));
	affresco::TextModel& text = *buffers.find("a.txt");
	text.insert({0, 5}, "!");
	CHECK(buffers.dirty("a.txt"));
	CHECK(buffers.model()->row(0)["dirty"] == true);
	text.insert({0, 6}, "?"); // still dirty: not told again
	text.erase({0, 5}, {0, 7});
	CHECK_FALSE(buffers.dirty("a.txt"));
	CHECK(log.calls == std::vector<std::pair<std::string, bool>>{{"a.txt", true}, {"a.txt", false}});
	CHECK(buffers.dirtyIds().empty());
}

TEST_CASE("forge editor: save writes the file and leaves nothing beside it") {
	TempFolder folder;
	folder.write("a.txt", "one");
	forge::Buffers buffers;
	REQUIRE_FALSE(buffers.open(folder.path, "a.txt"));
	buffers.find("a.txt")->assign("two");
	REQUIRE(buffers.dirty("a.txt"));
	CHECK_FALSE(buffers.save("a.txt"));
	CHECK(folder.read("a.txt") == "two");
	CHECK_FALSE(buffers.dirty("a.txt"));
	CHECK(std::distance(std::filesystem::directory_iterator(folder.path), {}) == 1); // nothing left beside it
	CHECK(buffers.save("b.txt").value_or("").starts_with("not open"));
}

#ifndef _WIN32
// Found in the security review of wave 6: save wrote through "<file>.forge-save", so a link left there by whoever
// made the folder (a cloned repository) had the user's text written over the file it pointed to
TEST_CASE("forge editor: save writes nothing through a link planted beside the file, and keeps its permissions") {
	TempFolder folder, elsewhere;
	folder.write("run.sh", "one");
	elsewhere.write("target", "untouched");
	std::filesystem::permissions(folder.path / "run.sh", std::filesystem::perms::owner_all);
	std::filesystem::create_symlink(elsewhere.path / "target", folder.path / "run.sh.forge-save");
	forge::Buffers buffers;
	REQUIRE_FALSE(buffers.open(folder.path, "run.sh"));
	buffers.find("run.sh")->assign("two");
	CHECK_FALSE(buffers.save("run.sh"));
	CHECK(folder.read("run.sh") == "two");
	CHECK(elsewhere.read("target") == "untouched");
	CHECK_FALSE(std::filesystem::is_symlink(folder.path / "run.sh"));
	CHECK(std::filesystem::status(folder.path / "run.sh").permissions() == std::filesystem::perms::owner_all);
}
#endif

TEST_CASE("forge editor: save all writes every dirty buffer") {
	TempFolder folder;
	folder.write("a.txt", "a");
	folder.write("b.txt", "b");
	folder.write("c.txt", "c");
	forge::Buffers buffers;
	for(const char* id : {"a.txt", "b.txt", "c.txt"}) REQUIRE_FALSE(buffers.open(folder.path, id));
	buffers.find("a.txt")->assign("A");
	buffers.find("c.txt")->assign("C");
	CHECK(buffers.dirtyIds() == std::vector<std::string>{"a.txt", "c.txt"});
	CHECK_FALSE(buffers.saveAll());
	CHECK(folder.read("a.txt") == "A");
	CHECK(folder.read("b.txt") == "b");
	CHECK(folder.read("c.txt") == "C");
	CHECK(buffers.dirtyIds().empty());
}

TEST_CASE("forge editor: revert reads the file again without telling the handler") {
	TempFolder folder;
	folder.write("a.txt", "kept");
	forge::Buffers buffers;
	DirtyLog log;
	log.attach(buffers);
	REQUIRE_FALSE(buffers.open(folder.path, "a.txt"));
	buffers.find("a.txt")->assign("changed");
	folder.write("a.txt", "changed on disk");
	CHECK_FALSE(buffers.revert("a.txt"));
	CHECK(buffers.find("a.txt")->text() == "changed on disk");
	CHECK_FALSE(buffers.dirty("a.txt"));
	CHECK(log.calls.size() == 1); // the edit, not the revert
}

TEST_CASE("forge editor: close forgets a buffer, an editor holding it keeps the text") {
	TempFolder folder;
	folder.write("a.txt", "text");
	forge::Buffers buffers;
	REQUIRE_FALSE(buffers.open(folder.path, "a.txt"));
	const std::shared_ptr<affresco::TextModel> held = buffers.find("a.txt");
	CHECK(buffers.close("a.txt"));
	CHECK_FALSE(buffers.close("a.txt"));
	CHECK_FALSE(buffers.find("a.txt"));
	CHECK(buffers.model()->size() == 0);
	held->assign("still editable"); // disconnected: nothing is told
	CHECK(held->text() == "still editable");
}

TEST_CASE("forge editor: paths outside the open folder, and what isn't a file, don't open") {
	TempFolder folder;
	folder.write("inside/a.txt", "a");
	forge::Buffers buffers;
	CHECK(buffers.open(folder.path / "inside", "../outside.txt").has_value());
	CHECK(buffers.open(folder.path, "/etc/hosts").has_value());
	CHECK(buffers.open(folder.path, "inside").has_value());
	CHECK(buffers.open(folder.path, "missing.txt").has_value());
	CHECK(buffers.open({}, "inside/a.txt").has_value()); // no folder open
	CHECK(buffers.size() == 0);
}

TEST_CASE("forge editor: the limits hold") {
	TempFolder folder;
	folder.write("a.txt", "a");
	folder.write("b.txt", "b");
	folder.write("big.txt", std::string(64, 'x'));
	forge::Buffers buffers({.buffers = 1, .fileBytes = 32, .keptBytes = 1, .todos = 1});
	CHECK(buffers.open(folder.path, "big.txt").value_or("").starts_with("the file is too large"));
	REQUIRE_FALSE(buffers.open(folder.path, "a.txt"));
	CHECK(buffers.open(folder.path, "b.txt").value_or("").starts_with("too many files"));
}

TEST_CASE("forge editor: a buffer larger than keptBytes stays dirty until saved") {
	TempFolder folder;
	folder.write("a.txt", "abcdef");
	forge::Buffers buffers({.keptBytes = 2});
	REQUIRE_FALSE(buffers.open(folder.path, "a.txt"));
	affresco::TextModel& text = *buffers.find("a.txt");
	text.insert({0, 0}, "x");
	text.erase({0, 0}, {0, 1});
	CHECK(buffers.dirty("a.txt")); // the same size, but its saved text wasn't kept
	CHECK_FALSE(buffers.save("a.txt"));
	CHECK_FALSE(buffers.dirty("a.txt"));
}

TEST_CASE("forge editor: TODO and FIXME lines, trimmed, cut and capped") {
	TempFolder folder;
	folder.write("a.cpp", "int a;\n  // TODO tidy  \n// FIXME " + std::string(200, 'y') + "\n");
	folder.write("b.cpp", "// TODO b\n");
	forge::Buffers buffers({.todos = 2});
	REQUIRE_FALSE(buffers.open(folder.path, "a.cpp"));
	REQUIRE_FALSE(buffers.open(folder.path, "b.cpp"));
	const affresco::json todos = buffers.todos();
	REQUIRE(todos.size() == 2); // the cap is for them all: b.cpp's isn't listed
	CHECK(todos[0]["message"] == "// TODO tidy");
	CHECK(todos[0]["severity"] == "info");
	CHECK(todos[0]["line"] == 2);
	CHECK(todos[1]["severity"] == "warning");
	CHECK(todos[1]["message"].get<std::string>().size() == 120);
}
