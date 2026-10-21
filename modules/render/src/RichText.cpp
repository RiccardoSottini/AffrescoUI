/// @file
/// @brief TextLayout: runs or plain text broken into lines of pieces, hit testing and caret positions.

#include "affresco/render/RichText.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace affresco {

namespace {

/// @brief Places content in a space, along one axis.
/// @param align The placement.
/// @param available The space's size.
/// @param used The content's size.
/// @return Where the content starts, from the space's start.
float alignedOffset(Align align, float available, float used) noexcept {
	if(!std::isfinite(available)) return 0.0f;
	switch(align) {
	case Align::Start:
		return 0.0f;
	case Align::Center:
		return (available - used) / 2.0f;
	case Align::End:
		return available - used;
	}
	return 0.0f;
}

/// @brief Where a segment of a run ends: after the spaces that follow a word, or at a '\n'.
/// @param text The text.
/// @param begin Where the segment starts.
/// @param end Where the run ends.
/// @return One past the segment's last code point.
std::size_t segmentEnd(std::u32string_view text, std::size_t begin, std::size_t end) noexcept {
	std::size_t at = begin;
	while(at < end && text[at] != U' ' && text[at] != U'\n') ++at;
	while(at < end && text[at] == U' ') ++at;
	return at;
}

/// @brief A text without the spaces it ends with.
/// @param text The text.
/// @return The rest.
std::u32string_view trimmed(std::u32string_view text) noexcept {
	const std::size_t last = text.find_last_not_of(U' ');
	return last == std::u32string_view::npos ? std::u32string_view{} : text.substr(0, last + 1);
}

} // namespace

std::u32string codepointsOf(std::string_view utf8) { return detail::fromUtf8(utf8); }

const Font& TextLayout::fontOf(const TextRun& run) const noexcept { return run.font ? *run.font : *base_; }

void TextLayout::clear(const Font& font) {
	base_ = &font;
	text_.clear();
	pieces_.clear();
	lines_.clear();
	pending_.clear();
	width_ = 0.0f;
	height_ = 0.0f;
}

void TextLayout::layOut(std::span<const TextRun> runs, const Font& font, float maxWidth, float boxWidth, Align align) {
	clear(font);
	std::size_t lineBegin = 0;
	float pen = 0.0f;
	bool mayBreak = false; // after a space, the next word may start a new line
	for(std::size_t index = 0; index < runs.size(); ++index) {
		const TextRun& run = runs[index];
		const Font& runFont = fontOf(run);
		const std::size_t runBegin = text_.size();
		if(run.icon.empty()) {
			text_ += run.text;
		} else if(const char32_t icon = runFont.codepointNamed(run.icon)) {
			text_ += icon;
		}
		const std::u32string_view text = text_;
		for(std::size_t begin = runBegin; begin < text_.size();) {
			if(text[begin] == U'\n') {
				finishLine(lineBegin, begin);
				lineBegin = begin + 1;
				pen = 0.0f;
				mayBreak = false;
				++begin;
				continue;
			}
			const std::size_t end = segmentEnd(text, begin, text_.size());
			const std::u32string_view segment = text.substr(begin, end - begin);
			if(mayBreak && !pending_.empty() && pen + runFont.measure(trimmed(segment)) > maxWidth) {
				finishLine(lineBegin, begin);
				lineBegin = begin;
				pen = 0.0f;
			}
			const float width = runFont.measure(segment);
			pending_.push_back({begin, end, index, &runFont, pen, width});
			pen += width;
			mayBreak = segment.back() == U' ';
			begin = end;
		}
	}
	if(lineBegin < text_.size() || !pending_.empty()) finishLine(lineBegin, text_.size());
	this->align(boxWidth, align);
}

