#pragma once

/// @file
/// @brief Colours an editor's lines in a declared language (Language.hpp): each token in the colour of its innermost
/// scope that has one (`@syntax.<scope>`, or its nearest parent scope's), with runs painted over them (e.g. a
/// language server's semantic tokens).

#include "LanguageTokenizer.hpp"

#include "affresco/format/SyntaxHighlighter.hpp"
#include "affresco/render/TextEditor.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <memory>
#include <optional>
#include <vector>

namespace affresco::format {

/// @brief An EditorHighlighter for a declared language.
///
/// Invariants: states_[n] is the regions open at line n's start, for every n it holds; lines_ holds only lines
/// tokenized from those states. Texts of more than maxLines lines aren't coloured.
class LanguageHighlighter final : public EditorHighlighter {
public:
	static constexpr std::size_t maxLines = 200'000;  ///< Lines of a text coloured, at most.
	static constexpr std::size_t maxKeptLines = 4096; ///< Lines' tokens kept between frames, at most.

	/// @brief A highlighter for a language.
	/// @param language The language.
	explicit LanguageHighlighter(std::shared_ptr<const Language> language);

	/// @brief The language.
	[[nodiscard]] const std::shared_ptr<const Language>& language() const noexcept { return language_; }

	/// @brief Sets the colours of the scopes: each scope is given to it, then its parents ("string.escape", then
	/// "string") until one has a colour.
	/// @param palette The colours.
	void setPalette(const SyntaxHighlighter::Palette& palette);

	/// @brief Replaces the runs painted over the language's colours (at most SyntaxHighlighter::maxOverlays).
	/// @param runs The runs.
	void setOverlay(std::vector<SyntaxHighlighter::Overlay> runs);

	/// @brief Tags it with its owner, e.g. the node key that installed it.
	/// @param owner Any address; nullptr for none.
	void setOwner(const void* owner) noexcept { owner_ = owner; }
	/// @brief Its tag (setOwner()).
	[[nodiscard]] const void* owner() const noexcept { return owner_; }

	/// @brief The scopes at a place, outermost first, e.g. to tell a comment or a string from code.
	/// @param model The text.
	/// @param line The line.
	/// @param column The column, in code points.
	/// @return Their names.
	[[nodiscard]] std::vector<std::string> scopesAt(const TextModel& model, std::size_t line, std::size_t column);

	void reset(const TextModel& model) override;
	void changed(const TextModel& model, const TextChange& change) override;
	void colorLine(const TextModel& model, std::size_t line, std::u32string_view text,
	               std::vector<Color>& colors) override;

private:
	/// @brief A line's tokens, kept.
	/// @param model The text.
	/// @param line The line.
	/// @return Them, or nullptr if the text is too long to colour.
	const TokenizedLine* tokensOf(const TextModel& model, std::size_t line);

	/// @brief A line's text in UTF-8.
	/// @param model The text.
	/// @param line The line.
	/// @return It.
	[[nodiscard]] static std::string textOf(const TextModel& model, std::size_t line);

	/// @brief The colour of a token: its innermost scope's that has one.
	/// @param scopes Its scopes.
	/// @return It, or nothing to leave the text colour.
	[[nodiscard]] std::optional<Color> colorOf(const std::vector<ScopeId>& scopes) const;

	std::shared_ptr<const Language> language_;         ///< The language.
	LanguageTokenizer tokenizer_;                      ///< Splits the lines.
	std::vector<LanguageState> states_;                ///< The regions open at each line's start, known so far.
	std::map<std::size_t, TokenizedLine> lines_;       ///< Lines' tokens, kept.
	std::vector<std::optional<Color>> colors_;         ///< Each scope's colour, by ScopeId - 1.
	SyntaxHighlighter::Palette palette_;               ///< The colours, for the runs' styles.
	std::vector<SyntaxHighlighter::Overlay> overlays_; ///< Runs painted over, by line.
	const void* owner_ = nullptr;                      ///< See setOwner().
};

} // namespace affresco::format
