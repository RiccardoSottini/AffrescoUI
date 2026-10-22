/// @file
/// @brief The imaging module's colour management: built-in profiles, conversions, soft proofing, the display table,
/// and profiles parsed under limits.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Color.hpp"

#include <doctest/doctest.h>

#include <cmath>

using namespace affresco::imaging;

namespace {

/// @brief Whether two pixels are the same within a tolerance.
/// @param left One.
/// @param right The other.
/// @param tolerance Per channel.
[[nodiscard]] bool near(const Pixel& left, const Pixel& right, float tolerance = 0.005f) {
	return std::abs(left.red - right.red) <= tolerance && std::abs(left.green - right.green) <= tolerance &&
	       std::abs(left.blue - right.blue) <= tolerance && std::abs(left.alpha - right.alpha) <= tolerance;
}

} // namespace

TEST_CASE("imaging color: conversions between built-in profiles keep alpha and round-trip") {
	const ColorProfile srgb = ColorProfile::srgb(), p3 = ColorProfile::displayP3(), wide = ColorProfile::wideRgb();
	CHECK(srgb.model() == ColorModel::rgb);
	CHECK(p3.name() == "Display P3");
	CHECK_FALSE(srgb.bytes().empty());
	CHECK(srgb == ColorProfile::parse(srgb.bytes(), {}));
	CHECK_FALSE(srgb == p3);
	const ColorTransform same(srgb, srgb, RenderingIntent::relativeColorimetric);
	CHECK(near(same.apply(Pixel{0.2f, 0.5f, 0.8f, 0.3f}), {0.2f, 0.5f, 0.8f, 0.3f}));
	const ColorTransform toP3(srgb, p3, RenderingIntent::relativeColorimetric);
	const Pixel red = toP3.apply(Pixel{1.0f, 0.0f, 0.0f, 0.5f});
	CHECK(red.red < 0.95f); // sRGB's red sits inside P3
	CHECK(red.green > 0.1f);
	CHECK(red.alpha == 0.5f);
	const ColorTransform there(srgb, wide, RenderingIntent::relativeColorimetric);
	const ColorTransform back(wide, srgb, RenderingIntent::relativeColorimetric);
	std::vector<Pixel> pixels{{0.1f, 0.6f, 0.3f, 1.0f}, {0.9f, 0.9f, 0.9f, 0.0f}, {}};
	const std::vector<Pixel> before = pixels;
	there.apply(pixels);
	back.apply(pixels);
	for(std::size_t index = 0; index < pixels.size(); ++index) CHECK(near(pixels[index], before[index]));
}

TEST_CASE("imaging color: grey, soft proofing and the display table") {
	const ColorProfile srgb = ColorProfile::srgb(), gray = ColorProfile::gray();
	CHECK(gray.model() == ColorModel::gray);
	const Pixel grey = ColorTransform(srgb, gray, RenderingIntent::perceptual).apply(Pixel{1.0f, 0.0f, 0.0f, 0.7f});
	CHECK(grey.red == grey.green);
	CHECK(grey.red == grey.blue);
	CHECK(grey.alpha == 0.7f);
	CHECK(near(ColorTransform(gray, srgb, RenderingIntent::perceptual).apply(Pixel{0.5f, 0.0f, 0.0f, 1.0f}),
	           {0.5f, 0.5f, 0.5f, 1.0f}, 0.01f));
	const ColorTransform proofed(srgb, srgb, RenderingIntent::relativeColorimetric, gray);
	const Pixel shown = proofed.apply(Pixel{1.0f, 0.0f, 0.0f, 1.0f});
	CHECK(std::abs(shown.red - shown.green) < 0.01f); // red, as a grey device would show it
	const std::vector<float> table = ColorTransform(srgb, srgb, RenderingIntent::perceptual).table(2);
	REQUIRE(table.size() == 8 * 3);
	CHECK(table[3] == doctest::Approx(1.0f).epsilon(0.01)); // red's corner
	CHECK(table[4] == doctest::Approx(0.0f));
	CHECK(ColorTransform(srgb, srgb, RenderingIntent::perceptual).table(1000).size() == 65 * 65 * 65 * 3);
}

TEST_CASE("imaging color: profiles are parsed under limits, and the malformed refused") {
	const ColorProfile srgb = ColorProfile::srgb();
	std::vector<std::uint8_t> bytes = srgb.bytes();
	const ColorProfile parsed = ColorProfile::parse(bytes, {});
	CHECK(parsed.name() == srgb.name());
	Limits small;
	small.profileBytes = bytes.size() - 1;
	CHECK_THROWS_AS((void)ColorProfile::parse(bytes, small), affresco::DocumentError);
	CHECK_THROWS_AS((void)ColorProfile::parse(std::span(bytes).first(100), {}), affresco::DocumentError);
	CHECK_THROWS_AS((void)ColorProfile::parse(std::span(bytes).first(bytes.size() - 8), {}), affresco::DocumentError);
	std::vector<std::uint8_t> noise(4096, 0x5a);
	CHECK_THROWS_AS((void)ColorProfile::parse(noise, {}), affresco::DocumentError);
	std::vector<std::uint8_t> lab = bytes;
	lab[16] = 'L', lab[17] = 'a', lab[18] = 'b', lab[19] = ' '; // its colour space, now Lab
	CHECK_THROWS_AS((void)ColorProfile::parse(lab, {}), affresco::DocumentError);
	std::vector<std::uint8_t> link = bytes;
	link[12] = 'l', link[13] = 'i', link[14] = 'n', link[15] = 'k'; // a device link
	CHECK_THROWS_AS((void)ColorProfile::parse(link, {}), affresco::DocumentError);
	// Found by fuzzing (tests/fuzz, icc, under UBSan): a class or connection space that no signature names was read
	// into an enum variable that can't hold it
	std::vector<std::uint8_t> unnamed = bytes;
	unnamed[20] = 0xa3, unnamed[21] = 0xac, unnamed[22] = 0xff, unnamed[23] = 0xf5; // its connection space
	CHECK_THROWS_AS((void)ColorProfile::parse(unnamed, {}), affresco::DocumentError);
	unnamed = bytes;
	unnamed[12] = 0xa3, unnamed[13] = 0xac, unnamed[14] = 0xff, unnamed[15] = 0xf5; // its class
	CHECK_THROWS_AS((void)ColorProfile::parse(unnamed, {}), affresco::DocumentError);
	std::size_t refused = 0; // every byte of P3 changed in turn: parsed or refused, never a crash
	const std::vector<std::uint8_t> p3 = ColorProfile::displayP3().bytes();
	for(std::size_t index = 0; index < p3.size(); ++index) {
		std::vector<std::uint8_t> changed = p3;
		changed[index] = static_cast<std::uint8_t>(changed[index] ^ (index % 2 ? 0xff : 0x80));
		try {
			(void)ColorTransform(ColorProfile::parse(changed, {}), srgb, RenderingIntent::perceptual)
			    .apply(Pixel{0.5f, 0.5f, 0.5f, 1.0f});
		} catch(const affresco::DocumentError&) {
			++refused;
		}
	}
	CHECK(refused > 0);
}
