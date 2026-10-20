#pragma once

/// @file
/// @brief detail::PlainText: words of one font that need no shaping, laid out from glyph advances and kerning pairs,
/// with exactly HarfBuzz's results.

#include <hb.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace affresco::detail {

class FontFace;

/// @brief Lays out words that need no shaping without HarfBuzz: most text in most apps (one face, Latin-1, no
/// ligature or contextual form that applies to it).
///
/// **Which words.** A word is plain when every character is a plain character of the font's first face, each one
/// alone and every pair of neighbours comes out of HarfBuzz unchanged but for kerning:
/// - a plain character is a printable Latin-1 character (U+0021..U+00FF, without the soft hyphen) that the face has,
///   whose glyph isn't a mark, and that no lookup wider than a pair may change. HarfBuzz's default features are read
///   from the default language of the Latin and default scripts (a union: more lookups only make fewer characters
///   plain), and LookupSpans tells how many neighbours each lookup reads: a lookup that reads three or more (a
///   ligature of three, a context of three) makes every glyph it reads not plain;
/// - every lookup left reads at most two neighbours, so HarfBuzz shaping each character alone and each pair alone shows
///   everything they do to a word. A character is plain alone when it comes out as its own glyph, unmoved; a pair when
///   it comes out as the two glyphs, unmoved, the first one's advance changed at most (pair kerning). Both are shaped
///   the first time they are met, per script (Latin, or none for words of digits and punctuation, which HarfBuzz
///   shapes with the default script).
/// A face with AAT tables, variations, or a legacy `kern` table HarfBuzz would use, or whose tables can't be read,
/// has no plain characters: all its text is shaped.
///
/// **Results.** A plain word's glyphs are the characters' own, and its advances the face's advances plus the pairs'
/// kerning, in font units, scaled and summed as Shaping.cpp does with HarfBuzz's positions: equal to the last bit
/// (tests/unit/plain_text.cpp compares both over the templates' text and generated words).
///
/// **Tables.** Per script, the advance of every plain character before every other one, in pixels, ready to add:
/// kerning included, and a column for a character before a space or the end of the text. An entry is filled the first
/// time it is met; one HarfBuzz doesn't leave alone holds NaN.
///
/// Main thread only, like its font. Memory: about 2 KiB for the characters, and 195 KiB per script once a word of
/// that script is laid out (one table for both when the lookups only one of the scripts has do nothing to words of no
/// script).
class PlainText {
public:
	/// @brief Reads which characters of a face are plain.
	/// @param face The font's first face; outlives this object.
	explicit PlainText(const FontFace& face);

	/// @brief Lays out the plain words of a text and the spaces between them, from its start up to the first word that
	/// isn't plain.
	///
	/// One pass, one table read per character: an entry is filled only if both of its characters are plain, alone and
	/// together. Widths add up as Shaping.cpp adds them: each word's advances from zero, then the word and each space
	/// to the pen.
	/// @param text The text.
	/// @param advances Gets each character's advance, in pixels; as long as @p text. Written to past what is laid out
	/// too.
	/// @param spaceAdvance A space's advance, in pixels.
	/// @param pen Where the text starts on its line; moved past what is laid out.
	/// @param buffer A HarfBuzz buffer to shape characters and pairs met for the first time with.
	/// @return How many characters were laid out: the whole text, or up to the start of the first word that isn't
	/// plain (0 when the fast path is off).
	[[nodiscard]] std::size_t layOut(std::u32string_view text, std::span<float> advances, float spaceAdvance,
	                                 float& pen, hb_buffer_t* buffer);
	/// @brief The glyph of a plain character.
	/// @param codepoint A character of a word layOut() accepted.
	[[nodiscard]] unsigned glyphIndex(char32_t codepoint) const noexcept {
		return characters_[codepoint - firstCharacter].glyph;
	}
	/// @brief How many characters are plain: 0 when the face has none.
	[[nodiscard]] std::size_t plainCharacters() const noexcept;
	/// @brief The bytes its tables hold.
	[[nodiscard]] std::size_t bytes() const noexcept;

	/// @brief Turns the fast path off (or on again), so that every word is shaped: for tests that compare both.
	/// @param enabled Whether plain words skip shaping.
	void setEnabled(bool enabled) noexcept { enabled_ = enabled && plainCharacters() > 0; }

private:
	static constexpr char32_t firstCharacter = 0x21; ///< '!': spaces are never inside a word.
	static constexpr char32_t lastCharacter = 0xFF;  ///< The end of Latin-1.
	static constexpr std::size_t characterCount = lastCharacter - firstCharacter + 1;
	static constexpr std::size_t columnCount = characterCount + 1; ///< A space or the end, then each character.
	static constexpr float unknown = std::numeric_limits<float>::infinity(); ///< An entry not filled yet.

	/// @brief What a character is, in the first face.
	struct Character {
		unsigned glyph = 0; ///< Its glyph.
		bool plain = false; ///< Whether it can be plain: what the face's lookups and classes say, before shaping.
		bool latin = false; ///< Whether its script is Latin (else it has none: digits, punctuation).
	};

	/// @brief Marks the characters no lookup of HarfBuzz's default features may change, beyond pairs, and tells
	/// whether the Latin and default scripts are alike for plain text.
	/// @return false if the face's tables can't be read: then nothing is plain.
	[[nodiscard]] bool readLookups();
	/// @brief The script whose table a word uses: Latin if it has a Latin character or both scripts are alike.
	/// @param text The text, starting with the word.
	/// @return It: 1 for Latin, 0 for none.
	[[nodiscard]] std::size_t scriptOf(std::u32string_view text) const noexcept;
	/// @brief A script's table, made on first use.
	/// @param script 1 for Latin, 0 for none.
	/// @return Its entries: by row (a space, then each character from '!'), then by the column of what follows.
	[[nodiscard]] const float* table(std::size_t script);
	/// @brief Fills an entry of a script's table, shaping with HarfBuzz.
	/// @param script 1 for Latin, 0 for none.
	/// @param character The character's index in characters_.
	/// @param column What follows it: 0 for a space or the end, else 1 plus the next character's index.
	/// @param buffer A HarfBuzz buffer.
	/// @return The entry: the character's advance in pixels, or NaN.
	float fill(std::size_t script, std::size_t character, std::size_t column, hb_buffer_t* buffer);
	/// @brief Shapes one character or a pair alone with HarfBuzz.
	/// @param indices The characters' indices in characters_.
	/// @param count 1 or 2.
	/// @param latin Whether to shape them as Latin.
	/// @param buffer A HarfBuzz buffer.
	/// @return The first glyph's extra advance in font units, or nothing if HarfBuzz did more.
	[[nodiscard]] std::optional<std::int32_t> shape(std::array<std::size_t, 2> indices, unsigned count, bool latin,
	                                                hb_buffer_t* buffer) const;

	const FontFace& face_;                                ///< The font's first face.
	std::array<Character, characterCount> characters_{};  ///< From '!' to U+00FF.
	std::array<std::int32_t, characterCount> advances_{}; ///< Their glyphs' advances, in font units.
	std::array<std::vector<float>, 2> tables_;            ///< None, then Latin: see the class.
	float scale_ = 0.0f;                                  ///< Pixels per font unit, as the face's.
	/// Whether HarfBuzz does the same to words of no script shaped as Latin: then every word uses the Latin tables.
	bool scriptsAlike_ = false;
	bool enabled_ = false; ///< See setEnabled().
};

} // namespace affresco::detail
