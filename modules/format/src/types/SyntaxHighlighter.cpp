/// @file
/// @brief SyntaxHighlighter (affresco/format/SyntaxHighlighter.hpp): a TextEditor's text kept parsed by the syntax
/// engine, coloured a block of lines at a time.

#include "affresco/format/SyntaxHighlighter.hpp"

#include "affresco/services/Syntax.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>

namespace affresco::format {

using services::SyntaxDocument;
using services::SyntaxPoint;

namespace {

constexpr std::uint32_t blockRows = 64;    ///< Lines highlighted at once, and read at once by the parser.
constexpr std::size_t mostCachedRows = 4096; ///< Lines whose runs are kept; past it, the cache starts over.
/// @brief The largest text whose edits are followed (byte offsets fit the engine's 32 bits).
constexpr std::size_t largestFollowed = std::numeric_limits<std::uint32_t>::max() / 2;

/// @brief A line's length in bytes, without its line feed.
/// @param model The text.
/// @param line The line, below lineCount().
std::size_t bytesOf(const TextModel& model, std::size_t line) {
	const std::size_t start = model.offsetOf({line, 0});
	const std::size_t end = line + 1 < model.lineCount() ? model.offsetOf({line + 1, 0}) - 1 : model.size();
	return end - start;
}

/// @brief The bytes each code point of a line takes in UTF-8.
/// @param c The code point.
std::size_t utf8Length(char32_t c) noexcept { return c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4; }

} // namespace

/// @brief A highlighter's state.
struct SyntaxHighlighter::State {
	std::string language;                                    ///< The language.
	std::unique_ptr<SyntaxDocument> document;                ///< The parsed text.
	Palette palette;                                         ///< See setPalette().
	std::vector<std::optional<Color>> colors;                ///< By style: its colour, once resolved.
	std::vector<bool> resolved;                              ///< By style: whether colors has it.
	const void* owner = nullptr;                             ///< See setOwner().
	std::vector<std::size_t> lineBytes;                      ///< Each line's length in bytes, before the next edit.
	std::map<std::uint32_t, std::vector<services::SyntaxSpan>> spans; ///< The runs of the lines coloured so far.
	bool needsParse = true;                                  ///< Edits came since the last parse.
	bool followed = false;                                   ///< The text is small enough to follow.
	std::string chunk;                                       ///< What the reader gave last.
	std::vector<std::size_t> starts;                         ///< One line's byte offsets, by code point, reused.
	std::map<std::size_t, std::vector<Overlay>> overlay;     ///< See setOverlay(), by line.
	std::map<std::string, std::optional<Color>, std::less<>> overlayColors; ///< The overlay's styles, resolved.

	/// @brief Reads the text for the engine: up to blockRows lines from a byte.
	/// @param model The text.
	services::SyntaxReader reader(const TextModel& model) {
		return [this, &model](std::size_t byte, SyntaxPoint) -> std::string_view {
			if(byte >= model.size()) return {};
			const TextPosition from = model.positionOf(byte);
			const TextPosition to = from.line + blockRows < model.lineCount() ? TextPosition{from.line + blockRows, 0}
			                                                                  : model.positionOf(model.size());
			chunk = model.textIn(from, to);
			return chunk;
		};
	}

	/// @brief Parses the text if edits came since the last parse, and forgets the runs of the lines it changed.
	/// @param model The text.
	void parse(const TextModel& model) {
		if(!needsParse || !followed) return;
		needsParse = false;
		std::vector<services::SyntaxRows> changed;
		document->parse(reader(model), model.size(), &changed);
		for(const services::SyntaxRows& rows : changed)
			spans.erase(spans.lower_bound(rows.first), spans.upper_bound(rows.last));
	}

