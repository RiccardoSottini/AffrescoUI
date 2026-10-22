#pragma once

/// @file
/// @brief PdfDocument: writes panel trees as pages of a vector PDF (paths, text with embedded subset fonts, images).

#include "affresco/render/Geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace affresco {

class Panel;

/// @brief What a PDF may grow to: a page, a font or an image past these is refused with an error, so a document from
/// a peer can't make an export without bound (SECURITY.md, "Limits").
struct PdfLimits {
	std::size_t pages = 2000;                         ///< The most pages.
	std::size_t bytes = std::size_t{256} << 20;       ///< The largest file, everything included.
	std::size_t fontBytes = std::size_t{64} << 20;    ///< The most bytes of embedded fonts, after subsetting.
	std::size_t imagePixels = std::size_t{1} << 26;   ///< The most image pixels, all images together.
	std::size_t contentBytes = std::size_t{64} << 20; ///< The largest page content (drawing operators).
};

/// @brief A PDF being written: each page draws a laid-out panel tree, as the window would, but as vectors.
///
/// What each Painter call becomes:
/// - rectangles, rounded rectangles and their outlines: paths (rounded corners as Bézier curves);
/// - meshes (vector paths' fills and strokes): their outline, recovered from the anti-aliased quads where the coverage
///   crosses one half, filled as one nonzero path;
/// - text: the glyphs of the font files, subset with HarfBuzz (glyph indices kept), embedded as CID fonts with their
///   widths and a ToUnicode map so the text can be searched and copied; synthetic bold and italic as the screen draws
///   them; colour glyphs (emoji) as images;
/// - images: RGB with an alpha mask (8 bits per channel, uncompressed);
/// - shadows: a few layers of the shape, growing, whose alphas add up to the shadow's colour.
///
/// Panels are clipped to their bounds and transformed (rotated, scaled) as on screen; hidden panels and overlays are
/// left out. A GPU surface draws nothing. Streams aren't compressed (no zlib), so files are larger than they could be.
/// Errors throw std::runtime_error naming what went past a limit.
class PdfDocument {
public:
	/// @brief An empty document.
	/// @param limits What it may grow to.
	explicit PdfDocument(PdfLimits limits = {});
	/// @brief Frees what was gathered.
	~PdfDocument();
	PdfDocument(const PdfDocument&) = delete;            ///< One document, one file.
	PdfDocument& operator=(const PdfDocument&) = delete; ///< One document, one file.

	/// @brief Adds a page that shows a panel and its subtree.
	/// @param root A panel of a window, laid out (its frame and bounds are those of the last layout).
	/// @param area The part to show, in window units: fitted to the page, centred, keeping its proportions.
	/// @param pageSize The page's width and height, in points (1/72 inch): {842, 595} is A4 landscape.
	/// @throws std::runtime_error Past the limits.
	void addPage(const Panel& root, const Rect& area, Vec2 pageSize);
	/// @brief Adds a page that shows a panel's whole frame.
	/// @param root As addPage().
	/// @param pageSize As addPage().
	/// @throws std::runtime_error Past the limits.
	void addPage(const Panel& root, Vec2 pageSize);
	/// @brief How many pages were added.
	[[nodiscard]] std::size_t pageCount() const noexcept;

	/// @brief Writes the fonts and the file's structure.
	/// @param title The document's title, in its information dictionary (empty: none).
	/// @return The whole file. The document can't be used afterwards.
	/// @throws std::runtime_error Past the limits, or if called twice.
	[[nodiscard]] std::vector<std::uint8_t> finish(std::string_view title = {});
	/// @brief finish(), written to a file.
	/// @param file Where to write it (replaced).
	/// @param title As finish().
	/// @throws std::runtime_error As finish(), or if the file can't be written.
	void save(const std::filesystem::path& file, std::string_view title = {});

private:
	struct State;                  ///< The objects, the resources and the pages; defined in PdfDocument.cpp.
	std::unique_ptr<State> state_; ///< Null once finished.
};

} // namespace affresco
