/// @file
/// @brief DiffView: the script turned into each editor's rows (fillers keep both sides aligned), line numbers and
/// decorations, the editors laid out side by side and scrolled together.

#include "affresco/render/DiffView.hpp"
#include "affresco/render/Painter.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace affresco {

namespace {

constexpr float dividerWidth = 1.0f;          ///< The line between both sides.
constexpr std::size_t characterCost = 50'000; ///< Steps one line pair's character diff may spend.
constexpr std::size_t characterGap = 2;       ///< Kept characters between two changes that are marked with them.

/// @brief A line's code points, as numbers for diffSequences().
/// @param line The line, UTF-8.
/// @return Its code points.
std::vector<std::uint32_t> codePoints(std::string_view line) {
	const std::u32string decoded = detail::fromUtf8(line);
	return {decoded.begin(), decoded.end()};
}

/// @brief Adds a background over some rows, or lengthens the last one when it ends just before them in that colour.
/// @param decorations Where.
/// @param row The first row.
/// @param count How many (none: nothing is added).
/// @param color The colour.
void background(std::vector<EditorDecoration>& decorations, std::size_t row, std::size_t count, Color color) {
	if(count == 0 || color.a <= 0.0f) return;
	if(!decorations.empty()) {
		EditorDecoration& last = decorations.back();
		if(last.color == color && last.endColumn == SIZE_MAX && last.endLine + 1 == row) {
			last.endLine = row + count - 1;
			return;
		}
	}
	decorations.push_back({.line = row,
	                       .endLine = row + count - 1,
	                       .kind = EditorDecoration::Kind::Background,
	                       .color = color,
	                       .text = {}});
}

/// @brief A change in the line script: the lines deleted, facing the lines inserted.
struct Change {
	std::size_t original = 0; ///< The first deleted line, in the original.
	std::size_t deleted = 0;  ///< How many lines are deleted.
	std::size_t modified = 0; ///< The first inserted line, in the modified text.
	std::size_t inserted = 0; ///< How many lines are inserted.
};

/// @brief Reads the change at an edit that isn't Equal: a deletion followed by an insertion replaces those lines.
/// @param edits The script.
/// @param e The edit's index; moved on to the insertion when it is read with the deletion.
/// @return The change.
Change changeAt(const std::vector<DiffEdit>& edits, std::size_t& e) noexcept {
	const DiffEdit& edit = edits[e];
	if(edit.kind == DiffEdit::Kind::Insert) return {.modified = edit.modified, .inserted = edit.count};
	Change change{.original = edit.original, .deleted = edit.count};
	if(change.deleted > 0 && e + 1 < edits.size() && edits[e + 1].kind == DiffEdit::Kind::Insert) {
		const DiffEdit& insert = edits[++e];
		change.modified = insert.modified;
		change.inserted = insert.count;
	}
	return change;
}

/// @brief Whether a changed run is marked with the last one: on the same row, a few kept characters after it.
/// @param decorations The side's decorations.
/// @param first The first of them this line pair added.
/// @param row The run's row.
/// @param column The run's first column.
/// @return true if the last decoration should be lengthened to cover it.
bool continuesLast(const std::vector<EditorDecoration>& decorations, std::size_t first, std::size_t row,
                   std::size_t column) noexcept {
	return decorations.size() > first && decorations.back().line == row &&
	       column - decorations.back().endColumn <= characterGap;
}

} // namespace

DiffView::DiffView(EditorFactory factory) : factory_(std::move(factory)) { makeEditors(); }

void DiffView::setTexts(std::string_view original, std::string_view modified) {
	if(original == originalText_ && modified == modifiedText_) return;
	originalText_.assign(original);
	modifiedText_.assign(modified);
	originalLines_ = splitLines(originalText_);
	modifiedLines_ = splitLines(modifiedText_);
	edits_ = diffLines(originalLines_, modifiedLines_);
	refresh();
}

void DiffView::setMode(Mode mode) {
	if(mode == mode_) return;
	mode_ = mode;
	makeEditors();
}

void DiffView::setLanguage(std::string_view language) {
	if(language == language_) return;
	language_.assign(language);
	makeEditors();
}

