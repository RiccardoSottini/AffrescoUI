/// @file
/// @brief TextEditor's find and replace (plain, whole word or PCRE2, line by line) and bracket matching.

#include "affresco/render/TextEditor.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>

namespace affresco {

namespace {

constexpr std::size_t maxMatchesPerLine = 1000;   ///< Matches highlighted on one line, at most.
constexpr std::size_t maxVisibleMatches = 10'000; ///< Matches highlighted on the lines drawn, at most.

/// @brief Encodes a line to UTF-8, and where one of its columns starts.
/// @param text The line.
/// @param column The column.
/// @param out Gets the line in UTF-8.
/// @return The column's byte.
std::size_t encodeLine(const std::u32string& text, std::size_t column, std::string& out) {
	out.clear();
	std::size_t at = std::string::npos;
	for(std::size_t i = 0; i < text.size(); ++i) {
		if(i == column) at = out.size();
		detail::appendUtf8(out, text[i]);
	}
	return at == std::string::npos ? out.size() : at;
}

/// @brief The code points in some bytes of UTF-8 (those that aren't continuation bytes).
/// @param text The UTF-8.
/// @param from The first byte.
/// @param to One past the last.
std::size_t codepoints(std::string_view text, std::size_t from, std::size_t to) noexcept {
	std::size_t n = 0;
	for(std::size_t i = from; i < to && i < text.size(); ++i) n += (static_cast<unsigned char>(text[i]) & 0xC0) != 0x80;
	return n;
}

/// @brief The bracket that pairs with one, and whether it is found after it.
/// @param c The code point.
/// @return The other bracket (0: c isn't one) and true if c opens.
std::pair<char32_t, bool> pairOf(char32_t c) noexcept {
	switch(c) {
	case U'(':
		return {U')', true};
	case U'[':
		return {U']', true};
	case U'{':
		return {U'}', true};
	case U')':
		return {U'(', false};
	case U']':
		return {U'[', false};
	case U'}':
		return {U'{', false};
	default:
		return {0, false};
	}
}

/// @brief The bracket next to a caret: the one after it, else the one before it.
/// @param text The caret's line.
/// @param column The caret's column.
/// @return The bracket's column, or std::nullopt if neither is a bracket.
std::optional<std::size_t> bracketNear(const std::u32string& text, std::size_t column) noexcept {
	if(column < text.size() && pairOf(text[column]).first != 0) return column;
	if(column > 0 && column <= text.size() && pairOf(text[column - 1]).first != 0) return column - 1;
	return std::nullopt;
}

/// @brief Where the next search on a line starts after a match: past it (or one byte past an empty one), on a code
/// point's first byte.
/// @param subject The line, in UTF-8.
/// @param match The match.
/// @return The byte; past the end (subject.size() + 1) after an empty match at the end.
std::size_t afterMatch(std::string_view subject, const PatternMatch& match) noexcept {
	std::size_t byte = match.offset + std::max<std::size_t>(match.length, 1);
	while(byte < subject.size() && (static_cast<unsigned char>(subject[byte]) & 0xC0) == 0x80) ++byte;
	return byte;
}

/// @brief The column of the code point a bracket scan stepped over, onto a position.
/// @param line The position's line.
/// @param column The position's column.
/// @param forward Whether the scan goes forward.
/// @return The column, or std::nullopt if it stepped over a line break.
std::optional<std::size_t> steppedOver(const std::u32string& line, std::size_t column, bool forward) noexcept {
	if(forward && column == 0) return std::nullopt;
	const std::size_t at = forward ? column - 1 : column;
	return at < line.size() ? std::optional(at) : std::nullopt;
}

} // namespace

std::optional<std::string> TextEditor::setFind(std::string_view text, PatternOptions options) {
	if(text == findText_ && options == findOptions_ && (find_.valid() || text.empty())) return std::nullopt;
	++findGeneration_;
	invalidatePaint();
	findText_ = text;
	findOptions_ = options;
	if(text.empty()) {
		find_ = TextPattern();
		return std::nullopt;
	}
	std::optional<std::string> error = find_.compile(text, options);
	if(error) find_ = TextPattern();
	return error;
}

void TextEditor::lineMatches(std::size_t line, std::size_t from, std::size_t cap,
                             std::vector<std::pair<std::size_t, std::size_t>>& out) const {
	out.clear();
	if(!find_.valid() || line >= lines_.size()) return;
	std::size_t byte = encodeLine(lines_[line], from, subject_);
	std::size_t column = std::min(from, lines_[line].size()), counted = byte;
	while(out.size() < cap && byte <= subject_.size()) {
		const PatternFind found = find_.find(subject_, byte);
		if(!found.match) break; // none left, or the limits stopped it
		const PatternMatch m = *found.match;
		column += codepoints(subject_, counted, m.offset);
		const std::size_t length = codepoints(subject_, m.offset, m.offset + m.length);
		if(length > 0) out.emplace_back(column, length);
		byte = afterMatch(subject_, m); // an empty match (e.g. ^) is stepped over by one code point
		column += codepoints(subject_, m.offset, byte);
		counted = byte;
	}
	if(findScope_) std::erase_if(out, [&](const auto& m) { return !inScope(line, m.first, m.second); });
}

bool TextEditor::inScope(std::size_t line, std::size_t column, std::size_t length) const noexcept {
	if(!findScope_) return true;
	const auto [from, to] = *findScope_;
	return Position{line, column} >= from && Position{line, column + length} <= to;
}

void TextEditor::setFindInSelection(bool inSelection) {
	const auto [from, to] = selection();
	if(inSelection == findScope_.has_value() || (inSelection && from == to)) return;
	if(inSelection) findScope_.emplace(from, to);
	else findScope_.reset();
	++findGeneration_;
	invalidatePaint();
}

bool TextEditor::findStep(FindStep step, std::string_view replacement, std::uint64_t request) {
	if(request == findRequest_) return false;
	findRequest_ = request;
	switch(step) {
	case FindStep::First: {
		const Position start = findScope_ ? findScope_->first : selection().first;
		anchor_ = caret_ = start;
		if(!findNext()) onCaretMoved(caret_); // the count changed, maybe not the caret
		break;
	}
	case FindStep::Next:
	case FindStep::Previous:
		findNext(step == FindStep::Previous);
		break;
	case FindStep::Replace:
		replaceNext(replacement);
		break;
	case FindStep::ReplaceAll:
		replaceAll(replacement);
		onCaretMoved(caret_); // the count changed, maybe not the caret
		break;
	}
	return true;
}

void TextEditor::findVisibleMatches(std::size_t firstLine, std::size_t lastLine) const {
	const std::array<std::uint64_t, 4> key{model_->version(), firstLine, lastLine, findGeneration_};
	if(key == matchesKey_) return;
	matchesKey_ = key;
	visibleMatches_.clear();
	if(!find_.valid()) return;
	for(std::size_t line = firstLine; line < lastLine && visibleMatches_.size() < maxVisibleMatches;
	    line = nextShown(line)) {
		lineMatches(line, 0, maxMatchesPerLine, found_);
		for(const auto& [column, length] : found_) visibleMatches_.push_back({line, column, length});
	}
}

std::size_t TextEditor::matchCount() const {
	if(!find_.valid()) return 0;
	std::size_t count = 0;
	for(std::size_t line = 0; line < lines_.size() && count < maxMatches; ++line) {
		lineMatches(line, 0, maxMatches - count, found_);
		count += found_.size();
	}
	return count;
}

bool TextEditor::findNext(bool backwards) {
	if(!find_.valid()) return false;
	const auto [start, end] = selection();
	const std::size_t lines = lines_.size();
	for(std::size_t scanned = 0; scanned <= lines; ++scanned) {
		const std::size_t line =
		    backwards ? (start.line + lines - scanned % lines) % lines : (end.line + scanned) % lines;
		lineMatches(line, scanned == 0 && !backwards ? end.column : 0, maxMatchesPerLine, found_);
		if(backwards && scanned == 0) std::erase_if(found_, [&](const auto& m) { return m.first >= start.column; });
		if(found_.empty()) continue;
		const auto [column, length] = backwards ? found_.back() : found_.front();
		unfoldAround(line);
		anchor_ = {line, column};
		moveCaret({line, column + length}, true);
		return true;
	}
	return false;
}

bool TextEditor::replaceNext(std::string_view replacement) {
	if(!find_.valid() || !editable()) return false;
	const auto [from, to] = selection();
	// The selection is a match if one starts there and ends where it does
	const std::size_t byte = from.line == to.line ? encodeLine(lines_[from.line], from.column, subject_) : 0;
	const PatternFind found = from.line == to.line ? find_.find(subject_, byte) : PatternFind{};
	const bool selected = found.match && found.match->offset == byte &&
	                      codepoints(subject_, byte, byte + found.match->length) == to.column - from.column &&
	                      from != to;
	if(!selected) {
		findNext();
		return false;
	}
	const std::optional<std::string> text = find_.expand(subject_, *found.match, replacement);
	if(!text) return false;
	clearCursors();
	beginEdit(Edit::Other);
	replaceSelection(detail::fromUtf8(*text));
	endEdit();
	findNext();
	return true;
}

std::size_t TextEditor::replaceAll(std::string_view replacement) {
	if(!find_.valid() || !editable()) return 0;
	struct Replacement {
		Position from, to;
		std::u32string text;
	};
	std::vector<Replacement> replacements;
	for(std::size_t line = 0; line < lines_.size() && replacements.size() < maxMatches; ++line) {
		encodeLine(lines_[line], 0, subject_);
		std::size_t byte = 0;
		while(replacements.size() < maxMatches) {
			const PatternFind found = find_.find(subject_, byte);
			if(!found.match) break;
			const PatternMatch m = *found.match;
			const std::size_t column = codepoints(subject_, 0, m.offset);
			const std::size_t length = codepoints(subject_, m.offset, m.offset + m.length);
			const std::optional<std::string> text = find_.expand(subject_, m, replacement);
			if(!text) return 0;
			if(m.length > 0 && inScope(line, column, length))
				replacements.push_back({{line, column}, {line, column + length}, detail::fromUtf8(*text)});
			byte = afterMatch(subject_, m);
			if(byte > subject_.size()) break;
		}
	}
	if(replacements.empty()) return 0;
	clearCursors();
	beginEdit(Edit::Other);
	// Last first, so the positions before each stay where they were found
	for(const auto& change : std::views::reverse(replacements)) {
		erase(change.from, change.to);
		insert(change.from, change.text);
	}
	caret_ = anchor_ = clamped(caret_);
	endEdit();
	return replacements.size();
}

std::optional<std::pair<TextEditor::Position, TextEditor::Position>> TextEditor::matchingBrackets() const {
	if(!brackets_) return std::nullopt;
	const std::u32string& text = lines_[caret_.line];
	const auto bracket = bracketNear(text, caret_.column);
	if(!bracket) return std::nullopt;
	const Position at{caret_.line, *bracket};
	if(std::optional<Position> paired; highlighter_ && highlighter_->matchingBracket(*model_, at, paired))
		return paired ? std::optional(std::pair{at, *paired}) : std::nullopt;
	const char32_t self = text[at.column];
	const auto [other, opens] = pairOf(self);
	std::size_t depth = 0, scanned = 0;
	Position p = at;
	while(scanned++ < maxBracketScan) {
		const Position next = step(p, opens, false);
		if(next == p) return std::nullopt;
		p = next;
		const std::u32string& line = lines_[p.line]; // once a step: each look-up searches the lines' cache
		const std::optional<std::size_t> column = steppedOver(line, p.column, opens);
		if(!column) continue;
		const char32_t c = line[*column];
		if(c == self) ++depth;
		else if(c == other && depth-- == 0) return std::pair{at, Position{p.line, *column}};
	}
	return std::nullopt;
}

} // namespace affresco
