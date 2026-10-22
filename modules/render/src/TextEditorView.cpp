/// @file
/// @brief TextEditor's view: the rows lines take on screen (word wrap and folds), positions on rows, and the view's
/// settings. With neither wrap nor folds a row is a line, and none of this costs anything.

#include "affresco/render/TextEditor.hpp"

#include "TextEditorRows.hpp"
#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace affresco {

namespace {

constexpr std::size_t maxRowsPerLine = 100'000; ///< Rows one wrapped line takes, at most (the rest run past the edge).
constexpr std::size_t maxFoldScan = 100'000;    ///< Lines foldable() reads after a line, at most.

/// @brief A line number after an edit: lines before it stay, lines after it move, lines it replaced go to its start.
/// @param line The line.
/// @param change The edit.
std::size_t movedLine(std::size_t line, const TextChange& change) noexcept {
	if(line <= change.from.line) return line;
	if(line > change.oldEnd.line) return line - change.oldEnd.line + change.newEnd.line;
	return change.from.line;
}

} // namespace

/* Settings */

void TextEditor::setWrap(bool wrap) {
	if(wrap == wrap_) return;
	wrap_ = wrap;
	wrapRows_.clear();
	scroll_.x = 0.0f;
	rebuildRows();
	invalidate();
}

void TextEditor::setScroll(Vec2 offset) {
	const Vec2 before = scroll_;
	scroll_ = offset;
	clampScroll();
	if(scroll_.x == before.x && scroll_.y == before.y) return;
	invalidatePaint();
	onScrolled(scroll_);
}

void TextEditor::setComposition(std::u32string text) {
	if(text.size() > maxCompositionLength) text.resize(maxCompositionLength);
	composition_ = std::move(text);
	invalidatePaint();
}

void TextEditor::setComposition(std::string_view text) {
	std::u32string decoded = detail::fromUtf8(text);
	if(decoded != composition_) setComposition(std::move(decoded));
}

void TextEditor::setBracketMatching(bool shown) {
	if(shown == brackets_) return;
	brackets_ = shown;
	invalidatePaint();
}

void TextEditor::setIndentGuides(bool shown) {
	if(shown == guides_) return;
	guides_ = shown;
	invalidatePaint();
}

std::size_t TextEditor::rowCount() const noexcept { return rows_ ? rows_->total() : lines_.size(); }

/* Folds */

std::optional<FoldRange> TextEditor::foldable(std::size_t line) const {
	if(line + 1 >= lines_.size()) return std::nullopt;
	if(std::optional<FoldRange> range; highlighter_ && highlighter_->foldable(*model_, line, range)) return range;
	const std::size_t indent = indentOf(line);
	if(indent == lines_[line].size()) return std::nullopt; // a blank line folds nothing
	std::size_t end = line;
	const std::size_t limit = std::min(lines_.size(), line + 1 + maxFoldScan);
	for(std::size_t next = line + 1; next < limit; ++next) {
		const std::size_t nextIndent = indentOf(next);
		if(nextIndent == lines_[next].size()) continue; // blank lines go with what surrounds them
		if(nextIndent <= indent) break;
		end = next;
	}
	if(end == line) return std::nullopt;
	return FoldRange{line, end};
}

bool TextEditor::fold(std::size_t line) {
	const std::optional<FoldRange> range = foldable(line);
	if(!range || folds_.size() >= maxFolds) return false;
	const auto at = std::ranges::lower_bound(folds_, line, {}, &FoldRange::line);
	if(at != folds_.end() && at->line == line) return false;
	folds_.insert(at, *range);
	rebuildRows();
	// A hidden caret goes to the end of the line left shown
	if(caret_.line > range->line && caret_.line <= range->endLine) {
		caret_ = anchor_ = {line, lines_[line].size()};
		onCaretMoved(caret_);
	}
	invalidate();
	return true;
}

