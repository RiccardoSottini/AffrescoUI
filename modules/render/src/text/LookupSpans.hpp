#pragma once

/// @file
/// @brief detail::LookupSpans: how many neighbouring glyphs the lookups of an OpenType GSUB or GPOS table read, in
/// text made of a given set of glyphs.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace affresco::detail {

/// @brief Reads, from a GSUB or GPOS table, how many neighbouring glyphs each lookup may read or change at once in text
/// made only of some glyphs (the candidates): its span. PlainText uses it to tell which lookups shaping single
/// characters and pairs alone shows completely.
///
/// - 0: it does nothing to such text: it only attaches marks, or each of its rules needs a glyph that isn't a
///   candidate somewhere (e.g. a mark after the letter it changes).
/// - 1: it acts on one glyph (single, multiple and alternate substitutions; single positioning).
/// - n: its rules match n glyphs together (a ligature of n components; backtrack, input and lookahead of a contextual
///   rule), widened by the lookups a rule applies inside it.
/// - unbounded: what pairs can't show: pair kerning that also moves the second glyph (it then skips being the first
///   of the next pair), cursive attachment (chains of glyphs), and lookups that skip base glyphs.
/// A rule's first glyph comes from its subtable's coverage and isn't checked: a rule is only left out for its other
/// glyphs, which is the safe side.
///
/// The table comes from a font file, so it is untrusted: every read is checked against the table's end, nested
/// lookups are followed at most four deep, and at most a million rules are read; past either limit a lookup is
/// unbounded. A class definition whose ranges aren't sorted can't be read. Not thread-safe.
class LookupSpans {
public:
	static constexpr std::size_t unbounded = 255; ///< See the class.

	/// @brief A reader of a table.
	/// @param table The table's bytes; they outlive the reader.
	/// @param positioning Whether it is GPOS (else GSUB).
	/// @param candidates The glyphs the text may hold, sorted; they outlive the reader.
	LookupSpans(std::span<const std::uint8_t> table, bool positioning, std::span<const unsigned> candidates) noexcept
	    : table_(table), candidates_(candidates), positioning_(positioning) {}

	/// @brief The span of a lookup.
	/// @param lookup Its index in the table's lookup list.
	/// @return Its span, or nothing if the table can't be read there.
	[[nodiscard]] std::optional<std::size_t> span(unsigned lookup) { return lookupSpan(lookup, 0); }

private:
	/// @brief How a rule names the glyphs it matches.
	enum class Encoding {
		glyphs,    ///< Glyph ids (format 1).
		classes,   ///< Classes of the subtable's class definitions (format 2).
		coverages, ///< Offsets to coverage tables, one per glyph (format 3).
	};
	/// @brief Where a rule's glyphs are, and how to tell whether they can be candidates.
	struct Rules {
		std::size_t subtable = 0;             ///< The subtable, which coverage offsets are relative to.
		Encoding encoding = Encoding::glyphs; ///< How glyphs are named.
		bool chained = false;                 ///< Backtrack, input and lookahead (else input only).
		/// For Encoding::classes: the classes candidates have, in the backtrack, input and lookahead definitions.
		std::array<std::vector<std::size_t>, 3> liveClasses{};
	};

	/// @brief A 16-bit number of the table, or nothing past its end.
	/// @param at Its offset.
	[[nodiscard]] std::optional<std::size_t> u16(std::size_t at) const noexcept;
	/// @brief A 32-bit number of the table, or nothing past its end.
	/// @param at Its offset.
	[[nodiscard]] std::optional<std::size_t> u32(std::size_t at) const noexcept;
	/// @brief Whether a glyph is a candidate.
	/// @param glyph The glyph.
	[[nodiscard]] bool isCandidate(std::size_t glyph) const noexcept;

