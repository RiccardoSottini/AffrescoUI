/// @file
/// @brief LanguageHighlighter: the regions open at each line's start are kept, so a line is tokenized from the one
/// before it; an edit forgets them from its first line on.

#include "LanguageHighlighter.hpp"

#include <algorithm>
#include <string>

namespace affresco::format {

namespace {

/// @brief The bytes a code point takes in UTF-8.
/// @param c The code point.
/// @return 1 to 4.
std::size_t utf8Length(char32_t c) noexcept { return c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4; }

/// @brief The colour of a style, or of its nearest parent style that has one.
/// @param palette The colours.
/// @param style The style, e.g. "string.escape".
/// @return It, or nothing.
std::optional<Color> colorWithParents(const SyntaxHighlighter::Palette& palette, std::string_view style) {
	while(!style.empty()) {
		if(std::optional<Color> color = palette(style)) return color;
		const std::size_t dot = style.rfind('.');
		style = dot == std::string_view::npos ? std::string_view() : style.substr(0, dot);
	}
	return std::nullopt;
}

} // namespace

LanguageHighlighter::LanguageHighlighter(std::shared_ptr<const Language> language)
    : language_(language), tokenizer_(std::move(language)) {}

void LanguageHighlighter::setPalette(const SyntaxHighlighter::Palette& palette) {
	colors_.clear();
	for(const std::string& scope : language_->scopes)
		colors_.push_back(palette ? colorWithParents(palette, scope) : std::nullopt);
	palette_ = palette;
}

void LanguageHighlighter::setOverlay(std::vector<SyntaxHighlighter::Overlay> runs) {
	if(runs.size() > SyntaxHighlighter::maxOverlays) runs.resize(SyntaxHighlighter::maxOverlays);
	std::ranges::stable_sort(runs, {}, &SyntaxHighlighter::Overlay::line);
	overlays_ = std::move(runs);
}

void LanguageHighlighter::reset(const TextModel& model) {
	static_cast<void>(model);
	states_.clear();
	lines_.clear();
	overlays_.clear();
}

void LanguageHighlighter::changed(const TextModel& model, const TextChange& change) {
	static_cast<void>(model);
	if(states_.size() > change.from.line + 1) states_.resize(change.from.line + 1);
	lines_.erase(lines_.lower_bound(change.from.line), lines_.end());
	// Runs on edited lines go; those after them move with the lines
	const auto removed = [&change](const SyntaxHighlighter::Overlay& run) {
		return run.line >= change.from.line && run.line <= change.oldEnd.line;
	};
	std::erase_if(overlays_, removed);
	for(SyntaxHighlighter::Overlay& run : overlays_)
		if(run.line > change.oldEnd.line) run.line = run.line - change.oldEnd.line + change.newEnd.line;
}

std::string LanguageHighlighter::textOf(const TextModel& model, std::size_t line) {
	return model.textIn({line, 0}, {line, model.lineLength(line)});
}

const TokenizedLine* LanguageHighlighter::tokensOf(const TextModel& model, std::size_t line) {
	if(model.lineCount() > maxLines || line >= model.lineCount()) return nullptr;
	if(const auto kept = lines_.find(line); kept != lines_.end()) return &kept->second;
	if(states_.empty()) states_.emplace_back();
	while(states_.size() <= line) {
		const std::size_t before = states_.size() - 1;
		states_.push_back(tokenizer_.tokenize(states_[before], textOf(model, before)).end);
	}
	TokenizedLine tokens = tokenizer_.tokenize(states_[line], textOf(model, line));
	if(states_.size() == line + 1) states_.push_back(tokens.end);
	if(lines_.size() >= maxKeptLines) lines_.clear();
	return &lines_.emplace(line, std::move(tokens)).first->second;
}

std::optional<Color> LanguageHighlighter::colorOf(const std::vector<ScopeId>& scopes) const {
	for(auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
		if(*scope != 0 && *scope <= colors_.size() && colors_[*scope - 1]) return colors_[*scope - 1];
	return std::nullopt;
}

void LanguageHighlighter::colorLine(const TextModel& model, std::size_t line, std::u32string_view text,
                                    std::vector<Color>& colors) {
	const TokenizedLine* tokens = tokensOf(model, line);
	if(tokens == nullptr) return;
	std::vector<std::size_t> starts; // each code point's first byte, then the line's length
	starts.reserve(text.size() + 1);
	std::size_t byte = 0;
	for(const char32_t c : text) {
		starts.push_back(byte);
		byte += utf8Length(c);
	}
	starts.push_back(byte);
	const auto column = [&starts](std::size_t at) {
		return static_cast<std::size_t>(std::ranges::lower_bound(starts, at) - starts.begin());
	};
	const std::size_t count = std::min(colors.size(), text.size());
	for(const LanguageToken& token : tokens->tokens)
		if(const std::optional<Color> color = colorOf(token.scopes))
			std::fill(colors.begin() + static_cast<std::ptrdiff_t>(std::min(column(token.from), count)),
			          colors.begin() + static_cast<std::ptrdiff_t>(std::min(column(token.to), count)), *color);
	const auto first = std::ranges::lower_bound(overlays_, line, {}, &SyntaxHighlighter::Overlay::line);
	for(auto run = first; run != overlays_.end() && run->line == line; ++run) {
		const std::optional<Color> color = palette_ ? colorWithParents(palette_, run->style) : std::nullopt;
		if(!color || run->column >= count) continue;
		const std::size_t end = run->column + std::min(run->length, count - run->column);
		std::fill(colors.begin() + static_cast<std::ptrdiff_t>(run->column),
		          colors.begin() + static_cast<std::ptrdiff_t>(end), *color);
	}
}

std::vector<std::string> LanguageHighlighter::scopesAt(const TextModel& model, std::size_t line, std::size_t column) {
	std::vector<std::string> names;
	const TokenizedLine* tokens = tokensOf(model, line);
	if(tokens == nullptr) return names;
	std::u32string text;
	model.line(line, text);
	std::size_t byte = 0;
	for(std::size_t i = 0; i < column && i < text.size(); ++i) byte += utf8Length(text[i]);
	const LanguageToken* found = nullptr;
	for(const LanguageToken& token : tokens->tokens)
		if(token.from <= byte && byte < token.to) found = &token; // the last painted wins, as when colouring
	if(found != nullptr)
		for(const ScopeId scope : found->scopes) names.push_back(language_->scopes[scope - 1]);
	return names;
}

} // namespace affresco::format
