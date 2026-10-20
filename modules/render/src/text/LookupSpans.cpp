/// @file
/// @brief detail::LookupSpans: the spans of GSUB and GPOS lookups, read from the table with every offset checked.

#include "text/LookupSpans.hpp"

#include <algorithm>

namespace affresco::detail {

namespace {

constexpr int deepestNesting = 4;                ///< Lookups applied inside lookups, followed this deep.
constexpr std::size_t ruleBudget = 1'000'000;    ///< Rules, ligatures and what they list, read in a table at most.
constexpr std::size_t ignoreBaseGlyphs = 0x0002; ///< A lookup flag: such a lookup sees other neighbours than pairs.

/// @brief The wider of two spans, either of which may be unreadable.
/// @param a A span.
/// @param b Another.
std::optional<std::size_t> widest(std::optional<std::size_t> a, std::optional<std::size_t> b) {
	if(!a || !b) return std::nullopt;
	return std::max(*a, *b);
}

} // namespace

/* Reading */

std::optional<std::size_t> LookupSpans::u16(std::size_t at) const noexcept {
	if(at > table_.size() || table_.size() - at < 2) return std::nullopt;
	return static_cast<std::size_t>(table_[at]) << 8 | table_[at + 1];
}

std::optional<std::size_t> LookupSpans::u32(std::size_t at) const noexcept {
	const auto high = u16(at), low = u16(at + 2);
	if(!high || !low) return std::nullopt;
	return *high << 16 | *low;
}

bool LookupSpans::isCandidate(std::size_t glyph) const noexcept {
	return std::ranges::binary_search(candidates_, static_cast<unsigned>(glyph));
}

/* Lookups */

std::optional<std::size_t> LookupSpans::lookupSpan(unsigned lookup, int depth) {
	if(depth > deepestNesting) return unbounded;
	if(const auto found = spans_.find(lookup); found != spans_.end()) return found->second;
	spans_[lookup] = unbounded; // while it is read: a lookup that applies itself is unbounded
	std::optional<std::size_t> span = readLookup(lookup, depth);
	if(span) span = std::min(*span, unbounded); // a ligature or a context can count up to 65,535 glyphs
	spans_[lookup] = span;
	return span;
}

std::optional<std::size_t> LookupSpans::readLookup(unsigned lookup, int depth) {
	const auto list = u16(8); // the header's offset to the lookup list
	const auto entry = list ? u16(*list + 2 + 2 * std::size_t{lookup}) : std::nullopt;
	if(!list || !entry) return std::nullopt;
	const std::size_t at = *list + *entry;
	const auto type = u16(at), flags = u16(at + 2), count = u16(at + 4);
	if(!type || !flags || !count) return std::nullopt;
	if(*flags & ignoreBaseGlyphs) return unbounded;
	rulesRead_ += *count; // up to 65,535 subtables, in each of up to 65,535 lookups
	if(rulesRead_ > ruleBudget) return unbounded;

	std::optional<std::size_t> span = 0;
	for(std::size_t i = 0; i < *count && span; ++i) {
		const auto offset = u16(at + 6 + 2 * i);
		if(!offset) return std::nullopt;
		span = widest(span, extendedSpan(*type, at + *offset, depth));
	}
	return span;
}

std::optional<std::size_t> LookupSpans::extendedSpan(std::size_t type, std::size_t subtable, int depth) {
	const std::size_t extension = positioning_ ? 9 : 7;
	if(type != extension) return subtableSpan(type, subtable, depth);
	const auto real = u16(subtable + 2), far = u32(subtable + 4); // the subtable says its real type, and where it is
	if(!real || !far) return std::nullopt;
	return subtableSpan(*real, subtable + *far, depth);
}

std::optional<std::size_t> LookupSpans::subtableSpan(std::size_t type, std::size_t subtable, int depth) {
	if(!positioning_) return substitutionSpan(type, subtable, depth);
	switch(type) {
	case 1: // single adjustment
		return 1;
	case 2: { // pair adjustment: a value for the second glyph makes it skip being the first of the next pair
		const auto second = u16(subtable + 6);
		if(!second) return std::nullopt;
		return *second == 0 ? 2 : unbounded;
	}
	case 4: // mark-to-base, mark-to-ligature, mark-to-mark
	case 5:
	case 6:
		return 0;
	case 7:
		return contextSpan(subtable, false, depth);
	case 8:
		return contextSpan(subtable, true, depth);
	default: // cursive attachment, and anything unknown
		return unbounded;
	}
}

std::optional<std::size_t> LookupSpans::substitutionSpan(std::size_t type, std::size_t subtable, int depth) {
	switch(type) {
	case 1: // single, multiple and alternate substitutions
	case 2:
	case 3:
		return 1;
	case 4:
		return ligatureSpan(subtable);
	case 5:
		return contextSpan(subtable, false, depth);
	case 6:
		return contextSpan(subtable, true, depth);
	case 8:
		return reverseChainSpan(subtable);
	default:
		return unbounded;
	}
}

std::optional<std::size_t> LookupSpans::ligatureSpan(std::size_t subtable) {
	const auto sets = u16(subtable + 4);
	if(!sets) return std::nullopt;
	const Rules components{subtable, Encoding::glyphs, false, {}};
	std::size_t span = 0;
	for(std::size_t s = 0; s < *sets; ++s) {
		const auto setOffset = u16(subtable + 6 + 2 * s);
		const auto ligatures = setOffset ? u16(subtable + *setOffset) : std::nullopt;
		if(!setOffset || !ligatures) return std::nullopt;
		const std::size_t set = subtable + *setOffset;
		for(std::size_t l = 0; l < *ligatures; ++l) {
			if(++rulesRead_ > ruleBudget) return unbounded;
			const auto offset = u16(set + 2 + 2 * l);
			const auto count = offset ? u16(set + *offset + 2) : std::nullopt;
			if(!offset || !count) return std::nullopt;
			if(*count == 0) return unbounded;
			const auto live = sequenceLive(components, set + *offset + 4, *count - 1, 1);
			if(!live) return std::nullopt;
			if(*live) span = std::max(span, *count);
		}
	}
	return span;
}

std::optional<std::size_t> LookupSpans::reverseChainSpan(std::size_t subtable) {
	const Rules rules{subtable, Encoding::coverages, true, {}};
	const auto backtrack = u16(subtable + 4);
	const auto lookahead = backtrack ? u16(subtable + 6 + 2 * *backtrack) : std::nullopt;
	if(!backtrack || !lookahead) return std::nullopt;
	const std::size_t behind = *backtrack, ahead = *lookahead;
	const auto before = sequenceLive(rules, subtable + 6, behind, 0);
	const auto after = sequenceLive(rules, subtable + 8 + 2 * behind, ahead, 2);
	if(!before || !after) return std::nullopt;
	return before.value_or(false) && after.value_or(false) ? behind + 1 + ahead : 0;
}

/* Contextual rules */

std::optional<std::size_t> LookupSpans::contextSpan(std::size_t subtable, bool chained, int depth) {
	const auto format = u16(subtable);
	if(!format) return std::nullopt;
	Rules rules{subtable, Encoding::glyphs, chained, {}};
	if(*format == 1) return ruleSetsSpan(rules, subtable + 4, depth);
	if(*format == 3) { // the subtable is laid out as one rule, with a coverage per glyph
		rules.encoding = Encoding::coverages;
		return ruleSpan(rules, subtable + 2, depth);
	}
	if(*format != 2) return unbounded;

	// Format 2: classes of one definition (input), or of three (backtrack, input, lookahead)
	rules.encoding = Encoding::classes;
	const std::size_t definitions = chained ? 3 : 1;
	for(std::size_t part = 0; part < definitions; ++part) {
		const auto offset = u16(subtable + 4 + 2 * part);
		if(!offset) return std::nullopt;
		auto classes = *offset == 0 ? std::vector<std::size_t>{0} : classesOf(subtable + *offset);
		if(!classes) return std::nullopt;
		rules.liveClasses[chained ? part : 1] = std::move(*classes);
	}
	return ruleSetsSpan(rules, subtable + 4 + 2 * definitions, depth);
}

std::optional<std::size_t> LookupSpans::ruleSetsSpan(const Rules& rules, std::size_t sets, int depth) {
	const auto setCount = u16(sets);
	if(!setCount) return std::nullopt;
	std::optional<std::size_t> span = 0;
	for(std::size_t s = 0; s < *setCount && span; ++s) {
		const auto setOffset = u16(sets + 2 + 2 * s);
		if(!setOffset) return std::nullopt;
		if(*setOffset == 0) continue; // a class no rule starts with
		const std::size_t set = rules.subtable + *setOffset;
		const auto count = u16(set);
		if(!count) return std::nullopt;
		const std::size_t rulesInSet = *count;
		for(std::size_t r = 0; r < rulesInSet && span; ++r) {
			if(++rulesRead_ > ruleBudget) return unbounded;
			const auto offset = u16(set + 2 + 2 * r);
			if(!offset) return std::nullopt;
			span = widest(span, ruleSpan(rules, set + *offset, depth));
		}
	}
	return span;
}

std::optional<std::size_t> LookupSpans::ruleSpan(const Rules& rules, std::size_t rule, int depth) {
	const std::optional<RuleShape> shape = rules.chained ? chainedRule(rules, rule) : contextRule(rules, rule);
	if(!shape) return std::nullopt;
	if(shape->glyphs == unbounded) return unbounded;
	if(!shape->live) return 0; // it never matches text made of candidates
	return withNested(shape->glyphs, shape->records, shape->lookups, depth);
}

// Backtrack count and glyphs, input count and glyphs, lookahead count and glyphs, lookup count and records. Rules leave
// the first input glyph out (the coverage has it); format 3 has a coverage for every input glyph
std::optional<LookupSpans::RuleShape> LookupSpans::chainedRule(const Rules& rules, std::size_t rule) {
	const std::size_t skipped = rules.encoding == Encoding::coverages ? 0 : 1;
	RuleShape shape;
	std::size_t at = rule;
	for(std::size_t part = 0; part < 3; ++part) {
		const auto count = u16(at);
		if(!count) return std::nullopt;
		if(part == 1 && *count == 0) return RuleShape{unbounded, 0, 0, true};
		const std::size_t listed = part == 1 ? *count - skipped : *count;
		const auto live = sequenceLive(rules, at + 2, listed, part);
		if(!live) return std::nullopt;
		shape.live = shape.live && *live;
		shape.glyphs += *count;
		at += 2 + 2 * listed;
	}
	const auto lookups = u16(at);
	if(!lookups) return std::nullopt;
	shape.records = at + 2;
	shape.lookups = *lookups;
	return shape;
}

// Glyph count, lookup count, the glyphs (all but the first, or a coverage for each in format 3), the records
std::optional<LookupSpans::RuleShape> LookupSpans::contextRule(const Rules& rules, std::size_t rule) {
	const std::size_t skipped = rules.encoding == Encoding::coverages ? 0 : 1;
	const auto glyphs = u16(rule), lookups = u16(rule + 2);
	if(!glyphs || !lookups) return std::nullopt;
	if(*glyphs == 0) return RuleShape{unbounded, 0, 0, true};
	const auto live = sequenceLive(rules, rule + 4, *glyphs - skipped, 1);
	if(!live) return std::nullopt;
	return RuleShape{*glyphs, rule + 4 + 2 * (*glyphs - skipped), *lookups, *live};
}

// A lookup applied at a glyph of the rule may read past the rule's last glyph: as far as its own span reaches
std::optional<std::size_t> LookupSpans::withNested(std::size_t glyphs, std::size_t records, std::size_t count,
                                                   int depth) {
	rulesRead_ += count; // many rules can share one rule of 65,535 records
	if(rulesRead_ > ruleBudget) return unbounded;
	std::size_t nested = 0;
	for(std::size_t i = 0; i < count; ++i) {
		const auto lookup = u16(records + 4 * i + 2);
		if(!lookup) return std::nullopt;
		const auto span = lookupSpan(static_cast<unsigned>(*lookup), depth + 1);
		if(!span) return std::nullopt;
		nested = std::max(nested, *span);
	}
	return nested <= 1 ? glyphs : std::min(glyphs + nested - 1, unbounded);
}

/* Which glyphs a rule may match */

std::optional<bool> LookupSpans::sequenceLive(const Rules& rules, std::size_t sequence, std::size_t count,
                                              std::size_t part) {
	rulesRead_ += count; // as withNested(): a shared sequence of 65,535 glyphs
	if(rulesRead_ > ruleBudget) return true;
	for(std::size_t i = 0; i < count; ++i) {
		const auto value = u16(sequence + 2 * i);
		if(!value) return std::nullopt;
		switch(rules.encoding) {
		case Encoding::glyphs:
			if(!isCandidate(*value)) return false;
			break;
		case Encoding::classes:
			if(!std::ranges::binary_search(rules.liveClasses[part], *value)) return false;
			break;
		case Encoding::coverages: {
			const auto live = coverageLive(rules.subtable + *value);
			if(!live || !*live) return live;
			break;
		}
		}
	}
	return true;
}

std::optional<bool> LookupSpans::coverageLive(std::size_t coverage) {
	const auto format = u16(coverage), count = u16(coverage + 2);
	if(!format || !count) return std::nullopt;
	rulesRead_ += *count; // a coverage can be large, and many rules can name it
	if(rulesRead_ > ruleBudget) return true;
	for(std::size_t i = 0; i < *count; ++i) {
		if(*format == 1) {
			const auto glyph = u16(coverage + 4 + 2 * i);
			if(!glyph) return std::nullopt;
			if(isCandidate(*glyph)) return true;
		} else if(*format == 2) {
			const auto first = u16(coverage + 4 + 6 * i), last = u16(coverage + 6 + 6 * i);
			if(!first || !last) return std::nullopt;
			const auto next = std::ranges::lower_bound(candidates_, static_cast<unsigned>(*first));
			if(next != candidates_.end() && *next <= *last) return true;
		} else {
			return true; // unknown: it may match anything
		}
	}
	return false;
}

std::optional<std::vector<std::size_t>> LookupSpans::classesOf(std::size_t classes) {
	const auto format = u16(classes);
	std::optional<std::vector<std::size_t>> found;
	if(format == std::optional<std::size_t>(1)) found = listedClasses(classes);
	else if(format == std::optional<std::size_t>(2)) found = rangedClasses(classes);
	if(!found) return std::nullopt;
	std::ranges::sort(*found);
	const auto [first, last] = std::ranges::unique(*found);
	found->erase(first, last);
	return found;
}

// Format 1: a first glyph, and a class for each glyph from it
std::optional<std::vector<std::size_t>> LookupSpans::listedClasses(std::size_t classes) const {
	const auto first = u16(classes + 2), count = u16(classes + 4);
	if(!first || !count) return std::nullopt;
	std::vector<std::size_t> found;
	for(const unsigned glyph : candidates_) {
		const bool listed = glyph >= *first && glyph - *first < *count;
		const auto value = listed ? u16(classes + 6 + 2 * (glyph - *first)) : std::optional<std::size_t>(0);
		if(!value) return std::nullopt;
		found.push_back(*value);
	}
	return found;
}

// Format 2: ranges of glyphs and their classes, sorted, as shaping searches them
std::optional<std::vector<std::size_t>> LookupSpans::rangedClasses(std::size_t classes) const {
	const auto count = u16(classes + 2);
	if(!count) return std::nullopt;
	std::vector<std::array<std::size_t, 3>> ranges; // first, last, class
	ranges.reserve(*count);
	for(std::size_t i = 0; i < *count; ++i) {
		const auto first = u16(classes + 4 + 6 * i), last = u16(classes + 6 + 6 * i), value = u16(classes + 8 + 6 * i);
		if(!first || !last || !value || *last < *first || (!ranges.empty() && *first <= ranges.back()[1]))
			return std::nullopt;
		ranges.push_back({*first, *last, *value});
	}
	std::vector<std::size_t> found;
	for(const unsigned glyph : candidates_) {
		const auto range = std::ranges::upper_bound(ranges, std::size_t{glyph}, {}, [](const auto& r) { return r[0]; });
		found.push_back(range != ranges.begin() && glyph <= (range - 1)->at(1) ? (range - 1)->at(2) : 0);
	}
	return found;
}

} // namespace affresco::detail
