/// @file
/// @brief PatternHighlighter (PatternHighlighter.hpp): a line coloured by the leftmost match of its patterns.

#include "PatternHighlighter.hpp"

#include <algorithm>

namespace affresco::format {

namespace {

/// @brief Appends a code point in UTF-8, keeping where each code point starts.
/// @param c The code point.
/// @param out The text so far.
void appendUtf8(char32_t c, std::string& out) {
	if(c < 0x80) out += static_cast<char>(c);
	else if(c < 0x800) {
		out += static_cast<char>(0xC0 | (c >> 6));
		out += static_cast<char>(0x80 | (c & 0x3F));
	} else if(c < 0x10000) {
		out += static_cast<char>(0xE0 | (c >> 12));
		out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (c & 0x3F));
	} else {
		out += static_cast<char>(0xF0 | (c >> 18));
		out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (c & 0x3F));
	}
}

} // namespace

std::optional<std::string> PatternHighlighter::setRules(std::vector<Rule> rules) {
	if(rules.size() > maxRules) rules.resize(maxRules);
	const bool same = std::ranges::equal(rules, rules_, [](const Rule& a, const Rule& b) {
		return a.pattern == b.pattern;
	});
	rules_ = std::move(rules);
	if(same && compiled_.size() == rules_.size()) return std::nullopt;
	compiled_.clear();
	std::vector<TextPattern> compiled(rules_.size());
	for(std::size_t i = 0; i < rules_.size(); ++i)
		if(auto error = compiled[i].compile(rules_[i].pattern, {.regex = true, .caseSensitive = true}))
			return "pattern " + std::to_string(i) + ": " + *error;
	compiled_ = std::move(compiled);
	return std::nullopt;
}

void PatternHighlighter::reset(const TextModel& model) {
	(void)model;
}

void PatternHighlighter::changed(const TextModel& model, const TextChange& change) {
	(void)model, (void)change;
}

void PatternHighlighter::colorLine(const TextModel& model, std::size_t line, std::u32string_view text,
                                   std::vector<Color>& colors) {
	(void)model, (void)line;
	if(compiled_.empty() || text.empty()) return;
	std::string subject;
	std::vector<std::size_t> columnAt; // the code point each byte belongs to, and one past the end
	subject.reserve(text.size());
	for(std::size_t i = 0; i < text.size(); ++i) {
		const std::size_t before = subject.size();
		appendUtf8(text[i], subject);
		columnAt.insert(columnAt.end(), subject.size() - before, i);
	}
	columnAt.push_back(text.size());
	// Each pattern's next match, found again only once the search passes its start
	std::vector<std::optional<PatternMatch>> next(compiled_.size());
	std::vector<bool> searched(compiled_.size(), false);
	for(std::size_t at = 0; at < subject.size();) {
		std::size_t best = compiled_.size();
		for(std::size_t i = 0; i < compiled_.size(); ++i) {
			if(!searched[i] || (next[i] && next[i]->offset < at)) {
				PatternFind found = compiled_[i].find(subject, at);
				// An empty match colours nothing: look on from the next code point
				while(found.match && found.match->length == 0 && found.match->offset < subject.size()) {
					std::size_t from = found.match->offset + 1;
					while(from < subject.size() && (static_cast<unsigned char>(subject[from]) & 0xC0) == 0x80) ++from;
					found = compiled_[i].find(subject, from);
				}
				next[i] = found.match;
				searched[i] = true;
			}
			if(next[i] && next[i]->length > 0 && (best == compiled_.size() || next[i]->offset < next[best]->offset))
				best = i;
		}
		if(best == compiled_.size()) break;
		const PatternMatch match = *next[best];
		if(const auto& color = rules_[best].color)
			for(std::size_t c = columnAt[match.offset]; c < columnAt[match.offset + match.length] && c < colors.size(); ++c)
				colors[c] = *color;
		at = match.offset + match.length;
	}
}

} // namespace affresco::format
