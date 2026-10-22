/// @file
/// @brief Forge's source control: the git commands Forge runs (GitArguments.hpp).

#include "GitArguments.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace forge {

namespace {

constexpr std::size_t mostPaths = 1000;         ///< Paths one `add` or `restore` names, at most.
constexpr std::size_t longestPath = 4096;       ///< A path, at most 4 KiB.
constexpr std::size_t longestBranchName = 200; ///< A branch name, at most 200 characters.

/// @brief Whether arguments are exactly some words.
/// @param arguments The arguments.
/// @param words The words.
/// @return Whether they are.
template <std::size_t Count>
bool exactly(const std::vector<std::string>& arguments, const std::array<std::string_view, Count>& words) {
	return std::equal(arguments.begin(), arguments.end(), words.begin(), words.end());
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

} // namespace

bool isBranchName(std::string_view name) noexcept {
	if(name.empty() || name.size() > longestBranchName || name == "HEAD" || name == "@") return false;
	if(name.front() == '-' || name.front() == '/' || name.back() == '/' || name.back() == '.') return false;
	if(name.find("..") != std::string_view::npos || name.find("//") != std::string_view::npos ||
	   name.find("/.") != std::string_view::npos || name.front() == '.')
		return false;
	if(name.size() >= 5 && name.substr(name.size() - 5) == ".lock") return false;
	if(name.find(".lock/") != std::string_view::npos) return false;
	return std::all_of(name.begin(), name.end(), [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
		       c == '/' || c == '-';
	});
}

bool isFolderPath(std::string_view path) noexcept {
	if(path.empty() || path.size() > longestPath || path.front() == '/') return false;
	if(std::any_of(path.begin(), path.end(),
	               [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f || c == '\\'; }))
		return false;
	if(path.size() >= 2 && path[1] == ':') return false; // a drive, on Windows
	for(std::size_t start = 0; start <= path.size();) { // no ".." step
		const std::size_t end = std::min(path.find('/', start), path.size());
		if(path.substr(start, end - start) == "..") return false;
		start = end + 1;
	}
	return true;
}

std::optional<std::string> checkGitArguments(const std::vector<std::string>& arguments) {
	if(arguments.empty()) return "no git command";
	const std::string& verb = arguments.front();
	if(verb == "status") {
		if(exactly<5>(arguments, {"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"}))
			return std::nullopt;
	} else if(verb == "add") {
		if(arguments.size() > 1 && arguments[1] == "--") return checkPaths(arguments, 2);
	} else if(verb == "restore") {
		if(arguments.size() > 2 && arguments[1] == "--staged" && arguments[2] == "--") return checkPaths(arguments, 3);
	} else if(verb == "commit") {
		if(exactly<3>(arguments, {"commit", "-F", "-"})) return std::nullopt;
	} else if(verb == "for-each-ref") {
		if(exactly<3>(arguments, {"for-each-ref", "--format=%(HEAD)%00%(refname:short)", "refs/heads/"}))
			return std::nullopt;
	} else if(verb == "switch") {
		const bool create = arguments.size() == 3 && arguments[1] == "-c";
		if((arguments.size() == 2 || create) && isBranchName(arguments.back())) return std::nullopt;
		return "not a branch name Forge switches to";
	} else if(verb == "show") {
		const std::string_view object = arguments.size() == 3 ? std::string_view(arguments[2]) : std::string_view();
		if(arguments.size() == 3 && arguments[1] == "--no-textconv" && object.starts_with("HEAD:") &&
		   isFolderPath(object.substr(5)))
			return std::nullopt;
	}
	return "Forge doesn't run this git command";
}

} // namespace forge