bool TextEditor::unfold(std::size_t line) {
	const auto at = std::ranges::lower_bound(folds_, line, {}, &FoldRange::line);
	if(at == folds_.end() || at->line != line) return false;
	folds_.erase(at);
	rebuildRows();
	invalidate();
	return true;
}

void TextEditor::toggleFold(std::size_t line) {
	if(!unfold(line)) fold(line);
}

void TextEditor::offerFolds(std::vector<FoldRange> folds) {
	if(folds == givenFolds_) return;
	givenFolds_ = folds;
	setFolds(std::move(folds));
}

void TextEditor::setFolds(std::vector<FoldRange> folds) {
	std::erase_if(folds, [this](const FoldRange& f) { return f.endLine <= f.line || f.endLine >= lines_.size(); });
	std::ranges::sort(folds, {}, &FoldRange::line);
	const auto [first, last] = std::ranges::unique(folds, {}, &FoldRange::line);
	folds.erase(first, last);
	if(folds.size() > maxFolds) folds.resize(maxFolds);
	if(folds == folds_) return;
	folds_ = std::move(folds);
	rebuildRows();
	if(hidden(caret_.line)) unfoldAround(caret_.line);
	invalidate();
}

void TextEditor::unfoldAll() {
	if(folds_.empty()) return;
	folds_.clear();
	rebuildRows();
	invalidate();
}

void TextEditor::unfoldAround(std::size_t line) {
	const std::size_t before = folds_.size();
	std::erase_if(folds_, [line](const FoldRange& f) { return f.line < line && line <= f.endLine; });
	if(folds_.size() == before) return;
	rebuildRows();
	invalidate();
}

/* Rows */

float TextEditor::wrapWidth() const noexcept {
	const Font* f = font();
	const float least = f ? f->glyph(U'M').advance * 4.0f : 40.0f;
	return std::max(frame().width() - gutterWidth() - 16.0f - scrollBarWidth - minimapWidth(), least);
}

void TextEditor::lineBreaks(std::size_t line, std::vector<std::size_t>& starts) const {
	starts.assign(1, 0);
	const Font* f = font();
	if(!wrap_ || !f) return;
	const float width = wrapWidth();
	// A line short enough even in the widest letters fits without being shaped
	const std::size_t length = model_->lineLength(line);
	if(static_cast<float>(length) * 2.0f * f->glyph(U'M').advance <= width) return;

	const std::u32string& text = lines_[line];
	const std::vector<float>& advances = f->shape(text).advances;
	if(advances.size() < text.size()) return;
	// Greedy: a row ends after the last space that fits, else at the code point that doesn't
	float x = 0.0f;
	std::size_t rowStart = 0, afterSpace = 0;
	for(std::size_t i = 0; i < text.size() && starts.size() < maxRowsPerLine; ++i) {
		if(x + advances[i] > width && i > rowStart) {
			rowStart = afterSpace > rowStart ? afterSpace : i;
			starts.push_back(rowStart);
			x = 0.0f;
			for(std::size_t j = rowStart; j < i; ++j) x += advances[j];
		}
		x += advances[i];
		if(text[i] == U' ') afterSpace = i + 1;
	}
}

std::uint32_t TextEditor::wrappedRows(std::size_t line) const {
	lineBreaks(line, breaks_);
	return static_cast<std::uint32_t>(breaks_.size());
}

