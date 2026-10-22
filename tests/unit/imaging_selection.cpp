/// @file
/// @brief The imaging module's selections (shapes, wand, colour range, combining, feather) and filters.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Filter.hpp"
#include "affresco/imaging/Selection.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>

using namespace affresco::imaging;

namespace {

/// @brief A coverage's value at a pixel, 0 to 255.
/// @param coverage The coverage.
/// @param x Across.
/// @param y Down.
[[nodiscard]] int valueAt(const Coverage& coverage, int x, int y) {
	const auto found = coverage.find({x / tileSize, y / tileSize});
	if(found == coverage.end()) return 0;
	return found->second->bytes()[static_cast<std::size_t>((y % tileSize) * tileSize + x % tileSize)];
}

/// @brief The selection's value at a pixel, 0 to 255.
/// @param document The document.
/// @param x Across.
/// @param y Down.
[[nodiscard]] int selectedAt(const Document& document, int x, int y) {
	return byteFromUnit(document.selection().pixel(x, y).alpha);
}

/// @brief Paints a rectangle of one colour on an image, as one step.
/// @param document The document.
/// @param image The image.
/// @param area x, y, width and height.
/// @param pixel The colour.
void paint(Document& document, std::uint64_t image, std::array<int, 4> area, const Pixel& pixel) {
	document.begin("Paint");
	for(int y = area[1]; y < area[1] + area[3]; ++y)
		for(int x = area[0]; x < area[0] + area[2]; ++x)
			document.editTile(image, x / tileSize, y / tileSize).setPixel(x % tileSize, y % tileSize, pixel);
	document.commit();
}

} // namespace

TEST_CASE("imaging selection: rectangles and ellipses cover their pixels, sparsely") {
	const Coverage rectangle = rectangleCoverage(1200, 600, {10.0f, 20.0f}, {900.5f, 590.0f});
	CHECK(valueAt(rectangle, 10, 20) == 255);
	CHECK(valueAt(rectangle, 9, 20) == 0);
	CHECK(valueAt(rectangle, 900, 300) == 128);
	CHECK(valueAt(rectangle, 899, 589) == 255);
	CHECK(valueAt(rectangle, 500, 590) == 0);
	// The tiles at (1, 1) and (2, 1) are entirely inside: the one shared white tile; the last column is outside: absent
	CHECK(rectangle.at({1, 1}) == rectangle.at({2, 1}));
	CHECK(rectangle.at({1, 1}) != rectangle.at({3, 1}));
	CHECK_FALSE(rectangle.contains({4, 0}));

	const Coverage ellipse = ellipseCoverage(400, 400, {100.0f, 100.0f}, {300.0f, 300.0f});
	CHECK(valueAt(ellipse, 200, 200) == 255);
	CHECK(valueAt(ellipse, 105, 105) == 0);
	CHECK(valueAt(ellipse, 299, 200) > 0);
	int area = 0;
	for(int y = 0; y < 400; ++y)
		for(int x = 0; x < 400; ++x) area += valueAt(ellipse, x, y);
	CHECK(static_cast<float>(area) / 255.0f == doctest::Approx(3.14159265f * 100.0f * 100.0f).epsilon(0.002));
}

TEST_CASE("imaging selection: lassos are even-odd, bounded, and ignore bad points") {
	const std::array<Point, 5> star{{{50.0f, 0.0f}, {80.0f, 100.0f}, {0.0f, 35.0f}, {100.0f, 35.0f}, {20.0f, 100.0f}}};
	const Coverage coverage = polygonCoverage(100, 100, star, Limits{});
	CHECK(valueAt(coverage, 50, 20) == 255);
	CHECK(valueAt(coverage, 50, 55) == 0); // the pentagon inside a star is outside, even-odd
	Limits limits;
	limits.polygonPoints = 4;
	CHECK_THROWS_AS(static_cast<void>(polygonCoverage(100, 100, star, limits)), affresco::DocumentError);
	const std::array<Point, 3> bad{{{NAN, 0.0f}, {INFINITY, 5.0f}, {1.0f, 1.0f}}};
	CHECK(polygonCoverage(100, 100, bad, Limits{}).empty());
}

TEST_CASE("imaging selection: modes combine, invert, deselect, and undo") {
	Document document(300, 300);
	select(document, "Rectangular Marquee", rectangleCoverage(300, 300, {0.0f, 0.0f}, {100.0f, 100.0f}),
	       SelectionMode::replace);
	CHECK(document.hasSelection());
	select(document, "Add", rectangleCoverage(300, 300, {200.0f, 200.0f}, {300.0f, 300.0f}), SelectionMode::add);
	CHECK(selectedAt(document, 50, 50) == 255);
	CHECK(selectedAt(document, 250, 250) == 255);
	select(document, "Subtract", rectangleCoverage(300, 300, {0.0f, 0.0f}, {50.0f, 300.0f}), SelectionMode::subtract);
	CHECK(selectedAt(document, 25, 25) == 0);
	CHECK(selectedAt(document, 75, 75) == 255);
	select(document, "Intersect", rectangleCoverage(300, 300, {60.0f, 60.0f}, {300.0f, 300.0f}),
	       SelectionMode::intersect);
	CHECK(selectedAt(document, 55, 55) == 0);
	CHECK(selectedAt(document, 70, 70) == 255);
	CHECK(selectionBounds(document) == std::array<int, 4>{0, 0, 300, 300});
	invertSelection(document);
	CHECK(selectedAt(document, 70, 70) == 0);
	CHECK(selectedAt(document, 150, 150) == 255);
	deselect(document);
	CHECK_FALSE(document.hasSelection());
	CHECK(selectedAt(document, 150, 150) == 0);
	document.undo();
	CHECK(document.hasSelection());
	CHECK(selectedAt(document, 150, 150) == 255);
	select(document, "Intersect", rectangleCoverage(300, 300, {0.0f, 0.0f}, {1.0f, 1.0f}), SelectionMode::subtract);
	selectAll(document);
	CHECK(selectedAt(document, 299, 299) == 255);
}

