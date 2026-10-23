/// @file
/// @brief Reading the "languages" section (Language.hpp): names, files, comments, brackets, pairs, indentation and
/// the word pattern, each checked and bounded; the patterns are compiled by LanguageRules.cpp.

#include "Language.hpp"

#include "affresco/render/RichText.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

namespace affresco::format {

namespace {

using expect::fail;
using expect::object;
using expect::string;

/// @brief Lower case, for names and aliases.
/// @param text ASCII or UTF-8 text.
/// @return It with A-Z lowered.
std::string lowered(std::string_view text) {
	std::string out(text);
	std::ranges::transform(out, out.begin(),
	                       [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
	return out;
}

/// @brief Reads a short name: a non-empty string of at most LanguageCatalog::maxNameBytes.
/// @param value The value.
/// @param at Its place.
/// @return It.
std::string shortName(const json& value, const std::string& at) {
	std::string name = string(value, at);
	if(name.empty() || name.size() > LanguageCatalog::maxNameBytes)
		fail(at, std::format("expected a name of 1 to {} bytes", LanguageCatalog::maxNameBytes));
	return name;
}

/// @brief Reads a list of short names ("aliases", "files").
/// @param language The declaration.
/// @param key The key.
/// @param at The declaration's place.
/// @param lower Whether to lower their case.
/// @return The names, or none without the key.
std::vector<std::string> names(const json& language, const char* key, const std::string& at, bool lower) {
	std::vector<std::string> out;
	const auto found = language.find(key);
	if(found == language.end()) return out;
	const std::string place = at + "." + key;
	if(!found->is_array() || found->size() > Language::maxNames)
		fail(place, std::format("expected a list of at most {} names", Language::maxNames));
	for(std::size_t index = 0; index < found->size(); ++index) {
		std::string name = shortName((*found)[index], std::format("{}[{}]", place, index));
		out.push_back(lower ? lowered(name) : std::move(name));
	}
	return out;
}

/// @brief Reads one code point given as a string of exactly one.
/// @param value The value.
/// @param at Its place.
/// @return The code point.
char32_t onePoint(const json& value, const std::string& at) {
	const std::u32string points = codepointsOf(string(value, at));
	if(points.size() != 1) fail(at, "expected one character");
	return points[0];
}

/// @brief Reads "brackets" or "pairs": `[["(", ")"], ...]`, one character each.
/// @param language The declaration.
/// @param key The key.
/// @param at The declaration's place.
/// @return The pairs, or none without the key.
std::vector<std::pair<char32_t, char32_t>> characterPairs(const json& language, const char* key,
                                                          const std::string& at) {
	std::vector<std::pair<char32_t, char32_t>> out;
	const auto found = language.find(key);
	if(found == language.end()) return out;
	const std::string place = at + "." + key;
	if(!found->is_array() || found->size() > Language::maxPairs)
		fail(place, std::format("expected a list of at most {} pairs", Language::maxPairs));
	for(std::size_t index = 0; index < found->size(); ++index) {
		const json& pair = (*found)[index];
		const std::string item = std::format("{}[{}]", place, index);
		if(!pair.is_array() || pair.size() != 2) fail(item, R"x(expected a pair, e.g. ["(", ")"])x");
		out.emplace_back(onePoint(pair[0], item + "[0]"), onePoint(pair[1], item + "[1]"));
	}
	return out;
}

/// @brief Reads "comments": `{ "line": "//", "block": ["/*", "*/"] }`.
/// @param language The language, which gets them.
/// @param value The declaration.
/// @param at The declaration's place.
void readComments(Language& language, const json& value, const std::string& at) {
	const auto found = value.find("comments");
	if(found == value.end()) return;
	const std::string place = at + ".comments";
	object(*found, place);
	for(const auto& [key, item] : found->items()) {
		if(key == "line") language.lineComment = shortName(item, place + ".line");
		else if(key == "block") {
			if(!item.is_array() || item.size() != 2) fail(place + ".block", R"(expected a pair, e.g. ["/*", "*/"])");
			language.blockComment = {shortName(item[0], place + ".block[0]"), shortName(item[1], place + ".block[1]")};
		} else fail(place + "." + key, "expected line or block");
	}
}

/// @brief Reads "indent": `{ "increase": regex, "decrease": regex }`.
/// @param language The language, which gets them.
/// @param value The declaration.
/// @param at The declaration's place.
void readIndent(Language& language, const json& value, const std::string& at) {
	const auto found = value.find("indent");
	if(found == value.end()) return;
	const std::string place = at + ".indent";
	object(*found, place);
	for(const auto& [key, item] : found->items()) {
		const std::string where = place + "." + key;
		if(key == "increase") language.indentIncrease = compileLanguagePattern(string(item, where), where);
		else if(key == "decrease") language.indentDecrease = compileLanguagePattern(string(item, where), where);
		else fail(where, "expected increase or decrease");
	}
}

/// @brief What documents read of a language (the `language()` function): its name, title, aliases, files,
/// comments, brackets and pairs.
/// @param language The language.
/// @param value Its declaration (already checked), for brackets and pairs as written.
/// @return It as JSON.
json aboutOf(const Language& language, const json& value) {
	json comments = json::object();
	if(!language.lineComment.empty()) comments["line"] = language.lineComment;
	if(!language.blockComment.first.empty())
		comments["block"] = json::array({language.blockComment.first, language.blockComment.second});
	return json{{"name", language.name},
	            {"title", language.title},
	            {"aliases", language.aliases},
	            {"files", language.files},
	            {"comments", std::move(comments)},
	            {"brackets", value.value("brackets", json::array())},
	            {"pairs", value.value("pairs", json::array())}};
}

/// @brief Reads one language.
/// @param name Its key.
/// @param value Its declaration.
/// @return It, compiled.
std::shared_ptr<const Language> readLanguage(const std::string& name, const json& value) {
	const std::string at = "languages." + name;
	object(value, at);
	static constexpr std::array<std::string_view, 10> known = {"title",    "aliases",   "files",  "comments",
	                                                           "brackets", "pairs",     "indent", "wordPattern",
	                                                           "patterns", "repository"};
	for(const auto& [key, item] : value.items())
		if(std::ranges::find(known, key) == known.end())
			fail(at + "." + key, "expected title, aliases, files, comments, brackets, pairs, indent, wordPattern, "
			                     "patterns or repository");
	auto language = std::make_shared<Language>();
	language->name = name;
	language->title = value.contains("title") ? shortName(value["title"], at + ".title") : name;
	language->aliases = names(value, "aliases", at, true);
	language->files = names(value, "files", at, false);
	readComments(*language, value, at);
	language->brackets = characterPairs(value, "brackets", at);
	language->pairs = characterPairs(value, "pairs", at);
	readIndent(*language, value, at);
	if(value.contains("wordPattern"))
		language->wordPattern =
		    compileLanguagePattern(string(value["wordPattern"], at + ".wordPattern"), at + ".wordPattern");
	compileLanguageRules(*language, value, at);
	language->about = aboutOf(*language, value);
	return language;
}

} // namespace

std::shared_ptr<const TextPattern> compileLanguagePattern(std::string_view source, const std::string& at) {
	auto pattern = std::make_shared<TextPattern>();
	if(source.empty()) fail(at, "expected a regular expression");
	if(const std::optional<std::string> error =
	       pattern->compile(source, PatternOptions{.regex = true, .wholeWord = false, .caseSensitive = true}))
		fail(at, "invalid regular expression: " + *error);
	return pattern;
}

bool fileNameMatches(std::string_view pattern, std::string_view name) noexcept {
	// Iterative wildcard matching: linear backtracking to the last '*' only, so no pattern is slow
	std::size_t p = 0, n = 0, star = std::string_view::npos, resume = 0;
	const auto same = [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	};
	while(n < name.size()) {
		if(p < pattern.size() && (pattern[p] == '?' || (pattern[p] != '*' && same(pattern[p], name[n])))) {
			++p;
			++n;
		} else if(p < pattern.size() && pattern[p] == '*') {
			star = p++;
			resume = n;
		} else if(star != std::string_view::npos) {
			p = star + 1;
			n = ++resume;
		} else return false;
	}
	while(p < pattern.size() && pattern[p] == '*') ++p;
	return p == pattern.size();
}

std::shared_ptr<const LanguageCatalog> LanguageCatalog::read(const json& section) {
	object(section, "languages");
	if(section.size() > maxLanguages) fail("languages", std::format("expected at most {} languages", maxLanguages));
	auto catalog = std::make_shared<LanguageCatalog>();
	for(const auto& [name, value] : section.items()) {
		static_cast<void>(shortName(json(name), "languages"));
		catalog->languages_.emplace(name, readLanguage(name, value));
	}
	return catalog;
}

std::shared_ptr<const Language> LanguageCatalog::find(std::string_view name) const {
	if(name.empty()) return nullptr;
	const std::string lower = lowered(name);
	for(const auto& [key, language] : languages_)
		if(lowered(key) == lower || std::ranges::find(language->aliases, lower) != language->aliases.end())
			return language;
	const std::size_t slash = name.find_last_of("/\\");
	const std::string_view file = slash == std::string_view::npos ? name : name.substr(slash + 1);
	for(const auto& [key, language] : languages_)
		for(const std::string& pattern : language->files)
			if(fileNameMatches(pattern, file)) return language;
	return nullptr;
}

} // namespace affresco::format