void TextLayout::layOutPlain(std::u32string_view text, const Font& font, float maxWidth, float boxWidth, Align align) {
	clear(font);
	text_ = text;
	for(const std::u32string_view line : font.wrap(text_, maxWidth)) {
		const auto begin = static_cast<std::size_t>(line.data() - text_.data());
		const float width = font.measure(line);
		pieces_.push_back({begin, begin + line.size(), 0, &font, 0.0f, width});
		lines_.push_back({pieces_.size() - 1, pieces_.size(), begin, begin + line.size(), height_, font.ascent(),
		                  font.lineHeight(), width});
		height_ += font.lineHeight();
		width_ = std::max(width_, width);
	}
	this->align(boxWidth, align);
}

void TextLayout::finishLine(std::size_t begin, std::size_t end) {
	Line line{pieces_.size(), pieces_.size(), begin, end, height_, 0.0f, 0.0f, 0.0f};
	for(const Piece& word : pending_) {
		Piece* last = pieces_.size() > line.firstPiece ? &pieces_.back() : nullptr;
		if(last && last->run == word.run && last->end == word.begin) {
			last->end = word.end;
			last->width += word.width;
		} else {
			pieces_.push_back(word);
		}
		line.ascent = std::max(line.ascent, word.font->ascent());
		line.height = std::max(line.height, word.font->lineHeight());
	}
	pending_.clear();
	line.endPiece = pieces_.size();
	if(line.firstPiece == line.endPiece) {
		line.ascent = base_->ascent();
		line.height = base_->lineHeight();
	} else {
		const Piece& last = pieces_.back();
		line.width =
		    last.x + last.font->measure(trimmed(std::u32string_view(text_).substr(last.begin, last.end - last.begin)));
	}
	height_ += line.height;
	width_ = std::max(width_, line.width);
	lines_.push_back(line);
}

void TextLayout::align(float boxWidth, Align align) noexcept {
	for(const Line& line : lines_) {
		const float offset = alignedOffset(align, boxWidth, line.width);
		for(std::size_t piece = line.firstPiece; piece < line.endPiece; ++piece) pieces_[piece].x += offset;
	}
}

std::size_t TextLayout::positionAt(Vec2 point) const {
	if(lines_.empty()) return 0;
	const auto below = std::ranges::find_if(lines_, [&](const Line& line) { return point.y < line.top + line.height; });
	const Line& line = below == lines_.end() ? lines_.back() : *below;
	for(std::size_t index = line.firstPiece; index < line.endPiece; ++index) {
		const Piece& piece = pieces_[index];
		if(point.x < piece.x) return piece.begin;
		if(point.x >= piece.x + piece.width) continue;
		const std::vector<float>& carets =
		    piece.font->shape(std::u32string_view(text_).substr(piece.begin, piece.end - piece.begin)).carets;
		std::size_t nearest = 0;
		for(std::size_t caret = 1; caret < carets.size(); ++caret)
			if(std::abs(piece.x + carets[caret] - point.x) < std::abs(piece.x + carets[nearest] - point.x))
				nearest = caret;
		return piece.begin + nearest;
	}
	return line.end;
}

std::optional<std::size_t> TextLayout::pieceAt(Vec2 point) const {
	for(const Line& line : lines_) {
		if(point.y < line.top || point.y >= line.top + line.height) continue;
		for(std::size_t index = line.firstPiece; index < line.endPiece; ++index)
			if(point.x >= pieces_[index].x && point.x < pieces_[index].x + pieces_[index].width) return index;
	}
	return std::nullopt;
}

float TextLayout::caretX(const Line& line, std::size_t position) const {
	if(line.firstPiece == line.endPiece) return 0.0f;
	for(std::size_t index = line.firstPiece; index < line.endPiece; ++index) {
		const Piece& piece = pieces_[index];
		if(position < piece.begin) return piece.x;
		if(position >= piece.end) continue;
		const std::u32string_view text = std::u32string_view(text_).substr(piece.begin, piece.end - piece.begin);
		return piece.x + piece.font->shape(text).carets[position - piece.begin];
	}
	const Piece& last = pieces_[line.endPiece - 1];
	return last.x + last.width;
}

} // namespace affresco
