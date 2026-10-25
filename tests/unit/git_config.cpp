/// @file
/// @brief The repository the git grant runs on (Grants.hpp, checkGitRepository()): its own `.git` folder, and a config
/// whose keys name no program. A folder that arrives with its `.git` can't make a granted git run its filter.

#include "process/Process.hpp" // modules/services/src

#include <affresco/services/Grants.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <vector>

using affresco::services::checkGitConfig;

namespace {

/// @brief Runs git and waits for it.
/// @param git The program.
/// @param folder Where it runs.
/// @param arguments Its arguments.
/// @return Its exit code.
int runGit(const std::filesystem::path& git, const std::filesystem::path& folder, std::vector<std::string> arguments) {
	arguments.insert(arguments.begin(), {"-c", "init.defaultBranch=main"});
	const auto child = affresco::services::ChildProcess::start({git, arguments, folder, std::nullopt});
	child->closeInput();
	char buffer[4096];
	while(child->read(buffer, sizeof buffer) > 0) {
	}
	return child->wait();
}

} // namespace

TEST_CASE("git config: a repository's data keys pass, any key that may name a program or a file is refused") {
	CHECK_FALSE(checkGitConfig(""));
	CHECK_FALSE(checkGitConfig("[core]\n\trepositoryformatversion = 0\n\tfilemode = true\n\tbare = false\n"
	                           "\tlogallrefupdates = true\n\tignorecase = true\n\tprecomposeunicode = true\n"
	                           "[remote \"origin\"]\n\turl = https://example.com/x.git\n"
	                           "\tfetch = +refs/heads/*:refs/remotes/origin/*\n"
	                           "[branch \"main\"]\n\tremote = origin\n\tmerge = refs/heads/main\n"
	                           "# a comment\n; another\n[User]\n\tName = Someone\r\n\temail\n"));
	for(const char* config :
	    {"[filter \"x\"]\n\tclean = touch ran\n", "[Filter \"x\"]\n\tclean = touch ran\n", "[filter.x]\n\tclean = y\n",
	     "[filter \"x\"] clean = y\n", "[core]\n\tpager = y\n", "[core]\n\tsshCommand = y\n", "[include]\n\tpath = y\n",
	     "[includeIf \"gitdir:/\"]\n\tpath = y\n", "[diff \"x\"]\n\ttextconv = y\n", "[gpg]\n\tprogram = y\n",
	     "[credential]\n\thelper = y\n", "[core \"x\"]\n\tbare = true\n", "[extensions]\n\tworktreeConfig = true\n",
	     "[core]\n\tworktree = /\n", "[core]\n\teditor = y\n", "[core]\n\tattributesFile = y\n"}) {
		CAPTURE(config);
		CHECK(checkGitConfig(config).value_or("").contains("which the git grant doesn't run with"));
	}
	// What git might read differently is refused, rather than guessed at
	for(const char* config : {"bare = true\n", "[core]\n\tbare = \\\n[filter \"x\"]\n", "[core\n", "[a \"b]\n",
	                          "[a.b \"c\"]\n", "[core]\n\t1x = y\n", "[core]\n\tbare\x01 = y\n"}) {
		CAPTURE(config);
		CHECK(checkGitConfig(config));
	}
	CHECK(checkGitConfig(std::string("[core]\n\0", 9)));
	CHECK(checkGitConfig("[core]\n" + std::string(1 << 20, '#')));
}

#ifndef _WIN32
TEST_CASE("git config: a folder's own filter never runs through the git grant, and the grant says why") {
	const std::filesystem::path git = affresco::services::findProgram("git");
	if(git.empty()) return; // no git here: nothing to run
	const std::filesystem::path folder =
	    std::filesystem::temp_directory_path() / ("aui-git-config-" + std::to_string(std::random_device{}()));
	std::filesystem::create_directories(folder);
	REQUIRE(runGit(git, folder, {"init", "-q"}) == 0);
	std::ofstream(folder / ".git" / "config", std::ios::app) << "[filter \"x\"]\n\tclean = touch ran; cat\n";
	std::ofstream(folder / ".gitattributes") << "*.txt filter=x\n";
	std::ofstream(folder / "kept.txt") << "one\n";

	// The grant's options alone don't stop it: git runs the filter on `add`
	std::vector<std::string> plain = affresco::services::gitOptions();
	plain.insert(plain.end(), {"add", "--", "kept.txt"});
	CHECK(runGit(git, folder, plain) == 0);
	const bool ranWithout = std::filesystem::exists(folder / "ran");
	std::filesystem::remove(folder / "ran");

	// The grant refuses before git starts
	for(const std::vector<std::string>& arguments :
	    {std::vector<std::string>{"add", "--", "kept.txt"},
	     {"restore", "--", "kept.txt"},
	     {"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"},
	     {"switch", "-c", "other"}}) {
		affresco::services::ProcessRun run{arguments, folder};
		const std::optional<std::string> refused = affresco::services::gitShaper()(run);
		CHECK(refused.value_or("").contains("filter.<name>.clean"));
	}

	// Nor does it follow a .git that is a file naming a repository elsewhere, or a link to one
	const std::filesystem::path other = folder / "other";
	std::filesystem::create_directories(other);
	std::ofstream(other / ".git") << "gitdir: " << (folder / ".git").string() << "\n";
	affresco::services::ProcessRun pointed{{"init"}, other};
	CHECK(affresco::services::gitShaper()(pointed).value_or("").contains("isn't a folder"));
	std::filesystem::remove(other / ".git");
	std::filesystem::create_directory_symlink(folder / ".git", other / ".git");
	affresco::services::ProcessRun linked{{"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"},
	                                      other};
	CHECK(affresco::services::gitShaper()(linked).value_or("").contains("isn't a folder"));

	std::error_code error;
	std::filesystem::remove_all(folder, error);
	CHECK(ranWithout);
}
#endif
