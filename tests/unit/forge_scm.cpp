/// @file
/// @brief Forge's source control (examples/forge/features/scm): git's status and branch list read strictly, the git
/// commands Forge allows, the line diff and its gutter marks, and the status of a real repository made in a temporary
/// folder (skipped where git isn't installed).

#include "Process.hpp"
#include "features/scm/GitArguments.hpp"
#include "features/scm/GitStatus.hpp"
#include "features/scm/LineDiff.hpp"
#include "features/scm/Scm.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using affresco::json;
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
	arguments.insert(arguments.begin(), {"-c", "user.name=Forge", "-c", "user.email=forge@example.invalid", "-c",
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

TEST_CASE("forge scm: the status's branch headers and an ordinary entry") {
	const json status = forge::parseStatus(nulEnded({"# branch.oid " + zeros, "# branch.head main",
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

TEST_CASE("forge scm: renamed, unmerged, untracked and ignored entries") {
	const json status = forge::parseStatus(
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

TEST_CASE("forge scm: malformed status output is refused") {
	CHECK_THROWS_AS((void)forge::parseStatus("? unended"), std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"x what"})), std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"1 .M N... 100644"})), std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"1 .Z N... 100644 100644 100644 " + zeros + " " + zeros + " a"})),
	                std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"1 .M N... 1006x4 100644 100644 " + zeros + " " + zeros + " a"})),
	                std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"2 R. N... 100644 100644 100644 " + zeros + " " + zeros + " R1 a"})),
	                std::runtime_error); // a rename without its old path
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"? /etc/passwd"})), std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"? "})), std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"# branch.ab +x -1"})), std::runtime_error);
}

TEST_CASE("forge scm: the status is bounded") {
	const std::string three = nulEnded({"? a", "? b", "? c"});
	CHECK(forge::parseStatus(three, {.entries = 3})["files"].size() == 3);
	CHECK_THROWS_AS((void)forge::parseStatus(three, {.entries = 2}), std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(three, {.bytes = 8}), std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseStatus(nulEnded({"? " + std::string(20, 'a')}), {.path = 10}), std::runtime_error);
	CHECK(forge::parseStatus("")["files"].empty());
}

TEST_CASE("forge scm: the branch list") {
	const json branches = forge::parseBranches(" \0feature/x\n*\0main\n"s);
	CHECK(branches["current"] == "main");
	REQUIRE(branches["branches"].size() == 2);
	CHECK(branches["branches"][0]["id"] == "feature/x");
	CHECK_THROWS_AS((void)forge::parseBranches("main\n"), std::runtime_error);
	CHECK_THROWS_AS((void)forge::parseBranches("*\0main"s), std::runtime_error); // not ended
}

TEST_CASE("forge scm: only Forge's own git commands run") {
	CHECK_FALSE(forge::checkGitArguments({"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"}));
	CHECK_FALSE(forge::checkGitArguments({"add", "--", "src/a.cpp", "."}));
	CHECK_FALSE(forge::checkGitArguments({"restore", "--staged", "--", "-odd name"})); // after "--", never an option
	CHECK_FALSE(forge::checkGitArguments({"commit", "-F", "-"}));
	CHECK_FALSE(forge::checkGitArguments({"switch", "-c", "feature/scm-view"}));
	CHECK_FALSE(forge::checkGitArguments({"show", "--no-textconv", "HEAD:src/a.cpp"}));
	CHECK(forge::checkGitArguments({}));
	CHECK(forge::checkGitArguments({"-c", "core.pager=sh", "status"}));
	CHECK(forge::checkGitArguments({"commit", "-m", "message"}));
	CHECK(forge::checkGitArguments({"add", "src/a.cpp"}));
	CHECK(forge::checkGitArguments({"add", "--", "../outside"}));
	CHECK(forge::checkGitArguments({"add", "--", "/etc/passwd"}));
	CHECK(forge::checkGitArguments({"switch", "--orphan", "x"}));
	CHECK(forge::checkGitArguments({"switch", "-c", "--upload-pack=x"}));
	CHECK(forge::checkGitArguments({"show", "--no-textconv", "HEAD:../x"}));
	CHECK(forge::checkGitArguments({"push"}));
}