void TextEditor::rebuildRows() {
	if(!wrap_ && folds_.empty()) {
		rows_.reset();
		wrapRows_.clear();
		return;
	}
	const std::size_t lines = lines_.size();
	if(wrap_ && (wrapRows_.size() != lines || rowsWidth_ != wrapWidth() || rowsFont_ != font())) {
		wrapRows_.resize(lines);
		for(std::size_t line = 0; line < lines; ++line) wrapRows_[line] = wrappedRows(line);
		rowsWidth_ = wrapWidth();
		rowsFont_ = font();
	}
	std::vector<std::uint32_t> counts = wrap_ ? wrapRows_ : std::vector<std::uint32_t>(lines, 1);
	for(const FoldRange& f : folds_)
		std::fill(counts.begin() + static_cast<std::ptrdiff_t>(std::min(f.line + 1, lines)),
		          counts.begin() + static_cast<std::ptrdiff_t>(std::min(f.endLine + 1, lines)), 0);
	if(!rows_) rows_ = std::make_unique<detail::ViewRows>();
	rows_->assign(counts);
}

void TextEditor::rowsChanged(const TextChange& change) {
	if(!rows_) return;
	const bool linesChanged = change.oldEnd.line != change.from.line || change.newEnd.line != change.from.line;
	if(wrap_ && wrapRows_.size() == rows_->lines()) {
		const auto from = static_cast<std::ptrdiff_t>(change.from.line);
		wrapRows_.erase(wrapRows_.begin() + from + 1,
		                wrapRows_.begin() + static_cast<std::ptrdiff_t>(change.oldEnd.line) + 1);
		wrapRows_.insert(wrapRows_.begin() + from + 1, change.newEnd.line - change.from.line, 1);
		for(std::size_t line = change.from.line; line <= change.newEnd.line; ++line)
			wrapRows_[line] = wrappedRows(line);
	}
	if(linesChanged || rows_->lines() != lines_.size()) return rebuildRows();
	if(rows_->count(change.from.line) != 0) rows_->set(change.from.line, wrap_ ? wrapRows_[change.from.line] : 1);
}

bool TextEditor::hidden(std::size_t line) const noexcept {
	return rows_ && line < rows_->lines() && rows_->count(line) == 0;
}

std::size_t TextEditor::nextShown(std::size_t line) const noexcept {
	if(!rows_ || line + 1 >= rows_->lines()) return line + 1;
	const std::size_t row = rows_->rowOf(line + 1);
	return row >= rows_->total() ? rows_->lines() : rows_->lineAt(row).first;
}

TextEditor::RowSpan TextEditor::rowSpan(std::size_t row) const {
	if(!rows_) return {row, 0, model_->lineLength(row)};
	const auto [line, part] = rows_->lineAt(row);
	const std::size_t length = model_->lineLength(line);
	if(!wrap_) return {line, 0, length};
	lineBreaks(line, breaks_);
	const std::size_t index = std::min(part, breaks_.size() - 1);
	return {line, breaks_[index], index + 1 < breaks_.size() ? breaks_[index + 1] : length};
}

std::size_t TextEditor::rowOf(Position at) const {
	if(!rows_) return at.line;
	const std::size_t line = std::min(at.line, rows_->lines() - 1);
	const std::size_t first = rows_->rowOf(line);
	if(rows_->count(line) == 0) return first == 0 ? 0 : first - 1; // folded: the shown line's last row
	if(!wrap_) return first;
	lineBreaks(line, breaks_);
	const auto after = std::ranges::upper_bound(breaks_, at.column);
	return first + static_cast<std::size_t>(std::distance(breaks_.begin(), after)) - 1;
}

float TextEditor::xOnRow(Position at) const {
	if(!wrap_) return columnX(at.line, at.column);
	lineBreaks(at.line, breaks_);
	const std::size_t start = *std::prev(std::ranges::upper_bound(breaks_, at.column));
	return columnX(at.line, at.column) - columnX(at.line, start);
}

