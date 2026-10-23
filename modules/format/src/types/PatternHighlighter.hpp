#pragma once
/// @file
/// @brief An editor highlighter from regular expressions: each line coloured by the leftmost match of a list of
/// patterns, for the languages that have no grammar (the editor's "patterns" key).

#include "affresco/core/TextPattern.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/TextEditor.hpp"

#include <optional>
#include <string>
#include <vector>

namespace affresco::format {

/// @brief Colours a TextEditor's lines by patterns (TextEditor::setHighlighter()).
///
/// On each line, the pattern whose match starts first (the earlier pattern on a tie) colours that match, and the
/// search goes on after it. Lines are coloured on their own: a match never spans lines.
class PatternHighlighter final : public EditorHighlighter {
public:
	static constexpr std::size_t maxRules = 64; ///< Patterns kept, at most; the rest are dropped.

	/// @brief One pattern and its colour.
	struct Rule {
		std::string pattern;        ///< The regular expression (PCRE2 syntax).
		std::string style;          ///< Its style, e.g. "comment", for the palette.
		std::optional<Color> color; ///< The colour it paints; nothing leaves the text colour.

		/// @brief Whether two rules are the same.
		[[nodiscard]] bool operator==(const Rule&) const = default;
	};

	/// @brief Replaces the patterns, compiling only when they changed.
	/// @param rules The patterns, in order (at most maxRules).
	/// @return An error for a pattern that doesn't compile (then nothing is coloured), or nothing.
	std::optional<std::string> setRules(std::vector<Rule> rules);
	/// @brief The patterns, as setRules() last gave them.
	[[nodiscard]] const std::vector<Rule>& rules() const noexcept { return rules_; }
	/// @brief Tags who installed it, so that only they replace it.
	/// @param owner Any address; nullptr for none.
	void setOwner(const void* owner) noexcept { owner_ = owner; }
	/// @brief Its tag (setOwner()).
	[[nodiscard]] const void* owner() const noexcept { return owner_; }

	void reset(const TextModel& model) override;
	void changed(const TextModel& model, const TextChange& change) override;
	void colorLine(const TextModel& model, std::size_t line, std::u32string_view text,
	               std::vector<Color>& colors) override;

private:
	std::vector<Rule> rules_;            ///< The patterns and their colours.
	std::vector<TextPattern> compiled_;  ///< Each rule's pattern, compiled; empty when one didn't compile.
	const void* owner_ = nullptr;        ///< See setOwner().
};

} // namespace affresco::format
