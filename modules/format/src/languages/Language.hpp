#pragma once

/// @file
/// @brief The "languages" section: code languages declared in JSON (TextMate-style patterns whose scopes are
/// coloured by the `@syntax.*` tokens, plus comments, brackets, auto-closing pairs, indentation rules and a word
/// pattern), read and compiled once per document with limits on every part.

#include "affresco/core/Json.hpp"
#include "affresco/core/TextPattern.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

/// @brief A scope's index in Language::scopes, from 1; 0 is none.
using ScopeId = std::uint16_t;

/// @brief One rule of a language's patterns: a match, a begin/end region, or a group of other rules.
struct LanguageRule {
	/// @brief What it is.
	enum class Kind : std::uint8_t {
		Match,   ///< "match": one regular expression.
		Region,  ///< "begin" and "end": a region, which may span lines, with its own patterns inside.
		Group,   ///< Only "patterns" (or the language's top level).
		Include, ///< "include": another rule, by its place in the repository or "$self".
	};
	/// @brief A scope given to one group of a match.
	struct Capture {
		std::uint8_t group = 0; ///< The group, 0 for the whole match.
		ScopeId scope = 0;      ///< Its scope.
	};

	Kind kind = Kind::Group;                    ///< What it is.
	ScopeId scope = 0;                          ///< "scope": of the match, or of the whole region.
	ScopeId contentScope = 0;                   ///< "contentScope": of a region's text between begin and end.
	std::shared_ptr<const TextPattern> pattern; ///< "match", or a region's "begin".
	std::shared_ptr<const TextPattern> end;     ///< A region's "end", unless it refers to begin's groups.
	std::string endSource;                      ///< A region's "end" as written, for one that refers (`\1`).
	bool anchored = false;                      ///< The expression uses `\G`, so a find can't be reused.
	std::vector<Capture> captures;              ///< "captures" (of a match, or of both a region's ends).
	std::vector<Capture> beginCaptures;         ///< "beginCaptures".
	std::vector<Capture> endCaptures;           ///< "endCaptures".
	std::vector<std::uint32_t> patterns;        ///< The rules inside, as written (a group's or a region's).
	std::uint32_t target = 0;                   ///< What an include names.
	std::vector<std::uint32_t> candidates;      ///< The matches and regions its patterns come to, includes opened.
};

/// @brief A language as the document declares it, compiled.
struct Language {
	static constexpr std::size_t maxRules = 2048;     ///< Rules of one language, at most.
	static constexpr std::size_t maxCandidates = 512; ///< Matches and regions one place tries, at most.
	static constexpr std::size_t maxNesting = 16;     ///< Depth of "patterns" inside "patterns", at most.
	static constexpr std::size_t maxScopes = 1024;    ///< Distinct scopes of one language, at most.
	static constexpr std::size_t maxScopeBytes = 128; ///< The longest scope name.
	static constexpr std::size_t maxNames = 64;       ///< Aliases, or file patterns, at most.
	static constexpr std::size_t maxPairs = 32;       ///< Brackets, or auto-closing pairs, at most.

	std::string name;                                    ///< Its key in "languages".
	std::string title;                                   ///< "title", e.g. "C++"; the name if none.
	std::vector<std::string> aliases;                    ///< "aliases", lower case.
	std::vector<std::string> files;                      ///< "files": file names or patterns (`*.py`).
	std::string lineComment;                             ///< "comments.line", e.g. "//".
	std::pair<std::string, std::string> blockComment;    ///< "comments.block", e.g. "/*" and "*/".
	std::vector<std::pair<char32_t, char32_t>> brackets; ///< "brackets": opening and closing.
	std::vector<std::pair<char32_t, char32_t>> pairs;    ///< "pairs": typed one, the other is added.
	std::shared_ptr<const TextPattern> indentIncrease;   ///< "indent.increase": the line before a deeper one.
	std::shared_ptr<const TextPattern> indentDecrease;   ///< "indent.decrease": a line that goes back a level.
	std::shared_ptr<const TextPattern> wordPattern;      ///< "wordPattern": what a word is, or null for the default.
	std::vector<LanguageRule> rules;                     ///< Its rules; [0] is its top level.
	std::vector<std::string> scopes;                     ///< Every scope its rules name; ScopeId n is [n - 1].
	json about;                                          ///< What documents read of it (the `language()` function).
};

/// @brief The languages of a document: read from its "languages" section, found by name, alias or file name.
///
/// Invariants: every language compiled within Language's limits; at most maxLanguages. Read-only once made, so
/// editors share it.
class LanguageCatalog {
public:
	static constexpr std::size_t maxLanguages = 64; ///< Languages of one document, at most.
	static constexpr std::size_t maxNameBytes = 64; ///< The longest name, alias or file pattern.

	/// @brief Reads a "languages" section.
	/// @param section `name → language`.
	/// @return The catalog.
	/// @throws DocumentError If a language is invalid or over a limit (the message names its place).
	[[nodiscard]] static std::shared_ptr<const LanguageCatalog> read(const json& section);

	/// @brief Finds a language by its name, an alias (any case) or a file's name or path its "files" match.
	/// @param name The name, alias or file.
	/// @return It, or nullptr.
	[[nodiscard]] std::shared_ptr<const Language> find(std::string_view name) const;

	/// @brief The languages, by name.
	[[nodiscard]] const std::map<std::string, std::shared_ptr<const Language>, std::less<>>&
	languages() const noexcept {
		return languages_;
	}

private:
	std::map<std::string, std::shared_ptr<const Language>, std::less<>> languages_; ///< See languages().
};

/// @brief Compiles a language's "patterns" and "repository" into its rules (LanguageRules.cpp).
/// @param language The language, its top level not yet compiled.
/// @param value Its declaration.
/// @param at Its place, for errors.
/// @throws DocumentError If a rule is invalid or over a limit.
void compileLanguageRules(Language& language, const json& value, const std::string& at);

/// @brief Compiles a regular expression of a language.
/// @param source The expression.
/// @param at Its place, for errors.
/// @return It.
/// @throws DocumentError If it doesn't compile or is too long.
[[nodiscard]] std::shared_ptr<const TextPattern> compileLanguagePattern(std::string_view source, const std::string& at);

/// @brief Whether a region's "end" refers to its begin's groups (`\1` to `\9`).
/// @param source The expression.
/// @return Whether it does.
[[nodiscard]] bool refersToGroups(std::string_view source) noexcept;

/// @brief A region's "end" with its begin's groups put in, their punctuation escaped so each matches only itself.
/// @param source The expression.
/// @param groups The groups' text, [0] the whole begin; a missing one is empty.
/// @return The expression to compile.
[[nodiscard]] std::string withGroups(std::string_view source, const std::vector<std::string>& groups);

/// @brief Whether a file name matches a pattern of "files": `*` any run of characters, `?` one, anything else itself
/// (any case).
/// @param pattern The pattern.
/// @param name The file's name (not its folder).
/// @return Whether it does.
[[nodiscard]] bool fileNameMatches(std::string_view pattern, std::string_view name) noexcept;

} // namespace affresco::format
