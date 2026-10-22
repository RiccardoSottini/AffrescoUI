#pragma once

/// @file
/// @brief detail::PdfFonts: the font files a PDF draws text with, subset to the glyphs it uses and embedded.

#include "PdfObjects.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace affresco::detail {

/// @brief The font files of a document, one embedded font each (whatever the sizes text uses them at): a CID font
/// whose ids are the file's glyph indices (Identity-H), with its widths and a ToUnicode map.
class PdfFonts {
public:
	/// @brief No fonts yet.
	/// @param objects Where the fonts are written.
	/// @param limit The most bytes of font files, after subsetting.
	PdfFonts(PdfObjects& objects, std::size_t limit);
	/// @brief Frees the HarfBuzz faces.
	~PdfFonts();
	PdfFonts(const PdfFonts&) = delete;            ///< Owns HarfBuzz faces.
	PdfFonts& operator=(const PdfFonts&) = delete; ///< Owns HarfBuzz faces.

	/// @brief A font file's resource name, the first time with a new embedded font.
	/// @param bytes The whole file, valid until finish().
	/// @return Its index, for name() and use().
	[[nodiscard]] std::size_t face(std::span<const unsigned char> bytes);
	/// @brief A face's resource name ("F1").
	/// @param face An index from face().
	[[nodiscard]] const std::string& name(std::size_t face) const noexcept;
	/// @brief A face's units per em: text at size S uses a font size of S.
	/// @param face An index from face().
	[[nodiscard]] unsigned unitsPerEm(std::size_t face) const noexcept;
	/// @brief Records that a glyph is drawn, so the subset keeps it.
	/// @param face An index from face().
	/// @param glyph Its index in the file.
	void use(std::size_t face, unsigned glyph);

	/// @brief Writes every font, subset.
	/// @param resources The /Font dictionary's entries are appended here ("/F1 5 0 R ...").
	/// @throws std::runtime_error Past the limit.
	void finish(std::string& resources);

private:
	struct Face; ///< One font file and what is drawn from it; defined in PdfFonts.cpp.
	/// @brief Writes one font's objects.
	/// @param face The font.
	void write(Face& face);

	PdfObjects& objects_;                                ///< Where the fonts are written.
	std::size_t limit_;                                  ///< The most bytes of font files.
	std::size_t written_ = 0;                            ///< Bytes of font files so far.
	std::vector<std::unique_ptr<Face>> faces_;           ///< By index.
	std::map<const unsigned char*, std::size_t> byFile_; ///< Indices, by the file's first byte.
};

} // namespace affresco::detail
