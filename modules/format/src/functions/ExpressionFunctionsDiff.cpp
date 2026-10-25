/// @file
/// @brief Expression functions of changes: diffLines, which compares two texts line by line, and gitStatus and
/// gitBranches, which read the git grant's output (services' GitOutput.hpp) for a source control view.

#include "ExpressionFunctions.hpp"

#include <affresco/core/TextDiff.hpp>
#include <affresco/services/GitOutput.hpp>

#include <array>
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format::detail {
namespace {

constexpr std::size_t mostRows = 20000; ///< The rows and hunks diffLines gives, at most.

/// @brief A text's lines as a diff counts them: a last empty line (after a final "\n") is left out, so "" has none.
/// @param text The text.
/// @return Views into it.
std::vector<std::string_view> linesOf(std::string_view text) {
	std::vector<std::string_view> lines = splitLines(text);
	if(lines.back().empty()) lines.pop_back();
	return lines;
}

/// @brief `diffLines(original, modified)`.
/// @param call [original, modified].
/// @return `{ rows: [{ kind, original, modified, text }], hunks: [{ original, originalCount, modified, modifiedCount }],
/// added, removed, originalLines, modifiedLines, truncated }`.
Value diffLines(const Call& call) {
	const std::string original = call[0].toText(), modified = call[1].toText();
	const std::vector<std::string_view> before = linesOf(original), after = linesOf(modified);
	json rows = json::array(), hunks = json::array();
	std::size_t added = 0, removed = 0;
	bool truncated = false;
	const auto row = [&](const char* kind, std::size_t from, std::size_t to, std::string_view text) {
		if(rows.size() == mostRows) truncated = true;
		else rows.push_back({{"kind", kind}, {"original", from}, {"modified", to}, {"text", std::string(text)}});
	};
	const std::vector<DiffEdit> edits = affresco::diffLines(before, after);
	for(std::size_t i = 0; i < edits.size();) {
		const DiffEdit& edit = edits[i];
		if(edit.kind == DiffEdit::Kind::Equal) {
			for(std::size_t k = 0; k < edit.count; ++k)
				row("same", edit.original + k + 1, edit.modified + k + 1, after[edit.modified + k]);
			++i;
			continue;
		}
		// A hunk: the changed runs between two equal ones (deletions come before insertions)
		std::size_t deletedLines = 0, insertedLines = 0;
		for(; i < edits.size() && edits[i].kind != DiffEdit::Kind::Equal; ++i) {
			const DiffEdit& change = edits[i];
			const bool deleted = change.kind == DiffEdit::Kind::Delete;
			(deleted ? deletedLines : insertedLines) += change.count;
			for(std::size_t k = 0; k < change.count; ++k)
				if(deleted) row("removed", change.original + k + 1, 0, before[change.original + k]);
				else row("added", 0, change.modified + k + 1, after[change.modified + k]);
		}
		removed += deletedLines;
		added += insertedLines;
		if(hunks.size() == mostRows) truncated = true;
		else
			hunks.push_back({{"original", edit.original + 1},
			                 {"originalCount", deletedLines},
			                 {"modified", edit.modified + 1},
			                 {"modifiedCount", insertedLines}});
	}
	return Value::ownJson({{"rows", std::move(rows)},
	                       {"hunks", std::move(hunks)},
	                       {"added", added},
	                       {"removed", removed},
	                       {"originalLines", before.size()},
	                       {"modifiedLines", after.size()},
	                       {"truncated", truncated}});
}

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
    {.name = "diffLines",
	 .arguments = "original, modified",
	 .about = "The two texts compared line by line: { rows: [{ kind ('same', 'removed', 'added'), original, modified, "
	          "text }], hunks: [{ original, originalCount, modified, modifiedCount }], added, removed, originalLines, "
	          "modifiedLines, truncated } (line numbers from 1; a hunk's line in a text it has none of is the one it "
	          "comes before)",
	 .fewest = 2,
	 .most = 2,
	 .apply = diffLines,
	 .remark = "up to 20000 rows and hunks"},
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

std::span<const Function> diffFunctions() { return table; }

} // namespace affresco::format::detail
