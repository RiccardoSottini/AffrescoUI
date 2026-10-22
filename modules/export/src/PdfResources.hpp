#pragma once

/// @file
/// @brief detail::PdfResources: what a PDF's pages share (fonts, images, transparency states), in one dictionary.

#include "affresco/export/PdfDocument.hpp"

#include "PdfFonts.hpp"
#include "PdfObjects.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <utility>

namespace affresco::detail {

/// @brief The resources of every page: one dictionary, written last, that names the fonts, the images and the graphics
/// states that set transparency.
class PdfResources {
public:
	/// @brief None yet.
	/// @param objects Where they are written.
	/// @param limits Their limits.
	PdfResources(PdfObjects& objects, const PdfLimits& limits);

	/// @brief The resource dictionary's object, which pages refer to.
	[[nodiscard]] std::uint32_t id() const noexcept { return id_; }
	/// @brief The fonts.
	[[nodiscard]] PdfFonts& fonts() noexcept { return fonts_; }
	/// @brief A graphics state's name that sets fill and stroke alpha ("/Ga128 gs" draws with it).
	/// @param alpha The alpha, in [0, 1]; quantised to 1/255.
	/// @return Its name ("Ga128"); empty for opaque, which needs none.
	[[nodiscard]] std::string alpha(float alpha);
	/// @brief An image's name, written the first time.
	/// @param key Tells images apart: the same key is the same image.
	/// @param width Its width, in pixels.
	/// @param height Its height.
	/// @param rgba Its pixels, top row first, 4 bytes each, not premultiplied.
	/// @return Its name ("Im1"), drawn in a unit square.
	/// @throws std::runtime_error Past the image limit.
	[[nodiscard]] std::string image(std::pair<const void*, std::uint64_t> key, int width, int height,
	                                std::span<const std::uint8_t> rgba);
	/// @brief Writes the fonts and the dictionary.
	/// @throws std::runtime_error Past the limits.
	void finish();

private:
	PdfObjects& objects_;                                                ///< Where they are written.
	PdfFonts fonts_;                                                     ///< The fonts.
	std::uint32_t id_;                                                   ///< The dictionary's object.
	std::size_t pixelLimit_;                                             ///< The most image pixels.
	std::size_t pixels_ = 0;                                             ///< Image pixels so far.
	std::map<int, std::uint32_t> alphas_;                                ///< Graphics states, by alpha in 1/255.
	std::map<std::pair<const void*, std::uint64_t>, std::string> names_; ///< Image names, by key.
	std::map<std::string, std::uint32_t> images_;                        ///< Image objects, by name.
};

} // namespace affresco::detail
