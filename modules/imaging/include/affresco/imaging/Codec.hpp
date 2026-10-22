#pragma once

/// @file
/// @brief PNG and JPEG files: decoded to tiled images (8-bit to rgba8, 16-bit PNG to rgba16f) with their embedded ICC
/// profile, and encoded from them with one, all within Limits.

#include "affresco/imaging/Color.hpp"
#include "affresco/imaging/Image.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace affresco::imaging {

/// @brief The image files read and written.
enum class ImageFileFormat : std::uint8_t {
	png,  ///< PNG: lossless, alpha, 8 or 16 bits (written at 8).
	jpeg, ///< JPEG: lossy, no alpha.
};

/// @brief A decoded file.
struct DecodedImage {
	std::shared_ptr<Image> image;        ///< Its pixels, straight alpha.
	std::optional<ColorProfile> profile; ///< Its embedded profile; none means sRGB.
	std::string warning;                 ///< Why an embedded profile was ignored, or empty.
};

/// @brief What a file is, by its first bytes.
/// @param file The file.
/// @return Its format, or nothing if neither.
[[nodiscard]] std::optional<ImageFileFormat> imageFileFormat(std::span<const std::uint8_t> file) noexcept;

/// @brief Decodes a PNG or JPEG.
/// @param file The file.
/// @param limits imageFileBytes, imageFileSide, side and pixels bound it; profileBytes its profile.
/// @return The image. A broken or unusable embedded profile is ignored, with a warning: the image is then sRGB.
/// @throws DocumentError when it is too big, not a PNG or JPEG, or broken.
[[nodiscard]] DecodedImage decodeImageFile(std::span<const std::uint8_t> file, const Limits& limits);

/// @brief Encodes an image at 8 bits a channel, rounding rgba16f.
/// @param image The image (rgba8 or rgba16f).
/// @param format PNG, or JPEG (alpha dropped: composite on a background first).
/// @param profile The profile to embed (PNG iCCP chunk, JPEG APP2 segments), or nothing.
/// @param quality JPEG's, 1 to 100.
/// @return The file.
/// @throws DocumentError when the encoder fails.
[[nodiscard]] std::vector<std::uint8_t> encodeImageFile(const Image& image, ImageFileFormat format,
                                                        const std::optional<ColorProfile>& profile, int quality = 90);

/// @brief The embedded ICC profile's bytes of a PNG (iCCP) or JPEG (APP2 ICC_PROFILE), not parsed.
/// @param file The file.
/// @param limits profileBytes bounds the profile.
/// @return The bytes, or nothing if there are none.
/// @throws DocumentError when the embedding is broken or too big.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> embeddedProfile(std::span<const std::uint8_t> file,
                                                                       const Limits& limits);

} // namespace affresco::imaging
