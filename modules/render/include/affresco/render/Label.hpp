#pragma once

/// @file
/// @brief Label: a panel that draws text.

#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

namespace affresco {

/// @brief A panel that draws text, with padding, alignment, optional wrap and optional fit-to-text sizing.
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

	/// @brief Shows an icon before the text (with a space between when there is text), in the text's colour.
	///
	/// The icon is a glyph named in the label's font or its fallbacks (Font::codepointNamed()): give the font an icon
	/// font as a fallback, and the icon is drawn at the text's size, in the same batch. A name no face has shows
	/// nothing.
	/// @param name The glyph's name, e.g. "search"; empty for none.
	void setIcon(std::string name);
	/// @brief The icon's name, or empty.
	[[nodiscard]] const std::string& icon() const noexcept { return icon_; }

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

protected:
	void prepareLayout() override;
	void paint(Painter& painter) const override;

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

	std::string icon_;                 ///< See icon().
	std::u32string shown_;             ///< The icon's character, a space and the text: see shown().
	std::uint64_t shownFont_ = 0;      ///< The Font::id() shown_ was made with (0: to make again).
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
