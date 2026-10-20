/// @file
/// @brief Font::Data's shaping: text split into runs of one face and one script, each shaped by HarfBuzz, cached
/// by word and by line.

#include "text/FontData.hpp"

#include <algorithm>
#include <cstring>
#include <span>

namespace affresco {

namespace {

constexpr char32_t variationSelectorEmoji = 0xFE0F; ///< Asks for the colour (emoji) form of the character before.

/// @brief Whether a script is one of its own, rather than shared (Common: digits, punctuation, emoji) or taken from
/// the character before (Inherited: marks).
/// @param script The script.
bool isOwnScript(hb_script_t script) noexcept {
	return script != HB_SCRIPT_COMMON && script != HB_SCRIPT_INHERITED && script != HB_SCRIPT_UNKNOWN;
}

/// @brief Whether a character belongs to the cluster before it, whichever face has it: marks, joiners and variation
/// selectors.
/// @param unicode HarfBuzz's Unicode data.
/// @param codepoint The character.
bool joinsPrevious(hb_unicode_funcs_t* unicode, char32_t codepoint) noexcept {
	switch(hb_unicode_general_category(unicode, codepoint)) {
	case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
	case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
	case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
		return true;
	case HB_UNICODE_GENERAL_CATEGORY_FORMAT:
		return codepoint == 0x200C || codepoint == 0x200D; // zero-width non-joiner and joiner
	default:
		return false;
	}
}

/// @brief Whether a text has a character of a right-to-left script.
/// @param unicode HarfBuzz's Unicode data.
/// @param text The text.
bool hasRightToLeft(hb_unicode_funcs_t* unicode, std::u32string_view text) noexcept {
	// Hebrew is the first right-to-left block: anything before it is left to right or neutral
	return std::ranges::any_of(text, [unicode](char32_t codepoint) {
		return codepoint >= 0x0590 &&
		       hb_script_get_horizontal_direction(hb_unicode_script(unicode, codepoint)) == HB_DIRECTION_RTL;
	});
}

/// @brief Empties shaped text and sizes it for a text.
/// @param shaped The shaped text.
/// @param length The text's code points.
void reset(Font::ShapedText& shaped, std::size_t length) {
	shaped.glyphs.clear();
	shaped.advances.assign(length, 0.0f);
	shaped.carets.assign(length + 1, 0.0f);
	shaped.width = 0.0f;
}

} // namespace

const Font::ShapedText& Font::Data::shape(std::u32string_view text) {
	if(const ShapedText* hit = lines.find(text)) return *hit;

	const bool keep = lines.keeps(text.size());
	ShapedText kept;
	ShapedText& shaped = keep ? kept : uncached;
	// A right-to-left run must be shaped whole, so its words come out in its own order; anything else word by word
	if(hasRightToLeft(hb_unicode_funcs_get_default(), text)) shapeRuns(text, shaped);
	else shapeWords(text, shaped);
	return keep ? lines.insert(text, std::move(kept)) : uncached;
}

const Font::Data::Measured& Font::Data::measure(std::u32string_view text) {
	// Compared as bytes: a char32_t string compares character by character, which wrap() would pay for every line
	if(text.size() == measured.text.size() &&
	   std::memcmp(text.data(), measured.text.data(), text.size() * sizeof(char32_t)) == 0)
		return measured;

	measured.text.assign(text); // reuses its storage: no allocation once it is large enough
	measured.advances.resize(text.size());
	// Most lines are plain from end to end: no cache, no shaping, no allocation
	float width = 0.0f;
	std::size_t start = plainText().layOut(text, measured.advances, spaceAdvance, width, buffer.get());
	if(start == text.size()) {
		measured.width = width;
	} else if(const ShapedText* line = lines.find(text)) {
		measured.advances.assign(line->advances.begin(), line->advances.end());
		measured.width = line->width;
	} else if(hasRightToLeft(hb_unicode_funcs_get_default(), text)) {
		const ShapedText& shaped = shape(text);
		measured.advances.assign(shaped.advances.begin(), shaped.advances.end());
		measured.width = shaped.width;
	} else { // as shapeWords(), without the glyphs: a new line of known words takes no shaping and no allocation
		while(start < text.size()) {
			if(text[start] == U' ') {
				width += measured.advances[start++] = spaceAdvance;
				continue;
			}
			const std::size_t end = std::min(text.find(U' ', start), text.size());
			const ShapedText& shaped = word(text.substr(start, end - start));
			std::ranges::copy(shaped.advances, measured.advances.begin() + static_cast<std::ptrdiff_t>(start));
			width += shaped.width;
			const std::span<float> rest(measured.advances.data() + end, text.size() - end);
			start = end + plainText().layOut(text.substr(end), rest, spaceAdvance, width, buffer.get());
		}
		measured.width = width;
	}
	return measured;
}

detail::PlainText& Font::Data::plainText() {
	if(!plain) plain.emplace(faces.front());
	return *plain;
}

// Walks what PlainText laid out as it did, so glyph positions and carets are those shapeWords() gives HarfBuzz's
std::size_t Font::Data::layOutPlainWords(std::u32string_view text, std::size_t start, float& pen, ShapedText& shaped) {
	detail::PlainText& fast = plainText();
	const std::span<float> advances(shaped.advances.data() + start, text.size() - start);
	float linePen = pen;
	const std::size_t end = start + fast.layOut(text.substr(start), advances, spaceAdvance, pen, buffer.get());

	float wordPen = 0.0f;
	for(std::size_t i = start; i < end; ++i) {
		if(text[i] == U' ') {
			linePen += wordPen;
			wordPen = 0.0f;
			shaped.carets[i] = linePen;
			linePen += spaceAdvance;
			continue;
		}
		const Glyph& found = glyph(0, fast.glyphIndex(text[i]));
		if(found.quad.right > found.quad.left) shaped.glyphs.push_back({&found, wordPen + linePen, -0.0f});
		shaped.carets[i] = linePen + wordPen;
		wordPen += shaped.advances[i];
	}
	return end;
}

const Font::ShapedText& Font::Data::word(std::u32string_view text) {
	if(const ShapedText* found = words.find(text)) return *found;
	if(!words.keeps(text.size())) {
		shapeRuns(text, longWord);
		return longWord;
	}
	ShapedText fresh;
	shapeRuns(text, fresh);
	return words.insert(text, std::move(fresh));
}

// Spaces separate words: kerning across a space is lost, which browsers accept too, and a line of known words then
// takes no shaping at all
void Font::Data::shapeWords(std::u32string_view text, ShapedText& shaped) {
	reset(shaped, text.size());

	float pen = 0.0f;
	std::size_t start = 0;
	while(start < text.size()) {
		if(text[start] == U' ') {
			shaped.advances[start] = spaceAdvance;
			shaped.carets[start] = pen;
			pen += spaceAdvance;
			++start;
			continue;
		}

		if(const std::size_t plainEnd = layOutPlainWords(text, start, pen, shaped); plainEnd > start) {
			start = plainEnd;
			continue;
		}
		const std::size_t end = std::min(text.find(U' ', start), text.size());
		const ShapedText& found = word(text.substr(start, end - start));
		for(const ShapedGlyph& glyph : found.glyphs) shaped.glyphs.push_back({glyph.glyph, glyph.x + pen, glyph.y});
		std::ranges::copy(found.advances, shaped.advances.begin() + static_cast<std::ptrdiff_t>(start));
		for(std::size_t i = start; i < end; ++i) shaped.carets[i] = pen + found.carets[i - start];
		pen += found.width;
		start = end;
	}

	shaped.carets[text.size()] = pen;
	shaped.width = pen;
}

void Font::Data::shapeRuns(std::u32string_view text, ShapedText& shaped) {
	reset(shaped, text.size());
	codepoints.assign(text.begin(), text.end());
	itemize(text);

	// Left to right, except that a sequence of right-to-left runs is laid out from its last run to its first
	float pen = 0.0f;
	for(std::size_t first = 0; first < runs.size();) {
		std::size_t end = first + 1;
		while(runs[first].rightToLeft && end < runs.size() && runs[end].rightToLeft) ++end;
		for(std::size_t run = end; run > first; --run)
			shapeRun(runs[runs[first].rightToLeft ? run - 1 : end - run + first], pen, shaped);
		first = end;
	}
	shaped.width = pen;
}

void Font::Data::resolveDirections() noexcept {
	for(Run& run : runs)
		run.rightToLeft = isOwnScript(run.script) && hb_script_get_horizontal_direction(run.script) == HB_DIRECTION_RTL;

	// A neutral run takes the direction of the strong runs on both sides when they agree. Neutral runs in a row share
	// both sides, so each is visited once: a text that alternates faces can make one run per character
	bool before = false; // the direction of the last run with a script of its own
	for(std::size_t i = 0; i < runs.size();) {
		if(isOwnScript(runs[i].script)) {
			before = runs[i++].rightToLeft;
			continue;
		}
		std::size_t after = i; // the next run with a script of its own
		while(after < runs.size() && !isOwnScript(runs[after].script)) ++after;
		const bool rightToLeft = before && after < runs.size() && runs[after].rightToLeft;
		for(; i < after; ++i) runs[i].rightToLeft = rightToLeft;
	}
}

void Font::Data::itemize(std::u32string_view text) {
	hb_unicode_funcs_t* unicode = hb_unicode_funcs_get_default();
	runs.clear();

	for(std::size_t i = 0; i < text.size(); ++i) {
		const hb_script_t script = hb_unicode_script(unicode, text[i]);
		const bool wantsColor = i + 1 < text.size() && text[i + 1] == variationSelectorEmoji;
		Run* last = runs.empty() ? nullptr : &runs.back();
		const std::size_t face = faceAt(text[i], script, wantsColor, last);

		if(last && face == last->face &&
		   (!isOwnScript(script) || !isOwnScript(last->script) || script == last->script)) {
			if(isOwnScript(script)) last->script = script;
			last->end = i + 1;
		} else {
			runs.push_back({i, i + 1, face, isOwnScript(script) ? script : HB_SCRIPT_COMMON});
		}
	}
	resolveDirections();
}

std::size_t Font::Data::faceAt(char32_t codepoint, hb_script_t script, bool wantsColor, const Run* last) const {
	if(last) {
		if(joinsPrevious(hb_unicode_funcs_get_default(), codepoint)) return last->face;
		const detail::FontFace& current = faces[last->face];
		if(!isOwnScript(script) && current.glyphIndex(codepoint) != 0 && (!wantsColor || current.hasColor()))
			return last->face;
	}
	if(wantsColor)
		if(const auto face = colorFaceFor(codepoint)) return *face;
	if(const auto face = faceFor(codepoint)) return *face;
	return last ? last->face : 0; // no face has it: '?', in the run it is in
}

void Font::Data::shapeRun(const Run& run, float& pen, ShapedText& shaped) {
	const detail::FontFace& face = faces[run.face];
	hb_buffer_t* hb = buffer.get();
	hb_buffer_clear_contents(hb);
	// The whole text goes in, so the run is shaped with its neighbours as context (e.g. Arabic joining)
	hb_buffer_add_utf32(hb, codepoints.data(), static_cast<int>(codepoints.size()), static_cast<unsigned>(run.start),
	                    static_cast<int>(run.end - run.start));
	hb_buffer_set_direction(hb, run.rightToLeft ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
	hb_buffer_set_script(hb, run.script);
	// No language: the default forms, the same on every machine (guessing would read the process's locale)
	hb_shape(face.shaper(), hb, nullptr, 0);

	unsigned count = 0;
	const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(hb, &count);
	const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(hb, nullptr);
	const float scale = face.scale(), runStart = pen;

	// Glyphs in drawing order; a glyph the face lacks (index 0) draws as '?'
	clusters.clear();
	for(unsigned i = 0; i < count; ++i) {
		const bool missing = infos[i].codepoint == 0;
		const Glyph& glyph = missing ? *question : this->glyph(run.face, infos[i].codepoint);
		const float advance = missing ? question->advance : static_cast<float>(positions[i].x_advance) * scale;
		if(glyph.quad.right > glyph.quad.left)
			shaped.glyphs.push_back({&glyph, pen + static_cast<float>(positions[i].x_offset) * scale,
			                         -static_cast<float>(positions[i].y_offset) * scale});
		if(clusters.empty() || clusters.back().start != infos[i].cluster)
			clusters.push_back({infos[i].cluster, pen, 0.0f});
		clusters.back().width += advance;
		pen += advance;
	}

	spreadClusters(run, runStart, pen, shaped);
}

// Each cluster's width goes to its first code point; carets are spread over it, in the run's direction
void Font::Data::spreadClusters(const Run& run, float runStart, float runEnd, ShapedText& shaped) {
	if(run.rightToLeft) std::ranges::reverse(clusters); // drawn right to left: now in the text's order
	for(std::size_t c = 0; c < clusters.size(); ++c) {
		const Cluster& cluster = clusters[c];
		const std::size_t first = cluster.start;
		const std::size_t last = c + 1 < clusters.size() ? clusters[c + 1].start : run.end;
		if(last <= first) continue;
		shaped.advances[first] += cluster.width;
		const auto length = static_cast<float>(last - first);
		for(std::size_t i = first; i < last; ++i) {
			const float share = cluster.width * static_cast<float>(i - first) / length;
			shaped.carets[i] = run.rightToLeft ? cluster.left + cluster.width - share : cluster.left + share;
		}
	}
	shaped.carets[run.end] = run.rightToLeft ? runStart : runEnd;
}

} // namespace affresco
