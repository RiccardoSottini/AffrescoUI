#pragma once

/// @file
/// @brief DiffView: two texts compared, side by side (rows aligned, scrolled together) or inline (deleted lines above
/// the lines inserted for them), in read-only editors with each text's line numbers and the changed characters
/// marked.

#include "affresco/render/Panel.hpp"
#include "affresco/render/TextEditor.hpp"

#include <affresco/core/TextDiff.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief The colours of a DiffView's changes.
struct DiffColors {
	Color deleted{0.85f, 0.25f, 0.25f, 0.16f};      ///< Behind deleted lines.
	Color inserted{0.25f, 0.70f, 0.35f, 0.16f};     ///< Behind inserted lines.
	Color deletedText{0.85f, 0.25f, 0.25f, 0.34f};  ///< Behind the characters deleted from a changed line.
	Color insertedText{0.25f, 0.70f, 0.35f, 0.34f}; ///< Behind the characters inserted into a changed line.
	Color filler{0.5f, 0.5f, 0.5f, 0.08f};          ///< Behind the blank rows that keep both sides aligned.
	Color divider{0.5f, 0.5f, 0.5f, 0.35f};         ///< The line between both sides.

	/// @brief Whether two sets of colours are the same.
	bool operator==(const DiffColors&) const = default;
};

/// @brief Two texts compared line by line (core's diffLines()), then character by character on changed lines.
///
/// The editors come from a factory given a language id, so an application can supply syntax-coloured ones; they are
/// read-only, never wrap, and have no folds, so rows stay aligned. Texts from anywhere are diffed within the default
/// DiffLimits, and characters only on lines up to maxCharacterLine code points.
class DiffView : public Panel {
public:
	/// @brief How both texts are shown.
	enum class Mode {
		Split,  ///< The original on the left, the modified text on the right.
		Inline, ///< One editor: kept lines once, deleted lines then their replacements.
	};

	/// @brief Makes an editor for a language id (e.g. "cpp"; empty: plain text).
	using EditorFactory = std::function<std::unique_ptr<TextEditor>(std::string_view language)>;

	static constexpr std::size_t maxCharacterLine = 2'000;  ///< Longest line diffed character by character.
	static constexpr std::size_t maxCharacterPairs = 2'000; ///< Changed line pairs diffed character by character.

	/// @brief Creates an empty, side by side view.
	/// @param factory Makes its editors (empty: plain TextEditors).
	explicit DiffView(EditorFactory factory = {});

	/// @brief Compares two texts.
	/// @param original The first, UTF-8.
	/// @param modified The second.
	void setTexts(std::string_view original, std::string_view modified);
	/// @brief Shows them side by side or inline.
	/// @param mode The mode.
	void setMode(Mode mode);
	/// @brief Makes new editors for a language (from the factory).
	/// @param language Its id.
	void setLanguage(std::string_view language);
	/// @brief Sets the colours of the changes.
	/// @param colors The colours.
	void setColors(const DiffColors& colors);
	/// @brief Sets the editors' font.
	/// @param font The font, or nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief Sets the editors' text colour.
	/// @param color The colour (transparent: the editors' own).
	void setTextColor(Color color);

	/// @brief The mode.
	[[nodiscard]] Mode mode() const noexcept { return mode_; }
	/// @brief The line edit script between both texts.
	[[nodiscard]] const std::vector<DiffEdit>& edits() const noexcept { return edits_; }
	/// @brief The left editor (the only one, inline).
	[[nodiscard]] TextEditor& originalEditor() const noexcept { return *original_; }
	/// @brief The right editor, or nullptr inline.
	[[nodiscard]] TextEditor* modifiedEditor() const noexcept { return modified_; }

protected:
	void arrange() override;
	void paint(Painter& painter) const override;

private:
	/// @brief One editor's text, line numbers and decorations, built from the script.
	struct Side {
		std::string text;                          ///< UTF-8, lines joined by "\n".
		std::vector<std::size_t> labels;           ///< Each row's number (0: a filler row).
		std::vector<EditorDecoration> decorations; ///< Line backgrounds, then changed characters.
		std::size_t rows = 0;                      ///< Rows so far.
	};

	/// @brief Replaces the editors with new ones from the factory, as the mode needs.
	void makeEditors();
	/// @brief Fills the editors from the texts and the script.
	void refresh();
	/// @brief Gives an editor what a side built.
	/// @param editor The editor.
	/// @param side Its text, numbers and decorations.
	void fill(TextEditor& editor, Side side) const;
	/// @brief Builds both sides, aligned.
	/// @param left The original's side.
	/// @param right The modified side.
	void buildSplit(Side& left, Side& right) const;
	/// @brief Builds the one side of the inline mode.
	/// @param side It.
	void buildInline(Side& side) const;
	/// @brief Marks the characters changed between two lines, on each one's row.
	/// @param from The original line.
	/// @param to The modified line.
	/// @param fromSide Where the original line is shown.
	/// @param fromRow On which row.
	/// @param toSide Where the modified line is shown.
	/// @param toRow On which row.
	/// @param pairs Pairs diffed so far; counts this one.
	void markCharacters(std::string_view from, std::string_view to, Side& fromSide, std::size_t fromRow, Side& toSide,
	                    std::size_t toRow, std::size_t& pairs) const;

	EditorFactory factory_;                        ///< Makes the editors.
	Mode mode_ = Mode::Split;                      ///< See setMode().
	std::string language_;                         ///< See setLanguage().
	DiffColors colors_;                            ///< See setColors().
	std::shared_ptr<const Font> font_;             ///< See setFont().
	Color textColor_{0.0f, 0.0f, 0.0f, 0.0f};      ///< See setTextColor().
	std::string originalText_, modifiedText_;      ///< See setTexts().
	std::vector<std::string_view> originalLines_;  ///< originalText_'s lines.
	std::vector<std::string_view> modifiedLines_;  ///< modifiedText_'s lines.
	std::vector<DiffEdit> edits_;                  ///< See edits().
	TextEditor* original_ = nullptr;               ///< See originalEditor(); a child.
	TextEditor* modified_ = nullptr;               ///< See modifiedEditor(); a child, or nullptr.
	bool syncing_ = false;                         ///< While one editor's scroll is given to the other.
};

} // namespace affresco
