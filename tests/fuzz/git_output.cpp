/// @file
/// @brief Fuzzes how Forge reads git's output (examples/forge/features/scm): `git status --porcelain=v2 -z`,
/// `git for-each-ref`, and the line diff of two texts git shows. A repository's file names and contents are untrusted:
/// whoever made the repository chose them.
///
/// The first byte picks the reader; small limits let inputs reach them. What reads is within the limits, every file has
/// a known kind, at most one branch is current, and a diff's rows account for every line of both texts.

#include "Fuzz.hpp"

#include "features/scm/GitStatus.hpp"
#include "features/scm/LineDiff.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

/// @brief The limits the readers are fuzzed with, small so inputs reach them.
constexpr forge::GitLimits limits{.bytes = 4096, .entries = 16, .path = 64};

/// @brief Reads a status and checks it.
/// @param output git's output.
void readStatus(std::string_view output) {
	static const std::set<std::string> kinds = {"modified", "added", "deleted", "renamed", "copied", "typechange",
	                                            "unmerged", "untracked", "ignored"};
	const affresco::json status = forge::parseStatus(output, limits);
	if(status.at("files").size() > limits.entries) affresco::fuzz::broken("at most the entries limit of files");
	for(const affresco::json& file : status.at("files")) {
		if(!kinds.contains(file.at("kind").get<std::string>())) affresco::fuzz::broken("a file's kind is known");
		if(file.at("path").get<std::string>().empty() || file.at("path").get<std::string>().size() > limits.path)
			affresco::fuzz::broken("a path is not empty, within the limit");
	}
}

/// @brief Reads branches and checks them.
/// @param output git's output.
void readBranches(std::string_view output) {
	const affresco::json branches = forge::parseBranches(output, limits);
	if(branches.at("branches").size() > limits.entries) affresco::fuzz::broken("at most the entries limit of branches");
	int current = 0;
	for(const affresco::json& branch : branches.at("branches")) current += branch.at("current").get<bool>() ? 1 : 0;
	if(current > 1) affresco::fuzz::broken("at most one branch is current");
}

/// @brief Counts the lines of a text as the diff does: each line feed ends one, and a last line without one counts.
/// @param text The text.
/// @return How many.
std::size_t lineCount(std::string_view text) {
	std::size_t lines = 0;
	for(const char character : text) lines += character == '\n' ? 1 : 0;
	return lines + (!text.empty() && !text.ends_with('\n') ? 1 : 0);
}

/// @brief Diffs the two texts on either side of the first NUL and checks the rows.
/// @param input The texts.
void diff(std::string_view input) {
	const std::size_t nul = input.find('\0');
	const std::string_view original = input.substr(0, nul);
	const std::string_view modified = nul == std::string_view::npos ? std::string_view() : input.substr(nul + 1);
	const affresco::json result = forge::diffLines(original, modified, {.matched = 32, .rows = 256});
	if(result.at("rows").size() > 256) affresco::fuzz::broken("a diff returns at most its rows limit");
	if(result.at("truncated").get<bool>()) return;
	std::size_t same = 0;
	for(const affresco::json& row : result.at("rows")) same += row.at("kind") == "same" ? 1 : 0;
	if(same + result.at("removed").get<std::size_t>() != lineCount(original) ||
	   same + result.at("added").get<std::size_t>() != lineCount(modified))
		affresco::fuzz::broken("a diff's rows account for every line of both texts");
	static_cast<void>(forge::gutterMarks(result));
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0) return 0;
	const std::string_view output = affresco::fuzz::text(data + 1, size - 1);
	try {
		switch(data[0] % 3) {
		case 0: readStatus(output); break;
		case 1: readBranches(output); break;
		default: diff(output); break;
		}
	} catch(const std::runtime_error&) {
		return 0; // malformed or over a limit: refused, as it should be
	}
	return 0;
}
