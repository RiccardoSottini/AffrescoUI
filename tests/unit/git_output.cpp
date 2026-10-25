/// @file
/// @brief Unit tests for services' GitOutput: git's status and branch list read strictly (each field checked, every
/// size bounded), and the status of a real repository made in a temporary folder (skipped where git isn't installed).

#include "process/Process.hpp"

#include <affresco/services/GitOutput.hpp>
#include <affresco/services/Grants.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using affresco::json;
using affresco::services::readGitBranches;
using affresco::services::readGitStatus;
using namespace std::string_literals;

namespace {

const std::string zeros(40, '0'); ///< An object name.
const std::string ordinaryEntry = "1 .M N... 100644 100644 100644 " + zeros + " " + zeros + " src/main file.cpp"s;

/// @brief Joins records the way `-z` ends them, each with a NUL.
/// @param records The records.
/// @return The output.
std::string nulEnded(const std::vector<std::string>& records) {
	std::string output;
	for(const std::string& record : records) output += record + '\0';
	return output;
}

/// @brief Runs git in a folder and returns what it wrote.
/// @param git The program.
/// @param folder The folder.
/// @param arguments Its arguments.
/// @param input Its input.
/// @return Its output.
std::string runGit(const std::filesystem::path& git, const std::filesystem::path& folder,
                   std::vector<std::string> arguments, const std::string& input = "") {
	arguments.insert(arguments.begin(), {"-c", "user.name=Tester", "-c", "user.email=tester@example.invalid", "-c",
	                                     "commit.gpgsign=false", "-c", "init.defaultBranch=main"});
	const auto child = affresco::services::ChildProcess::start({git, arguments, folder, std::nullopt});
	if(!input.empty()) (void)child->write(input);
	child->closeInput();
	std::string output;
	char buffer[4096];
	for(std::ptrdiff_t count; (count = child->read(buffer, sizeof buffer)) > 0;)
		output.append(buffer, static_cast<std::size_t>(count));
	REQUIRE(child->wait() == 0);
	return output;
}

} // namespace

TEST_CASE("git output: the status's branch headers and an ordinary entry") {
	const json status = readGitStatus(nulEnded({"# branch.oid " + zeros, "# branch.head main",
	                                                 "# branch.upstream origin/main", "# branch.ab +2 -1",
	                                                 ordinaryEntry}));
	CHECK(status["branch"]["head"] == "main");
	CHECK(status["branch"]["upstream"] == "origin/main");
	CHECK(status["branch"]["ahead"] == 2);
	CHECK(status["branch"]["behind"] == 1);
	REQUIRE(status["files"].size() == 1);
	const json& file = status["files"][0];
	CHECK(file["path"] == "src/main file.cpp"); // a path may hold spaces
	CHECK(file["kind"] == "modified");
	CHECK_FALSE(file["staged"].get<bool>());
	CHECK(file["unstaged"].get<bool>());
}

TEST_CASE("git output: renamed, unmerged, untracked and ignored entries") {
	const json status = readGitStatus(
	    nulEnded({"# branch.oid (initial)", "# branch.head (detached)",
	              "2 R. N... 100644 100644 100644 " + zeros + " " + zeros + " R100 new name.txt", "old name.txt",
	              "u UU N... 100644 100644 100644 100644 " + zeros + " " + zeros + " " + zeros + " both.txt",
	              "? notes/todo.md", "! build/out.o"}));
	const json& files = status["files"];
	REQUIRE(files.size() == 4);
	CHECK(files[0]["kind"] == "renamed");
	CHECK(files[0]["original"] == "old name.txt");
	CHECK(files[0]["staged"].get<bool>());
	CHECK(files[1]["kind"] == "unmerged");
	CHECK(files[1]["unstaged"].get<bool>());
	CHECK(files[2]["kind"] == "untracked");
	CHECK(files[3]["kind"] == "ignored");
	CHECK(status["branch"]["head"] == "(detached)");
}

TEST_CASE("git output: malformed status output is refused") {
	CHECK_THROWS_AS((void)readGitStatus("? unended"), std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"x what"})), std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"1 .M N... 100644"})), std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"1 .Z N... 100644 100644 100644 " + zeros + " " + zeros + " a"})),
	                std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"1 .M N... 1006x4 100644 100644 " + zeros + " " + zeros + " a"})),
	                std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"2 R. N... 100644 100644 100644 " + zeros + " " + zeros + " R1 a"})),
	                std::runtime_error); // a rename without its old path
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"? /etc/passwd"})), std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"? "})), std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"# branch.ab +x -1"})), std::runtime_error);
}

TEST_CASE("git output: the status is bounded") {
	const std::string three = nulEnded({"? a", "? b", "? c"});
	CHECK(readGitStatus(three, {.entries = 3})["files"].size() == 3);
	CHECK_THROWS_AS((void)readGitStatus(three, {.entries = 2}), std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(three, {.bytes = 8}), std::runtime_error);
	CHECK_THROWS_AS((void)readGitStatus(nulEnded({"? " + std::string(20, 'a')}), {.path = 10}), std::runtime_error);
	CHECK(readGitStatus("")["files"].empty());
}

TEST_CASE("git output: the branch list") {
	const json branches = readGitBranches(" \0feature/x\n*\0main\n"s);
	CHECK(branches["current"] == "main");
	REQUIRE(branches["branches"].size() == 2);
	CHECK(branches["branches"][0]["id"] == "feature/x");
	CHECK_THROWS_AS((void)readGitBranches("main\n"), std::runtime_error);
	CHECK_THROWS_AS((void)readGitBranches("*\0main"s), std::runtime_error); // not ended
	CHECK_THROWS_AS((void)readGitBranches("*\0a\n*\0b\n"s), std::runtime_error); // two current
}

TEST_CASE("git output: a real repository's status") {
	const std::filesystem::path git = affresco::services::findProgram("git");
	if(git.empty()) return; // no git here: nothing to compare with
	const std::filesystem::path folder =
	    std::filesystem::temp_directory_path() / ("aui-git-output-" + std::to_string(std::random_device{}()));
	std::filesystem::create_directories(folder);
	std::ofstream(folder / "kept.txt") << "one\n";
	runGit(git, folder, {"init", "-q"});
	runGit(git, folder, {"add", "--", "kept.txt"});
	runGit(git, folder, {"commit", "-q", "-F", "-"}, "first\n");
	std::ofstream(folder / "kept.txt") << "two\n";
	std::ofstream(folder / "new file.txt") << "new\n";
	const json status = readGitStatus(runGit(
	    git, folder, {"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"}));
	std::error_code error;
	std::filesystem::remove_all(folder, error);
	CHECK(status["branch"]["head"] == "main");
	REQUIRE(status["files"].size() == 2);
	CHECK(status["files"][0]["path"] == "kept.txt");
	CHECK(status["files"][0]["kind"] == "modified");
	CHECK(status["files"][1]["path"] == "new file.txt");
	CHECK(status["files"][1]["kind"] == "untracked");
}
