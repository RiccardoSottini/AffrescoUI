#pragma once

/// @file
/// @brief detail::FontFile, FontFiles and FontFace: font files read once and shared, and one file at one size, opened
/// with FreeType (rasterising) and HarfBuzz (shaping).

#include "affresco/render/Font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::detail {

/// @brief Frees FreeType and HarfBuzz objects, for std::unique_ptr.
struct TextLibraryDeleter {
	/// @brief Frees a FreeType library.
	void operator()(FT_Library library) const noexcept { FT_Done_FreeType(library); }
	/// @brief Frees a FreeType face.
	void operator()(FT_Face face) const noexcept { FT_Done_Face(face); }
	/// @brief Frees a HarfBuzz font.
	void operator()(hb_font_t* font) const noexcept { hb_font_destroy(font); }
	/// @brief Frees a HarfBuzz buffer.
	void operator()(hb_buffer_t* buffer) const noexcept { hb_buffer_destroy(buffer); }
};

using FreeTypeLibrary = std::unique_ptr<FT_LibraryRec_, TextLibraryDeleter>; ///< Owns a FreeType library.
using HarfBuzzBuffer = std::unique_ptr<hb_buffer_t, TextLibraryDeleter>;     ///< Owns a HarfBuzz buffer.

/// @brief Starts a FreeType library: one per font, so fonts share no FreeType state.
/// @return The library.
/// @throws std::runtime_error If FreeType can't start (out of memory).
[[nodiscard]] FreeTypeLibrary startFreeType();

/// @brief The bytes of a font file. FreeType and HarfBuzz point into them, so they never change once read.
struct FontFile {
	std::vector<unsigned char> bytes; ///< The whole file.
};

/// @brief Reads font files, and shares a file's bytes between the fonts that use it at the same time (one file at
/// several sizes, or the same fallback for several fonts).
///
/// It doesn't own the files: each lives as long as a font that uses it. Main thread only, like the fonts.
class FontFiles {
public:
	/// @brief The largest font file read, in bytes. Large CJK collections are about 100 MiB.
	static constexpr std::size_t largestFile = std::size_t{256} << 20;

	/// @brief Reads a file, or shares it if a font uses it already.
	/// @param path The file: a regular file of at most largestFile bytes.
	/// @return Its bytes.
	/// @throws std::runtime_error If it isn't a regular file, is larger, or can't be read.
	[[nodiscard]] std::shared_ptr<const FontFile> read(const std::filesystem::path& path);
	/// @brief The bytes of the files fonts use now, each counted once.
	[[nodiscard]] std::size_t bytes() const noexcept;

private:
	std::map<std::filesystem::path, std::weak_ptr<const FontFile>> files_; ///< By path; expired ones are dropped.
};

/// @brief One font file opened at one pixel height: what a Font asks for its glyphs and shapes runs of text with.
///
/// Glyphs are rasterised unhinted, so their shapes and advances follow the outlines exactly (as HarfBuzz positions
/// them) at any size.
class FontFace {
public:
	/// @brief A rendered glyph: FreeType's bitmap, valid until the face renders another one.
	struct Bitmap {
		const FT_Bitmap* pixels; ///< Grey coverage, or premultiplied BGRA for colour glyphs.
		int left;                ///< Its first column, from the pen position.
		int top;                 ///< Its first row, upwards from the baseline.
		bool color;              ///< Whether it is BGRA.
	};

	/// @brief What a face's size measures.
	enum class SizeOf {
		height, ///< From the top of its tallest glyph to the bottom of its deepest (a font's own size).
		em,     ///< Its em square (a fallback: the same em as the font's, so its text looks as large).
	};

	/// The largest size, in pixels. A document can ask for any size, but no glyph larger than an atlas page
	/// (GlyphAtlas::largestPage) can be drawn, and much larger sizes don't fit the integers FreeType and the atlas use.
	static constexpr float largestSize = 4096.0f;

	/// @brief Opens a file at a size.
	/// @param library The FreeType library; it outlives the face.
	/// @param file The file's bytes, kept alive by the face.
	/// @param size The size, in pixels: more than 0, at most largestSize.
	/// @param sizeOf What @p size measures.
	/// @param path Its path, for errors.
	/// @param style Bold or italic, synthesised from its outlines (not its colour glyphs).
	/// @throws std::runtime_error If it isn't a usable scalable font, or the size is out of range.
	FontFace(FT_Library library, std::shared_ptr<const FontFile> file, float size, SizeOf sizeOf,
	         const std::filesystem::path& path, FontStyle style = {});

	/// @brief Its glyph for a character.
	/// @param codepoint The character.
	/// @return The glyph's index, or 0 if it has none.
	[[nodiscard]] unsigned glyphIndex(char32_t codepoint) const noexcept;
	/// @brief The character of the glyph a name names (an icon font's "search", from its PostScript glyph names).
	/// @param name The glyph's name.
	/// @return The first character mapped to that glyph, or 0 if the face has no such name or no character for it.
	[[nodiscard]] char32_t codepointNamed(const std::string& name) const noexcept;
	/// @brief Whether it has colour glyphs (a COLR table, as colour emoji fonts do).
	[[nodiscard]] bool hasColor() const noexcept { return color_; }
	/// @brief Pixels per font unit.
	[[nodiscard]] float scale() const noexcept { return scale_; }
	/// @brief How large its em square is, in pixels.
	[[nodiscard]] float emPixels() const noexcept;
	/// @brief Distance from the baseline to the top of the tallest glyph, in pixels.
	[[nodiscard]] float ascent() const noexcept;
	/// @brief Distance from one baseline to the next, in pixels.
	[[nodiscard]] float lineHeight() const noexcept;
	/// @brief A glyph's advance before shaping, in pixels.
	/// @param glyph The glyph's index.
	[[nodiscard]] float advance(unsigned glyph) const noexcept;
	/// @brief The HarfBuzz font to shape with: positions come in font units (multiply by scale()).
	[[nodiscard]] hb_font_t* shaper() const noexcept { return shaper_.get(); }
	/// @brief Renders a glyph.
	/// @param glyph The glyph's index.
	/// @return Its bitmap, or nothing if it can't be rendered (a broken outline, out of memory).
	[[nodiscard]] std::optional<Bitmap> render(unsigned glyph) const noexcept;

private:
	/// @brief Thickens or slants a glyph's outline in place, as its style asks.
	/// @param outline The outline, in 26.6 pixels.
	void synthesize(FT_Outline& outline) const noexcept;

	std::shared_ptr<const FontFile> file_; ///< Declared first, so the face and the font are freed before it.
	std::unique_ptr<FT_FaceRec_, TextLibraryDeleter> face_; ///< FreeType's face, pointing into file_.
	std::unique_ptr<hb_font_t, TextLibraryDeleter> shaper_; ///< HarfBuzz's font, pointing into file_.
	float scale_ = 0.0f;                                    ///< See scale().
	bool color_ = false;                                    ///< See hasColor().
	FontStyle style_;                                       ///< Synthesised when a glyph is rendered.
};

} // namespace affresco::detail
