/// @file
/// @brief Pictures with round corners: how a recorder draws an image only inside a rounded shape.

#include "paint/PaintRecorder.hpp"
#include "paint/RetainedGeometry.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

using affresco::Bitmap;
using affresco::CornerRadii;
using affresco::Rect;
using affresco::detail::PaintRecord;
using affresco::detail::PaintRecorder;
using affresco::detail::Vertex;

namespace {

/// @brief A 2 by 2 white image.
/// @return The image.
std::shared_ptr<const Bitmap> whiteImage() {
	const std::vector<std::uint8_t> pixels(16, 255);
	return Bitmap::fromPixels(2, 2, pixels, {});
}

/// @brief The rectangle the vertices of a record cover.
/// @param record The record.
/// @return Their bounds (empty if none).
Rect extentOf(const PaintRecord& record) {
	if(record.vertices.empty()) return {};
	Rect bounds{record.vertices[0].x, record.vertices[0].y, record.vertices[0].x, record.vertices[0].y};
	for(const Vertex& vertex : record.vertices) {
		bounds.left = std::min(bounds.left, vertex.x);
		bounds.top = std::min(bounds.top, vertex.y);
		bounds.right = std::max(bounds.right, vertex.x);
		bounds.bottom = std::max(bounds.bottom, vertex.y);
	}
	return bounds;
}

} // namespace

TEST_CASE("render: a rounded image is cut into rows inside its corners, a square one stays one quad") {
	const auto image = whiteImage();
	const Rect clip{0.0f, 0.0f, 400.0f, 400.0f}, frame{10.0f, 10.0f, 110.0f, 60.0f};

	PaintRecord square;
	PaintRecorder(square, clip, clip).drawRoundedImage(*image, frame, frame, {});
	CHECK(square.vertices.size() == 4);

	PaintRecord round;
	PaintRecorder(round, clip, clip).drawRoundedImage(*image, frame, frame, CornerRadii::all(10.0f));
	REQUIRE(round.vertices.size() > 4);
	REQUIRE(round.ops.size() == 1);
	CHECK(round.ops[0].bitmap == image.get());
	// Every row stays within the shape, and the corner's own row is narrower than the middle band
	const Rect extent = extentOf(round);
	CHECK(extent.left >= frame.left);
	CHECK(extent.right <= frame.right);
	CHECK(extent.top >= frame.top);
	CHECK(extent.bottom <= frame.bottom);
	const Vertex& first = round.vertices[0];
	CHECK(first.y == doctest::Approx(frame.top));
	CHECK(first.x > frame.left + 5.0f); // the top row starts well inside the rounded corner
}

TEST_CASE("render: a rounded image keeps its panel's clip, and its texture follows the rows") {
	const auto image = whiteImage();
	const Rect clip{0.0f, 0.0f, 60.0f, 400.0f}, frame{10.0f, 10.0f, 110.0f, 60.0f};
	PaintRecord record;
	PaintRecorder(record, clip, clip).drawRoundedImage(*image, frame, frame, CornerRadii::all(8.0f));
	REQUIRE_FALSE(record.vertices.empty());
	CHECK(extentOf(record).right <= clip.right);
	// The picture is cut at the clip halfway across, so no row reaches its right edge's texels
	for(const Vertex& vertex : record.vertices) CHECK(vertex.u < 40000);
}
