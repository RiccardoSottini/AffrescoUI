/// @file
/// @brief Expression functions of source control: gitStatus and gitBranches, which read the git grant's output
/// (services' GitOutput.hpp) for a source control view.

#include "ExpressionFunctions.hpp"

#include <affresco/services/GitOutput.hpp>

#include <array>
#include <exception>

namespace affresco::format::detail {
namespace {

/// @brief `gitStatus(output)`.
/// @param call [output].
/// @return services::readGitStatus()'s result, or null if the output isn't git's status or is over a limit.
Value gitStatus(const Call& call) {
	try {
		return Value::ownJson(services::readGitStatus(call[0].toText()));
	} catch(const std::exception&) {
		return {};
	}
}

/// @brief `gitBranches(output)`.
/// @param call [output].
/// @return services::readGitBranches()'s result, or null if the output isn't git's branch list or is over a limit.
Value gitBranches(const Call& call) {
	try {
		return Value::ownJson(services::readGitBranches(call[0].toText()));
	} catch(const std::exception&) {
		return {};
	}
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "gitStatus",
	 .arguments = "output",
	 .about = "The output of `git status --porcelain=v2 -z --branch --untracked-files=all` read: { branch: { head, oid, "
	          "upstream, ahead, behind }, files: [{ path, original, index, worktree, kind, staged, unstaged }] }, or "
	          "null if it isn't",
	 .takes = Takes::One,
	 .apply = gitStatus,
	 .remark = "up to 10000 files"},
    {.name = "gitBranches",
	 .arguments = "output",
	 .about = "The output of `git for-each-ref --format=%(HEAD)%00%(refname:short) refs/heads/` read: { current, "
	          "branches: [{ id, label, current }] }, or null if it isn't",
	 .takes = Takes::One,
	 .apply = gitBranches,
	 .remark = "up to 10000 branches"},
});

} // namespace

std::span<const Function> gitFunctions() { return table; }

} // namespace affresco::format::detail
