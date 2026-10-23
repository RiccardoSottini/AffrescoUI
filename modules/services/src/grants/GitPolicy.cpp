/// @file
/// @brief The git grant's policy: the commands it runs, and on which repository (Grants.hpp).

#include "affresco/services/Grants.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

namespace {

constexpr std::size_t mostPaths = 1000;        ///< Paths one `add` or `restore` names, at most.
constexpr std::size_t longestPath = 4096;      ///< A path, at most 4 KiB.
constexpr std::size_t longestBranchName = 200; ///< A branch name, at most 200 characters.
constexpr int mostCommits = 1000;              ///< Commits one `log` lists, at most.

/// @brief Whether arguments are exactly some words.
/// @param arguments The arguments.
/// @param words The words.
/// @return Whether they are.
template <std::size_t Count>
bool exactly(const std::vector<std::string>& arguments, const std::array<std::string_view, Count>& words) {
	return std::equal(arguments.begin(), arguments.end(), words.begin(), words.end());
}

/// @brief Whether arguments start with some words.
/// @param arguments The arguments.
/// @param words The words.
/// @return Whether they do.
template <std::size_t Count>
bool startsWith(const std::vector<std::string>& arguments, const std::array<std::string_view, Count>& words) {
	return arguments.size() >= Count && std::equal(words.begin(), words.end(), arguments.begin());
}

/// @brief Whether arguments are one of the commands that take nothing from the screen.
/// @param arguments The arguments.
/// @return Whether they are `init`, `status`, `commit` or `for-each-ref`, as checkGitArguments() lists them.
bool isFixedCommand(const std::vector<std::string>& arguments) {
	return exactly<1>(arguments, {"init"}) ||
	       exactly<5>(arguments, {"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"}) ||
	       exactly<3>(arguments, {"commit", "-F", "-"}) ||
	       exactly<3>(arguments, {"for-each-ref", "--format=%(HEAD)%00%(refname:short)", "refs/heads/"});
}

/// @brief Checks `VERB [OPTION] -- PATH...`: the words, then 1 to mostPaths folder paths.
/// @param arguments The arguments.
/// @param before The words before the paths, `--` last.
/// @return Why they are refused, or std::nullopt.
std::optional<std::string> checkPaths(const std::vector<std::string>& arguments, std::size_t before) {
	if(arguments.size() <= before) return "no path is named";
	if(arguments.size() - before > mostPaths) return "too many paths";
	for(std::size_t i = before; i < arguments.size(); ++i)
		if(!isFolderPath(arguments[i])) return "a path is outside the folder or malformed";
	return std::nullopt;
}

/// @brief Checks `switch NAME` and `switch -c NAME`.
/// @param arguments The arguments, `switch` first.
/// @return Why they are refused, or std::nullopt.
std::optional<std::string> checkSwitch(const std::vector<std::string>& arguments) {
	const bool create = arguments.size() == 3 && arguments[1] == "-c";
	if((arguments.size() == 2 || create) && isBranchName(arguments.back())) return std::nullopt;
	return "not a branch name to switch to";
}

/// @brief Checks `log --format=%H%x00%an%x00%at%x00%s -n N [-- PATH]`.
/// @param arguments The arguments, `log` first.
/// @return Why they are refused, or std::nullopt.
std::optional<std::string> checkLog(const std::vector<std::string>& arguments) {
	if(!(arguments.size() == 4 || arguments.size() == 6) ||
	   !startsWith<3>(arguments, {"log", "--format=%H%x00%an%x00%at%x00%s", "-n"}))
		return "not a log the git grant runs";
	int count = 0;
	const std::string& text = arguments[3];
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
	if(error != std::errc() || end != text.data() + text.size() || count < 1 || count > mostCommits)
		return "a log lists 1 to 1000 commits";
	if(arguments.size() == 6 && (arguments[4] != "--" || !isFolderPath(arguments[5])))
		return "a path is outside the folder or malformed";
	return std::nullopt;
}

/// @brief Whether arguments are `show --no-textconv HEAD:PATH`, PATH inside the folder.
/// @param arguments The arguments.
/// @return Whether they are.
bool isShowOfHead(const std::vector<std::string>& arguments) {
	if(arguments.size() != 3 || !startsWith<2>(arguments, {"show", "--no-textconv"})) return false;
	const std::string_view object = arguments[2];
	return object.starts_with("HEAD:") && isFolderPath(object.substr(5));
}

/// @brief Whether a character can be in a branch name the grant switches to.
/// @param c The character.
/// @return true for ASCII letters and digits, and `._/-`.
bool isBranchCharacter(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
	       c == '/' || c == '-';
}

/// @brief Whether a name breaks git's rules on where '.', '/' and '-' may go in a ref name.
/// @param name The name, not empty.
/// @return Whether it does.
bool breaksRefRules(std::string_view name) noexcept {
	if(name.front() == '-' || name.front() == '/' || name.front() == '.' || name.back() == '/' || name.back() == '.')
		return true;
	return name.contains("..") || name.contains("//") || name.contains("/.") || name.ends_with(".lock") ||
	       name.contains(".lock/");
}

} // namespace