	/// @brief A style's colour.
	/// @param style Its index in the document's styles.
	const std::optional<Color>& colorOf(std::uint16_t style) {
		const std::vector<std::string>& styles = document->styles();
		if(colors.size() != styles.size()) {
			colors.assign(styles.size(), std::nullopt);
			resolved.assign(styles.size(), false);
		}
		static const std::optional<Color> none;
		if(style >= colors.size()) return none;
		if(!resolved[style]) {
			resolved[style] = true;
			std::string_view name = styles[style];
			while(palette && !name.empty()) {
				colors[style] = palette(name);
				if(colors[style]) break;
				const std::size_t dot = name.rfind('.');
				name = dot == std::string_view::npos ? std::string_view() : name.substr(0, dot);
			}
		}
		return colors[style];
	}

	/// @brief An overlay style's colour: the palette's, or its parent's.
	/// @param style The style.
	const std::optional<Color>& overlayColor(const std::string& style) {
		const auto found = overlayColors.find(style);
		if(found != overlayColors.end()) return found->second;
		std::optional<Color> color;
		for(std::string_view name = style; palette && !name.empty() && !color;) {
			color = palette(name);
			const std::size_t dot = name.rfind('.');
			name = dot == std::string_view::npos ? std::string_view() : name.substr(0, dot);
		}
		return overlayColors.emplace(style, color).first->second;
	}

