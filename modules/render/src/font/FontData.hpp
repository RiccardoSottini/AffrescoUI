#pragma once

/// @file
/// @brief Font::Data: what a font holds besides its Latin-1 table (files, atlas, glyphs, shaping caches), shared by
/// Font.cpp (loading, glyphs, atlas) and Shaping.cpp (shaping).

#include "affresco/render/Font.hpp"

#include "font/FontFace.hpp"
#include "font/GlyphAtlas.hpp"
#include "text/PlainText.hpp"
#include "text/ShapedTextCache.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace affresco {

/// @brief Everything that grows as text is shaped and glyphs are rasterised.
struct Font::Data {
	/// @brief A run of text that one face shapes in one script and direction.
	struct Run {
		std::size_t start = 0;                 ///< Its first code point.
		std::size_t end = 0;                   ///< One past its last.
		std::size_t face = 0;                  ///< The face that draws it.
		hb_script_t script = HB_SCRIPT_COMMON; ///< Its script (Common if it has none of its own, e.g. digits only).
		bool rightToLeft = false;              ///< Whether it is laid out right to left.
		bool number = false;                   ///< Digits and the separators between them: always left to right.
		std::uint8_t level = 0;                ///< Its bidirectional level: odd is right to left (UAX #9).

		/// @brief Whether it sets the direction of the neutral runs beside it: a script of its own, and no number.
		[[nodiscard]] bool isStrong() const noexcept;
		/// @brief Whether its script is written right to left.
		[[nodiscard]] bool hasRightToLeftScript() const noexcept;
	};

	/// @brief Glyphs of one cluster, together: what carets and widths are spread over.
	struct Cluster {
		std::uint32_t start; ///< Its first code point.
		float left;          ///< Where it starts, from the text's start.
		float width;         ///< Its glyphs' advances.
	};

	detail::FreeTypeLibrary library;                 ///< Declared first: the faces are freed before it.
	std::vector<detail::FontFace> faces;             ///< The font first, then its fallbacks.
	detail::GlyphAtlas atlas;                        ///< Made by bakeLatin1(), at the size Latin-1 fits.
	std::unordered_map<std::uint64_t, Glyph> glyphs; ///< By face and glyph index: rasterised so far. Nodes never move.
	std::unordered_map<char32_t, const Glyph*> nominal;    ///< Unshaped glyphs beyond Latin-1, by code point.
	std::unordered_map<std::string, char32_t> namedGlyphs; ///< codepointNamed(), by name: at most namedGlyphsKept.
	static constexpr std::size_t namedGlyphsKept = 1024;   ///< Names of glyphs remembered; more empties the map.
	const Glyph* question = nullptr; ///< '?': what a missing character, or one past the atlas limit, draws.
	float spaceAdvance = 0.0f;       ///< How wide a space is, between words.

	detail::ShapedTextCache words{8192, 65536};  ///< Words: what new lines are made of (64 Ki code points).
	detail::ShapedTextCache lines{4096, 131072}; ///< Whole texts: what each frame draws again (128 Ki code points).
	ShapedText uncached;                         ///< A text too long to keep, until the next one.
	ShapedText longWord;                         ///< A word too long to keep, until the next one.
	/// @brief The text measure() and wrap() saw last, its advances and width: Label measures and wraps the same text in
	/// a row, and wrapping a new line needs no shaped glyphs, only these.
	struct Measured {
		std::u32string text;         ///< The text.
		std::vector<float> advances; ///< Per code point, as ShapedText::advances.
		float width = 0.0f;          ///< Their sum.
	} measured;
	/// @brief Words of the first face that need no shaping: made the first time text is measured or shaped.
	std::optional<detail::PlainText> plain;
	detail::HarfBuzzBuffer buffer;         ///< Reused for every run.
	std::vector<std::uint32_t> codepoints; ///< The text being shaped, as HarfBuzz takes it.
	std::vector<Run> runs;                 ///< The runs of the text being shaped.
	std::vector<Cluster> clusters;         ///< The clusters of the run being shaped.

	/// @brief Finds the first face that has a glyph for a character.
	/// @param codepoint The character.
	/// @return The face, or nothing.
	[[nodiscard]] std::optional<std::size_t> faceFor(char32_t codepoint) const noexcept;
	/// @brief Finds the first colour face that has a glyph for a character.
	/// @param codepoint The character.
	/// @return The face, or nothing.
	[[nodiscard]] std::optional<std::size_t> colorFaceFor(char32_t codepoint) const noexcept;
	/// @brief A face's glyph, rasterised into the atlas the first time.
	/// @param face The face.
	/// @param index Its glyph index.
	/// @return The glyph; '?' if it can't be rasterised or the atlas is full (remembered, so it isn't tried again).
	const Glyph& glyph(std::size_t face, unsigned index);
	/// @brief Rasterises a glyph into the atlas.
	/// @param face The face.
	/// @param index Its glyph index.
	/// @param mayAddPage Whether a page may be added for it.
	/// @return The glyph, or nothing if it can't be rendered or doesn't fit.
	[[nodiscard]] std::optional<Glyph> rasterise(std::size_t face, unsigned index, bool mayAddPage);

