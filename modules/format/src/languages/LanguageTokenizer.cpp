/// @file
/// @brief LanguageTokenizer: a walk along a line that, at each place, finds the earliest match among the rules the
/// innermost region (or the top level) tries, keeping each rule's find while it stays ahead, so a line costs about
/// one find per token.

#include "LanguageTokenizer.hpp"

#include <algorithm>
#include <chrono>

namespace affresco::format {

namespace {

constexpr std::size_t none = std::string::npos; ///< No place.

/// @brief Where the next character starts.
/// @param text UTF-8 text.
/// @param at A character's start.
/// @return The next one's.
std::size_t nextCharacter(std::string_view text, std::size_t at) noexcept {
	++at;
	while(at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0u) == 0x80u) ++at;
	return at;
}

} // namespace

/// @brief One line's walk: where it is, the regions open, the finds kept and the tokens so far.
struct LanguageTokenizer::Walk {
	/// @brief A rule's find, kept while it stays ahead of the walk.
	struct Found {
		std::size_t from = none;                         ///< Where it searched from; none: not searched.
		std::optional<PatternMatch> match;               ///< What it found.
		std::vector<std::optional<PatternMatch>> groups; ///< Its groups.
	};
	std::string text;                               ///< The line as searched: its bytes kept, then "\n".
	std::size_t length = 0;                         ///< Its bytes kept.
	std::size_t position = 0;                       ///< Where the walk is.
	std::size_t lastEmpty = none;                   ///< Where the last empty match was.
	std::size_t finds = 0;                          ///< Finds so far.
	std::chrono::steady_clock::time_point deadline; ///< When it stops.
	TokenizedLine out;                              ///< The tokens and, at the end, the open regions.
	std::vector<Found> kept;                        ///< Each rule's find at the current place.
	Found endFound;                                 ///< The innermost region's end's.

	/// @brief The scopes at the walk's place.
	/// @param inEnd Whether for the innermost region's end, which is outside its content.
	/// @return Them, outermost first.
	[[nodiscard]] std::vector<ScopeId> path(bool inEnd = false) const {
		std::vector<ScopeId> scopes;
		for(std::size_t i = 0; i < out.end.size(); ++i) {
			if(out.end[i].scope != 0) scopes.push_back(out.end[i].scope);
			if(out.end[i].contentScope != 0 && !(inEnd && i + 1 == out.end.size()))
				scopes.push_back(out.end[i].contentScope);
		}
		return scopes;
	}

	/// @brief Adds a token, its end kept to the line.
	/// @param from Its first byte.
	/// @param to The byte after it.
	/// @param scopes Its scopes.
	void emit(std::size_t from, std::size_t to, std::vector<ScopeId> scopes) {
		if(to > from && !scopes.empty())
			out.tokens.push_back({static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(to), std::move(scopes)});
	}

	/// @brief Adds a match's tokens: the whole, then its groups.
	/// @param match The match.
	/// @param base Its scopes.
	/// @param captures The scopes of its groups.
	/// @param groups Its groups.
	void emitMatch(PatternMatch match, const std::vector<ScopeId>& base,
	               const std::vector<LanguageRule::Capture>& captures,
	               const std::vector<std::optional<PatternMatch>>& groups) {
		const auto clamp = [this](std::size_t at) { return std::min(at, length); };
		emit(match.offset, clamp(match.offset + match.length), base);
		for(const LanguageRule::Capture& capture : captures) {
			if(capture.group >= groups.size() || !groups[capture.group]) continue;
			const PatternMatch group = *groups[capture.group];
			std::vector<ScopeId> scopes = base;
			scopes.push_back(capture.scope);
			emit(group.offset, clamp(group.offset + group.length), std::move(scopes));
		}
	}

	/// @brief Forgets the finds kept: the regions open changed, so the rules tried did.
	/// @param count How many rules the new place tries.
	void forget(std::size_t count) { kept.assign(count, Found{}); }
};

/// @brief The winner at one place of a walk.
struct LanguageTokenizer::Best {
	const Walk::Found* found = nullptr; ///< Its find.
	std::uint32_t rule = 0;             ///< Its rule, if not the end.
	bool end = false;                   ///< Whether it is the innermost region's end.
};

LanguageTokenizer::LanguageTokenizer(std::shared_ptr<const Language> language)
    : language_(std::move(language)), lineEnd_(compileLanguagePattern("$", "languages")) {
	groups_.setStepLimit(stepsPerFind);
}

std::shared_ptr<const TextPattern> LanguageTokenizer::endOf(const LanguageRule& rule, std::string_view text,
                                                            const std::vector<std::optional<PatternMatch>>& groups) {
	if(rule.end) return rule.end;
	std::vector<std::string> texts;
	for(const std::optional<PatternMatch>& group : groups)
		texts.emplace_back(group ? text.substr(group->offset, std::min(group->length, maxGroupBytes)) : "");
	std::string source = withGroups(rule.endSource, texts);
	if(const auto found = ends_.find(source); found != ends_.end()) return found->second;
	auto pattern = std::make_shared<TextPattern>();
	if(source.empty() ||
	   pattern->compile(source, PatternOptions{.regex = true, .wholeWord = false, .caseSensitive = true}))
		return lineEnd_; // e.g. a group cut inside a character: the region ends with its line
	if(ends_.size() >= maxEnds) ends_.clear();
	ends_.emplace(std::move(source), pattern);
	return pattern;
}