TEST_CASE("forge scm: branch names") {
	CHECK(forge::isBranchName("main"));
	CHECK(forge::isBranchName("feature/scm-view_2"));
	for(const char* name : {"", "-x", "a..b", "a/", "/a", ".a", "a/.b", "a.lock", "a b", "a~1", "a^", "a:b", "HEAD",
	                        "a@{1}", "a//b"})
		CHECK_FALSE(forge::isBranchName(name));
}

TEST_CASE("forge scm: a line diff and its gutter marks") {
	const json diff = forge::diffLines("a\nb\nc\nd\n", "a\nB\nc\nd\ne\n");
	CHECK(diff["added"] == 2);
	CHECK(diff["removed"] == 1);
	const json& rows = diff["rows"];
	REQUIRE(rows.size() == 6);
	CHECK(rows[1]["kind"] == "removed");
	CHECK(rows[1]["text"] == "b");
	CHECK(rows[2]["kind"] == "added");
	CHECK(rows[2]["modified"] == 2);
	const json marks = forge::gutterMarks(diff);
	REQUIRE(marks.size() == 2);
	CHECK(marks[0]["text"] == "modified");
	CHECK(marks[0]["line"] == 2);
	CHECK(marks[1]["text"] == "added");
	CHECK(marks[1]["line"] == 5);
	const json removed = forge::diffLines("a\nb\nc", "a\nc");
	CHECK(forge::gutterMarks(removed)[0]["text"] == "deleted");
	CHECK(forge::gutterMarks(removed)[0]["line"] == 2);
}

TEST_CASE("forge scm: a large diff is bounded") {
	std::string before, after;
	for(int i = 0; i < 30; ++i) before += "line " + std::to_string(i) + "\n", after += "other " + std::to_string(i) + "\n";
	const json diff = forge::diffLines(before, after, {.matched = 10, .rows = 40});
	CHECK(diff["removed"] == 30);
	CHECK(diff["added"] == 30);
	CHECK(diff["rows"].size() == 40);
	CHECK(diff["truncated"].get<bool>());
}

TEST_CASE("forge scm: a real repository's status") {
	const std::filesystem::path git = forge::findGit();
	if(git.empty()) return; // no git here: nothing to compare with
	const std::filesystem::path folder =
	    std::filesystem::temp_directory_path() / ("aui-forge-scm-" + std::to_string(std::random_device{}()));
	std::filesystem::create_directories(folder);
	std::ofstream(folder / "kept.txt") << "one\n";
	runGit(git, folder, {"init", "-q"});
	runGit(git, folder, {"add", "--", "kept.txt"});
	runGit(git, folder, {"commit", "-q", "-F", "-"}, "first\n");
	std::ofstream(folder / "kept.txt") << "two\n";
	std::ofstream(folder / "new file.txt") << "new\n";
	const json status = forge::parseStatus(runGit(
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

#ifndef _WIN32
TEST_CASE("forge scm: Forge's git runs no hook the repository plants") {
	const std::filesystem::path git = forge::findGit();
	if(git.empty()) return; // no git here: nothing to run
	const std::filesystem::path folder =
	    std::filesystem::temp_directory_path() / ("aui-forge-hooks-" + std::to_string(std::random_device{}()));
	std::filesystem::create_directories(folder);
	runGit(git, folder, {"init", "-q"});
	const std::filesystem::path hook = folder / ".git" / "hooks" / "pre-commit";
	std::ofstream(hook) << "#!/bin/sh\ntouch ran\n"; // a hook runs at the top of the work tree
	std::filesystem::permissions(hook, std::filesystem::perms::owner_all);
	std::ofstream(folder / "kept.txt") << "one\n";
	runGit(git, folder, {"add", "--", "kept.txt"});

	runGit(git, folder, {"commit", "-q", "--allow-empty", "-F", "-"}, "plain git runs it\n");
	const bool ranWithout = std::filesystem::exists(folder / "ran");
	std::filesystem::remove(folder / "ran");
	std::vector<std::string> arguments = forge::gitOptions();
	arguments.insert(arguments.end(), {"commit", "-q", "--allow-empty", "-F", "-"});
	runGit(git, folder, arguments, "Forge's doesn't\n");
	const bool ranWith = std::filesystem::exists(folder / "ran");
	std::error_code error;
	std::filesystem::remove_all(folder, error);
	CHECK(ranWithout);
	CHECK_FALSE(ranWith);
}
#endif
