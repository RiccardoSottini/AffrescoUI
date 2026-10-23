/// @file
/// @brief TextModel: positions and offsets over the piece tree, UTF-8 checked as it comes in, and undo as operations.

#include "affresco/render/TextModel.hpp"

#include "detail/Utf8.hpp"
#include "text/PieceTree.hpp"

#include <algorithm>
#include <cstring>
#include <ranges>
#include <utility>

namespace affresco {

namespace {

/// @brief Whether a byte continues a UTF-8 sequence (10xxxxxx).
/// @param byte The byte.
constexpr bool continues(char byte) noexcept { return (static_cast<unsigned char>(byte) & 0xC0u) == 0x80u; }

/// @brief Where UTF-8 text first goes wrong.
/// @param text The text.
/// @return The offset of the first invalid sequence, or npos if it is all valid.
// NOLINTNEXTLINE(bugprone-exception-escape): substr() is called only at offsets inside the text
std::size_t firstInvalid(std::string_view text) noexcept {
	constexpr std::uint64_t highBits = 0x8080808080808080u;
	std::size_t at = 0;
	while(at < text.size()) {
		std::uint64_t word = 0;
		if(at + sizeof word <= text.size()) {
			std::memcpy(&word, text.data() + at, sizeof word);
			if((word & highBits) == 0) { // eight ASCII bytes
				at += sizeof word;
				continue;
			}
		}
		if(static_cast<unsigned char>(text[at]) < 0x80) {
			++at;
			continue;
		}
		const detail::Decoded decoded = detail::decodeUtf8(text.substr(at));
		if(decoded.codepoint == 0xFFFD && text.substr(at, 3) != "\xEF\xBF\xBD") return at;
		at += decoded.length;
	}
	return std::string_view::npos;
}

/// @brief UTF-8 text with each invalid sequence replaced by U+FFFD; the text itself when it is valid, as most is.
/// @param text The text.
/// @param scratch Holds the repaired copy, if one is needed.
std::string_view validUtf8(std::string_view text, std::string& scratch) {
	std::size_t at = firstInvalid(text);
	if(at == std::string_view::npos) return text;
	scratch.assign(text.substr(0, at));
	while(at < text.size()) {
		const detail::Decoded decoded = detail::decodeUtf8(text.substr(at));
		detail::appendUtf8(scratch, decoded.codepoint);
		at += decoded.length;
	}
	return scratch;
}

/// @brief Code points in valid UTF-8.
/// @param text The text.
std::size_t codepoints(std::string_view text) noexcept {
	return static_cast<std::size_t>(std::ranges::count_if(text, [](char byte) { return !continues(byte); }));
}

/// @brief Where text inserted at a position ends.
/// @param at Where it starts.
/// @param utf8 The text, valid UTF-8.
// NOLINTNEXTLINE(bugprone-exception-escape): substr() is called only at offsets inside the text
TextPosition advance(TextPosition at, std::string_view utf8) noexcept {
	const std::size_t lastFeed = utf8.rfind('\n');
	if(lastFeed == std::string_view::npos) return {at.line, at.column + codepoints(utf8)};
	const auto feeds = static_cast<std::size_t>(std::ranges::count(utf8, '\n'));
	return {at.line + feeds, codepoints(utf8.substr(lastFeed + 1))};
}

/// @brief Decodes valid UTF-8 that comes in parts, which may cut a code point.
struct Decoder {
	std::u32string& out;       ///< Gets the code points.
	char32_t pending = 0;      ///< The code point being read.
	std::size_t remaining = 0; ///< Its bytes still to come.

