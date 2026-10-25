/// @file
/// @brief The imaging module's PNG and JPEG files: round trips with embedded ICC profiles, 16 bits, and the broken
/// or too big refused.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Codec.hpp"

#include <doctest/doctest.h>

#include <cmath>

using namespace affresco::imaging;

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

namespace {

/// @brief A 300 by 300 image of four tiles: a red to blue ramp, half transparent on the right, the top left tile empty.
[[nodiscard]] Image ramp() {
	Image image(300, 300, PixelFormat::rgba8);
	for(int y = 0; y < 300; ++y)
		for(int x = 0; x < 300; ++x)
			if(x >= tileSize || y >= tileSize)
				image.setPixel(
				    x, y,
				    {static_cast<float>(x) / 299.0f, 0.25f, static_cast<float>(y) / 299.0f, x >= 280 ? 0.5f : 1.0f});
	return image;
}

/// @brief Reads a big-endian 32-bit number. @param bytes The bytes. @param at Where. @return It.
[[nodiscard]] std::uint32_t read32(const std::vector<std::uint8_t>& bytes, std::size_t at) {
	return static_cast<std::uint32_t>(bytes[at]) << 24 | static_cast<std::uint32_t>(bytes[at + 1]) << 16 |
	       static_cast<std::uint32_t>(bytes[at + 2]) << 8 | bytes[at + 3];
}

/// @brief Writes a big-endian 32-bit number. @param bytes The bytes. @param at Where. @param value It.
void write32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
	for(int shift = 24, i = 0; shift >= 0; shift -= 8, ++i)
		bytes[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> shift);
}

/// @brief An ICC profile with one more tag, private and as big as asked, which lcms keeps when it saves it.
/// @param profile The profile. @param size The tag's bytes.
/// @return The bigger profile.
[[nodiscard]] std::vector<std::uint8_t> withPrivateTag(std::vector<std::uint8_t> profile, std::size_t size) {
	constexpr std::size_t table = 128, entry = 12;
	const std::uint32_t tags = read32(profile, table);
	for(std::uint32_t tag = 0; tag < tags; ++tag) // the data moves down by the new entry
		write32(profile, table + 4 + tag * entry + 4, read32(profile, table + 4 + tag * entry + 4) + entry);
	const std::size_t end = (profile.size() + entry + 3) / 4 * 4;
	const std::vector<std::uint8_t> added{'z', 'z', 'z', 'z', 0, 0, 0, 0, 0, 0, 0, 0};
	profile.insert(profile.begin() + static_cast<std::ptrdiff_t>(table + 4 + tags * entry), added.begin(), added.end());
	write32(profile, table, tags + 1);
	write32(profile, table + 4 + tags * entry + 4, static_cast<std::uint32_t>(end));
	write32(profile, table + 4 + tags * entry + 8, static_cast<std::uint32_t>(size));
	profile.resize(end + size);
	write32(profile, 0, static_cast<std::uint32_t>(profile.size()));
	return profile;
}

/// @brief A PNG's first bytes: a grey image of a size, its header, then the start of its pixels' stream.
/// @param width Its width. @param height Its height.
/// @return The bytes.
[[nodiscard]] std::vector<std::uint8_t> pngHeader(std::uint32_t width, std::uint32_t height) {
	std::vector<std::uint8_t> bytes{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
	bytes.resize(bytes.size() + 8);
	write32(bytes, 16, width);
	write32(bytes, 20, height);
	bytes.insert(bytes.end(), {8, 0, 0, 0, 0, 0, 0, 0, 0}); // 8-bit grey, then the CRC (stb_image doesn't check it)
	bytes.insert(bytes.end(), {0, 0, 0, 2, 'I', 'D', 'A', 'T', 0x78, 0x01, 0, 0, 0, 0}); // stb_image reads up to here
	return bytes;
}

} // namespace

TEST_CASE("imaging codec: a small file whose header says hundreds of megapixels is refused before it is decoded") {
	// Half a megabyte of PNG holds a gigapixel of one grey: 17000 by 17000 is within the sides, beyond the pixels
	const std::vector<std::uint8_t> bomb = pngHeader(17000, 17000);
	CHECK_THROWS_WITH_AS((void)decodeImageFile(bomb, {}), "imaging: the image has more pixels than the limit",
	                     affresco::DocumentError);
	Limits host; // a host that opens bigger files raises the limit: the file is then read, and found broken
	host.imageFilePixels = host.pixels;
	CHECK_THROWS_WITH_AS((void)decodeImageFile(bomb, host), "imaging: the PNG or JPEG file is broken",
	                     affresco::DocumentError);
}

