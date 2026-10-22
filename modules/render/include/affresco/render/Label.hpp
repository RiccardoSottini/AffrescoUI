#pragma once

/// @file
/// @brief Label: a panel that draws text.

#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/RichText.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

/// @brief A panel that draws text, with padding, alignment, optional wrap and optional fit-to-text sizing.
///
/// The text is one style, or rich: runs of their own font, colour, link or inline icon (setRuns()). A selectable
/// label lets the user select its text with the mouse and copy it with Cmd/Ctrl+C.
///
/// Transparent background by default; uses the window's default font unless given one. Glyphs land on whole pixels,
/// which keeps them sharp.
class Label : public Panel {
public:
	/// @brief An empty label.
	Label();

	/// @brief A label showing some text.
	/// @param text UTF-8 text.
	explicit Label(std::string_view text);

	/// @brief Replaces the text.
	/// @param utf8 UTF-8 text; invalid sequences become U+FFFD.
	void setText(std::string_view utf8);
	/// @brief The text, as UTF-8.
	[[nodiscard]] std::string text() const;
	/// @brief The text, one element per code point.
	[[nodiscard]] const std::u32string& codepoints() const noexcept { return text_; }

	/// @brief Replaces the text with runs of styled text; setText() goes back to one style.
	/// @param runs The runs, in order; text() is their text, without icons.
	void setRuns(std::vector<TextRun> runs);
	/// @brief The runs, or empty when the text has one style.
	[[nodiscard]] const std::vector<TextRun>& runs() const noexcept { return runs_; }

	/// @brief Lets the user select the text with the mouse, and copy it with Cmd/Ctrl+C (Cmd/Ctrl+A selects all).
	/// @param selectable true to allow it.
	void setSelectable(bool selectable);
	/// @brief Whether the text can be selected.
	[[nodiscard]] bool selectable() const noexcept { return selectable_; }
	/// @brief The selection, as code point positions in textLayout().text(): from and to, from <= to.
	[[nodiscard]] std::pair<std::size_t, std::size_t> selection() const noexcept {
		return {std::min(anchor_, caret_), std::max(anchor_, caret_)};
	}
	/// @brief Selects code points of textLayout().text().
	/// @param from The first.
	/// @param to One past the last.
	void select(std::size_t from, std::size_t to);
	/// @brief The selected text as UTF-8, without icons (code points in the Private Use Areas, where icon fonts put
	/// their glyphs).
	[[nodiscard]] std::string selectedText() const;
	/// @brief Sets the selection's colour.
	/// @param color The colour.
	void setSelectionColor(Color color);

	/// @brief The text as laid out for the label's current box: lines of pieces, from the block of lines' top-left.
	/// Empty without a font.
	[[nodiscard]] const TextLayout& textLayout() const;

	/// @brief Shows an icon before the text (with a space between when there is text), in the text's colour.
	///
	/// The icon is a glyph named in the label's font or its fallbacks (Font::codepointNamed()): give the font an icon
	/// font as a fallback, and the icon is drawn at the text's size, in the same batch. A name no face has shows
	/// nothing.
	/// @param name The glyph's name, e.g. "search"; empty for none.
	void setIcon(std::string name);
	/// @brief The icon's name, or empty.
	[[nodiscard]] const std::string& icon() const noexcept { return icon_; }
	/// @brief Sets whether the icon is mirrored right to left: "left" and "right" swap in its name ("chevron-left"
	/// draws "chevron-right"), when the font has the swapped one.
	/// @param mirrored Whether it is.
	void setIconMirrored(bool mirrored);

	/// @brief Sets the label's own font.
	/// @param font The font, or nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief The font in use: its own, else the window's default, else nullptr (nothing is drawn).
	[[nodiscard]] const Font* font() const noexcept;

	/// @brief Sets the text colour.
	/// @param color The colour.
	void setTextColor(Color color);
	/// @brief The text colour.
	[[nodiscard]] Color textColor() const noexcept { return textColor_; }

	/// @brief Sets the space between the edges and the text.
	/// @param padding In pixels, on every side.
	void setPadding(float padding);
	/// @brief The space between the edges and the text, in pixels.
	[[nodiscard]] float padding() const noexcept { return padding_; }

	/// @brief Sets where the text sits.
	/// @param horizontal Each line's placement.
	/// @param vertical The block of lines' placement.
	void setAlignment(Align horizontal, Align vertical);

	/// @brief Turns word wrap on or off.
	/// @param wrap true to break lines that don't fit the width.
	void setWrap(bool wrap);

