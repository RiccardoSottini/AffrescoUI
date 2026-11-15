/// @file
/// @brief The unit scale (render's platform/UnitScale.hpp): a display's content scale rounded to a quarter and kept
/// between 1 and 4, window units to pixels and back on Win32 and X11, and the identity at a scale of 1.

#include "platform/UnitScale.hpp"

#include <doctest/doctest.h>

#include <bit>
#include <cstdint>
#include <limits>

using affresco::platform::pixelsToUnits;
using affresco::platform::pointToUnits;
using affresco::platform::unitScale;
using affresco::platform::unitsToPixels;

TEST_CASE("unit scale: a display's content scale rounded to a quarter, from 1 to 4") {
	CHECK(unitScale(1.0f) == 1.0f);
	CHECK(unitScale(1.24f) == 1.25f);
	CHECK(unitScale(1.25f) == 1.25f);
	CHECK(unitScale(1.5f) == 1.5f);
	CHECK(unitScale(1.74f) == 1.75f);
	CHECK(unitScale(2.0f) == 2.0f);
	CHECK(unitScale(1.3333334f) == 1.25f); // 128 dots per inch
	CHECK(unitScale(0.5f) == 1.0f);        // never less than a pixel a unit
	CHECK(unitScale(0.0f) == 1.0f);
	CHECK(unitScale(-2.0f) == 1.0f);
	CHECK(unitScale(9.0f) == 4.0f);
	CHECK(unitScale(std::numeric_limits<float>::infinity()) == 4.0f);
	CHECK(unitScale(std::numeric_limits<float>::quiet_NaN()) == 1.0f);
}

TEST_CASE("unit scale: window units to pixels and back") {
	CHECK(unitsToPixels(1280, 1.5f) == 1920); // a 1280 by 800 window at 150% is 1920 by 1200 pixels
	CHECK(unitsToPixels(800, 1.5f) == 1200);
	CHECK(pixelsToUnits(1920, 1.5f) == 1280);
	CHECK(pixelsToUnits(1200, 1.5f) == 800);
	CHECK(unitsToPixels(641, 1.25f) == 801);                     // 801.25, to the nearest pixel
	CHECK(unitsToPixels(3, 1.5f) == 5);                          // 4.5 rounds away from zero
	CHECK(pointToUnits(300.0, 1.5f) == doctest::Approx(200.0f)); // a pointer at pixel (300, 150) is at unit (200, 100)
	CHECK(pointToUnits(150.0, 1.5f) == doctest::Approx(100.0f));
	CHECK(pointToUnits(1.0, 1.75f) == doctest::Approx(1.0f / 1.75f)); // positions aren't rounded
}

TEST_CASE("unit scale: every size in units survives the trip to pixels and back") {
	for(const float scale : {1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 3.0f, 4.0f})
		for(int units = 0; units <= 4000; ++units) {
			CAPTURE(scale);
			CAPTURE(units);
			REQUIRE(pixelsToUnits(unitsToPixels(units, scale), scale) == units);
		}
}

TEST_CASE("unit scale: an odd pixel count goes to the nearest unit and doesn't drift") {
	int pixels = 1921; // a window the user dragged to an odd width at 150%
	const int units = pixelsToUnits(pixels, 1.5f);
	CHECK(units == 1281); // 1280.67
	for(int trip = 0; trip < 10; ++trip) {
		pixels = unitsToPixels(pixelsToUnits(pixels, 1.5f), 1.5f);
		CHECK(pixelsToUnits(pixels, 1.5f) == units);
	}
	CHECK(pixels == 1922);
	CHECK(pixelsToUnits(1001, 1.75f) == 572); // 572.0
	CHECK(pixelsToUnits(1003, 1.75f) == 573); // 573.14
}

TEST_CASE("unit scale: at a scale of 1 every function is the identity, bit for bit") {
	for(const int value :
	    {0, 1, -1, 799, 1280, 16'777'217, std::numeric_limits<int>::max(), std::numeric_limits<int>::min()}) {
		CHECK(unitsToPixels(value, 1.0f) == value);
		CHECK(pixelsToUnits(value, 1.0f) == value);
	}
	for(const double position : {0.0, -0.0, 0.1, 299.5, -12.75, 1e9 + 0.3, 16'777'217.0}) {
		CAPTURE(position);
		const auto expected = std::bit_cast<std::uint32_t>(static_cast<float>(position)); // what GLFW's value gave
		CHECK(std::bit_cast<std::uint32_t>(pointToUnits(position, 1.0f)) == expected);
	}
}
