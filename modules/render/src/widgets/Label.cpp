#include "affresco/render/Label.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace affresco {

namespace {

/// @brief Places content in a space, along one axis.
/// @param align The placement.
/// @param start Where the space starts.
/// @param available The space's size.
/// @param used The content's size.
/// @return Where the content starts.
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

/// @brief An icon's name with "left" and "right" swapped, as it reads right to left.
/// @param name The name, e.g. "chevron-left".
/// @return It mirrored, e.g. "chevron-right".
std::string mirroredIconName(std::string_view name) {
	std::string mirrored;
	for(std::size_t i = 0; i < name.size();) {
		if(name.substr(i).starts_with("left")) {
			mirrored += "right";
			i += 4;
		} else if(name.substr(i).starts_with("right")) {
			mirrored += "left";
			i += 5;
		} else {
			mirrored += name[i++];
		}
	}
	return mirrored;
}

} // namespace

Label::Label() { setColor(colors::transparent); }

Label::Label(std::string_view text) : Label() { text_ = detail::fromUtf8(text); }

void Label::setText(std::string_view utf8) {
	text_ = detail::fromUtf8(utf8);
	runs_.clear();
	shownFont_ = 0;
	++textVersion_;
	anchor_ = caret_ = 0;
	invalidate();
}

void Label::setRuns(std::vector<TextRun> runs) {
	runs_ = std::move(runs);
	text_.clear();
	for(const TextRun& run : runs_)
		if(run.icon.empty()) text_ += run.text;
	++textVersion_;
	anchor_ = caret_ = 0;
	invalidate();
}

void Label::setIcon(std::string name) {
	if(name == icon_) return;
	icon_ = std::move(name);
	shownFont_ = 0;
	++textVersion_;
	invalidate();
}

void Label::setIconMirrored(bool mirrored) {
	if(mirrored == iconMirrored_) return;
	iconMirrored_ = mirrored;
	shownFont_ = 0;
	invalidate();
}