TextEditor::Position TextEditor::positionOnRow(std::size_t row, float x) const {
	const RowSpan span = rowSpan(row);
	if(!wrap_) return {span.line, columnAt(span.line, x)};
	const Font* f = font();
	if(!f) return {span.line, span.start};
	const std::vector<float>& carets = f->shape(lines_[span.line]).carets;
	if(carets.empty()) return {span.line, 0};
	// A row that isn't the line's last ends before its last column, which starts the next row
	const bool last = span.end == lines_[span.line].size();
	const std::size_t end = std::min(last ? span.end : span.end - 1, carets.size() - 1);
	const float origin = carets[std::min(span.start, carets.size() - 1)];
	std::size_t nearest = span.start;
	for(std::size_t column = span.start; column <= end; ++column)
		if(std::abs(carets[column] - origin - x) < std::abs(carets[nearest] - origin - x)) nearest = column;
	return {span.line, std::min(nearest, span.end)};
}

void TextEditor::moveRows(long rows, bool extend) {
	if(preferredX_ < 0.0f) preferredX_ = xOnRow(caret_);
	const long target = static_cast<long>(rowOf(caret_)) + rows;
	if(target < 0) return moveCaret({0, 0}, extend, true);
	if(target >= static_cast<long>(rowCount()))
		return moveCaret({lines_.size() - 1, lines_.back().size()}, extend, true);
	moveCaret(positionOnRow(static_cast<std::size_t>(target), preferredX_), extend, true);
}

/* Edits */

void TextEditor::shiftView(const TextChange& change) {
	const bool sameLines = change.oldEnd.line == change.from.line && change.newEnd.line == change.from.line;
	if(sameLines || (folds_.empty() && decorations_.empty())) return;
	// Lines inserted at the start of a line go before it: that line moves down with what follows
	const bool beforeLine = change.from.column == 0 && change.oldEnd == change.from;
	const auto moved = [&change, beforeLine](std::size_t line) {
		return beforeLine && line >= change.from.line ? line + change.newEnd.line - change.from.line
		                                              : movedLine(line, change);
	};
	for(EditorDecoration& d : decorations_) {
		d.line = moved(d.line);
		d.endLine = std::max(moved(d.endLine), d.line);
	}
	// A fold stays if the edit is wholly inside what it hides, or wholly before or after it
	std::erase_if(folds_, [&](FoldRange& f) {
		if(f.endLine < change.from.line) return false;
		if(f.line > change.oldEnd.line || (beforeLine && f.line >= change.from.line)) {
			f.line = moved(f.line);
			f.endLine = moved(f.endLine);
			return false;
		}
		if(f.line < change.from.line && change.oldEnd.line <= f.endLine) {
			f.endLine = f.endLine - change.oldEnd.line + change.newEnd.line;
			return f.endLine <= f.line;
		}
		return true;
	});
}

/* View colours */

void TextEditor::setGuideColor(Color color) {
	guideColor_ = color;
	invalidatePaint();
}

void TextEditor::setMatchColor(Color color) {
	matchColor_ = color;
	invalidatePaint();
}

void TextEditor::setBracketColor(Color color) {
	bracketColor_ = color;
	invalidatePaint();
}

/* Geometry */

TextEditor::Position TextEditor::positionAt(Vec2 point) const {
	const float y = point.y - frame().top - textPadding + scroll_.y;
	const float row = std::floor(y / lineHeight());
	if(row < 0.0f) return {}; // above the text: the start
	// Below (or not a number, from a huge frame): the end, before the row is converted to an integer
	if(!(row < static_cast<float>(rowCount()))) return {lines_.size() - 1, lines_.back().size()};
	return positionOnRow(static_cast<std::size_t>(row), point.x - frame().left - gutterWidth() + scroll_.x);
}

std::optional<Rect> TextEditor::rectAt(Position at) const {
	if(!font()) return std::nullopt;
	at = clamped(at);
	if(hidden(at.line)) return std::nullopt;
	const float lh = lineHeight();
	const float top = std::round(frame().top + textPadding - scroll_.y + static_cast<float>(rowOf(at)) * lh);
	const float textLeft = std::round(frame().left + gutterWidth() - scroll_.x);
	const float x = xOnRow(at);
	// The next code point's start, unless the line ends (or wraps) here
	float width = lh / 2.0f;
	if(at.column < lines_[at.line].size())
		if(const Position next{at.line, at.column + 1}; rowOf(next) == rowOf(at)) width = std::max(xOnRow(next) - x, 1.0f);
	const Rect box{textLeft + x, top, textLeft + x + width, top + lh};
	const Rect& shown = frame();
	if(box.right <= shown.left + gutterWidth() || box.left >= shown.right || box.bottom <= shown.top ||
	   box.top >= shown.bottom)
		return std::nullopt;
	return box;
}