TEST_CASE("imaging codec: PNG round-trips pixels and an embedded profile") {
	const Image image = ramp();
	const ColorProfile p3 = ColorProfile::displayP3();
	const std::vector<std::uint8_t> png = encodeImageFile(image, ImageFileFormat::png, p3);
	CHECK(imageFileFormat(png) == ImageFileFormat::png);
	const DecodedImage decoded = decodeImageFile(png, {});
	REQUIRE(decoded.image);
	CHECK(decoded.warning.empty());
	REQUIRE(decoded.profile);
	CHECK(decoded.profile.value() == p3);
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
	CHECK(jpeg.profile.value() == wide);
	CHECK(jpeg.image->pixel(0, 0).alpha == 1.0f); // JPEG has no alpha
	CHECK(std::abs(jpeg.image->pixel(150, 270).red - image.pixel(150, 270).red) < 0.05f);
	const std::vector<std::uint8_t> split = encodeImageFile(image, ImageFileFormat::jpeg, std::nullopt);
	CHECK_FALSE(embeddedProfile(split, {}));
	// A 16-bit PNG: one pixel, written by hand (IHDR depth 16, one stored deflate block)
	const std::vector<std::uint8_t> sixteen{
	    0x89, 'P',  'N',  'G',  '\r', '\n', 0x1a, '\n', 0,    0,    0,    13,   'I',  'H',  'D',  'R',
	    0,    0,    0,    1,    0,    0,    0,    1,    16,   6,    0,    0,    0,    0x4f, 0x85, 0x18,
	    0xca, 0,    0,    0,    20,   'I',  'D',  'A',  'T',  0x78, 0x01, 0x01, 9,    0,    0xf6, 0xff,
	    0,    0x80, 0x00, 0x40, 0x00, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x7f, 0x04, 0xbd, 0x4f, 0x5d, 0x99,
	    0x8c, 0,    0,    0,    0,    'I',  'E',  'N',  'D',  0xae, 0x42, 0x60, 0x82};
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
	small = {};
	small.imageFilePixels = 300 * 300 - 1;
	CHECK_THROWS_AS((void)decodeImageFile(png, small), affresco::DocumentError);
	CHECK_THROWS_AS((void)decodeImageFile(std::span(png).first(60), {}), affresco::DocumentError);
	const std::vector<std::uint8_t> text{'h', 'e', 'l', 'l', 'o'};
	CHECK_THROWS_AS((void)decodeImageFile(text, {}), affresco::DocumentError);
	small = {};
	small.profileBytes = 100; // the profile no longer fits: the image opens as sRGB, with a warning
	const DecodedImage warned = decodeImageFile(png, small);
	CHECK_FALSE(warned.profile);
	CHECK_FALSE(warned.warning.empty());
	// Sizes past what the encoder's arithmetic holds are refused before any pixel is gathered
	const Image huge(32768, 32768, PixelFormat::rgba8);
	CHECK_THROWS_AS((void)encodeImageFile(huge, ImageFileFormat::png, std::nullopt), affresco::DocumentError);
	CHECK_THROWS_AS((void)encodeImageFile(huge, ImageFileFormat::jpeg, std::nullopt), affresco::DocumentError);
	const Image wide(32768, 8, PixelFormat::rgba8);
	CHECK_FALSE(encodeImageFile(wide, ImageFileFormat::png, std::nullopt).empty());
	// A profile JPEG's 255 numbered segments can't hold is refused, not numbered past 255
	const std::vector<std::uint8_t> padded = withPrivateTag(ColorProfile::srgb().bytes(), std::size_t{17} << 20);
	small = {};
	small.profileBytes = padded.size();
	const ColorProfile big = ColorProfile::parse(padded, small);
	REQUIRE(big.bytes().size() > std::size_t{17} << 20);
	CHECK_THROWS_AS((void)encodeImageFile(image, ImageFileFormat::jpeg, big), affresco::DocumentError);
	CHECK_FALSE(encodeImageFile(image, ImageFileFormat::png, big).empty());
	std::vector<std::uint8_t> broken = png;
	broken[33 + 8 + 5] ^= 0xff; // the iCCP chunk's zlib header
	const DecodedImage ignored = decodeImageFile(broken, {});
	CHECK(ignored.image);
	CHECK_FALSE(ignored.profile);
	CHECK_FALSE(ignored.warning.empty());
}

// NOLINTEND(bugprone-unchecked-optional-access)