TEST_CASE("imaging selection: the magic wand and colour range") {
	Document document(600, 300);
	const std::uint64_t pixels = document.activeLayer()->pixels;
	paint(document, pixels, {0, 0, 600, 300}, {1.0f, 1.0f, 1.0f, 1.0f});
	paint(document, pixels, {100, 100, 400, 20}, {0.0f, 0.0f, 0.0f, 1.0f});
	paint(document, pixels, {550, 10, 20, 20}, {0.05f, 0.05f, 0.05f, 1.0f});
	const Coverage wand = wandCoverage(document, 300, 110, {});
	CHECK(valueAt(wand, 100, 100) == 255);
	CHECK(valueAt(wand, 499, 119) == 255);
	CHECK(valueAt(wand, 99, 100) == 0);
	CHECK(valueAt(wand, 560, 20) == 0); // like it, but not connected
	CHECK(valueAt(wandCoverage(document, 300, 110, {32, false, false}), 560, 20) == 255);
	CHECK(valueAt(wandCoverage(document, 300, 110, {5, false, true}), 560, 20) == 0);
	const Coverage range = colorRangeCoverage(document, {0.0f, 0.0f, 0.0f, 1.0f}, 26, false);
	CHECK(valueAt(range, 300, 110) == 255);
	CHECK(valueAt(range, 560, 20) > 100);
	CHECK(valueAt(range, 560, 20) < 200);
	CHECK(valueAt(range, 5, 5) == 0);
	Limits limits;
	limits.floodPixels = 1000;
	Document small(100, 100, limits);
	CHECK_THROWS_AS(static_cast<void>(wandCoverage(small, 5, 5, {})), affresco::DocumentError);
}

TEST_CASE("imaging filters: blur keeps flat colour, spreads edges, and stays within the selection") {
	Document document(512, 256);
	const std::uint64_t pixels = document.activeLayer()->pixels;
	paint(document, pixels, {0, 0, 256, 256}, {1.0f, 0.0f, 0.0f, 1.0f});
	Filter blur;
	blur.radius = 4.0f;
	applyFilter(document, pixels, blur);
	const Image& image = *document.image(pixels);
	CHECK(image.pixel(100, 100).red == doctest::Approx(1.0f).epsilon(0.01));
	CHECK(image.pixel(100, 100).alpha == doctest::Approx(1.0f).epsilon(0.01));
	CHECK(image.pixel(256, 100).alpha == doctest::Approx(0.5f).epsilon(0.1));
	CHECK(image.pixel(256, 100).red == doctest::Approx(1.0f).epsilon(0.01)); // premultiplied: no dark fringe
	CHECK(image.pixel(300, 100).alpha == 0.0f);
	CHECK(document.history().steps().back().name == "Gaussian Blur");
	document.undo();
	select(document, "Marquee", rectangleCoverage(512, 256, {0.0f, 0.0f}, {512.0f, 128.0f}), SelectionMode::replace);
	applyFilter(document, pixels, blur);
	CHECK(image.pixel(256, 50).alpha < 0.9f);
	CHECK(image.pixel(256, 200).alpha == 0.0f);
	CHECK(image.pixel(255, 200).alpha == 1.0f);
}

TEST_CASE("imaging filters: sharpen, noise, motion, preview runs, and feather") {
	Document document(256, 256);
	const std::uint64_t pixels = document.activeLayer()->pixels;
	paint(document, pixels, {0, 0, 256, 256}, {0.5f, 0.5f, 0.5f, 1.0f});
	paint(document, pixels, {128, 0, 128, 256}, {0.7f, 0.7f, 0.7f, 1.0f});
	Filter unsharp;
	unsharp.kind = FilterKind::unsharpMask;
	unsharp.amount = 1.0f;
	applyFilter(document, pixels, unsharp);
	const Image& image = *document.image(pixels);
	CHECK(image.pixel(127, 50).red < 0.49f);
	CHECK(image.pixel(128, 50).red > 0.71f);
	CHECK(image.pixel(10, 50).red == doctest::Approx(0.5f).epsilon(0.01));
	Filter noise;
	noise.kind = FilterKind::addNoise;
	noise.amount = 0.2f;
	applyFilter(document, pixels, noise);
	CHECK(image.pixel(10, 50).red != image.pixel(11, 50).red);
	{
		Filter motion;
		motion.kind = FilterKind::motionBlur;
		FilterRun run(document, pixels, motion, "Motion Blur");
		CHECK_FALSE(run.runRegion(0, 0, 256, 256, 0));
		CHECK(run.runRegion(0, 0, 10, 10));
	} // a run never finished puts the tiles back
	CHECK(document.history().steps().back().name == "Add Noise");
	CHECK(byteFromUnit(filterKindOf("motion-blur") == FilterKind::motionBlur ? 1.0f : 0.0f) == 255);

	select(document, "Marquee", rectangleCoverage(256, 256, {64.0f, 64.0f}, {192.0f, 192.0f}), SelectionMode::replace);
	featherSelection(document, 8.0f);
	CHECK(selectedAt(document, 128, 128) == 255);
	CHECK(selectedAt(document, 64, 128) > 100);
	CHECK(selectedAt(document, 64, 128) < 155);
	CHECK(selectedAt(document, 40, 128) == 0);
	CHECK(document.history().steps().back().name == "Feather");
}
