/// @file
/// @brief Forge's git grant (Scm.hpp).

#include "Scm.hpp"
#include "GitArguments.hpp"

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace forge {

namespace {

/// @brief The host's rule for git's runs: checkGitArguments(), and the open folder's repository only.
/// @param workspace The open folder.
/// @return The shaper.
affresco::services::ProcessShaper gitRule(const std::shared_ptr<Workspace>& workspace) {
	return [workspace](affresco::services::ProcessRun& run) -> std::optional<std::string> {
		if(workspace->root.empty()) return "no folder is open";
		if(std::optional<std::string> refused = checkGitArguments(run.arguments)) return refused;
		const std::filesystem::path root = workspace->root;
		// before the command: its own repository, never one above the folder
		run.arguments.insert(run.arguments.begin(),
		                     {"--git-dir=" + (root / ".git").string(), "--work-tree=" + root.string()});
		run.directory = root;
		return std::nullopt;
	};
}

} // namespace

std::filesystem::path findGit() {
#ifdef _WIN32
	constexpr char separator = ';';
	const char* name = "git.exe";
#else
	constexpr char separator = ':';
	const char* name = "git";
#endif
	const char* path = std::getenv("PATH");
	for(std::string_view folders = path ? path : ""; !folders.empty();) {
		const std::size_t end = std::min(folders.find(separator), folders.size());
		const std::filesystem::path folder(folders.substr(0, end));
		folders.remove_prefix(std::min(end + 1, folders.size()));
		std::error_code error;
		if(folder.is_absolute() && std::filesystem::is_regular_file(folder / name, error)) return folder / name;
	}
	return {};
}

std::vector<std::string> gitOptions() {
	// A folder Forge opens may come from anywhere: its .git/config and .git/hooks can name programs for git to run
	return {"-c", "core.fsmonitor=false", "-c", "core.hooksPath=/dev/null", "-c", "commit.gpgSign=false", "-c",
	        "core.quotePath=false", "--no-optional-locks", "--literal-pathspecs"};
}

void registerScm(affresco::Screen& screen, const std::shared_ptr<Workspace>& workspace) {
	if(const std::filesystem::path git = findGit(); !git.empty())
		screen.services().allowProcess("git", git, gitOptions(), std::nullopt, gitRule(workspace));
}

} // namespace forge