	/// @brief Sizes the label to its text plus padding, instead of its width and height.
	/// @param fit true to fit the text.
	/// @param maxWidth With wrap on, the widest the label may get.
	void setFitToText(bool fit, float maxWidth = std::numeric_limits<float>::infinity());

	Signal<const std::string&> onLink; ///< A press on a run with a link: the link.

protected:
	void prepareLayout() override;
	void paint(Painter& painter) const override;
	void keyEvent(const KeyEvent& event) override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void focusEvent(bool focused) override;

	/// @brief The frame minus the padding.
	[[nodiscard]] Rect contentRect() const noexcept;
	/// @brief What is drawn: the icon and the text, as of the last layout. Without an icon, the text.
	[[nodiscard]] const std::u32string& shown() const noexcept { return icon_.empty() ? text_ : shown_; }

	std::u32string text_; ///< One element per code point.

private:
	/// @brief Draws one line of text, placed by the horizontal alignment.
	/// @param painter The painter.
	/// @param font The font.
	/// @param line The line.
	/// @param top Its top, in window units.
	void paintLine(Painter& painter, const Font& font, std::u32string_view line, float top) const;
	/// @brief Puts the icon before the text in shown_, if the text, the icon or the font changed.
	void updateShown();
	/// @brief Draws the runs.
	/// @param painter The painter.
	/// @param origin The block of lines' top-left.
	void paintRuns(Painter& painter, Vec2 origin) const;
	/// @brief Highlights the selection.
	/// @param painter The painter.
	/// @param origin The block of lines' top-left.
	void paintSelection(Painter& painter, Vec2 origin) const;
	/// @brief Where the block of lines starts: the content's left, and its top by the vertical alignment.
	[[nodiscard]] Vec2 blockOrigin() const;

	/// @brief What layout_ was made from, to make it again only when it changes.
	struct LaidOut {
		std::uint64_t text = 0;     ///< textVersion_ (0: not yet).
		std::uint64_t font = 0;     ///< Font::id().
		float maxWidth = 0.0f;      ///< Where lines break.
		float boxWidth = 0.0f;      ///< The width they are aligned in.
		Align align = Align::Start; ///< Their placement.
		std::size_t length = 0;     ///< The shown text's length (a text area changes it without a version).
		/// @brief Whether two are the same.
		friend bool operator==(const LaidOut&, const LaidOut&) = default;
	};

	std::vector<TextRun> runs_;                   ///< See runs().
	mutable TextLayout layout_;                   ///< See textLayout().
	mutable LaidOut laidOut_;                     ///< What layout_ was made from.
	std::uint64_t textVersion_ = 1;               ///< Changes with the text, its runs or its icon.
	bool selectable_ = false;                     ///< See selectable().
	bool selecting_ = false;                      ///< The mouse is selecting.
	std::size_t anchor_ = 0;                      ///< Where the selection started.
	std::size_t caret_ = 0;                       ///< Where it ends.
	Color selectionColor_ = Color::hex(0xb4d5fe); ///< See setSelectionColor().

	std::string icon_;                 ///< See icon().
	std::u32string shown_;             ///< The icon's character, a space and the text: see shown().
	std::uint64_t shownFont_ = 0;      ///< The Font::id() shown_ was made with (0: to make again).
	bool iconMirrored_ = false;        ///< See setIconMirrored().
	bool shownMirrored_ = false;       ///< Whether shown_ has the mirrored icon.
	std::shared_ptr<const Font> font_; ///< nullptr: the window's default.
	Color textColor_ = colors::black;  ///< See textColor().
	float padding_ = 0.0f;             ///< See padding().
	Align horizontal_ = Align::Start;  ///< Horizontal alignment.
	Align vertical_ = Align::Start;    ///< Vertical alignment.
	bool wrap_ = false;                ///< See setWrap().
	bool fitToText_ = false;           ///< See setFitToText().
	float fitMaxWidth_ = std::numeric_limits<float>::infinity(); ///< See setFitToText().

	/// @brief What prepareLayout() last fitted the label to, so a layout that changes none of it (a resize, a
	/// sibling's change) doesn't shape the text again.
	struct Fitted {
		std::u32string text;    ///< The text.
		std::uint64_t font = 0; ///< Its font's Font::id() (0: none yet).
		float maxWidth = 0.0f;  ///< The widest a line could be.
		float width = 0.0f;     ///< The widest line.
		std::size_t lines = 0;  ///< How many lines.
	} fitted_;
};

} // namespace affresco
