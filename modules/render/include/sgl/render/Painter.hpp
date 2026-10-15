#pragma once

/// @file
/// @brief Painter: what a panel draws with.

#include "sgl/render/Color.hpp"
#include "sgl/render/Geometry.hpp"

#include <string_view>

namespace sgl {

class Font;

/// @brief What a panel draws with in Panel::paint. Everything drawn is clipped to the panel's visible bounds.
class Painter {
public:
	virtual ~Painter() = default;

	/// @brief Fills a rectangle.
	/// @param rect In window units.
	/// @param color The colour; transparent rectangles are skipped.
	virtual void fillRect(const Rect& rect, Color color) = 0;

	/// @brief Draws one line of text.
	/// @param font The font.
	/// @param text The text (no line breaking here).
	/// @param origin Left end of the baseline, in window units.
	/// @param color The colour.
	virtual void drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) = 0;
};

} // namespace sgl
