/// @file
/// @brief Forge's source control: git's status and branch list, read strictly (GitStatus.hpp).

#include "GitStatus.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string>
#include <vector>

namespace forge {

namespace {

using affresco::json;

/// @brief Fails a read.
/// @param what What is wrong.
/// @throws std::runtime_error Always.
[[noreturn]] void malformed(const std::string& what) { throw std::runtime_error("git's output is malformed: " + what); }

/// @brief Splits a text at one character, the way git ends its records: the last piece must be empty (each record
/// ended), and there are at most `most` records.
/// @param text The text.
/// @param separator The character that ends each record.
/// @param most The most records it takes.
/// @return The records.
/// @throws std::runtime_error If a record isn't ended or there are too many.
std::vector<std::string_view> records(std::string_view text, char separator, std::size_t most) {
	std::vector<std::string_view> pieces;
	while(!text.empty()) {
		const std::size_t end = text.find(separator);
		if(end == std::string_view::npos) malformed("a record isn't ended");
		if(pieces.size() == most) throw std::runtime_error("git's output has too many entries");
		pieces.push_back(text.substr(0, end));
		text.remove_prefix(end + 1);
	}
	return pieces;
}

/// @brief Splits a record at its first `count` spaces: the fields before them, and the rest (a path, which may hold
/// spaces).
/// @param record The record.
/// @param count The fields before the rest.
/// @return The fields, then the rest.
/// @throws std::runtime_error If there are fewer fields.
std::vector<std::string_view> fields(std::string_view record, std::size_t count) {
	std::vector<std::string_view> parts;
	for(std::size_t i = 0; i < count; ++i) {
		const std::size_t space = record.find(' ');
		if(space == std::string_view::npos) malformed("an entry has too few fields");
		parts.push_back(record.substr(0, space));
		record.remove_prefix(space + 1);
	}
	parts.push_back(record);
	return parts;
}

/// @brief Whether every character of a field is one of a set.
/// @param field The field.
/// @param allowed The set.
/// @return Whether it is.
bool only(std::string_view field, std::string_view allowed) noexcept {
	return std::all_of(field.begin(), field.end(), [allowed](char c) { return allowed.contains(c); });
}

/// @brief Checks a field: a length and a set of characters.
/// @param field The field.
/// @param sizes The lengths it may have (one or two).
/// @param allowed Its characters.
/// @param name What it is, for the error.
/// @throws std::runtime_error If it isn't.
void expect(std::string_view field, std::initializer_list<std::size_t> sizes, std::string_view allowed,
            const char* name) {
	if(std::find(sizes.begin(), sizes.end(), field.size()) == sizes.end() || !only(field, allowed))
		malformed(std::string("a bad ") + name);
}

constexpr std::string_view octal = "01234567";               ///< A file mode's characters.
constexpr std::string_view hexadecimal = "0123456789abcdef"; ///< An object name's characters.
constexpr std::string_view states = ".MTADRCU";              ///< git's X and Y letters.

/// @brief Checks a path git reported: not empty, within the limit, no NUL (already split off), relative.
/// @param path The path.
/// @param limits The limits.
/// @return It.
/// @throws std::runtime_error If it isn't one.
std::string checkedPath(std::string_view path, const GitLimits& limits) {
	if(path.empty() || path.size() > limits.path || path.front() == '/') malformed("a bad path");
	return std::string(path);
}

/// @brief What an entry's two letters say happened to it.
/// @param index The X letter (the index against HEAD).
/// @param worktree The Y letter (the working tree against the index).
/// @return The kind (GitStatus.hpp).
std::string kindOf(char index, char worktree) {
	const char letter = index != '.' ? index : worktree;
	switch(letter) {
	case 'A': return "added";
	case 'D': return "deleted";
	case 'R': return "renamed";
	case 'C': return "copied";
	case 'T': return "typechange";
	default: return "modified";
	}
}

/// @brief Builds a file's entry.
/// @param path Its path.
/// @param original A rename's or copy's old path, else "".
/// @param letters The XY letters ("??" untracked, "!!" ignored, "UU"... unmerged).
/// @param kind Its kind.
/// @return The entry.
json fileEntry(std::string path, std::string original, std::string_view letters, std::string kind) {
	const bool tracked = letters[0] != '?' && letters[0] != '!', unmerged = kind == "unmerged";
	const bool staged = tracked && !unmerged && letters[0] != '.';
	const bool unstaged = !tracked || unmerged || letters[1] != '.';
	return {{"path", std::move(path)},
	        {"original", std::move(original)},
	        {"index", std::string(1, letters[0])},
	        {"worktree", std::string(1, letters[1])},
	        {"kind", std::move(kind)},
	        {"staged", staged},
	        {"unstaged", unstaged}};
}

/// @brief Reads an ordinary (`1`) or renamed or copied (`2`) entry.
/// @param record The record, without its `1 ` or `2 `.
/// @param renamed Whether it is a `2` entry.
/// @param original The `2` entry's old path (its own record), else empty.
/// @param limits The limits.
/// @return The entry.
/// @throws std::runtime_error If it is malformed.
json changedEntry(std::string_view record, bool renamed, std::string_view original, const GitLimits& limits) {
	const std::vector<std::string_view> parts = fields(record, renamed ? 8 : 7);
	expect(parts[0], {2}, states, "status");
	expect(parts[1], {4}, "NSCMU.", "submodule state");
	for(std::size_t i = 2; i < 5; ++i) expect(parts[i], {6}, octal, "file mode");
	for(std::size_t i = 5; i < 7; ++i) expect(parts[i], {40, 64}, hexadecimal, "object name");
	if(renamed) {
		const std::string_view score = parts[7];
		if(score.size() < 2 || (score[0] != 'R' && score[0] != 'C') || score.size() > 4 ||
		   !only(score.substr(1), "0123456789"))
			malformed("a bad score");
	}
	if(parts[0][0] == 'U' || parts[0][1] == 'U') malformed("an unmerged path in an ordinary entry");
	return fileEntry(checkedPath(parts.back(), limits), renamed ? checkedPath(original, limits) : std::string(),
	                 parts[0], kindOf(parts[0][0], parts[0][1]));
}

/// @brief Reads an unmerged (`u`) entry.
/// @param record The record, without its `u `.
/// @param limits The limits.
/// @return The entry.
/// @throws std::runtime_error If it is malformed.
json unmergedEntry(std::string_view record, const GitLimits& limits) {
	const std::vector<std::string_view> parts = fields(record, 9);
	expect(parts[0], {2}, "ADU", "status");
	expect(parts[1], {4}, "NSCMU.", "submodule state");
	for(std::size_t i = 2; i < 6; ++i) expect(parts[i], {6}, octal, "file mode");
	for(std::size_t i = 6; i < 9; ++i) expect(parts[i], {40, 64}, hexadecimal, "object name");
	return fileEntry(checkedPath(parts.back(), limits), "", parts[0], "unmerged");
}

/// @brief Reads a count from `# branch.ab +A -B`.
/// @param field `+A` or `-B`.
/// @param sign The sign it starts with.
/// @return A or B.
/// @throws std::runtime_error If it isn't one.
int count(std::string_view field, char sign) {
	int value = 0;
	if(field.size() < 2 || field.size() > 10 || field[0] != sign) malformed("a bad ahead or behind count");
	const auto [end, error] = std::from_chars(field.data() + 1, field.data() + field.size(), value);
	if(error != std::errc() || end != field.data() + field.size() || value < 0) malformed("a bad ahead or behind count");
	return value;
}

/// @brief Reads one `# branch.*` header into the branch.
/// @param header The header, without its `# `.
/// @param branch The branch so far.
/// @throws std::runtime_error If it is malformed. Other headers (`# stash`) are skipped.
void readHeader(std::string_view header, json& branch) {
	const std::size_t space = header.find(' ');
	if(space == std::string_view::npos) malformed("a header without a value");
	const std::string_view key = header.substr(0, space), value = header.substr(space + 1);
	if(value.empty() || value.size() > 1024 || value.contains('\n')) malformed("a bad header");
	if(key == "branch.oid") {
		if(value != "(initial)") expect(value, {40, 64}, hexadecimal, "commit");
		branch["oid"] = std::string(value);
	} else if(key == "branch.head") branch["head"] = std::string(value);
	else if(key == "branch.upstream") branch["upstream"] = std::string(value);
	else if(key == "branch.ab") {
		const std::vector<std::string_view> parts = fields(value, 1);
		branch["ahead"] = count(parts[0], '+');
		branch["behind"] = count(parts[1], '-');
	}
}

} // namespace

json parseStatus(std::string_view output, const GitLimits& limits) {
	if(output.size() > limits.bytes) throw std::runtime_error("git's output is too large");
	const std::vector<std::string_view> pieces = records(output, '\0', limits.entries * 2 + 8);
	json branch = {{"head", ""}, {"oid", ""}, {"upstream", ""}, {"ahead", 0}, {"behind", 0}};
	json files = json::array();
	for(std::size_t i = 0; i < pieces.size(); ++i) {
		const std::string_view record = pieces[i];
		if(record.size() < 2 || record[1] != ' ') malformed("an entry without a type");
		if(record[0] != '#' && files.size() == limits.entries)
			throw std::runtime_error("git's output has too many entries");
		const std::string_view rest = record.substr(2);
		switch(record[0]) {
		case '#': readHeader(rest, branch); break;
		case '1': files.push_back(changedEntry(rest, false, {}, limits)); break;
		case '2':
			if(++i == pieces.size()) malformed("a rename without its old path");
			files.push_back(changedEntry(rest, true, pieces[i], limits));
			break;
		case 'u': files.push_back(unmergedEntry(rest, limits)); break;
		case '?': files.push_back(fileEntry(checkedPath(rest, limits), "", "??", "untracked")); break;
		case '!': files.push_back(fileEntry(checkedPath(rest, limits), "", "!!", "ignored")); break;
		default: malformed("an unknown entry type");
		}
	}
	return {{"branch", std::move(branch)}, {"files", std::move(files)}};
}

json parseBranches(std::string_view output, const GitLimits& limits) {
	if(output.size() > limits.bytes) throw std::runtime_error("git's output is too large");
	json branches = json::array();
	std::string current;
	for(const std::string_view line : records(output, '\n', limits.entries)) {
		if(line.size() < 3 || (line[0] != '*' && line[0] != ' ') || line[1] != '\0') malformed("a bad branch line");
		const std::string_view name = line.substr(2);
		if(name.size() > 1024 || name.contains('\0')) malformed("a bad branch name");
		const bool here = line[0] == '*';
		if(here && !current.empty()) malformed("two current branches"); // git marks only the one HEAD is on
		if(here) current = name;
		branches.push_back({{"id", std::string(name)}, {"label", std::string(name)}, {"current", here}});
	}
	return {{"current", std::move(current)}, {"branches", std::move(branches)}};
}

} // namespace forge
