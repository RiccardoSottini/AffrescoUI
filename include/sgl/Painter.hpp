#pragma once

#include "sgl/Color.hpp"
#include "sgl/Geometry.hpp"

#include <string_view>

namespace sgl {

class Font;

// What a panel draws with in Panel::paint. Everything drawn is clipped to the panel's visible bounds.
class Painter {
public:
	virtual ~Painter() = default;

	virtual void fillRect(const Rect& rect, Color color) = 0;

	// One line of text; `origin` is the left end of its baseline
	virtual void drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) = 0;
};

} // namespace sgl
