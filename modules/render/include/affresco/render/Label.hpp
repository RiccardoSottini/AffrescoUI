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

	std::u32string text_; ///< One element per code point.

private:
	std::shared_ptr<const Font> font_;                           ///< nullptr: the window's default.
	Color textColor_ = colors::black;                            ///< See textColor().
	float padding_ = 0.0f;                                       ///< See padding().
	Align horizontal_ = Align::Start;                            ///< Horizontal alignment.
	Align vertical_ = Align::Start;                              ///< Vertical alignment.
	bool wrap_ = false;                                          ///< See setWrap().
	bool fitToText_ = false;                                     ///< See setFitToText().
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