	/// @brief Moves the overlay with an edit: the runs on the lines it touched go, those after move.
	/// @param change The edit.
	void moveOverlay(const TextChange& change) {
		if(overlay.empty()) return;
		std::map<std::size_t, std::vector<Overlay>> moved;
		for(auto& [line, runs] : overlay) {
			if(line < change.from.line) moved.emplace(line, std::move(runs));
			else if(line > change.oldEnd.line) {
				const std::size_t to = line - change.oldEnd.line + change.newEnd.line;
				for(Overlay& run : runs) run.line = to;
				moved.emplace(to, std::move(runs));
			}
		}
		overlay = std::move(moved);
	}
};

SyntaxHighlighter::SyntaxHighlighter(std::unique_ptr<State> state) : state_(std::move(state)) {}

SyntaxHighlighter::~SyntaxHighlighter() = default;

std::unique_ptr<SyntaxHighlighter> SyntaxHighlighter::create(std::string_view language) {
	auto state = std::make_unique<State>();
	state->language = SyntaxDocument::canonical(language);
	if(!state->language.empty()) state->document = SyntaxDocument::create(state->language);
	if(!state->document) return nullptr;
	return std::unique_ptr<SyntaxHighlighter>(new SyntaxHighlighter(std::move(state)));
}

std::string SyntaxHighlighter::canonical(std::string_view name) { return SyntaxDocument::canonical(name); }

std::string SyntaxHighlighter::languageFor(std::string_view fileName) { return SyntaxDocument::languageFor(fileName); }

const std::string& SyntaxHighlighter::language() const noexcept { return state_->language; }

const std::vector<std::string>& SyntaxHighlighter::styles() const noexcept { return state_->document->styles(); }

void SyntaxHighlighter::setPalette(Palette palette) {
	state_->palette = std::move(palette);
	state_->overlayColors.clear();
	state_->colors.clear();
	state_->resolved.clear();
}

void SyntaxHighlighter::setOverlay(std::vector<Overlay> runs) {
	State& s = *state_;
	s.overlay.clear();
	if(runs.size() > maxOverlays) runs.resize(maxOverlays);
	for(Overlay& run : runs)
		if(run.length > 0) s.overlay[run.line].push_back(std::move(run));
}

void SyntaxHighlighter::setOwner(const void* owner) noexcept { state_->owner = owner; }

const void* SyntaxHighlighter::owner() const noexcept { return state_->owner; }

/// @brief EditorHighlighter::reset(): a new parse of the whole text, on the next use, and each line measured.
/// @param model The text.
void SyntaxHighlighter::reset(const TextModel& model) {
	State& s = *state_;
	s.document = SyntaxDocument::create(s.language);
	s.spans.clear();
	s.overlay.clear();
	s.needsParse = true;
	s.followed = model.size() <= largestFollowed;
	s.lineBytes.resize(model.lineCount());
	for(std::size_t line = 0; s.followed && line < model.lineCount(); ++line) s.lineBytes[line] = bytesOf(model, line);
}

/// @brief EditorHighlighter::changed(): tells the parser the edit, so the next parse is incremental.
/// @param model The text, after it.
/// @param change The edit.
void SyntaxHighlighter::changed(const TextModel& model, const TextChange& change) {
	State& s = *state_;
	s.moveOverlay(change);
	if(!s.followed || model.size() > largestFollowed || change.oldEnd.line >= s.lineBytes.size()) return reset(model);
	// Where the edit ended before it: the bytes from the start of its first line, over the lines it replaced
	const std::size_t lineStart = model.offsetOf({change.from.line, 0});
	std::size_t oldEndLineStart = lineStart;
	for(std::size_t line = change.from.line; line < change.oldEnd.line; ++line) oldEndLineStart += s.lineBytes[line] + 1;
	const std::size_t oldEndByte = change.offset + change.removed;
	if(oldEndLineStart > oldEndByte) return reset(model);
	const auto u32 = [](std::size_t value) { return static_cast<std::uint32_t>(value); };
	s.document->edit({.startByte = u32(change.offset),
	                  .oldEndByte = u32(oldEndByte),
	                  .newEndByte = u32(change.offset + change.inserted),
	                  .start = {u32(change.from.line), u32(change.offset - lineStart)},
	                  .oldEnd = {u32(change.oldEnd.line), u32(oldEndByte - oldEndLineStart)},
	                  .newEnd = {u32(change.newEnd.line),
	                             u32(model.offsetOf(change.newEnd) - model.offsetOf({change.newEnd.line, 0}))}});
	// The lines edited take their new lengths; the runs of those after move with them
	std::vector<std::size_t> lengths;
	for(std::size_t line = change.from.line; line <= change.newEnd.line; ++line) lengths.push_back(bytesOf(model, line));
	const auto first = s.lineBytes.begin() + static_cast<std::ptrdiff_t>(change.from.line);
	s.lineBytes.erase(first, s.lineBytes.begin() + static_cast<std::ptrdiff_t>(change.oldEnd.line + 1));
	s.lineBytes.insert(s.lineBytes.begin() + static_cast<std::ptrdiff_t>(change.from.line), lengths.begin(),
	                   lengths.end());
	const auto edited = u32(change.from.line);
	s.spans.erase(s.spans.lower_bound(edited), s.spans.upper_bound(u32(change.oldEnd.line)));
	if(change.newEnd.line != change.oldEnd.line) {
		std::map<std::uint32_t, std::vector<services::SyntaxSpan>> moved;
		const std::int64_t by = static_cast<std::int64_t>(change.newEnd.line) - static_cast<std::int64_t>(change.oldEnd.line);
		for(auto& [row, runs] : s.spans) {
			if(row < edited) {
				moved.emplace(row, std::move(runs));
				continue;
			}
			const auto to = u32(static_cast<std::uint64_t>(row + by));
			for(services::SyntaxSpan& run : runs) run.row = to;
			moved.emplace(to, std::move(runs));
		}
		s.spans = std::move(moved);
	}
	s.needsParse = true;
}

/// @brief EditorHighlighter::colorLine(): paints each capture with its theme token's colour, then the overlay.
/// @param model The text.
/// @param line The line.
/// @param text Its code points.
/// @param colors One per code point, painted over.
void SyntaxHighlighter::colorLine(const TextModel& model, std::size_t line, std::u32string_view text,
                                  std::vector<Color>& colors) {
	State& s = *state_;
	if(!s.followed) return;
	s.parse(model);
	const auto row = static_cast<std::uint32_t>(line);
	auto found = s.spans.find(row);
	if(found == s.spans.end()) {
		if(s.spans.size() >= mostCachedRows) s.spans.clear();
		// A block of lines at once: the query walks the tree once for them all
		const std::uint32_t last = static_cast<std::uint32_t>(std::min<std::size_t>(line + blockRows, model.lineCount()) - 1);
		std::vector<services::SyntaxSpan> runs;
		s.document->highlight(s.reader(model), row, last, runs);
		for(std::uint32_t r = row; r <= last; ++r) s.spans.try_emplace(r);
		for(const services::SyntaxSpan& run : runs) s.spans[run.row].push_back(run);
		found = s.spans.find(row);
	}
	// Bytes to code points
	s.starts.resize(text.size() + 1);
	std::size_t byte = 0;
	for(std::size_t i = 0; i < text.size(); ++i) {
		s.starts[i] = byte;
		byte += utf8Length(text[i]);
	}
	s.starts[text.size()] = byte;
	for(const services::SyntaxSpan& run : found->second) {
		const std::optional<Color>& color = s.colorOf(run.style);
		if(!color) continue;
		const auto from = static_cast<std::size_t>(std::ranges::lower_bound(s.starts, run.start) - s.starts.begin());
		const auto to = static_cast<std::size_t>(std::ranges::lower_bound(s.starts, run.end) - s.starts.begin());
		std::fill(colors.begin() + static_cast<std::ptrdiff_t>(std::min(from, colors.size())),
		          colors.begin() + static_cast<std::ptrdiff_t>(std::min(to, colors.size())), *color);
	}
	// The overlay, over the grammar
	if(const auto runs = s.overlay.find(line); runs != s.overlay.end())
		for(const Overlay& run : runs->second) {
			const std::optional<Color>& color = s.overlayColor(run.style);
			if(!color || run.column >= colors.size()) continue;
			const std::size_t to = std::min(colors.size(), run.column + run.length);
			std::fill(colors.begin() + static_cast<std::ptrdiff_t>(run.column),
			          colors.begin() + static_cast<std::ptrdiff_t>(to), *color);
		}
}

/// @brief EditorHighlighter::foldable(): the grammar's fold that starts on the line.
/// @param model The text.
/// @param line The line.
/// @param range Gets it, or nothing.
/// @return false while the text is too big to follow or unparsed, to fold by indentation.
bool SyntaxHighlighter::foldable(const TextModel& model, std::size_t line, std::optional<FoldRange>& range) {
	State& s = *state_;
	if(!s.followed) return false;
	s.parse(model);
	if(!s.document->parsed()) return false;
	if(const auto fold = s.document->fold(static_cast<std::uint32_t>(line)); fold && fold->last < model.lineCount())
		range = FoldRange{fold->first, fold->last};
	return true;
}

/// @brief EditorHighlighter::matchingBracket(): the bracket the grammar pairs with one.
/// @param model The text.
/// @param at The bracket.
/// @param other Gets its pair, or nothing.
/// @return false to pair by counting: the text too big to follow or unparsed, or a bracket no query pairs.
bool SyntaxHighlighter::matchingBracket(const TextModel& model, TextPosition at, std::optional<TextPosition>& other) {
	State& s = *state_;
	if(!s.followed) return false;
	s.parse(model);
	if(!s.document->parsed()) return false;
	const std::size_t lineStart = model.offsetOf({at.line, 0});
	const auto pair = s.document->brackets(
	    {static_cast<std::uint32_t>(at.line), static_cast<std::uint32_t>(model.offsetOf(at) - lineStart)});
	if(!pair) return false; // a bracket no query pairs (e.g. in a comment): counted instead
	const SyntaxPoint mate = pair->open.row == at.line && pair->open.column == model.offsetOf(at) - lineStart
	                             ? pair->close
	                             : pair->open;
	if(mate.row >= model.lineCount()) return false;
	const std::size_t offset = model.offsetOf({mate.row, 0}) + mate.column;
	if(offset > model.size()) return false;
	other = model.positionOf(offset);
	return true;
}

} // namespace affresco::format
