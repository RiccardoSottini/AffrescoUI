/// @file
/// @brief detail::PlainText: which characters and pairs of a face need no shaping, and words laid out from their
/// advances and kerning.

#include "text/PlainText.hpp"
#include "font/FontFace.hpp"
#include "font/LookupSpans.hpp"

#include <hb-aat.h>
#include <hb-ot.h>

#include <algorithm>
#include <iterator>
#include <cstddef>
#include <limits>
#include <memory>

namespace affresco::detail {

namespace {

/// @brief The features HarfBuzz applies to left-to-right text of the default shaper, the one Latin uses
/// (hb-ot-shape.cc: the common and horizontal features, and those of the direction).
constexpr std::array<hb_tag_t, 17> defaultFeatures{
    HB_TAG('a', 'b', 'v', 'm'), HB_TAG('b', 'l', 'w', 'm'), HB_TAG('c', 'c', 'm', 'p'), HB_TAG('l', 'o', 'c', 'l'),
    HB_TAG('m', 'a', 'r', 'k'), HB_TAG('m', 'k', 'm', 'k'), HB_TAG('r', 'l', 'i', 'g'), HB_TAG('r', 'v', 'r', 'n'),
    HB_TAG('c', 'a', 'l', 't'), HB_TAG('c', 'l', 'i', 'g'), HB_TAG('c', 'u', 'r', 's'), HB_TAG('d', 'i', 's', 't'),
    HB_TAG('k', 'e', 'r', 'n'), HB_TAG('l', 'i', 'g', 'a'), HB_TAG('r', 'c', 'l', 't'), HB_TAG('l', 't', 'r', 'a'),
    HB_TAG('l', 't', 'r', 'm')};

/// @brief The scripts whose default language is read: Latin, and those HarfBuzz falls back to for text of no script.
constexpr std::array<hb_tag_t, 3> scripts{HB_TAG('l', 'a', 't', 'n'), HB_TAG('D', 'F', 'L', 'T'),
                                          HB_TAG('d', 'f', 'l', 't')};

/// @brief Frees a HarfBuzz blob or set, for std::unique_ptr.
struct HarfBuzzDeleter {
	/// @brief Frees a blob.
	void operator()(hb_blob_t* blob) const noexcept { hb_blob_destroy(blob); }
	/// @brief Frees a set.
	void operator()(hb_set_t* set) const noexcept { hb_set_destroy(set); }
};

/// @brief The lookups HarfBuzz may apply in the default language of the first of some scripts the table has, as it
/// picks one for a run.
/// @param face The face.
/// @param table GSUB or GPOS.
/// @param tags The scripts, in the order HarfBuzz tries them.
/// @param all Whether to take the lookups of every script in @p tags the table has, rather than of the first.
/// @return Their indices, sorted, each once.
std::vector<unsigned> defaultLookups(hb_face_t* face, hb_tag_t table, std::span<const hb_tag_t> tags, bool all) {
	std::vector<unsigned> lookups;
	const auto addFeature = [&](unsigned feature) {
		std::array<unsigned, 64> chunk{};
		for(unsigned start = 0;; start += static_cast<unsigned>(chunk.size())) {
			auto count = static_cast<unsigned>(chunk.size());
			hb_ot_layout_feature_get_lookups(face, table, feature, start, &count, chunk.data());
			lookups.insert(lookups.end(), chunk.begin(), chunk.begin() + count);
			if(count < chunk.size()) break;
		}
	};
	constexpr unsigned language = HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX;
	for(const hb_tag_t tag : tags) {
		unsigned script = 0, feature = 0;
		if(!hb_ot_layout_table_find_script(face, table, tag, &script)) continue;
		hb_tag_t required = 0;
		if(hb_ot_layout_language_get_required_feature(face, table, script, language, &feature, &required))
			addFeature(feature);
		for(const hb_tag_t name : defaultFeatures)
			if(hb_ot_layout_language_find_feature(face, table, script, language, name, &feature)) addFeature(feature);
		if(!all) break;
	}
	std::ranges::sort(lookups);
	const auto [first, last] = std::ranges::unique(lookups);
	lookups.erase(first, last);
	return lookups;
}

/// @brief Reads a table: adds the glyphs that its lookups wider than a pair read (shaping single characters and pairs
/// alone can't show what those do), and tells whether HarfBuzz does the same to plain text of no script whether it is
/// shaped as Latin or as no script.
/// @param face The face.
/// @param table GSUB or GPOS.
/// @param candidates The glyphs plain text may hold, sorted.
/// @param noScript Those of characters of no script (digits, punctuation, symbols), sorted: all a word of no script
/// holds.
/// @param glyphs Gets the glyphs.
/// @return Whether both scripts are alike: the lookups only one of them has do nothing to text of @p noScript. Nothing
/// if the table can't be read.
std::optional<bool> readTable(hb_face_t* face, hb_tag_t table, std::span<const unsigned> candidates,
                              std::span<const unsigned> noScript, hb_set_t* glyphs) {
	const std::unique_ptr<hb_blob_t, HarfBuzzDeleter> blob(hb_face_reference_table(face, table));
	unsigned length = 0;
	const auto* data = reinterpret_cast<const std::uint8_t*>(hb_blob_get_data(blob.get(), &length));
	const bool positioning = table == HB_OT_TAG_GPOS;
	LookupSpans spans({data, length}, positioning, candidates);
	for(const unsigned lookup : defaultLookups(face, table, scripts, true)) {
		const std::optional<std::size_t> span = spans.span(lookup);
		if(!span) return std::nullopt;
		if(*span > 2) hb_ot_layout_lookup_collect_glyphs(face, table, lookup, nullptr, glyphs, nullptr, nullptr);
	}

	// The scripts HarfBuzz picks for each, in the order it tries them
	constexpr std::array<hb_tag_t, 3> latin{scripts[0], scripts[1], scripts[2]}; // latn, DFLT, dflt
	constexpr std::array<hb_tag_t, 3> none{scripts[1], scripts[2], scripts[0]};  // DFLT, dflt, latn
	const std::vector<unsigned> forLatin = defaultLookups(face, table, latin, false);
	const std::vector<unsigned> forNone = defaultLookups(face, table, none, false);
	std::vector<unsigned> different;
	std::ranges::set_symmetric_difference(forLatin, forNone, std::back_inserter(different));
	LookupSpans noScriptSpans({data, length}, positioning, noScript);
	return std::ranges::all_of(different, [&](unsigned lookup) { return noScriptSpans.span(lookup) == 0u; });
}

/// @brief Whether HarfBuzz shapes a face its own way, which the tables read here don't describe: AAT tables,
/// variations, or a legacy kern table when there is no GPOS.
/// @param face The face.
bool shapedOtherwise(hb_face_t* face) { // NOLINT(misc-const-correctness): HarfBuzz takes faces as non-const
	if(hb_aat_layout_has_substitution(face) || hb_aat_layout_has_positioning(face) ||
	   hb_aat_layout_has_tracking(face) || hb_ot_var_has_data(face))
		return true;
	const std::unique_ptr<hb_blob_t, HarfBuzzDeleter> kern(hb_face_reference_table(face, HB_TAG('k', 'e', 'r', 'n')));
	return !hb_ot_layout_has_positioning(face) && hb_blob_get_length(kern.get()) > 0;
}

} // namespace

PlainText::PlainText(const FontFace& face) : face_(face), scale_(face.scale()) {
	hb_font_t* font = face.shaper();
	hb_face_t* shapingFace = hb_font_get_face(font);
	hb_unicode_funcs_t* unicode = hb_unicode_funcs_get_default();
	for(std::size_t i = 0; i < characterCount; ++i) {
		const char32_t codepoint = firstCharacter + static_cast<char32_t>(i);
		Character& character = characters_[i];
		character.glyph = face.glyphIndex(codepoint);
		advances_[i] = hb_font_get_glyph_h_advance(font, character.glyph);
		character.latin = hb_unicode_script(unicode, codepoint) == HB_SCRIPT_LATIN;
		const auto glyphClass = hb_ot_layout_get_glyph_class(shapingFace, character.glyph);
		const bool control = codepoint >= 0x7F && codepoint <= 0x9F, softHyphen = codepoint == 0xAD; // hidden
		character.plain =
		    character.glyph != 0 && !control && !softHyphen &&
		    (glyphClass == HB_OT_LAYOUT_GLYPH_CLASS_UNCLASSIFIED || glyphClass == HB_OT_LAYOUT_GLYPH_CLASS_BASE_GLYPH);
	}
	if(!readLookups())
		for(Character& character : characters_) character.plain = false;
	enabled_ = plainCharacters() > 0;
}

bool PlainText::readLookups() {
	hb_face_t* face = hb_font_get_face(face_.shaper());
	if(shapedOtherwise(face)) return false;

	std::vector<unsigned> candidates, noScript;
	for(const Character& character : characters_) {
		if(!character.plain) continue;
		candidates.push_back(character.glyph);
		if(!character.latin) noScript.push_back(character.glyph);
	}
	std::ranges::sort(candidates);
	std::ranges::sort(noScript);

	const std::unique_ptr<hb_set_t, HarfBuzzDeleter> wide(hb_set_create());
	if(!hb_set_allocation_successful(wide.get())) return false;
	const std::optional<bool> substitutionsAlike = readTable(face, HB_OT_TAG_GSUB, candidates, noScript, wide.get());
	const std::optional<bool> positionsAlike = readTable(face, HB_OT_TAG_GPOS, candidates, noScript, wide.get());
	if(!substitutionsAlike || !positionsAlike) return false;
	for(Character& character : characters_)
		if(hb_set_has(wide.get(), character.glyph)) character.plain = false;
	scriptsAlike_ = substitutionsAlike.value_or(false) && positionsAlike.value_or(false); // both read, checked above
	return true;
}

// The same operations, in the same order, as Shaping.cpp on HarfBuzz's positions: each word's advances added from
// zero, then the word and each space to the pen. This loop is the hot path of measuring text
std::size_t PlainText::layOut(std::u32string_view text, std::span<float> advances, float spaceAdvance, float& pen,
                              hb_buffer_t* buffer) {
	if(!enabled_) return 0;
	constexpr char32_t space = firstCharacter - 1; // column 0
	const std::size_t size = text.size();
	std::size_t laidOut = 0; // through the last whole word or space
	float linePen = pen;
	while(laidOut < size) {
		std::size_t column = text[laidOut] - space; // below a space wraps around to large values
		if(column == 0) {
			linePen += advances[laidOut++] = spaceAdvance;
			continue;
		}
		if(column >= columnCount) break;

		// A word: each character's entry is chosen by the one after it, which is the next one's row
		const std::size_t script = scriptOf(text.substr(laidOut));
		const float* entries = table(script);
		float word = 0.0f;
		std::size_t end = 0; // where the word ends, once all of it is laid out
		for(std::size_t i = laidOut; end == 0; ++i) {
			const std::size_t next = i + 1 < size ? text[i + 1] - space : 0;
			if(next >= columnCount) break;
			float advance = entries[column * columnCount + next];
			if(!(advance < unknown)) { // not filled yet, or NaN
				if(advance == unknown) advance = fill(script, column - 1, next, buffer);
				if(!(advance < unknown)) break;
			}
			advances[i] = advance;
			word += advance;
			if(next == 0) end = i + 1;
			column = next;
		}
		if(end == 0) break; // a character in it isn't plain
		linePen += word;
		laidOut = end;
	}
	pen = linePen;
	return laidOut;
}

std::size_t PlainText::scriptOf(std::u32string_view text) const noexcept {
	if(scriptsAlike_) return 1; // a word of no script comes out of the Latin table as it would of its own
	for(const char32_t codepoint : text) {
		if(codepoint == U' ') break;
		const std::size_t index = codepoint - firstCharacter;
		if(index < characterCount && characters_[index].latin) return 1;
	}
	return 0;
}

const float* PlainText::table(std::size_t script) {
	std::vector<float>& entries = tables_[script];
	if(entries.empty()) entries.assign(columnCount * columnCount, unknown); // row 0, a space, stays unused
	return entries.data();
}

// Before a space or the end, the character alone; before another one, the pair, if both are plain alone too
float PlainText::fill(std::size_t script, std::size_t character, std::size_t column, hb_buffer_t* buffer) {
	constexpr float notPlain = std::numeric_limits<float>::quiet_NaN();
	std::vector<float>& entries = tables_[script];
	float& entry = entries[(character + 1) * columnCount + column];
	if(column == 0) {
		const auto alone =
		    characters_[character].plain ? shape({character, character}, 1, script == 1, buffer) : std::nullopt;
		return entry = alone == 0 ? static_cast<float>(advances_[character]) * scale_ : notPlain;
	}
	const std::size_t second = column - 1;
	for(const std::size_t each : {character, second}) {
		const float alone = entries[(each + 1) * columnCount];
		if(!((alone == unknown ? fill(script, each, 0, buffer) : alone) < unknown)) return entry = notPlain;
	}
	const auto kerning = shape({character, second}, 2, script == 1, buffer);
	return entry = kerning ? static_cast<float>(advances_[character] + *kerning) * scale_ : notPlain;
}

std::optional<std::int32_t> PlainText::shape(std::array<std::size_t, 2> indices, unsigned count, bool latin,
                                             hb_buffer_t* buffer) const {
	std::array<std::uint32_t, 2> text{};
	for(unsigned i = 0; i < count; ++i) text[i] = static_cast<std::uint32_t>(firstCharacter + indices[i]);
	hb_buffer_clear_contents(buffer);
	hb_buffer_add_utf32(buffer, text.data(), static_cast<int>(count), 0, static_cast<int>(count));
	hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
	hb_buffer_set_script(buffer, latin ? HB_SCRIPT_LATIN : HB_SCRIPT_COMMON); // as Shaping.cpp gives a word's run
	hb_shape(face_.shaper(), buffer, nullptr, 0);

	// Unchanged but for the first glyph's advance: the same glyphs, one cluster each, nothing moved
	unsigned glyphs = 0;
	const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyphs);
	const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, nullptr);
	if(glyphs != count) return std::nullopt;
	for(unsigned i = 0; i < count; ++i) {
		const hb_glyph_position_t& position = positions[i];
		if(infos[i].codepoint != characters_[indices[i]].glyph || infos[i].cluster != i || position.x_offset != 0 ||
		   position.y_offset != 0 || position.y_advance != 0 || (i > 0 && position.x_advance != advances_[indices[i]]))
			return std::nullopt;
	}
	return positions[0].x_advance - advances_[indices[0]];
}

std::size_t PlainText::plainCharacters() const noexcept {
	return static_cast<std::size_t>(std::ranges::count(characters_, true, &Character::plain));
}

std::size_t PlainText::bytes() const noexcept {
	return sizeof(PlainText) + (tables_[0].capacity() + tables_[1].capacity()) * sizeof(float);
}

} // namespace affresco::detail
