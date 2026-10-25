/// @file
/// @brief Fuzzes how the git grant's output is read (services' GitOutput.hpp, the `gitStatus` and `gitBranches`
/// expression functions): `git status --porcelain=v2 -z`, `git for-each-ref`, and the line diff of two texts git shows
/// (core's diffLines(), under the `diffLines` function). A repository's file names and contents are untrusted: whoever
/// made the repository chose them.
///
/// The first byte picks the reader; small limits let inputs reach them. What reads is within the limits, every file has
/// a known kind, at most one branch is current, and a diff's runs account for every line of both texts.

#include "Fuzz.hpp"

#include <affresco/core/TextDiff.hpp>
#include <affresco/services/GitOutput.hpp>

#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

/// @brief The limits the readers are fuzzed with, small so inputs reach them.
constexpr affresco::services::GitOutputLimits limits{.bytes = 4096, .entries = 16, .path = 64};

/// @brief Reads a status and checks it.
/// @param output git's output.
void readStatus(std::string_view output) {
	static const std::set<std::string> kinds = {"modified", "added", "deleted", "renamed", "copied", "typechange",
	                                            "unmerged", "untracked", "ignored"};
	const affresco::json status = affresco::services::readGitStatus(output, limits);
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
	const affresco::json branches = affresco::services::readGitBranches(output, limits);
	if(branches.at("branches").size() > limits.entries) affresco::fuzz::broken("at most the entries limit of branches");
	int current = 0;
	for(const affresco::json& branch : branches.at("branches")) current += branch.at("current").get<bool>() ? 1 : 0;
	if(current > 1) affresco::fuzz::broken("at most one branch is current");
}

/// @brief Diffs the lines on either side of the first NUL, within small limits, and checks the runs.
/// @param input The texts.
void diff(std::string_view input) {
	const std::size_t nul = input.find('\0');
	const std::string_view original = input.substr(0, nul);
	const std::string_view modified = nul == std::string_view::npos ? std::string_view() : input.substr(nul + 1);
	const std::vector<std::string_view> before = affresco::splitLines(original), after = affresco::splitLines(modified);
	std::size_t kept = 0, deleted = 0, inserted = 0;
	for(const affresco::DiffEdit& edit : affresco::diffLines(before, after, {.maxCost = 4096, .maxDistance = 64})) {
		if(edit.kind == affresco::DiffEdit::Kind::Equal) {
			for(std::size_t k = 0; k < edit.count; ++k)
				if(before.at(edit.original + k) != after.at(edit.modified + k))
					affresco::fuzz::broken("a kept line is the same in both texts");
			kept += edit.count;
		} else (edit.kind == affresco::DiffEdit::Kind::Delete ? deleted : inserted) += edit.count;
	}
	if(kept + deleted != before.size() || kept + inserted != after.size())
		affresco::fuzz::broken("a diff's runs account for every line of both texts");
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