void DiffView::setColors(const DiffColors& colors) {
	if(colors == colors_) return;
	colors_ = colors;
	refresh();
}

void DiffView::setFont(std::shared_ptr<const Font> font) {
	if(font == font_) return;
	font_ = std::move(font);
	for(TextEditor* editor : {original_, modified_})
		if(editor) editor->setFont(font_);
}

void DiffView::setTextColor(Color color) {
	textColor_ = color;
	for(TextEditor* editor : {original_, modified_})
		if(editor && color.a > 0.0f) editor->setTextColor(color);
}

void DiffView::makeEditors() {
	removeChildren();
	original_ = modified_ = nullptr;
	const std::size_t count = mode_ == Mode::Split ? 2 : 1;
	for(std::size_t i = 0; i < count; ++i) {
		std::unique_ptr<TextEditor> made = factory_ ? factory_(language_) : nullptr;
		if(!made) made = std::make_unique<TextEditor>();
		TextEditor& editor = *made;
		addChild(std::move(made));
		editor.setReadOnly(true);
		editor.setWrap(false);
		editor.setLineNumbers(true);
		if(font_) editor.setFont(font_);
		if(textColor_.a > 0.0f) editor.setTextColor(textColor_);
		(i == 0 ? original_ : modified_) = &editor;
	}
	if(modified_) {
		// Rows are aligned, so each side scrolls down as the other does (sideways, each keeps its own)
		const auto follow = [this](TextEditor& other) {
			return [this, &other](Vec2 offset) {
				if(syncing_) return;
				syncing_ = true;
				other.setScroll({other.scroll().x, offset.y});
				syncing_ = false;
			};
		};
		original_->onScrolled.connect(follow(*modified_));
		modified_->onScrolled.connect(follow(*original_));
	}
	refresh();
	invalidate();
}

void DiffView::refresh() {
	if(mode_ == Mode::Split) {
		Side left, right;
		buildSplit(left, right);
		fill(*original_, std::move(left));
		fill(*modified_, std::move(right));
		return;
	}
	Side side;
	buildInline(side);
	fill(*original_, std::move(side));
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): declared in the public header
void DiffView::fill(TextEditor& editor, Side side) const {
	if(!side.text.empty() && side.text.back() == '\n') side.text.pop_back(); // rows are joined, not ended
	editor.setText(side.text);
	editor.setLineLabels(std::move(side.labels));
	editor.setDecorations(std::move(side.decorations));
}

void DiffView::buildSplit(Side& left, Side& right) const {
	std::size_t pairs = 0;
	const auto add = [](Side& side, std::string_view line, std::size_t number) {
		side.text.append(line).push_back('\n');
		side.labels.push_back(number);
		++side.rows;
	};
	for(std::size_t e = 0; e < edits_.size(); ++e) {
		const DiffEdit& edit = edits_[e];
		if(edit.kind == DiffEdit::Kind::Equal) {
			for(std::size_t k = 0; k < edit.count; ++k) {
				add(left, originalLines_[edit.original + k], edit.original + k + 1);
				add(right, modifiedLines_[edit.modified + k], edit.modified + k + 1);
			}
			continue;
		}
		// A change: the deleted lines facing the inserted ones, the shorter side padded with blank rows
		const Change change = changeAt(edits_, e);
		const std::size_t deleted = change.deleted, inserted = change.inserted, rows = std::max(deleted, inserted);
		const std::size_t leftStart = left.rows, rightStart = right.rows;
		background(left.decorations, leftStart, deleted, colors_.deleted);
		background(left.decorations, leftStart + deleted, rows - deleted, colors_.filler);
		background(right.decorations, rightStart, inserted, colors_.inserted);
		background(right.decorations, rightStart + inserted, rows - inserted, colors_.filler);
		for(std::size_t k = 0; k < rows; ++k) {
			if(k < deleted) add(left, originalLines_[change.original + k], change.original + k + 1);
			else add(left, {}, 0);
			if(k < inserted) add(right, modifiedLines_[change.modified + k], change.modified + k + 1);
			else add(right, {}, 0);
			if(k < deleted && k < inserted)
				markCharacters(originalLines_[change.original + k], modifiedLines_[change.modified + k], left,
				               leftStart + k, right, rightStart + k, pairs);
		}
	}
}

