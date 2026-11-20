/// @file
/// @brief The imaging module's vector content: paths flattened, strokes, shape layers drawn, and a layer's content and
/// pixels changed and undone together.

#include "affresco/imaging/Document.hpp"
#include "affresco/imaging/Shapes.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>

using namespace affresco::imaging;

namespace {

/// @brief A coverage's value at a pixel, 0 to 255.
/// @param coverage The coverage.
/// @param x Across.
/// @param y Down.
[[nodiscard]] int valueAt(const Coverage& coverage, int x, int y) {
	const auto found = coverage.find({x / tileSize, y / tileSize});
	if(found == coverage.end()) return 0;
	return found->second
	    ->bytes()[static_cast<std::size_t>(y % tileSize) * tileSize + static_cast<std::size_t>(x % tileSize)];
}

/// @brief A rectangle shape's content, filled red.
[[nodiscard]] LayerContent redBox() {
	LayerContent content;
	content.kind = ContentKind::shape;
	content.origin = {10.0f, 10.0f};
	content.corner = {50.0f, 30.0f};
	content.fill = {1.0f, 0.0f, 0.0f, 1.0f};
	return content;
}

} // namespace

TEST_CASE("a path flattens: straight between corners, curved through handles, closed back to its start") {
	const std::array<PathPoint, 2> corners{PathPoint{{0, 0}, {0, 0}, {0, 0}}, PathPoint{{10, 0}, {10, 0}, {10, 0}}};
	CHECK(flattenPath(corners, false).size() == 2);
	std::array<PathPoint, 2> curve = corners;
	curve[0].out = {0, 10};
	curve[1].in = {10, 10};
	const auto points = flattenPath(curve, false);
	CHECK(points.size() > 4);
	CHECK(points.back().x == doctest::Approx(10.0f));
	float lowest = 0.0f;
	for(const Point point : points) lowest = std::max(lowest, point.y);
	CHECK(lowest == doctest::Approx(7.5f)); // a cubic's middle is three quarters of the way to its handles
	CHECK(flattenPath({}, true).empty());
}

TEST_CASE("a stroke covers its line's width, with round caps, and nothing for no width") {
	const std::array<Point, 2> line{Point{10, 20}, Point{60, 20}};
	const Coverage stroke = strokeCoverage(100, 100, line, false, 6.0f, Limits{});
	CHECK(valueAt(stroke, 30, 20) == 255);
	CHECK(valueAt(stroke, 30, 26) == 0);
	CHECK(valueAt(stroke, 8, 20) > 0); // the round cap
	CHECK(strokeCoverage(100, 100, line, false, 0.0f, Limits{}).empty());
}

TEST_CASE("a shape layer is drawn from its content: filled inside, stroked on its outline") {
	LayerContent content = redBox();
	auto image = renderShape(content, 64, 64, PixelFormat::rgba8, Limits{});
	CHECK(image->pixel(30, 20).red == doctest::Approx(1.0f));
	CHECK(image->pixel(30, 20).alpha == doctest::Approx(1.0f));
	CHECK(image->pixel(5, 5).alpha == 0.0f);
	content.fill.alpha = 0.0f;
	content.stroke = {0.0f, 0.0f, 1.0f, 1.0f};
	content.strokeWidth = 4.0f;
	image = renderShape(content, 64, 64, PixelFormat::rgba8, Limits{});
	CHECK(image->pixel(30, 20).alpha == 0.0f);
	CHECK(image->pixel(30, 10).blue == doctest::Approx(1.0f));
}

TEST_CASE("a content layer refuses paint; its content and pixels change and undo as one step; rasterized it paints") {
	Document document(64, 64);
	const LayerContent content = redBox();
	const std::uint64_t id =
	    document.addContentLayer("Box", content, renderShape(content, 64, 64, PixelFormat::rgba8, Limits{}));
	const std::uint64_t pixels = document.layer(id)->pixels;
	CHECK(document.paintable(pixels).has_value());
	LayerContent moved = movedContent(content, 5.0f, 0.0f);
	document.setContent(id, "Edit Shape", moved, renderShape(moved, 64, 64, PixelFormat::rgba8, Limits{}));
	CHECK(document.layer(id)->content.origin.x == doctest::Approx(15.0f));
	CHECK(document.image(pixels)->pixel(12, 20).alpha == 0.0f);
	document.undo();
	CHECK(document.layer(id)->content.origin.x == doctest::Approx(10.0f));
	CHECK(document.image(pixels)->pixel(12, 20).alpha == doctest::Approx(1.0f));
	document.setContent(id, "Rasterize Layer", {}, nullptr);
	CHECK(document.layer(id)->content.kind == ContentKind::none);
	CHECK_FALSE(document.paintable(pixels).has_value());
	CHECK(document.image(pixels)->pixel(12, 20).alpha == doctest::Approx(1.0f));
}

TEST_CASE("content reads back from its JSON, and malformed or oversized content is refused") {
	LayerContent content = redBox();
	content.shape = ShapeKind::path;
	content.points = {PathPoint{{1, 2}, {3, 4}, {5, 6}}};
	LayerContent read;
	readContent(contentToJson(content), read);
	CHECK(read == content);
	CHECK_THROWS_AS(readContent(json{{"kind", "cloud"}}, read), DocumentError);
	CHECK_THROWS_AS(readContent(json{{"fill", "red"}}, read), DocumentError);
	CHECK_THROWS_AS(readContent(json{{"text", std::string(maxTextBytes + 1, 'a')}}, read), DocumentError);
	CHECK_THROWS_AS((void)pointsFromJson(json::array({1})), DocumentError);
	readContent(json{{"size", 1e9}}, read);
	CHECK(read.size == doctest::Approx(1000.0f));
}