/// @brief Finds the earliest match at the walk's place among the innermost region's end and the rules it tries.
/// @param walk The walk.
/// @param language The language.
/// @param groups Room for groups.
/// @return The winner, or none.
LanguageTokenizer::Best LanguageTokenizer::earliest(Walk& walk, const Language& language, PatternGroups& groups) {
	Best best;
	const auto run = [&](const TextPattern& pattern, Walk::Found& found) {
		found.match = pattern.find(walk.text, walk.position, groups).match;
		found.groups = groups.groups();
		found.from = walk.position;
		++walk.finds;
	};
	const LanguageState& open = walk.out.end;
	if(!open.empty() && open.back().end) {
		run(*open.back().end, walk.endFound);
		if(walk.endFound.match) best = {&walk.endFound, 0, true};
	}
	const LanguageRule& top = language.rules[open.empty() ? 0 : open.back().rule];
	for(std::size_t i = 0; i < top.candidates.size() && walk.finds < LanguageTokenizer::maxFinds; ++i) {
		const LanguageRule& rule = language.rules[top.candidates[i]];
		Walk::Found& found = walk.kept[i];
		const bool fresh = found.from != none && found.from <= walk.position && !rule.anchored &&
		                   (!found.match || found.match->offset >= walk.position);
		if(!fresh) run(*rule.pattern, found);
		if(found.match && (best.found == nullptr || found.match->offset < best.found->match->offset))
			best = {&found, top.candidates[i], false};
	}
	return best;
}

/// @brief Takes one step of a walk: the earliest match, its tokens, and the region it opens or closes.
/// @param walk The walk.
/// @return Whether to go on.
bool LanguageTokenizer::step(Walk& walk) {
	if(walk.position > walk.length) return false;
	if(walk.finds >= maxFinds || std::chrono::steady_clock::now() > walk.deadline) {
		walk.out.cut = true;
		return false;
	}
	const Best best = earliest(walk, *language_, groups_);
	if(best.found == nullptr) return false;
	const PatternMatch match = *best.found->match;
	walk.emit(walk.position, std::min(match.offset, walk.length), walk.path());
	if(match.length == 0 && match.offset == walk.lastEmpty) {
		// An empty match again at the same place, with nothing changed between: step over a character
		const std::size_t next = nextCharacter(walk.text, match.offset);
		walk.emit(match.offset, std::min(next, walk.length), walk.path());
		walk.position = next;
		walk.lastEmpty = none;
		return true;
	}
	walk.lastEmpty = match.length == 0 ? match.offset : none;
	walk.position = match.offset + match.length;
	if(best.end) closeRegion(walk, match);
	else openOrMatch(walk, best.rule, match, best.found->groups);
	return true;
}

/// @brief Applies the innermost region's end: its tokens, then the region closes.
/// @param walk The walk.
/// @param match The end's match.
void LanguageTokenizer::closeRegion(Walk& walk, PatternMatch match) {
	const LanguageFrame frame = walk.out.end.back();
	const LanguageRule& rule = language_->rules[frame.rule];
	std::vector<ScopeId> base = walk.path(true);
	walk.emitMatch(match, base, rule.endCaptures.empty() ? rule.captures : rule.endCaptures, walk.endFound.groups);
	walk.out.end.pop_back();
	const LanguageState& open = walk.out.end;
	walk.forget(language_->rules[open.empty() ? 0 : open.back().rule].candidates.size());
}

/// @brief Applies a rule's match: its tokens, and the region it opens if it begins one.
/// @param walk The walk.
/// @param index The rule.
/// @param match Its match.
/// @param groups Its groups.
void LanguageTokenizer::openOrMatch(Walk& walk, std::uint32_t index, PatternMatch match,
                                    const std::vector<std::optional<PatternMatch>>& groups) {
	const LanguageRule& rule = language_->rules[index];
	std::vector<ScopeId> base = walk.path();
	if(rule.scope != 0) base.push_back(rule.scope);
	if(rule.kind == LanguageRule::Kind::Match) {
		walk.emitMatch(match, base, rule.captures, groups);
		return;
	}
	walk.emitMatch(match, base, rule.beginCaptures.empty() ? rule.captures : rule.beginCaptures, groups);
	if(walk.out.end.size() >= maxDepth) return; // too deep: the begin is only a match
	walk.out.end.push_back({index, rule.scope, rule.contentScope, endOf(rule, walk.text, groups)});
	walk.forget(rule.candidates.size()); // last: the groups may be a find it forgets
}

TokenizedLine LanguageTokenizer::tokenize(const LanguageState& start, std::string_view line) {
	Walk walk;
	walk.length = std::min(line.size(), maxLineBytes);
	while(walk.length < line.size() && (static_cast<unsigned char>(line[walk.length]) & 0xC0u) == 0x80u)
		--walk.length; // kept whole characters
	walk.text.assign(line.substr(0, walk.length));
	walk.text += '\n';
	walk.out.end = start.size() > maxDepth ? LanguageState(start.begin(), start.begin() + maxDepth) : start;
	walk.deadline =
	    std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
	                                           std::chrono::duration<double, std::milli>(millisecondsPerLine));
	walk.forget(language_->rules[walk.out.end.empty() ? 0 : walk.out.end.back().rule].candidates.size());
	while(step(walk)) {
	}
	if(walk.length < line.size()) walk.out.cut = true;
	walk.length = line.size();
	walk.emit(std::min(walk.position, line.size()), line.size(), walk.path());
	return std::move(walk.out);
}

} // namespace affresco::format