	/// @brief A lookup's span, remembered once read.
	/// @param lookup Its index.
	/// @param depth How many lookups apply it (0: a feature does).
	[[nodiscard]] std::optional<std::size_t> lookupSpan(unsigned lookup, int depth);
	/// @brief A lookup's span, read: the widest of its subtables'.
	/// @param lookup Its index.
	/// @param depth As lookupSpan().
	[[nodiscard]] std::optional<std::size_t> readLookup(unsigned lookup, int depth);
	/// @brief A subtable's span.
	/// @param type Its lookup type, extensions resolved.
	/// @param subtable Where it is.
	/// @param depth As lookupSpan().
	[[nodiscard]] std::optional<std::size_t> subtableSpan(std::size_t type, std::size_t subtable, int depth);
	/// @brief A GSUB subtable's span.
	/// @param type Its lookup type, extensions resolved.
	/// @param subtable Where it is.
	/// @param depth As lookupSpan().
	[[nodiscard]] std::optional<std::size_t> substitutionSpan(std::size_t type, std::size_t subtable, int depth);
	/// @brief The widest ligature of a ligature substitution whose other components are all candidates.
	/// @param subtable Where it is.
	[[nodiscard]] std::optional<std::size_t> ligatureSpan(std::size_t subtable);
	/// @brief A reverse chaining substitution's span: backtrack, one glyph and lookahead, or 0 if a coverage of them
	/// has no candidate.
	/// @param subtable Where it is.
	[[nodiscard]] std::optional<std::size_t> reverseChainSpan(std::size_t subtable);
	/// @brief The widest rule of a contextual subtable (GSUB 5, GPOS 7) or a chained one (GSUB 6, GPOS 8).
	/// @param subtable Where it is.
	/// @param chained Whether it is chained.
	/// @param depth As lookupSpan().
	[[nodiscard]] std::optional<std::size_t> contextSpan(std::size_t subtable, bool chained, int depth);
	/// @brief The widest rule of rule sets: a list of offsets to sets, each a list of offsets to rules.
	/// @param rules How the rules name glyphs.
	/// @param sets Where the count of sets is; their offsets follow it.
	/// @param depth As lookupSpan().
	[[nodiscard]] std::optional<std::size_t> ruleSetsSpan(const Rules& rules, std::size_t sets, int depth);
	/// @brief What a rule matches, read.
	struct RuleShape {
		std::size_t glyphs = 0;  ///< Backtrack, input and lookahead glyphs; unbounded for a rule with no input.
		std::size_t records = 0; ///< Where its lookup records are.
		std::size_t lookups = 0; ///< How many there are.
		bool live = true;        ///< Whether every glyph it names (but the first) can be a candidate.
	};

	/// @brief Reads a chained rule (formats 1 and 2, or a format 3 subtable after its format).
	/// @param rules How the rule names glyphs.
	/// @param rule Where its backtrack count is.
	/// @return What it matches, or nothing if it can't be read.
	[[nodiscard]] std::optional<RuleShape> chainedRule(const Rules& rules, std::size_t rule);
	/// @brief Reads a contextual rule that isn't chained (formats 1 and 2, or a format 3 subtable after its format).
	/// @param rules How the rule names glyphs.
	/// @param rule Where its glyph count is.
	/// @return What it matches, or nothing if it can't be read.
	[[nodiscard]] std::optional<RuleShape> contextRule(const Rules& rules, std::size_t rule);
	/// @brief A rule's span, or 0 if one of its glyphs (but the first) can't be a candidate.
	/// @param rules How the rule names glyphs.
	/// @param rule Where it is: for a chained rule, its backtrack count; else its glyph count.
	/// @param depth As lookupSpan().
	[[nodiscard]] std::optional<std::size_t> ruleSpan(const Rules& rules, std::size_t rule, int depth);
	/// @brief Whether every glyph of a sequence can be a candidate.
	/// @param rules How the sequence names glyphs.
	/// @param sequence Where it is: one 16-bit value per glyph.
	/// @param count How many glyphs.
	/// @param part 0 backtrack, 1 input, 2 lookahead: which class definition classes refer to.
	/// @return Whether they can, or nothing if it can't be read.
	[[nodiscard]] std::optional<bool> sequenceLive(const Rules& rules, std::size_t sequence, std::size_t count,
	                                               std::size_t part);
	/// @brief Whether a coverage table holds a candidate.
	/// @param coverage Where it is.
	/// @return Whether it does, or nothing if it can't be read.
	[[nodiscard]] std::optional<bool> coverageLive(std::size_t coverage);
	/// @brief The classes candidates have in a class definition, sorted (0 for those it doesn't list).
	/// @param classes Where it is.
	/// @return The classes, or nothing if it can't be read.
	[[nodiscard]] std::optional<std::vector<std::size_t>> classesOf(std::size_t classes);
	/// @brief The class of each candidate in a class definition of format 1 (a first glyph and a class per glyph).
	/// @param classes Where it is.
	/// @return The classes, in the candidates' order, or nothing if it can't be read.
	[[nodiscard]] std::optional<std::vector<std::size_t>> listedClasses(std::size_t classes) const;
	/// @brief The class of each candidate in a class definition of format 2 (sorted ranges).
	/// @param classes Where it is.
	/// @return The classes, in the candidates' order, or nothing if it can't be read or its ranges aren't sorted.
	[[nodiscard]] std::optional<std::vector<std::size_t>> rangedClasses(std::size_t classes) const;
	/// @brief A rule's span: its glyphs, widened by the widest lookup it applies inside them.
	/// @param glyphs How many glyphs the rule matches.
	/// @param records Where its lookup records are (sequence index, lookup index: four bytes each).
	/// @param count How many there are.
	/// @param depth As lookupSpan().
	[[nodiscard]] std::optional<std::size_t> withNested(std::size_t glyphs, std::size_t records, std::size_t count,
	                                                    int depth);

	std::span<const std::uint8_t> table_;                            ///< The table.
	std::span<const unsigned> candidates_;                           ///< See the constructor.
	bool positioning_;                                               ///< Whether it is GPOS.
	std::size_t rulesRead_ = 0;                                      ///< Rules and ligatures read so far.
	std::unordered_map<unsigned, std::optional<std::size_t>> spans_; ///< By lookup, read so far.
};

} // namespace affresco::detail