void Label::updateShown() {
	const Font* f = font();
	const bool mirrored = iconMirrored_ && rightToLeft();
	if(icon_.empty() || !f || (shownFont_ == f->id() && shownMirrored_ == mirrored)) return;
	if(shownMirrored_ != mirrored) ++textVersion_;
	shownMirrored_ = mirrored;
	shown_.clear();
	char32_t codepoint = mirrored ? f->codepointNamed(mirroredIconName(icon_)) : 0;
	if(!codepoint) codepoint = f->codepointNamed(icon_);
	if(codepoint) {
		shown_ += codepoint;
		if(!text_.empty()) shown_ += U' ';
	}
	shown_ += text_;
	shownFont_ = f->id();
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

void Label::setTextColor(Color color) { setLook(textColor_, color); }

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

void Label::setFitToText(bool fit, float maxWidth) {
	fitToText_ = fit;
	fitMaxWidth_ = maxWidth;
	invalidate();
}

void Label::prepareLayout() {
	updateShown();
	const Font* f = font();
	if(!fitToText_ || !f) return;
	if(!runs_.empty()) {
		const float maxTextWidth = wrap_ ? fitMaxWidth_ - 2.0f * padding_ : std::numeric_limits<float>::infinity();
		layout_.layOut(runs_, *f, maxTextWidth, 0.0f, Align::Start);
		laidOut_ = {}; // made for fitting: textLayout() makes it again in the fitted box
		setSize(px(std::ceil(layout_.width()) + 2.0f * padding_),
		        px(std::max(layout_.height(), f->lineHeight()) + 2.0f * padding_));
		return;
	}

	const float maxTextWidth = wrap_ ? fitMaxWidth_ - 2.0f * padding_ : std::numeric_limits<float>::infinity();
	// The text changes only through invalidate(): until then, comparing it with the fitted one would find it the same
	if(fitted_.font != f->id() || fitted_.maxWidth != maxTextWidth || (isLayoutDirty() && fitted_.text != shown())) {
		const std::vector<std::u32string_view> lines = f->wrap(shown(), maxTextWidth);
		float textWidth = 0.0f;
		for(const std::u32string_view line : lines) textWidth = std::max(textWidth, f->measure(line));
		fitted_ = {shown(), f->id(), maxTextWidth, textWidth, lines.size()};
	}

	const auto lineCount = static_cast<float>(std::max<std::size_t>(fitted_.lines, 1));
	setSize(px(std::ceil(fitted_.width) + 2.0f * padding_), px(lineCount * f->lineHeight() + 2.0f * padding_));
}

Rect Label::contentRect() const noexcept {
	const Rect& f = frame();
	return {f.left + padding_, f.top + padding_, f.right - padding_, f.bottom - padding_};
}

void Label::paint(Painter& painter) const {
	Panel::paint(painter);

	const Font* f = font();
	if(!f || (shown().empty() && runs_.empty())) return;
	if(!runs_.empty() || anchor_ != caret_) {
		const Vec2 origin = blockOrigin();
		if(anchor_ != caret_) paintSelection(painter, origin);
		if(!runs_.empty()) return paintRuns(painter, origin);
	}

	const Rect content = contentRect();
	// Most labels are one line: then there is nothing to wrap, and no list of lines to make
	if(!wrap_ && !shown().contains(U'\n')) {
		paintLine(painter, *f, shown(), aligned(vertical_, content.top, content.height(), f->lineHeight()));
		return;
	}

	const float maxWidth = wrap_ ? content.width() : std::numeric_limits<float>::infinity();
	const std::vector<std::u32string_view> lines = f->wrap(shown(), maxWidth);
	const float textHeight = static_cast<float>(lines.size()) * f->lineHeight();
	float top = aligned(vertical_, content.top, content.height(), textHeight);
	for(const std::u32string_view line : lines) {
		paintLine(painter, *f, line, top);
		top += f->lineHeight();
	}
}

void Label::paintLine(Painter& painter, const Font& font, std::u32string_view line, float top) const {
	const Rect content = contentRect();
	const float left = aligned(readingAlign(horizontal_), content.left, content.width(), font.measure(line));
	// Whole pixels keep glyphs sharp: atlas texels map 1:1 onto screen pixels
	painter.drawText(font, line, {std::round(left), std::round(top + font.ascent())}, textColor_);
}

/* Rich text and selection */

namespace {

/// @brief Whether a code point is in a Private Use Area, where icon fonts put their glyphs.
/// @param codepoint The code point.
/// @return true for U+E000 to U+F8FF and planes 15 and 16.
bool privateUse(char32_t codepoint) noexcept {
	return (codepoint >= 0xE000 && codepoint <= 0xF8FF) || codepoint >= 0xF0000;
}

} // namespace

void Label::setSelectable(bool selectable) {
	if(selectable == selectable_) return;
	selectable_ = selectable;
	if(cursor() == (selectable ? Cursor::Arrow : Cursor::IBeam)) setCursor(selectable ? Cursor::IBeam : Cursor::Arrow);
	if(!selectable) select(0, 0);
}

void Label::select(std::size_t from, std::size_t to) {
	const std::size_t length = textLayout().text().size();
	anchor_ = std::min(from, length);
	caret_ = std::min(to, length);
	invalidatePaint();
}

std::string Label::selectedText() const {
	const auto [from, to] = selection();
	const std::u32string& text = textLayout().text();
	std::u32string picked;
	for(std::size_t index = from; index < std::min(to, text.size()); ++index)
		if(!privateUse(text[index])) picked += text[index];
	return detail::toUtf8(picked);
}

void Label::setSelectionColor(Color color) { setLook(selectionColor_, color); }

const TextLayout& Label::textLayout() const {
	const Font* f = font();
	if(!f) return layout_;
	const Rect content = contentRect();
	const float maxWidth = wrap_ ? content.width() : std::numeric_limits<float>::infinity();
	const Align horizontal = readingAlign(horizontal_);
	const LaidOut key{textVersion_, f->id(), maxWidth, content.width(), horizontal, shown().size()};
	if(key == laidOut_) return layout_;
	if(runs_.empty()) {
		layout_.layOutPlain(shown(), *f, maxWidth, content.width(), horizontal);
	} else {
		layout_.layOut(runs_, *f, maxWidth, content.width(), horizontal);
	}
	laidOut_ = key;
	return layout_;
}

Vec2 Label::blockOrigin() const {
	const Rect content = contentRect();
	return {content.left, aligned(vertical_, content.top, content.height(), textLayout().height())};
}

void Label::paintRuns(Painter& painter, Vec2 origin) const {
	const TextLayout& laid = textLayout();
	const std::u32string_view text = laid.text();
	for(const TextLayout::Line& line : laid.lines()) {
		// Whole pixels keep glyphs sharp, as in paintLine()
		const float baseline = std::round(origin.y + line.top + line.ascent);
		for(std::size_t index = line.firstPiece; index < line.endPiece; ++index) {
			const TextLayout::Piece& piece = laid.pieces()[index];
			const TextRun& run = runs_[piece.run];
			const Color color = run.color.value_or(textColor_);
			const float left = std::round(origin.x + piece.x);
			painter.drawText(*piece.font, text.substr(piece.begin, piece.end - piece.begin), {left, baseline}, color);
			if(!run.link.empty()) painter.fillRect({left, baseline + 1.0f, left + piece.width, baseline + 2.0f}, color);
		}
	}
}

void Label::paintSelection(Painter& painter, Vec2 origin) const {
	const TextLayout& laid = textLayout();
	const auto [from, to] = selection();
	for(const TextLayout::Line& line : laid.lines()) {
		if(to <= line.begin || from >= line.end) continue;
		const float left = laid.caretX(line, std::max(from, line.begin));
		const float right = laid.caretX(line, std::min(to, line.end));
		painter.fillRect({origin.x + std::min(left, right), origin.y + line.top, origin.x + std::max(left, right),
		                  origin.y + line.top + line.height},
		                 selectionColor_);
	}
}

void Label::keyEvent(const KeyEvent& event) {
	Panel::keyEvent(event);
	if(!selectable_ || event.action == Action::Release || !(event.mods.super() || event.mods.control())) return;
	if(window() && window()->keyTaken()) return; // a binding ran for it
	if(event.key == Key::C && anchor_ != caret_ && window()) window()->setClipboardText(selectedText());
	if(event.key == Key::A) select(0, textLayout().text().size());
}

void Label::mouseButtonEvent(const MouseButtonEvent& event) {
	Panel::mouseButtonEvent(event);
	if(event.button != MouseButton::Left) return;
	if(event.action == Action::Release) {
		selecting_ = false;
		return;
	}
	const TextLayout& laid = textLayout();
	const Vec2 origin = blockOrigin();
	const Vec2 at{event.position.x - origin.x, event.position.y - origin.y};
	if(const auto piece = runs_.empty() ? std::nullopt : laid.pieceAt(at)) {
		// A copy: the handler may change the runs
		if(const std::string link = runs_[laid.pieces()[*piece].run].link; !link.empty()) return onLink(link);
	}
	if(!selectable_) return;
	anchor_ = caret_ = laid.positionAt(at);
	selecting_ = true;
	invalidatePaint();
}

void Label::mouseMoveEvent(const MouseMoveEvent& event) {
	Panel::mouseMoveEvent(event);
	if(!selecting_) return;
	const Vec2 origin = blockOrigin();
	const std::size_t at = textLayout().positionAt({event.position.x - origin.x, event.position.y - origin.y});
	if(at == caret_) return;
	caret_ = at;
	invalidatePaint();
}

void Label::focusEvent(bool focused) {
	if(!focused) {
		selecting_ = false;
		if(anchor_ != caret_) select(0, 0);
	}
	Panel::focusEvent(focused);
}

} // namespace affresco
