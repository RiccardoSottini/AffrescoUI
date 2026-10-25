/// @file
/// @brief Forge's source control (examples/forge/features/scm): the git commands Forge allows, branch names, and that
/// its git runs no hook a repository plants (skipped where git isn't installed).

#include "process/Process.hpp"
#include "features/scm/GitArguments.hpp"
#include "features/scm/Scm.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

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
