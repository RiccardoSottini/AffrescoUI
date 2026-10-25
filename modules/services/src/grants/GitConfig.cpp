/// @file
/// @brief The repository the git grant runs on: its own folder, and a configuration that names no program (Grants.hpp).
///
/// A folder may arrive with its `.git` (an unpacked archive, a copied project), and git runs programs that the
/// repository's own configuration names: filter drivers, textconv, gpg, pagers, editors. The grant reads that
/// configuration first and runs only when every key is one of a short list of data keys.

#include "affresco/services/Grants.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace affresco::services {

namespace {

constexpr std::size_t longestConfig = 1 << 20; ///< A repository's config, at most 1 MiB.

/// @brief A section's keys the grant allows, as git spells them in lower case.
struct AllowedSection {
	std::string_view section;                ///< The section ("core").
	bool subsection;                         ///< Whether it has one ([remote "origin"]) rather than none ([core]).
	std::array<std::string_view, 18> keys{}; ///< Its keys; the empty ones are unused.
};

/// @brief The keys a repository's own config may set: data git reads, never a program, a file or another repository.
constexpr std::array<AllowedSection, 11> allowedSections = {{
    {"core",
	 false,
	 {"repositoryformatversion", "filemode", "bare", "logallrefupdates", "ignorecase", "precomposeunicode", "symlinks",
	  "autocrlf", "eol", "safecrlf", "checkstat", "trustctime", "untrackedcache", "quotepath", "longpaths",
	  "sharedrepository", "abbrev", "filesreflocktimeout"}},
    {"remote", true, {"url", "pushurl", "fetch", "push", "tagopt", "prune", "mirror"}},
    {"branch", true, {"remote", "merge", "rebase", "pushremote", "description"}},
    {"user", false, {"name", "email", "signingkey"}},
    {"init", false, {"defaultbranch"}},
    {"extensions", false, {"objectformat", "refstorage"}},
    {"pull", false, {"rebase", "ff"}},
    {"push", false, {"default", "autosetupremote"}},
    {"fetch", false, {"prune"}},
    {"gc", false, {"auto"}},
    {"lfs", false, {"repositoryformatversion"}},
}};

/// @brief Whether a character is a blank git skips.
/// @param c The character.
/// @return Whether it is.
bool blank(char c) noexcept { return c == ' ' || c == '\t' || c == '\v' || c == '\f'; }

/// @brief An ASCII text in lower case.
/// @param text The text.
/// @return It in lower case.
std::string lower(std::string_view text) {
	std::string result(text);
	std::ranges::transform(result, result.begin(),
	                       [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
	return result;
}

/// @brief Whether a key is one the grant allows.
/// @param section Its section, in lower case.
/// @param subsection Whether the section has a subsection.
/// @param key The key, in lower case.
/// @return Whether it is.
bool allowed(std::string_view section, bool subsection, std::string_view key) {
	return std::ranges::any_of(allowedSections, [&](const AllowedSection& allowed) {
		return allowed.section == section && allowed.subsection == subsection && !key.empty() &&
		       std::ranges::find(allowed.keys, key) != allowed.keys.end();
	});
}

/// @brief Where a config line's section is read.
struct Section {
	std::string name;        ///< The section, in lower case; empty before the first header.
	bool subsection = false; ///< Whether it has a subsection.
};

/// @brief Reads a section header, `[name]`, `[name.sub]` or `[name "sub"]`.
/// @param line The line, from its `[`.
/// @param section Gets the section.
/// @return What follows the header on its line, or std::nullopt if it is malformed.
std::optional<std::string_view> readHeader(std::string_view line, Section& section) {
	std::size_t at = 1;
	while(at < line.size() &&
	      (std::isalnum(static_cast<unsigned char>(line[at])) || line[at] == '-' || line[at] == '.'))
		++at;
	const std::string_view name = line.substr(1, at - 1);
	if(name.empty() || at == line.size()) return std::nullopt;
	if(line[at] == ']') { // [name] or the old [name.sub]
		const std::size_t dot = name.find('.');
		section = {lower(name.substr(0, dot)), dot != std::string_view::npos};
		return line.substr(at + 1);
	}
	if(!blank(line[at]) || name.contains('.')) return std::nullopt;
	while(at < line.size() && blank(line[at])) ++at;
	if(at == line.size() || line[at] != '"') return std::nullopt;
	for(++at; at < line.size() && line[at] != '"'; ++at)
		if(line[at] == '\\') ++at; // an escaped character, '"' or '\'
	if(at + 1 >= line.size() || line[at + 1] != ']') return std::nullopt;
	section = {lower(name), true};
	return line.substr(at + 2);
}

/// @brief Checks a key line, `name [= value]`.
/// @param line The line, from the key's first character.
/// @param section Its section.
/// @return Why it is refused, or std::nullopt.
std::optional<std::string> checkKey(std::string_view line, const Section& section) {
	if(section.name.empty() || !std::isalpha(static_cast<unsigned char>(line.front())))
		return "the repository's git config isn't one the git grant reads";
	std::size_t end = 1;
	while(end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '-')) ++end;
	if(end < line.size() && !blank(line[end]) && line[end] != '=' && line[end] != '#' && line[end] != ';')
		return "the repository's git config isn't one the git grant reads";
	const std::string key = lower(line.substr(0, end));
	if(!allowed(section.name, section.subsection, key))
		return "the repository's own git config sets " + section.name + (section.subsection ? ".<name>." : ".") +
		       key.substr(0, 64) + ", which the git grant doesn't run with (a key that may name a program or file)";
	if(line.back() == '\\') return "the repository's git config continues a line, which the git grant doesn't read";
	return std::nullopt;
}

} // namespace

std::optional<std::string> checkGitConfig(std::string_view text) {
	if(text.size() > longestConfig) return "the repository's git config is over 1 MiB";
	if(text.contains('\0')) return "the repository's git config isn't text";
	if(text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3); // git skips a byte order mark
	Section section;
	for(std::size_t start = 0; start < text.size();) {
		const std::size_t end = std::min(text.find('\n', start), text.size());
		std::string_view line = text.substr(start, end - start);
		start = end + 1;
		if(line.ends_with('\r')) line.remove_suffix(1);
		while(!line.empty() && blank(line.front())) line.remove_prefix(1);
		if(!line.empty() && line.front() == '[') {
			const std::optional<std::string_view> rest = readHeader(line, section);
			if(!rest) return "the repository's git config has a section header the git grant doesn't read";
			line = *rest;
			while(!line.empty() && blank(line.front())) line.remove_prefix(1);
		}
		if(line.empty() || line.front() == '#' || line.front() == ';') continue;
		if(std::optional<std::string> refused = checkKey(line, section)) return refused;
	}
	return std::nullopt;
}

std::optional<std::string> checkGitRepository(const std::filesystem::path& root) {
	std::error_code error;
	const std::filesystem::path gitDirectory = root / ".git";
	const std::filesystem::file_type kind = std::filesystem::symlink_status(gitDirectory, error).type();
	if(kind == std::filesystem::file_type::not_found) return std::nullopt; // no repository yet: git says so itself
	if(kind != std::filesystem::file_type::directory)
		return "the folder's .git isn't a folder (a link, or a file naming a repository elsewhere): the git grant "
		       "runs on the folder's own repository only";
	if(std::filesystem::symlink_status(gitDirectory / "commondir", error).type() !=
	   std::filesystem::file_type::not_found)
		return "the folder's repository shares another one's (.git/commondir): the git grant runs on its own only";
	const std::filesystem::path config = gitDirectory / "config";
	const std::filesystem::file_type configKind = std::filesystem::symlink_status(config, error).type();
	if(configKind == std::filesystem::file_type::not_found) return std::nullopt;
	if(configKind != std::filesystem::file_type::regular) return "the repository's git config isn't a plain file";
	std::ifstream file(config, std::ios::binary);
	std::string text(longestConfig + 1, '\0'); // one more, so that a longer one is refused
	file.read(text.data(), static_cast<std::streamsize>(text.size()));
	if(file.bad()) return "the repository's git config can't be read";
	text.resize(static_cast<std::size_t>(file.gcount()));
	return checkGitConfig(text);
}

} // namespace affresco::services
