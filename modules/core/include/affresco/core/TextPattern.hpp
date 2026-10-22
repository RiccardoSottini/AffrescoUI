#pragma once

/// @file
/// @brief TextPattern: what the editor's find and the search service look for, plain text, a whole word or a regular
/// expression (PCRE2), with limits that bound the work a pattern from a peer can cause.
///
/// Unlike the rest of core, it isn't header-only: it is compiled into affrescoui::regex (cmake/AffrescoUIRegex.cmake),
/// which the modules that match text link.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace affresco {

/// @brief How a pattern reads.
struct PatternOptions {
	bool regex = false;         ///< A regular expression (PCRE2 syntax); otherwise plain text.
	bool wholeWord = false;     ///< Matches only whole words.
	bool caseSensitive = false; ///< Upper and lower case differ.
	/// @brief Compares two options.
	/// @return Whether they are the same.
	[[nodiscard]] bool operator==(const PatternOptions&) const = default;
};

/// @brief Where a pattern matched, in bytes of the UTF-8 subject.
struct PatternMatch {
	std::size_t offset = 0; ///< Its first byte.
	std::size_t length = 0; ///< Its bytes (0 for an empty match, e.g. `^`).
};

/// @brief The result of one search.
struct PatternFind {
	std::optional<PatternMatch> match; ///< The first match, if any.
	bool exceeded = false;             ///< The match limit stopped it: the subject may still match.
};

/// @brief A compiled pattern. Matching is const and safe from several threads at once.
///
/// Invariants: compiled with UTF-8 and Unicode properties, invalid UTF-8 in a subject is skipped rather than
/// refused, and every match runs under maxSteps and maxDepth, so a pathological expression (`(a+)+$`) ends early.
class TextPattern {
public:
	static constexpr std::size_t maxPatternBytes = 4096;  ///< The longest pattern compiled.
	static constexpr std::uint32_t maxSteps = 1'000'000;  ///< Backtracking steps per match, at most.
	static constexpr std::uint32_t maxDepth = 10'000;     ///< Backtracking depth per match, at most.
	static constexpr std::size_t maxExpandedBytes = std::size_t{64} << 20; ///< The longest replacement expand() gives.
	static constexpr std::uint32_t maxHeapKibibytes = 8192; ///< Heap per match, at most.

	TextPattern();
	TextPattern(TextPattern&& other) noexcept;
	TextPattern& operator=(TextPattern&& other) noexcept;
	TextPattern(const TextPattern&) = delete;
	TextPattern& operator=(const TextPattern&) = delete;
	~TextPattern();

	/// @brief Compiles a pattern, replacing the previous one.
	/// @param pattern The text or expression; empty matches nothing.
	/// @param options How it reads.
	/// @return An error ("the pattern is too long", or PCRE2's message with the offset), or nothing.
	[[nodiscard]] std::optional<std::string> compile(std::string_view pattern, PatternOptions options);

	/// @brief Whether a pattern is compiled.
	/// @return Whether find() can match.
	[[nodiscard]] bool valid() const noexcept { return code_ != nullptr; }

	/// @brief Finds the first match at or after a byte.
	/// @param subject UTF-8 text.
	/// @param start The byte to start at.
	/// @return The match, or none (with `exceeded` if a limit stopped it).
	[[nodiscard]] PatternFind find(std::string_view subject, std::size_t start = 0) const;

	/// @brief The replacement for one match: in a regular expression, `$0`–`$9` and `${name}` give its groups and
	/// `$$` a dollar; in plain text, the replacement as it is.
	/// @param subject The text find() matched in.
	/// @param match What it matched.
	/// @param replacement The replacement.
	/// @return The text to put instead of the match, or nothing if the replacement is invalid or would be over
	/// maxExpandedBytes (`$0` repeated over a long match).
	[[nodiscard]] std::optional<std::string> expand(std::string_view subject, PatternMatch match,
	                                                std::string_view replacement) const;

private:
	struct Code;                 ///< PCRE2's compiled pattern and match context.
	std::unique_ptr<Code> code_; ///< The pattern, or null.
	bool regex_ = false;         ///< Whether it is a regular expression (expand() substitutes groups).
};

} // namespace affresco
