#pragma once

/// @file
/// @brief Splits lines of a declared language (Language.hpp) into scoped tokens, a line at a time, as TextMate
/// grammars do: the earliest match wins, a region's end wins ties, and regions carry over to the next line. Each
/// line is bounded: in bytes, in finds, in each find's steps and in time.

#include "Language.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace affresco::format {

/// @brief A region open at some place: its rule, its scopes and the expression that ends it.
struct LanguageFrame {
	std::uint32_t rule = 0;                 ///< The region's rule.
	ScopeId scope = 0;                      ///< Its "scope".
	ScopeId contentScope = 0;               ///< Its "contentScope".
	std::shared_ptr<const TextPattern> end; ///< What ends it (with begin's groups put in, if it refers to them).

	/// @brief Whether two frames are the same region in the same way.
	/// @param other The other.
	/// @return Whether they are.
	[[nodiscard]] bool operator==(const LanguageFrame& other) const = default;
};

/// @brief The regions open at a line's start, outermost first.
using LanguageState = std::vector<LanguageFrame>;

/// @brief A run of a line in some scopes.
struct LanguageToken {
	std::uint32_t from = 0;      ///< Its first byte in the line.
	std::uint32_t to = 0;        ///< The byte after it.
	std::vector<ScopeId> scopes; ///< Its scopes, outermost first.
};

/// @brief A line split into tokens: later ones paint over earlier ones (a match, then its groups).
struct TokenizedLine {
	std::vector<LanguageToken> tokens; ///< The tokens.
	LanguageState end;                 ///< The regions open at its end, so the next line's start.
	bool cut = false;                  ///< Whether a limit stopped it: the rest is in the scopes it had reached.
};

/// @brief Tokenizes lines of one language.
///
/// Invariants: never more than maxDepth regions open; each line costs at most maxFinds finds of at most
/// stepsPerFind steps, and about millisecondsPerLine. Not thread safe: one per editor.
class LanguageTokenizer {
public:
	static constexpr std::size_t maxLineBytes = 16 * 1024; ///< A line's bytes tokenized; the rest isn't split.
	static constexpr std::size_t maxDepth = 64;            ///< Regions open at once, at most.
	static constexpr std::size_t maxFinds = 2048;          ///< Finds for one line, at most.
	static constexpr std::uint32_t stepsPerFind = 20'000;  ///< PCRE2 steps of one find, at most.
	static constexpr double millisecondsPerLine = 10.0;    ///< Time for one line, about.
	static constexpr std::size_t maxGroupBytes = 256;      ///< Of a begin's group put in its end, at most.
	static constexpr std::size_t maxEnds = 64;             ///< Ends compiled with groups kept, at most.

	/// @brief A tokenizer for a language.
	/// @param language The language.
	explicit LanguageTokenizer(std::shared_ptr<const Language> language);

	/// @brief Splits a line.
	/// @param start The regions open at its start.
	/// @param line Its text (UTF-8, without its line break).
	/// @return Its tokens and the regions open at its end.
	[[nodiscard]] TokenizedLine tokenize(const LanguageState& start, std::string_view line);

	/// @brief The language.
	[[nodiscard]] const Language& language() const noexcept { return *language_; }

private:
	struct Walk;
	struct Best;

	/// @brief Finds the earliest match at a walk's place among the innermost region's end and the rules it tries.
	/// @param walk The walk.
	/// @param language The language.
	/// @param groups Room for groups.
	/// @return The winner, or none.
	static Best earliest(Walk& walk, const Language& language, PatternGroups& groups);

	/// @brief Takes one step of a walk: the earliest match, its tokens, and the region it opens or closes.
	/// @param walk The walk.
	/// @return Whether to go on.
	bool step(Walk& walk);

	/// @brief Applies the innermost region's end: its tokens, then the region closes.
	/// @param walk The walk.
	/// @param match The end's match.
	void closeRegion(Walk& walk, PatternMatch match);

	/// @brief Applies a rule's match: its tokens, and the region it opens if it begins one.
	/// @param walk The walk.
	/// @param index The rule.
	/// @param match Its match.
	/// @param groups Its groups.
	void openOrMatch(Walk& walk, std::uint32_t index, PatternMatch match,
	                 const std::vector<std::optional<PatternMatch>>& groups);

	/// @brief The expression that ends a region, its begin's groups put in if it refers to them.
	/// @param rule The region.
	/// @param text The line.
	/// @param groups The begin's groups.
	/// @return It.
	std::shared_ptr<const TextPattern> endOf(const LanguageRule& rule, std::string_view text,
	                                         const std::vector<std::optional<PatternMatch>>& groups);

	std::shared_ptr<const Language> language_;                                 ///< The language.
	PatternGroups groups_;                                                     ///< Reused by each find.
	std::unordered_map<std::string, std::shared_ptr<const TextPattern>> ends_; ///< Ends with groups put in.
	std::shared_ptr<const TextPattern> lineEnd_;                               ///< `$`, for an end that fails.
};

} // namespace affresco::format
