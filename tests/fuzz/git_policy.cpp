/// @file
/// @brief Fuzzes the git grant's policy (services/src/grants/GitPolicy.cpp): a screen, or a revision of it, chooses
/// every argument of a git run, and the policy is all that stands between them and git's options that run programs.
///
/// The input is arguments separated by zero bytes. What the policy accepts names no option after `--`, a path inside
/// the folder only, and no option the policy doesn't list; the shaper runs it on the root's own repository. The same
/// input is read as a repository's own config (checkGitConfig()), which a folder that arrives with its `.git` chooses.

#include "Fuzz.hpp"

#include <affresco/services/Grants.hpp>

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t maxArguments = 64; ///< Arguments in one input, at most.

/// @brief The options the policy may accept, anywhere before `--`.
const std::set<std::string_view> listed = {"--porcelain=v2",
                                           "-z",
                                           "--branch",
                                           "--untracked-files=all",
                                           "-F",
                                           "-",
                                           "--format=%(HEAD)%00%(refname:short)",
                                           "--no-textconv",
                                           "--staged",
                                           "-c",
                                           "--format=%H%x00%an%x00%at%x00%s",
                                           "-n"};

/// @brief Checks what the policy accepted.
/// @param arguments The arguments.
void checkAccepted(const std::vector<std::string>& arguments) {
	bool paths = false;
	for(std::size_t i = 1; i < arguments.size(); ++i) {
		const std::string& argument = arguments[i];
		if(paths) {
			if(!affresco::services::isFolderPath(argument)) affresco::fuzz::broken("a path is inside the folder");
		} else if(argument == "--") paths = true;
		else if(argument.starts_with('-') && !listed.contains(argument))
			affresco::fuzz::broken("an option the policy lists");
	}
	affresco::services::ProcessRun run{arguments, "/work/folder"};
	if(affresco::services::gitShaper()(run)) affresco::fuzz::broken("the shaper runs what the policy accepts");
	if(arguments.front() != "init" &&
	   (run.arguments.size() != arguments.size() + 2 || run.arguments.front() != "--git-dir=/work/folder/.git"))
		affresco::fuzz::broken("a run is on the root's own repository");
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	std::vector<std::string> arguments;
	for(std::size_t from = 0; from <= text.size() && arguments.size() < maxArguments;) {
		const std::size_t end = std::min(text.find('\0', from), text.size());
		arguments.emplace_back(text.substr(from, end - from));
		from = end + 1;
	}
	if(!affresco::services::checkGitArguments(arguments)) checkAccepted(arguments);
	for(const std::string& argument : arguments) static_cast<void>(affresco::services::isBranchName(argument));
	static_cast<void>(affresco::services::checkGitConfig(text)); // the input as a repository's config, too
	return 0;
}