void DiffView::buildInline(Side& side) const {
	std::size_t pairs = 0;
	const auto add = [&side](std::string_view line, std::size_t number) {
		side.text.append(line).push_back('\n');
		side.labels.push_back(number);
		++side.rows;
	};
	for(std::size_t e = 0; e < edits_.size(); ++e) {
		const DiffEdit& edit = edits_[e];
		if(edit.kind == DiffEdit::Kind::Equal) {
			for(std::size_t k = 0; k < edit.count; ++k) add(modifiedLines_[edit.modified + k], edit.modified + k + 1);
			continue;
		}
		const Change change = changeAt(edits_, e);
		const std::size_t deleted = change.deleted, inserted = change.inserted, deletedStart = side.rows;
		background(side.decorations, deletedStart, deleted, colors_.deleted);
		background(side.decorations, deletedStart + deleted, inserted, colors_.inserted);
		for(std::size_t k = 0; k < deleted; ++k) add(originalLines_[change.original + k], change.original + k + 1);
		for(std::size_t k = 0; k < inserted; ++k) add(modifiedLines_[change.modified + k], change.modified + k + 1);
		for(std::size_t k = 0; k < std::min(deleted, inserted); ++k)
			markCharacters(originalLines_[change.original + k], modifiedLines_[change.modified + k], side,
			               deletedStart + k, side, deletedStart + deleted + k, pairs);
	}
}

void DiffView::markCharacters(std::string_view from, std::string_view to, Side& fromSide, std::size_t fromRow,
                              Side& toSide, std::size_t toRow, std::size_t& pairs) const {
	if(pairs >= maxCharacterPairs || std::max(from.size(), to.size()) > maxCharacterLine * 4) return;
	const std::vector<std::uint32_t> a = codePoints(from), b = codePoints(to);
	if(std::max(a.size(), b.size()) > maxCharacterLine) return;
	++pairs;
	const std::size_t fromFirst = fromSide.decorations.size(), toFirst = toSide.decorations.size();
	for(const DiffEdit& edit : diffSequences(a, b, {.maxCost = characterCost})) {
		if(edit.kind == DiffEdit::Kind::Equal) continue;
		const bool deleted = edit.kind == DiffEdit::Kind::Delete;
		Side& side = deleted ? fromSide : toSide;
		const std::size_t row = deleted ? fromRow : toRow, column = deleted ? edit.original : edit.modified;
		// A change a few characters after the last one is marked with it: "int" to "long" keeps "n" by chance
		const std::size_t first = deleted ? fromFirst : toFirst;
		if(continuesLast(side.decorations, first, row, column)) {
			side.decorations.back().endColumn = column + edit.count;
			continue;
		}
		side.decorations.push_back({.line = row,
		                            .endLine = row,
		                            .column = column,
		                            .endColumn = column + edit.count,
		                            .kind = EditorDecoration::Kind::Background,
		                            .color = deleted ? colors_.deletedText : colors_.insertedText,
		                            .text = {}});
	}
}

void DiffView::arrange() {
	const Vec2 size = resolvedSize();
	const float half = modified_ ? std::floor((size.x - dividerWidth) / 2.0f) : size.x;
	float position = 0.0f;
	for(TextEditor* editor : {original_, modified_}) {
		if(!editor) continue;
		editor->clearAnchor(Edge::Right);
		editor->clearAnchor(Edge::Bottom);
		editor->setAnchor(Edge::Left, px(position));
		editor->setAnchor(Edge::Top, px(0.0f));
		const float width = editor == modified_ ? std::max(size.x - position, 0.0f) : std::max(half, 0.0f);
		arrangeSize(*editor, width, size.y);
		position += half + dividerWidth;
	}
}

void DiffView::paint(Painter& painter) const {
	Panel::paint(painter);
	if(!modified_) return;
	const Rect& box = frame();
	const float x = modified_->frame().left - dividerWidth;
	painter.fillRect({x, box.top, x + dividerWidth, box.bottom}, colors_.divider);
}

} // namespace affresco