	/// @brief Decodes a part.
	/// @param part The bytes.
	void operator()(std::string_view part) {
		for(const char c : part) {
			const auto byte = static_cast<unsigned char>(c);
			if(byte < 0x80) {
				out.push_back(byte);
			} else if(continues(c)) {
				pending = (pending << 6) | (byte & 0x3Fu);
				if(remaining > 0) {
					--remaining;
					if(remaining == 0) out.push_back(pending);
				}
			} else {
				remaining = byte >= 0xF0 ? 3 : byte >= 0xE0 ? 2 : 1;
				pending = byte & (0x3Fu >> remaining);
			}
		}
	}
};

} // namespace

TextModel::TextModel() : tree_(std::make_unique<detail::PieceTree>()) {}

TextModel::~TextModel() = default;

/* Reading */

std::size_t TextModel::lineCount() const noexcept { return tree_->lineFeeds() + 1; }

std::size_t TextModel::size() const noexcept { return tree_->size(); }

std::size_t TextModel::longestLine() const noexcept { return tree_->longestLine(); }

void TextModel::line(std::size_t line, std::u32string& out) const {
	out.clear();
	const std::size_t start = tree_->lineStart(line);
	const std::size_t end = line + 1 < lineCount() ? tree_->lineStart(line + 1) - 1 : size();
	tree_->read(start, end - start, Decoder{out});
}

std::size_t TextModel::lineLength(std::size_t line) const {
	const std::size_t start = tree_->lineStart(line);
	const std::size_t end = line + 1 < lineCount() ? tree_->lineStart(line + 1) - 1 : size();
	std::size_t length = 0;
	tree_->read(start, end - start, [&](std::string_view part) { length += codepoints(part); });
	return length;
}

std::string TextModel::text() const {
	std::string out;
	out.reserve(size());
	tree_->read(0, size(), [&](std::string_view part) { out.append(part); });
	return out;
}

std::string TextModel::textIn(TextPosition from, TextPosition to) const {
	const std::size_t start = offsetOf(from), end = std::max(offsetOf(to), start);
	std::string out;
	out.reserve(end - start);
	tree_->read(start, end - start, [&](std::string_view part) { out.append(part); });
	return out;
}

bool TextModel::equals(std::string_view utf8) const {
	if(utf8.size() != size()) return false;
	bool same = true;
	std::size_t at = 0;
	tree_->read(0, size(), [&](std::string_view part) {
		same = same && utf8.substr(at, part.size()) == part;
		at += part.size();
	});
	return same;
}

std::size_t TextModel::offsetOf(TextPosition at) const {
	const std::size_t line = std::min(at.line, lineCount() - 1);
	const std::size_t start = tree_->lineStart(line);
	const std::size_t end = line + 1 < lineCount() ? tree_->lineStart(line + 1) - 1 : size();
	// The column-th byte that starts a code point, or the line's end
	std::size_t offset = start, remaining = at.column;
	bool found = false;
	tree_->read(start, end - start, [&](std::string_view part) {
		for(std::size_t i = 0; i < part.size() && !found; ++i, ++offset) {
			if(continues(part[i])) continue;
			if(remaining == 0) found = true;
			else --remaining;
			if(found) return;
		}
	});
	return offset;
}

TextPosition TextModel::positionOf(std::size_t offset) const {
	offset = std::min(offset, size());
	const std::size_t line = tree_->lineFeedsBefore(offset);
	const std::size_t start = tree_->lineStart(line);
	std::size_t column = 0;
	tree_->read(start, offset - start, [&](std::string_view part) { column += codepoints(part); });
	return {line, column};
}

std::size_t TextModel::memory() const noexcept {
	std::size_t total = tree_->memory();
	for(const std::vector<Step>* steps : {&undo_, &redo_})
		for(const Step& step : *steps) total += step.bytes;
	return total;
}

/* Editing */

TextPosition TextModel::insert(TextPosition at, std::string_view utf8) {
	std::string scratch;
	const std::size_t offset = offsetOf(at);
	const std::string_view valid = validUtf8(utf8, scratch);
	const TextPosition from = positionOf(offset);
	replace(offset, 0, valid);
	return advance(from, valid);
}

void TextModel::erase(TextPosition from, TextPosition to) {
	const std::size_t start = offsetOf(from), end = offsetOf(to);
	if(end > start) replace(start, end - start, {});
}

void TextModel::assign(std::string_view utf8) {
	std::string scratch;
	replace(0, size(), validUtf8(utf8, scratch));
}

void TextModel::replace(std::size_t offset, std::size_t length, std::string_view utf8) {
	if(length == 0 && utf8.empty()) return;
	TextChange change{.from = positionOf(offset),
	                  .oldEnd = positionOf(offset + length),
	                  .newEnd = {},
	                  .offset = offset,
	                  .removed = length,
	                  .inserted = utf8.size()};
	if(recording_ && !replaying_) {
		std::string removed;
		removed.reserve(length);
		tree_->read(offset, length, [&](std::string_view part) { removed.append(part); });
		record({offset, std::move(removed), utf8.size()});
	} else if(!replaying_) { // an edit outside a step: the history no longer matches the text
		undo_.clear();
		redo_.clear();
	}
	tree_->erase(offset, length);
	tree_->insert(offset, utf8);
	++version_;
	change.newEnd = advance(change.from, utf8);
	onChanged(change);
}

/* Undo */

void TextModel::beginStep(bool merge, TextPosition caret, TextPosition anchor) {
	redo_.clear();
	if(!merge || undo_.empty()) undo_.push_back({{}, caret, anchor, sizeof(Step)});
	recording_ = true;
}

void TextModel::endStep() {
	if(!recording_) return;
	recording_ = false;
	// The oldest go first; the newest step stays, however large
	std::size_t kept = 0;
	for(const Step& step : undo_) kept += step.bytes;
	std::size_t dropped = 0;
	while(undo_.size() - dropped > 1 && (undo_.size() - dropped > maxUndo || kept > maxUndoBytes))
		kept -= undo_[dropped++].bytes;
	undo_.erase(undo_.begin(), undo_.begin() + static_cast<std::ptrdiff_t>(dropped));
}

void TextModel::record(Operation operation) {
	Step& step = undo_.back();
	step.bytes += operation.removed.size();
	if(!step.operations.empty()) {
		Operation& last = step.operations.back();
		if(operation.removed.empty() && operation.offset == last.offset + last.inserted) { // typing on
			last.inserted += operation.inserted;
			return;
		}
		if(last.inserted == 0 && operation.inserted == 0 &&
		   operation.offset + operation.removed.size() == last.offset) {
			last.removed.insert(0, operation.removed); // deleting backwards
			last.offset = operation.offset;
			return;
		}
		if(last.inserted == 0 && operation.inserted == 0 && operation.offset == last.offset) { // deleting forwards
			last.removed += operation.removed;
			return;
		}
	}
	step.bytes += sizeof(Operation);
	step.operations.push_back(std::move(operation));
}

bool TextModel::undo(bool redo, TextPosition& caret, TextPosition& anchor) {
	std::vector<Step>& from = redo ? redo_ : undo_;
	std::vector<Step>& to = redo ? undo_ : redo_;
	if(from.empty()) return false;
	Step step = std::move(from.back());
	from.pop_back();

	// Each operation is replaced by its opposite, last first; the opposite step keeps the text it removes, in the order
	// its operations were applied, as every step does
	Step opposite{{}, caret, anchor, sizeof(Step)};
	replaying_ = true;
	for(const auto& operation : std::views::reverse(step.operations)) {
		std::string removed;
		removed.reserve(operation.inserted);
		tree_->read(operation.offset, operation.inserted, [&](std::string_view part) { removed.append(part); });
		replace(operation.offset, operation.inserted, operation.removed);
		opposite.bytes += removed.size() + sizeof(Operation);
		opposite.operations.push_back({operation.offset, std::move(removed), operation.removed.size()});
	}
	replaying_ = false;
	caret = step.caret;
	anchor = step.anchor;
	to.push_back(std::move(opposite));
	return true;
}

/* Lines */

const std::u32string& TextLines::operator[](std::size_t line) const {
	for(const Entry& entry : lines_)
		if(entry.line == line && entry.version == model_->version()) return entry.text;
	Entry& entry = lines_[next_];
	next_ = (next_ + 1) % lines_.size();
	model_->line(line, entry.text);
	entry.line = line;
	entry.version = model_->version();
	return entry.text;
}

} // namespace affresco