void TextEditor::hoverAt(std::optional<Vec2> point) {
	std::optional<Position> under;
	if(point && frame().contains(*point) && point->x >= frame().left + gutterWidth()) {
		// positionAt() gives the nearest place between code points: the one under the mouse is after it or before it
		const Position near = positionAt(*point);
		for(const Position at : {near, Position{near.line, near.column > 0 ? near.column - 1 : 0}}) {
			if(at.column >= lines_[at.line].size()) continue;
			if(const std::optional<Rect> box = rectAt(at); box && box->contains(*point)) {
				under = at;
				break;
			}
		}
	}
	if(under == hovered_) return;
	hovered_ = under;
	onHoverPosition(hovered_);
}

std::size_t TextEditor::visibleLines() const noexcept {
	// No more than the text has: a huge height (or not a number) must not reach the conversion to an integer
	const float rows =
	    std::min(std::floor((frame().height() - textPadding) / lineHeight()), static_cast<float>(rowCount()));
	return rows >= 1.0f ? static_cast<std::size_t>(rows) : 1;
}

void TextEditor::clampScroll() {
	const Font* f = font();
	const float contentHeight = static_cast<float>(rowCount()) * lineHeight() + textPadding;
	// Like most code editors, the last line can scroll up to the top
	const float maxY = std::max(contentHeight - lineHeight() - textPadding, 0.0f);
	const float widest = f ? widestLine(*f) : 0.0f;
	const float textWidth = frame().width() - gutterWidth() - 16.0f - minimapWidth();
	const float maxX = wrap_ ? 0.0f : std::max(widest - textWidth, 0.0f);
	scroll_.x = std::clamp(scroll_.x, 0.0f, maxX);
	scroll_.y = std::clamp(scroll_.y, 0.0f, maxY);
}

void TextEditor::revealCaret() {
	if(hidden(caret_.line)) unfoldAround(caret_.line);
	const Vec2 before = scroll_;
	const float lh = lineHeight();
	const float top = static_cast<float>(rowOf(caret_)) * lh;
	const float height = frame().height() - textPadding;
	if(top < scroll_.y) scroll_.y = top;
	else if(top + lh > scroll_.y + height) scroll_.y = top + lh - height;

	const float x = xOnRow(caret_);
	const float width = frame().width() - gutterWidth() - 16.0f;
	if(wrap_) scroll_.x = 0.0f;
	else if(x < scroll_.x) scroll_.x = std::max(x - 40.0f, 0.0f);
	else if(x > scroll_.x + width) scroll_.x = x - width + 40.0f;
	invalidate();
	if(scroll_.x != before.x || scroll_.y != before.y) onScrolled(scroll_);
}

float TextEditor::widestLine(const Font& font) const {
	if(widest_ >= 0.0f && widestFont_ == &font) return widest_;
	widestFont_ = &font;
	widest_ = 0.0f;
	if(lines_.size() <= exactWidth) {
		for(std::size_t line = 0; line < lines_.size(); ++line) widest_ = std::max(widest_, font.measure(lines_[line]));
		return widest_;
	}
	// Measuring every line of a large text would cost more than opening it: a byte is at most one code point
	const float advance = std::max(font.glyph(U'M').advance, font.glyph(U'0').advance);
	widest_ = static_cast<float>(model_->longestLine()) * advance;
	return widest_;
}

} // namespace affresco