	/* Shaping (Shaping.cpp) */

	/// @brief The fast path for plain words, made on first use.
	/// @return It.
	detail::PlainText& plainText();
	/// @brief Lays out the plain words and spaces at a place of a text without shaping, into shaped text: their glyphs,
	/// advances and carets, up to the first word that isn't plain.
	/// @param text The whole text.
	/// @param start Where to start.
	/// @param pen Where it starts on the line; moved past what was laid out.
	/// @param shaped Where it goes: its advances and carets have one entry per code point of @p text (and one more).
	/// @return Where what was laid out ends: @p start if the first word isn't plain.
	std::size_t layOutPlainWords(std::u32string_view text, std::size_t start, float& pen, ShapedText& shaped);

	/// @brief Shapes a text, from the caches when they have it.
	/// @param text The text.
	/// @return As Font::shape().
	const ShapedText& shape(std::u32string_view text);
	/// @brief The advances and width of a text, without shaping it as a whole when its words will do.
	/// @param text The text.
	/// @return It in `measured`, valid until the next call.
	const Measured& measure(std::u32string_view text);
	/// @brief Shapes one word, from the word cache when it has it.
	/// @param text The word: no spaces.
	/// @return Its shaping, valid until the next call.
	const ShapedText& word(std::u32string_view text);
	/// @brief Shapes a text made only of left-to-right runs, word by word from the word cache.
	/// @param text The text.
	/// @param shaped Where it goes: emptied first.
	void shapeWords(std::u32string_view text, ShapedText& shaped);
	/// @brief Shapes a text run by run with HarfBuzz.
	/// @param text The text.
	/// @param shaped Where it goes: emptied first.
	void shapeRuns(std::u32string_view text, ShapedText& shaped);
	/// @brief Splits a text into runs of one face and one script, and gives each its direction.
	/// @param text The text.
	void itemize(std::u32string_view text);
	/// @brief The face a character of a text is drawn from.
	///
	/// It goes with the run before it when it belongs to it (a mark, a joiner) or is neutral and that face has it (a
	/// space, punctuation); else with the first face that has it, which is the font's coverage order. Before U+FE0F
	/// (the emoji form), colour faces come first.
	/// @param codepoint The character.
	/// @param script Its script.
	/// @param wantsColor Whether U+FE0F follows it.
	/// @param last The run before it, if any.
	/// @return The face.
	[[nodiscard]] std::size_t faceAt(char32_t codepoint, hb_script_t script, bool wantsColor, const Run* last) const;
	/// @brief Gives each run its bidirectional level and direction, after UAX #9 without explicit embeddings: the
	/// line runs the way of its first strong character (P2), numbers stay left to right (W7, I1, I2), and a run with no
	/// script of its own (spaces, punctuation) goes the way of the strong runs on both sides when they agree, else the
	/// line's way (N1, N2).
	void resolveDirections() noexcept;
	/// @brief Whether the line goes right to left: the way of its first strong run, else left to right (P2, P3).
	[[nodiscard]] bool lineIsRightToLeft() const noexcept;
	/// @brief Gives the strong runs and the numbers their level and the way each pulls the neutral runs beside it.
	/// @param base Whether the line goes right to left.
	void resolveStrongRuns(bool base) noexcept;
	/// @brief Gives the neutral runs their level, after resolveStrongRuns(): the way of the runs on both sides when
	/// they agree, else the line's (N1, N2).
	/// @param base Whether the line goes right to left.
	void resolveNeutralRuns(bool base) noexcept;
	/// @brief Splits the runs of a text that has right-to-left runs where numbers and spaces start and end.
	/// @param text The text the runs cover.
	void splitNumbers(std::u32string_view text);
	/// @brief Shapes one run with HarfBuzz and appends its glyphs, advances and carets.
	/// @param run The run.
	/// @param pen Where it starts; moved past it.
	/// @param shaped Where it goes: its advances and carets already have one entry per code point (and one more).
	/// @pre codepoints holds the whole text.
	void shapeRun(const Run& run, float& pen, ShapedText& shaped);
	/// @brief Gives the code points of a shaped run their advances and carets, from its clusters.
	/// @param run The run.
	/// @param runStart Where it starts.
	/// @param runEnd Where it ends.
	/// @param shaped Where they go.
	/// @pre clusters holds the run's clusters, in drawing order.
	void spreadClusters(const Run& run, float runStart, float runEnd, ShapedText& shaped);
};

} // namespace affresco
