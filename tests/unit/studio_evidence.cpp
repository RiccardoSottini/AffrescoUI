/// @file
/// @brief The Studio's evidence pictures (examples/studio/EvidencePicture.hpp): base64 decoded, and the changed
/// region outlined within the picture whatever region the connected peer sends.

#include "EvidencePicture.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace studio = examples::studio;

namespace {

/// @brief A white picture.
/// @param width Its width.
/// @param height Its height.
/// @return It.
std::shared_ptr<const affresco::Bitmap> white(int width, int height) {
	const std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width * height) * 4, 255);
	return affresco::Bitmap::fromPixels(width, height, rgba, affresco::Bitmap::Limits{});
}

/// @brief Whether a pixel of an outlined picture is the outline's magenta.
/// @param rgba The picture's pixels.
/// @param width Its width.
/// @param x The pixel's column.
/// @param y Its row.
/// @return true if it is.
bool magenta(const std::vector<std::uint8_t>& rgba, int width, int x, int y) {
	const auto at = static_cast<std::size_t>(y * width + x) * 4;
	return rgba[at] == 236 && rgba[at + 1] == 64 && rgba[at + 2] == 122;
}

} // namespace

TEST_SUITE("studio evidence") {
	TEST_CASE("base64 decodes, and anything else is refused") {
		CHECK(studio::fromBase64("aGk=") == std::vector<std::uint8_t>{'h', 'i'});
		CHECK(studio::fromBase64("").empty());
		CHECK_THROWS_AS((void)studio::fromBase64("a-b"), std::invalid_argument);
	}

	TEST_CASE("the outline is three pixels wide, around the region") {
		const auto picture = white(10, 10);
		const std::vector<std::uint8_t> rgba = studio::outlined(*picture, {4, 4, 2, 2});
		CHECK(magenta(rgba, 10, 3, 3));       // just outside the corner
		CHECK(magenta(rgba, 10, 1, 4));       // three to the left
		CHECK_FALSE(magenta(rgba, 10, 0, 4)); // four to the left
		CHECK_FALSE(magenta(rgba, 10, 4, 4)); // the region itself
		// an empty region draws nothing
		CHECK(std::ranges::equal(studio::outlined(*picture, {4, 4, 0, 2}), picture->pixels()));
	}

	TEST_CASE("a region at the ends of int is clipped, not overflowed or walked") {
		// UBSan reported the overflow of x + width before the outline was clipped to the picture in 64 bits, and a
		// region two billion pixels wide took seconds to walk
		const auto picture = white(4, 3);
		CHECK(std::ranges::equal(studio::outlined(*picture, {INT_MAX - 7, 0, 100, 1}), picture->pixels()));
		CHECK(std::ranges::equal(studio::outlined(*picture, {INT_MIN, INT_MIN, 100, 100}), picture->pixels()));
		CHECK(std::ranges::equal(studio::outlined(*picture, {INT_MAX, INT_MAX, INT_MAX, INT_MAX}), picture->pixels()));
		const std::vector<std::uint8_t> covered = studio::outlined(*picture, {-5, 1, INT_MAX, 1});
		CHECK(magenta(covered, 4, 0, 0)); // the line above the region, across the picture
		CHECK(magenta(covered, 4, 3, 2)); // and the one below
		CHECK_FALSE(magenta(covered, 4, 2, 1));
	}
}
