#include "sgl/Label.hpp"
#include "sgl/Painter.hpp"
#include "sgl/Window.hpp"

#include "detail/Utf8.hpp"

#include <cmath>
#include <limits>
#include <vector>

namespace sgl {

namespace {

float aligned(Align align, float start, float available, float used) noexcept {
	switch(align) {
	case Align::Start:
		return start;
	case Align::Center:
		return start + (available - used) / 2.0f;
	case Align::End:
		return start + available - used;
	}
	return start;
}

} // namespace

Label::Label() { setColor(colors::transparent); }

Label::Label(std::string_view text) : Label() { text_ = detail::fromUtf8(text); }

void Label::setText(std::string_view utf8) {
	text_ = detail::fromUtf8(utf8);
	invalidate();
}

std::string Label::text() const { return detail::toUtf8(text_); }

void Label::setFont(std::shared_ptr<const Font> font) {
	font_ = std::move(font);
	invalidate();
}

const Font* Label::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

void Label::setTextColor(Color color) {
	textColor_ = color;
	invalidate();
}

void Label::setPadding(float padding) {
	padding_ = padding;
	invalidate();
}

void Label::setAlignment(Align horizontal, Align vertical) {
	horizontal_ = horizontal;
	vertical_ = vertical;
	invalidate();
}

void Label::setWrap(bool wrap) {
	wrap_ = wrap;
	invalidate();
}

Rect Label::contentRect() const noexcept {
	const Rect& f = frame();
	return {f.left + padding_, f.top + padding_, f.right - padding_, f.bottom - padding_};
}

void Label::paint(Painter& painter) const {
	Panel::paint(painter);

	const Font* f = font();
	if(!f || text_.empty()) return;

	const Rect content = contentRect();
	const float maxWidth = wrap_ ? content.width() : std::numeric_limits<float>::infinity();
	const std::vector<std::u32string_view> lines = f->wrap(text_, maxWidth);

	const float textHeight = static_cast<float>(lines.size()) * f->lineHeight();
	float top = aligned(vertical_, content.top, content.height(), textHeight);

	for(std::u32string_view line : lines) {
		const float left = aligned(horizontal_, content.left, content.width(), f->measure(line));
		// Whole pixels keep glyphs sharp: atlas texels map 1:1 onto screen pixels
		painter.drawText(*f, line, {std::round(left), std::round(top + f->ascent())}, textColor_);
		top += f->lineHeight();
	}
}

} // namespace sgl