bool isBranchName(std::string_view name) noexcept {
	if(name.empty() || name.size() > longestBranchName || name == "HEAD" || name == "@") return false;
	return !breaksRefRules(name) && std::ranges::all_of(name, isBranchCharacter);
}

// NOLINTNEXTLINE(bugprone-exception-escape): start never passes the end, so substr() doesn't throw
bool isFolderPath(std::string_view path) noexcept {
	if(path.empty() || path.size() > longestPath || path.front() == '/') return false;
	if(std::ranges::any_of(path, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f || c == '\\'; }))
		return false;
	if(path.size() >= 2 && path[1] == ':') return false; // a drive, on Windows
	for(std::size_t start = 0; start <= path.size();) {  // no ".." step
		const std::size_t end = std::min(path.find('/', start), path.size());
		if(path.substr(start, end - start) == "..") return false;
		start = end + 1;
	}
	return true;
}

std::optional<std::string> checkGitArguments(const std::vector<std::string>& arguments) {
	if(arguments.empty()) return "no git command";
	if(isFixedCommand(arguments) || isShowOfHead(arguments)) return std::nullopt;
	if(startsWith<2>(arguments, {"add", "--"})) return checkPaths(arguments, 2);
	if(startsWith<3>(arguments, {"restore", "--staged", "--"})) return checkPaths(arguments, 3);
	if(startsWith<2>(arguments, {"restore", "--"})) return checkPaths(arguments, 2);
	if(arguments.front() == "switch") return checkSwitch(arguments);
	if(arguments.front() == "log") return checkLog(arguments);
	return "the git grant doesn't run this command";
}

std::vector<std::string> gitOptions() {
	// A folder may come from anywhere: its .git/config and .git/hooks can name programs for git to run
	return {"-c",
	        "core.fsmonitor=false",
	        "-c",
	        "core.hooksPath=/dev/null",
	        "-c",
	        "commit.gpgSign=false",
	        "-c",
	        "core.quotePath=false",
	        "--no-optional-locks",
	        "--literal-pathspecs"};
}

ProcessShaper gitShaper() {
	return [](ProcessRun& run) -> std::optional<std::string> {
		if(run.directory.empty()) return "git runs on a declaration's root only";
		if(std::optional<std::string> refused = checkGitArguments(run.arguments)) return refused;
		if(run.arguments.front() == "init") return std::nullopt; // makes the folder's own repository
		const std::filesystem::path& root = run.directory;
		// before the command: its own repository, never one above the folder
		const std::u8string gitDir = (root / ".git").u8string(), workTree = root.u8string();
		run.arguments.insert(run.arguments.begin(), {"--git-dir=" + std::string(gitDir.begin(), gitDir.end()),
		                                             "--work-tree=" + std::string(workTree.begin(), workTree.end())});
		return std::nullopt;
	};
}

} // namespace affresco::services
