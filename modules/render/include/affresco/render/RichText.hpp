#pragma once

/// @file
/// @brief Rich text: runs of styled text (font, colour, link, inline icon) and their layout into lines.

#include "affresco/render/Color.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Geometry.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief Decodes UTF-8 into code points, for TextRun::text; invalid bytes become U+FFFD.
/// @param utf8 The text.
/// @return Its code points.
[[nodiscard]] std::u32string codepointsOf(std::string_view utf8);

/// @brief A part of a text with its own style. Runs follow each other; a line breaks inside a run at a space or a
/// '\n', so a sentence can change style in the middle of a word.
struct TextRun {
	std::u32string text;              ///< The text; ignored when icon is set.
	std::shared_ptr<const Font> font; ///< Its font (e.g. a bold or larger one); nullptr: the label's.
	std::optional<Color> color;       ///< Its colour; none: the label's.
	std::string link;                 ///< What a click on it reports (e.g. a URL); empty: not a link.
	std::string icon;                 ///< A glyph named in its font (Font::codepointNamed()), drawn instead of text.
	bool underline = false;           ///< A line under it (a link has one too).
	bool strike = false;              ///< A line through it.
	std::optional<Color> background;  ///< A highlight behind it, as tall as its line; none: nothing.
	std::optional<Color> shadow;      ///< A copy of it in this colour, down and to the right, under it; none: no copy.
	/// @brief Whether two runs are the same (the same font object, not an equal one).
	friend bool operator==(const TextRun&, const TextRun&) = default;
};

/// @brief Lays runs, or one plain text, out into lines: pieces of one run each, placed on baselines.
///
/// Lines break greedily at spaces (a word wider than the line stays whole) and always at '\n'. Each line is as tall as
/// its tallest font. The layout is made again only when what it depends on changes, so drawing it allocates nothing.
class TextLayout {
public:
	/// @brief Text of one run on one line.
	struct Piece {
		std::size_t begin = 0;      ///< Its first code point in text().
		std::size_t end = 0;        ///< One past its last.
		std::size_t run = 0;        ///< Its run (0 for plain text).
		const Font* font = nullptr; ///< Its font.
		float x = 0.0f;             ///< Its left, from the box's left.
		float width = 0.0f;         ///< Its advance.
	};

	/// @brief One line of pieces.
	struct Line {
		std::size_t firstPiece = 0; ///< Its first piece.
		std::size_t endPiece = 0;   ///< One past its last piece.
		std::size_t begin = 0;      ///< Its first code point in text().
		std::size_t end = 0;        ///< One past its last (not counting the '\n' that ends it).
		float top = 0.0f;           ///< Its top, from the block's top.
		float ascent = 0.0f;        ///< From its top to its baseline.
		float height = 0.0f;        ///< Its height.
		float width = 0.0f;         ///< Its width, without the spaces it ends with.
	};

	/// @brief Lays runs out.
	/// @param runs The runs; an icon run whose font has no glyph by that name shows nothing.
	/// @param font The font of runs that have none.
	/// @param maxWidth Where lines break (infinity: only at '\n').
	/// @param boxWidth The width the lines are aligned in.
	/// @param align Each line's placement.
	void layOut(std::span<const TextRun> runs, const Font& font, float maxWidth, float boxWidth, Align align);
	/// @brief Lays one plain text out, in the same lines as Font::wrap() makes, so it matches what Label draws.
	/// @param text The text.
	/// @param font Its font.
	/// @param maxWidth Where lines break.
	/// @param boxWidth The width the lines are aligned in.
	/// @param align Each line's placement.
	void layOutPlain(std::u32string_view text, const Font& font, float maxWidth, float boxWidth, Align align);

	/// @brief Every code point laid out, icons included.
	[[nodiscard]] const std::u32string& text() const noexcept { return text_; }
	/// @brief The lines, top to bottom.
	[[nodiscard]] std::span<const Line> lines() const noexcept { return lines_; }
	/// @brief The pieces, line by line, left to right.
	[[nodiscard]] std::span<const Piece> pieces() const noexcept { return pieces_; }
	/// @brief The widest line.
	[[nodiscard]] float width() const noexcept { return width_; }
	/// @brief All lines.
	[[nodiscard]] float height() const noexcept { return height_; }

	/// @brief The code point a point is on, or the caret position nearest to it.
	/// @param point From the block's top-left.
	/// @return A position in text(), from 0 to its size: where a caret before the nearest code point goes.
	[[nodiscard]] std::size_t positionAt(Vec2 point) const;
	/// @brief The piece under a point.
	/// @param point From the block's top-left.
	/// @return Its index, or none when the point is between or outside pieces.
	[[nodiscard]] std::optional<std::size_t> pieceAt(Vec2 point) const;
	/// @brief Where a caret before a code point goes on its line.
	/// @param line The line.
	/// @param position A position from the line's begin to its end.
	/// @return From the box's left.
	[[nodiscard]] float caretX(const Line& line, std::size_t position) const;

private:
	/// @brief A run's font: its own, else the base one.
	/// @param run The run.
	/// @return The font.
	[[nodiscard]] const Font& fontOf(const TextRun& run) const noexcept;
	/// @brief Adds a line from the pending pieces, merging neighbours of one run, and places it.
	/// @param lineBegin Its first code point.
	/// @param lineEnd One past its last.
	void finishLine(std::size_t lineBegin, std::size_t lineEnd);
	/// @brief Places every line by the alignment, once all are made.
	/// @param boxWidth The width they are aligned in.
	/// @param align Their placement.
	void align(float boxWidth, Align align) noexcept;
	/// @brief Starts a layout: empties everything.
	/// @param font The base font.
	void clear(const Font& font);

	std::u32string text_;        ///< See text().
	std::vector<Piece> pieces_;  ///< See pieces().
	std::vector<Line> lines_;    ///< See lines().
	std::vector<Piece> pending_; ///< The line being built: one piece per word.
	const Font* base_ = nullptr; ///< The base font.
	float width_ = 0.0f;         ///< See width().
	float height_ = 0.0f;        ///< See height().
};

} // namespace affresco
