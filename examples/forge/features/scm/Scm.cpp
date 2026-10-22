/// @file
/// @brief Forge's source control natives and git grant (Scm.hpp).

#include "Scm.hpp"
#include "GitArguments.hpp"
#include "GitStatus.hpp"
#include "LineDiff.hpp"

#include <affresco/core/Json.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace forge {

namespace {

using affresco::json;

constexpr std::size_t largestText = std::size_t{8} << 20; ///< A text forge.scm.diff compares, at most 8 MiB.

/// @brief A text argument of a call.
/// @param with The call's arguments.
/// @param name The argument.
/// @return It, or "" if it isn't text.
/// @throws std::runtime_error If it is over largestText.
std::string textArgument(const json& with, const char* name) {
	std::string text = with.is_object() && with.contains(name) && with[name].is_string() ? with[name].get<std::string>()
	                                                                                    : std::string();
	if(text.size() > largestText) throw std::runtime_error(std::string("the ") + name + " text is too large");
	return text;
}

/// @brief A file as a list item for the source control view.
/// @param file A parseStatus() file.
/// @return `{ "id": path, "label": name, "detail": folder, "badge": letter, "kind" }`.
json listItem(const json& file) {
	const std::string path = file.at("path").get<std::string>();
	const std::size_t slash = path.rfind('/');
	const std::string kind = file.at("kind").get<std::string>();
	const char* badge = kind == "added"       ? "A"
	                    : kind == "deleted"   ? "D"
	                    : kind == "renamed"   ? "R"
	                    : kind == "copied"    ? "C"
	                    : kind == "unmerged"  ? "!"
	                    : kind == "untracked" ? "U"
	                                          : "M";
	return {{"id", path},
	        {"label", slash == std::string::npos ? path : path.substr(slash + 1)},
	        {"detail", slash == std::string::npos ? std::string() : path.substr(0, slash)},
	        {"badge", badge},
	        {"kind", kind}};
}

/// @brief The status, with its files sorted into the view's two lists (an ignored file is in neither).
/// @param output git's status output.
/// @return parseStatus()'s result, with `staged`, `changes` and `count`.
json statusForView(std::string_view output) {
	json status = parseStatus(output);
	json staged = json::array(), changes = json::array();
	for(const json& file : status.at("files")) {
		if(file.at("kind") == "ignored") continue;
		if(file.at("staged").get<bool>()) staged.push_back(listItem(file));
		if(file.at("unstaged").get<bool>()) changes.push_back(listItem(file));
	}
	status["count"] = staged.size() + changes.size();
	status["staged"] = std::move(staged);
	status["changes"] = std::move(changes);
	return status;
}

/// @brief The host's rule for git's runs: checkGitArguments(), and the open folder's repository only.
/// @param workspace The open folder.
/// @return The shaper.
affresco::services::ProcessShaper gitRule(std::shared_ptr<Workspace> workspace) {
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

void registerScm(affresco::Screen& screen, std::shared_ptr<Workspace> workspace) {
	if(const std::filesystem::path git = findGit(); !git.empty())
		screen.services().allowProcess("git", git, gitOptions(), std::nullopt, gitRule(workspace));
	screen.onCall("forge.scm.parseStatus", [](const std::string& arguments) {
		return statusForView(textArgument(affresco::parseUntrusted(arguments), "output")).dump();
	});
	screen.onCall("forge.scm.parseBranches", [](const std::string& arguments) {
		return parseBranches(textArgument(affresco::parseUntrusted(arguments), "output")).dump();
	});
	screen.onCall("forge.scm.diff", [](const std::string& arguments) {
		const json with = affresco::parseUntrusted(arguments);
		json diff = diffLines(textArgument(with, "original"), textArgument(with, "modified"));
		diff["gutter"] = gutterMarks(diff);
		return diff.dump();
	});
}

} // namespace forge
