/// @file
/// @brief Forge's `forge.lsp.apply` (examples/forge/features/lsp/LspEdits.hpp): a rename's edits applied to open
/// buffers and to files, all or none, and never to a file outside the open folder.

#include "features/editor/Buffers.hpp"
#include "features/lsp/LspEdits.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>

using affresco::json;
namespace lsp = forge::lsp;

namespace {

/// @brief A folder of its own under the system's temporary folder, removed with the object.
struct TempFolder {
	std::filesystem::path path =
	    std::filesystem::temp_directory_path() / ("aui-lsp-edits-" + std::to_string(std::random_device{}()));

	/// @brief Creates the folder.
	TempFolder() { std::filesystem::create_directories(path); }
	/// @brief Removes it and everything in it.
	~TempFolder() {
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}
	TempFolder(const TempFolder&) = delete;
	TempFolder& operator=(const TempFolder&) = delete;

	/// @brief Writes a file.
	/// @param relative Its path, relative to the folder.
	/// @param text Its text.
	void write(const std::string& relative, const std::string& text) const {
		std::ofstream(path / relative, std::ios::binary) << text;
	}

	/// @brief Reads a file back.
	/// @param relative Its path, relative to the folder.
	/// @return Its text.
	[[nodiscard]] std::string read(const std::string& relative) const {
		std::ifstream in(path / relative, std::ios::binary);
		return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	}
};

/// @brief An edit of one line of a file, as "rename" gives it (from 1, columns in code points).
/// @param id The file.
/// @param line The line.
/// @param column The first column.
/// @param endColumn The column after the last.
/// @param text The new text.
/// @return The edit.
json edit(const std::string& id, int line, int column, int endColumn, const std::string& text) {
	return {{"id", id},        {"line", line},           {"column", column},
	        {"endLine", line}, {"endColumn", endColumn}, {"text", text}};
}

} // namespace

TEST_SUITE("lsp edits") {
	TEST_CASE("an open file changes in its buffer, unsaved; a closed one on disk") {
		const TempFolder folder;
		folder.write("a.cpp", "int count = 0;\n");
		folder.write("b.cpp", "return count;\n");
		forge::Buffers buffers;
		REQUIRE_FALSE(buffers.open(folder.path, "a.cpp"));
		const json done = lsp::applyToFolder(
		    {{"edits", {edit("a.cpp", 1, 5, 10, "total"), edit("b.cpp", 1, 8, 13, "total")}}}, folder.path, buffers);
		CHECK(done == json{{"files", 2}, {"edits", 2}});
		CHECK(buffers.find("a.cpp")->text() == "int total = 0;\n");
		CHECK(buffers.dirty("a.cpp"));
		CHECK(folder.read("a.cpp") == "int count = 0;\n");
		CHECK(folder.read("b.cpp") == "return total;\n");
	}

	TEST_CASE("one bad edit changes nothing") {
		const TempFolder folder;
		folder.write("a.cpp", "int count = 0;\n");
		folder.write("b.cpp", "return count;\n");
		forge::Buffers buffers;
		REQUIRE_FALSE(buffers.open(folder.path, "a.cpp"));
		const json edits = {edit("a.cpp", 1, 5, 10, "total"), edit("b.cpp", 9, 1, 2, "x")};
		CHECK_THROWS_AS((void)lsp::applyToFolder({{"edits", edits}}, folder.path, buffers), std::runtime_error);
		CHECK(buffers.find("a.cpp")->text() == "int count = 0;\n");
		CHECK_FALSE(buffers.dirty("a.cpp"));
		CHECK(folder.read("b.cpp") == "return count;\n");
	}

	TEST_CASE("files outside the folder, missing files and malformed calls are refused") {
		const TempFolder folder;
		folder.write("a.cpp", "int count = 0;\n");
		const forge::Buffers buffers;
		for(const std::string id : {"../a.cpp", "/etc/hosts", "", "missing.cpp"})
			CHECK_THROWS_AS((void)lsp::applyToFolder({{"edits", {edit(id, 1, 1, 1, "x")}}}, folder.path, buffers),
			                std::runtime_error);
		CHECK_THROWS_AS((void)lsp::applyToFolder(json::array(), folder.path, buffers), std::runtime_error);
		CHECK_THROWS_AS((void)lsp::applyToFolder({{"edits", "a.cpp"}}, folder.path, buffers), std::runtime_error);
		CHECK_THROWS_AS((void)lsp::applyToFolder({{"edits", json::array()}}, "", buffers), std::runtime_error);
		CHECK(folder.read("a.cpp") == "int count = 0;\n");
		CHECK_FALSE(std::filesystem::exists(folder.path.parent_path() / "x"));
	}
}
