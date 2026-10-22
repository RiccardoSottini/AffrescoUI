/// @file
/// @brief The imaging module's PNG and JPEG files: round trips with embedded ICC profiles, 16 bits, and the broken
/// or too big refused.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Codec.hpp"

#include <doctest/doctest.h>

#include <cmath>

using namespace affresco::imaging;

namespace {

/// @brief A 300 by 300 image of four tiles: a red to blue ramp, half transparent on the right, the top left tile empty.
[[nodiscard]] Image ramp() {
	Image image(300, 300, PixelFormat::rgba8);
	for(int y = 0; y < 300; ++y)
		for(int x = 0; x < 300; ++x)
			if(x >= tileSize || y >= tileSize)
				image.setPixel(x, y, {static_cast<float>(x) / 299.0f, 0.25f, static_cast<float>(y) / 299.0f,
				                      x >= 280 ? 0.5f : 1.0f});
	return image;
}

} // namespace

TEST_CASE("imaging codec: PNG round-trips pixels and an embedded profile") {
	const Image image = ramp();
	const ColorProfile p3 = ColorProfile::displayP3();
	const std::vector<std::uint8_t> png = encodeImageFile(image, ImageFileFormat::png, p3);
	CHECK(imageFileFormat(png) == ImageFileFormat::png);
	const DecodedImage decoded = decodeImageFile(png, {});
	REQUIRE(decoded.image);
	CHECK(decoded.warning.empty());
	REQUIRE(decoded.profile);
	CHECK(*decoded.profile == p3);
	CHECK(decoded.image->format() == PixelFormat::rgba8);
	CHECK(decoded.image->width() == 300);
	for(const auto& [x, y] : {std::pair{0, 0}, {150, 270}, {299, 299}, {290, 10}})
		CHECK(decoded.image->pixel(x, y) == image.pixel(x, y));
	CHECK(decoded.image->writtenTiles().size() == 3); // transparent tiles stay absent
	const DecodedImage plain = decodeImageFile(encodeImageFile(image, ImageFileFormat::png, std::nullopt), {});
	CHECK_FALSE(plain.profile);
}

TEST_CASE("imaging codec: JPEG keeps a profile, and 16 bits decode to half floats") {
	const Image image = ramp();
	const ColorProfile wide = ColorProfile::wideRgb();
	const DecodedImage jpeg = decodeImageFile(encodeImageFile(image, ImageFileFormat::jpeg, wide, 95), {});
	REQUIRE(jpeg.profile);
	CHECK(*jpeg.profile == wide);
	CHECK(jpeg.image->pixel(0, 0).alpha == 1.0f); // JPEG has no alpha
	CHECK(std::abs(jpeg.image->pixel(150, 270).red - image.pixel(150, 270).red) < 0.05f);
	const std::vector<std::uint8_t> split = encodeImageFile(image, ImageFileFormat::jpeg, std::nullopt);
	CHECK_FALSE(embeddedProfile(split, {}));
	// A 16-bit PNG: one pixel, written by hand (IHDR depth 16, one stored deflate block)
	const std::vector<std::uint8_t> sixteen{
	    0x89, 'P',  'N',  'G',  '\r', '\n', 0x1a, '\n', 0,    0,    0,    13,   'I',  'H',  'D',  'R',  0,    0,
	    0,    1,    0,    0,    0,    1,    16,   6,    0,    0,    0,    0x4f, 0x85, 0x18, 0xca, 0,    0,    0,
	    20,   'I',  'D',  'A',  'T',  0x78, 0x01, 0x01, 9,    0,    0xf6, 0xff, 0,    0x80, 0x00, 0x40, 0x00, 0xff,
	    0xff, 0xff, 0xff, 0x0f, 0x7f, 0x04, 0xbd, 0x4f, 0x5d, 0x99, 0x8c,    0,    0,    0,    0,    'I',  'E',  'N',
	    'D',  0xae, 0x42, 0x60, 0x82};
	const DecodedImage deep = decodeImageFile(sixteen, {});
	CHECK(deep.image->format() == PixelFormat::rgba16f);
	CHECK(deep.image->pixel(0, 0).red == doctest::Approx(0.5f).epsilon(0.001));
	CHECK(deep.image->pixel(0, 0).green == doctest::Approx(0.25f).epsilon(0.001));
}

TEST_CASE("imaging codec: files beyond the limits, broken files and broken profiles") {
	const Image image = ramp();
	const std::vector<std::uint8_t> png = encodeImageFile(image, ImageFileFormat::png, ColorProfile::srgb());
	Limits small;
	small.imageFileBytes = png.size() - 1;
	CHECK_THROWS_AS((void)decodeImageFile(png, small), affresco::DocumentError);
	small = {};
	small.imageFileSide = 299;
	CHECK_THROWS_AS((void)decodeImageFile(png, small), affresco::DocumentError);
	small = {};
	small.pixels = 300 * 300 - 1;
	CHECK_THROWS_AS((void)decodeImageFile(png, small), affresco::DocumentError);
	CHECK_THROWS_AS((void)decodeImageFile(std::span(png).first(60), {}), affresco::DocumentError);
	const std::vector<std::uint8_t> text{'h', 'e', 'l', 'l', 'o'};
	CHECK_THROWS_AS((void)decodeImageFile(text, {}), affresco::DocumentError);
	small = {};
	small.profileBytes = 100; // the profile no longer fits: the image opens as sRGB, with a warning
	const DecodedImage warned = decodeImageFile(png, small);
	CHECK_FALSE(warned.profile);
	CHECK_FALSE(warned.warning.empty());
	std::vector<std::uint8_t> broken = png;
	broken[33 + 8 + 5] ^= 0xff; // the iCCP chunk's zlib header
	const DecodedImage ignored = decodeImageFile(broken, {});
	CHECK(ignored.image);
	CHECK_FALSE(ignored.profile);
	CHECK_FALSE(ignored.warning.empty());
}
