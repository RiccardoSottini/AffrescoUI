/// @file
/// @brief The SVG decoder's paint and transforms, which agents' pictures use: linear and radial gradients (in the
/// shape's box and in user space, with stops, opacity and href), and rotate, skew and matrix transforms.

#include <affresco/render/Bitmap.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace {

/// @brief Decodes an SVG.
/// @param svg The document.
/// @return The picture (twice the document's size).
std::shared_ptr<const affresco::Bitmap> draw(const std::string& svg) {
	return affresco::Bitmap::decode({reinterpret_cast<const std::uint8_t*>(svg.data()), svg.size()}, {});
}

/// @brief A pixel: red, green, blue, alpha (premultiplied).
/// @param bitmap The picture.
/// @param x Across.
/// @param y Down.
/// @return The pixel.
std::array<int, 4> at(const affresco::Bitmap& bitmap, int x, int y) {
	const std::size_t i =
	    (static_cast<std::size_t>(y) * static_cast<std::size_t>(bitmap.width()) + static_cast<std::size_t>(x)) * 4;
	const auto pixels = bitmap.pixels();
	return {pixels[i], pixels[i + 1], pixels[i + 2], pixels[i + 3]};
}

} // namespace

TEST_CASE("svg: a linear gradient goes from its first stop to its last across the shape's box") {
	const auto bitmap = draw(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="50" height="10">
		<defs><linearGradient id="sky"><stop offset="0" stop-color="#ff0000"/>
		<stop offset="100%" style="stop-color:#0000ff"/></linearGradient></defs>
		<rect width="50" height="10" fill="url(#sky)"/></svg>)svg");
	REQUIRE(bitmap->width() == 100);
	const auto left = at(*bitmap, 1, 10), middle = at(*bitmap, 50, 10), right = at(*bitmap, 98, 10);
	CHECK(left[0] > 240);
	CHECK(left[2] < 15);
	CHECK(right[2] > 240);
	CHECK(right[0] < 15);
	CHECK(middle[0] == doctest::Approx(128).epsilon(0.06));
	CHECK(middle[3] == 255);
}

TEST_CASE("svg: a radial gradient in user space, its stops' opacity, and a gradient that inherits stops") {
	const auto bitmap = draw(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40">
		<radialGradient id="base"><stop offset="0" stop-color="white"/><stop offset="1" stop-color="black"
		stop-opacity="0"/></radialGradient>
		<radialGradient id="moon" href="#base" gradientUnits="userSpaceOnUse" cx="20" cy="20" r="20"/>
		<rect width="40" height="40" fill="url(#moon)"/></svg>)svg");
	const auto centre = at(*bitmap, 40, 40), edge = at(*bitmap, 79, 40), missing = at(*bitmap, 1, 1);
	CHECK(centre[0] > 240);
	CHECK(centre[3] > 240);
	CHECK(edge[3] < 20);
	CHECK(missing[3] < 10); // past the radius: the last stop, transparent
}

TEST_CASE("svg: rotate, skew and matrix transforms are drawn, and a gradient not defined is nothing") {
	// A bar 40 wide and 4 high, turned a quarter about its centre: it stands upright
	const auto turned = draw(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40">
		<rect x="0" y="18" width="40" height="4" fill="#000" transform="rotate(90 20 20)"/></svg>)svg");
	CHECK(at(*turned, 40, 5)[3] == 255);
	CHECK(at(*turned, 5, 40)[3] == 0);
	const auto matrix = draw(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40">
		<rect width="10" height="10" fill="#000" transform="matrix(0 1 -1 0 30 0)"/></svg>)svg");
	CHECK(at(*matrix, 50, 10)[3] == 255); // x from 20 to 30, y from 0 to 10
	CHECK(at(*matrix, 10, 10)[3] == 0);
	const auto skewed = draw(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40">
		<rect width="10" height="40" fill="#000" transform="skewX(45) skewX(90)"/></svg>)svg"); // the 90 is skipped
	CHECK(at(*skewed, 5, 2)[3] == 255);
	CHECK(at(*skewed, 77, 76)[3] == 255); // x + y: at y 38, x from 38
	CHECK(at(*skewed, 70, 76)[3] == 0);
	const auto unknown = draw(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10">
		<rect width="10" height="10" fill="url(#later)"/></svg>)svg");
	CHECK(at(*unknown, 10, 10)[3] == 0);
}
