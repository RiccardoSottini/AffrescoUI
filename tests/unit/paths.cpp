/// @file
/// @brief Unit tests for vector paths: parsing SVG path data within limits (VectorPath), flattening, the meshes that
/// fill (both rules) and stroke (joins, caps, dashes) them, SVG icons decoded as images, and the format's `path` node.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Bitmap.hpp>
#include <affresco/render/PathView.hpp>
#include <affresco/render/VectorPath.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using affresco::FillRule;
using affresco::MeshVertex;
using affresco::StrokeStyle;
using affresco::VectorPath;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Flattens a path at its own size.
/// @param data Its path data.
/// @return Its contours.
std::vector<VectorPath::Contour> contoursOf(std::string_view data) { return VectorPath::parse(data).flatten({}); }

/// @brief The parse error of path data that should be refused.
/// @param data The data.
/// @return The error text.
std::string refusal(std::string_view data) {
	try {
		(void)VectorPath::parse(data);
	} catch(const std::invalid_argument& error) {
		return error.what();
	}
	FAIL("accepted: ", data);
	return {};
}

/// @brief The coverage a mesh gives a point: the largest of the fully covered quads (coverage 1) that hold it.
/// @param mesh The mesh, 4 vertices a quad.
/// @param point The point.
/// @return 1 if a fully covered quad holds it, otherwise 0.
float coverageAt(std::span<const MeshVertex> mesh, affresco::Vec2 point) {
	for(std::size_t quad = 0; quad + 3 < mesh.size(); quad += 4) {
		bool solid = true, inside = true;
		for(std::size_t corner = 0; corner < 4; ++corner) {
			const affresco::Vec2 from = mesh[quad + corner].position, to = mesh[quad + (corner + 1) % 4].position;
			solid = solid && mesh[quad + corner].coverage == 1.0f;
			const float cross = (to.x - from.x) * (point.y - from.y) - (to.y - from.y) * (point.x - from.x);
			inside = inside && cross >= -1e-4f;
		}
		bool reversed = true;
		for(std::size_t corner = 0; corner < 4 && !inside; ++corner) {
			const affresco::Vec2 from = mesh[quad + corner].position, to = mesh[quad + (corner + 1) % 4].position;
			reversed = reversed && (to.x - from.x) * (point.y - from.y) - (to.y - from.y) * (point.x - from.x) <= 1e-4f;
		}
		if(solid && (inside || reversed)) return 1.0f;
	}
	return 0.0f;
}

/// @brief A screen with one path node, "mark".
constexpr std::string_view markScreen = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"colors": { "accent": "#7c3aed" },
	"root": { "children": [
		{ "type": "path", "id": "mark", "width": 48, "height": 48, "viewBox": "0 0 24 24",
		  "d": "M4 12 L10 18 L20 6", "stroke": "@accent", "strokeWidth": 3, "strokeJoin": "round",
		  "strokeCap": "square", "strokeDash": [4, 2], "fillRule": "evenodd" }
	] }
})json";

} // namespace

TEST_CASE("render: path data parses every command, absolute and relative") {
	const VectorPath absolute = VectorPath::parse("M10 10 L20 10 H30 V20 C30 25 25 30 20 30 S10 25 10 20 Q10 15 15 15 "
	                                              "T20 10 A5 5 0 0 1 30 10 Z");
	const VectorPath relative = VectorPath::parse("m10 10 l10 0 h10 v10 c0 5-5 10-10 10 s-10-5-10-10 q0-5 5-5 t5-5 "
	                                              "a5 5 0 0 1 10 0 z");
	CHECK_FALSE(absolute.empty());
	CHECK(absolute.bounds() == relative.bounds());
	CHECK(VectorPath::parse("M0 0 1 1 2 0") == VectorPath::parse("M0 0 L1 1 L2 0")); // repeated pairs after M are lines
	CHECK(VectorPath::parse("M.5.5l1-1") == VectorPath::parse("M0.5 0.5 l1 -1"));    // packed numbers
	CHECK(VectorPath::parse("").empty());
	CHECK_FALSE(VectorPath::parse("").bounds());
}

TEST_CASE("render: broken path data is refused, naming the place") {
	CHECK(refusal("L10 10").find("must start with M") != std::string::npos);
	CHECK(refusal("M0 0 L10").find("expected a number at character") != std::string::npos);
	CHECK(refusal("M0 0 X5 5").find("unknown command 'X'") != std::string::npos);
	CHECK(refusal("M0 0 L5 5 Z 1 1").find("expected a command") != std::string::npos); // Z takes no numbers
	CHECK_FALSE(refusal("M1e9 0").empty()); // past the largest coordinate
	VectorPath::Limits small;
	small.commands = 4;
	CHECK_THROWS_AS((void)VectorPath::parse("M0 0 L1 1 L2 2 L3 3 L4 4", small), std::invalid_argument);
	small = {};
	small.dataBytes = 8;
	CHECK_THROWS_AS((void)VectorPath::parse("M0 0 L10 10", small), std::invalid_argument);
}

TEST_CASE("render: flattening keeps curves within the tolerance and the points within the limit") {
	const std::vector<VectorPath::Contour> lines = contoursOf("M0 0 L10 0 L10 10 Z M20 20 L30 30");
	REQUIRE(lines.size() == 2);
	CHECK(lines[0].closed);
	CHECK_FALSE(lines[1].closed);
	CHECK(lines[0].points.size() == 3);

	const VectorPath curve = VectorPath::parse("M0 0 C0 100 100 100 100 0");
	CHECK(curve.flatten({}, 0.05f).front().points.size() > curve.flatten({}, 2.0f).front().points.size());
	const std::vector<VectorPath::Contour> scaled = curve.flatten({{2.0f, 2.0f}, {5.0f, 5.0f}});
	CHECK(scaled.front().points.front() == affresco::Vec2{5.0f, 5.0f});
	CHECK(scaled.front().points.back() == affresco::Vec2{205.0f, 5.0f});

	std::string many = "M0 0";
	for(int index = 0; index < 3000; ++index) many += " c0 1e5 1e5 1e5 1e5 0"; // within the 64 KiB limit
	std::size_t points = 0;
	for(const VectorPath::Contour& contour : VectorPath::parse(many).flatten({}, 0.001f))
		points += contour.points.size();
	CHECK(points <= affresco::flattenPointLimit);
}

TEST_CASE("render: fills follow the rule, and anti-alias with a fringe") {
	// Two squares, one inside the other, wound the same way: non-zero fills the hole, even-odd leaves it
	const std::vector<VectorPath::Contour> nested = contoursOf("M0 0 H30 V30 H0 Z M10 10 H20 V20 H10 Z");
	std::vector<MeshVertex> nonZero, evenOdd;
	affresco::appendFill(nested, FillRule::nonZero, nonZero);
	affresco::appendFill(nested, FillRule::evenOdd, evenOdd);
	CHECK(coverageAt(nonZero, {15.0f, 15.0f}) == 1.0f);
	CHECK(coverageAt(evenOdd, {15.0f, 15.0f}) == 0.0f);
	CHECK(coverageAt(evenOdd, {5.0f, 5.0f}) == 1.0f);
	CHECK(coverageAt(evenOdd, {35.0f, 5.0f}) == 0.0f);
	CHECK(nonZero.size() % 4 == 0);

	std::vector<MeshVertex> sharp;
	affresco::appendFill(nested, FillRule::nonZero, sharp, 0.0f);
	CHECK(sharp.size() < nonZero.size()); // no fringe
	bool fades = false;
	for(const MeshVertex& vertex : nonZero) fades = fades || vertex.coverage == 0.0f;
	CHECK(fades);
}

TEST_CASE("render: strokes have their width, joins, caps and dashes") {
	const std::vector<VectorPath::Contour> line = contoursOf("M0 0 L100 0");
	StrokeStyle style;
	style.width = 10.0f;
	std::vector<MeshVertex> butt;
	affresco::appendStroke(line, style, butt, 0.0f);
	CHECK(coverageAt(butt, {50.0f, 4.0f}) == 1.0f);
	CHECK(coverageAt(butt, {50.0f, 6.0f}) == 0.0f);
	CHECK(coverageAt(butt, {-3.0f, 0.0f}) == 0.0f);

	style.cap = affresco::LineCap::square;
	std::vector<MeshVertex> square;
	affresco::appendStroke(line, style, square, 0.0f);
	CHECK(coverageAt(square, {-3.0f, 0.0f}) == 1.0f);

	style.cap = affresco::LineCap::butt;
	style.dash = {10.0f, 10.0f};
	std::vector<MeshVertex> dashed;
	affresco::appendStroke(line, style, dashed, 0.0f);
	CHECK(coverageAt(dashed, {5.0f, 0.0f}) == 1.0f);
	CHECK(coverageAt(dashed, {15.0f, 0.0f}) == 0.0f);
	CHECK(coverageAt(dashed, {25.0f, 0.0f}) == 1.0f);

	const std::vector<VectorPath::Contour> corner = contoursOf("M0 0 L50 0 L50 50");
	style.dash.clear();
	std::vector<MeshVertex> miter, bevel;
	affresco::appendStroke(corner, style, miter, 0.0f);
	style.join = affresco::LineJoin::bevel;
	affresco::appendStroke(corner, style, bevel, 0.0f);
	CHECK(coverageAt(miter, {54.0f, -4.0f}) == 1.0f); // the miter's tip
	CHECK(coverageAt(bevel, {54.0f, -4.0f}) == 0.0f);
}

TEST_CASE("render: fills and strokes stop at the quad limit, drawing whole pieces") {
	// Two combs whose teeth cross at every height: each piece of a band is a trapezoid a tooth
	std::string combs = "M0 0";
	for(int tooth = 0; tooth < 2000; ++tooth) combs += " l1 10 l1-10";
	combs += " Z M0.5 0";
	for(int tooth = 0; tooth < 2000; ++tooth) combs += " l1.01 7 l1.01-7";
	combs += " Z";
	std::vector<MeshVertex> fill;
	affresco::appendFill(contoursOf(combs), FillRule::evenOdd, fill, 0.0f);
	CHECK(fill.size() / 4 <= affresco::meshQuadLimit);

	// A long square stroked with short round-capped dashes and dots: circles, far more than the limit
	StrokeStyle style;
	style.width = 3.5f;
	style.join = affresco::LineJoin::round;
	style.cap = affresco::LineCap::round;
	style.dash = {3.0f, 0.5f, 0.0f}; // odd, so repeated: dashes of no length between, each a dot
	std::vector<MeshVertex> stroke;
	affresco::appendStroke(contoursOf("M0 0 H20000 V20000 H0 Z"), style, stroke);
	CHECK(stroke.size() / 4 <= affresco::meshQuadLimit);
	CHECK(stroke.size() / 4 > affresco::meshQuadLimit - 200); // drawn in part, up to the limit
}

TEST_CASE("render: a path scaled past the float range draws nothing, and its dashes end") {
	// A huge scale (a tiny view box) turns the points infinite, and the line between two of them NaN long
	const VectorPath path = VectorPath::parse("M0 0 L1e6 0 L1e6 1");
	const affresco::PathTransform huge{{1e33f, 1e33f}, {0.0f, 0.0f}};
	StrokeStyle style;
	style.width = 2.0f;
	style.dash = {4.0f, 4.0f};
	const float infinity = std::numeric_limits<float>::infinity();
	for(const std::vector<VectorPath::Contour>& contours :
	    {path.flatten(huge), std::vector<VectorPath::Contour>{{{{0.0f, 0.0f}, {infinity, 0.0f}, {infinity, 1.0f}}, false}}}) {
		std::vector<MeshVertex> stroke, fill;
		affresco::appendStroke(contours, style, stroke, 1.0f); // returns
		affresco::appendFill(contours, FillRule::nonZero, fill, 1.0f);
		CHECK(stroke.empty());
		CHECK(fill.empty());
	}
}

TEST_CASE("render: an arc and a path scaled far past the screen give finite meshes") {
	// The arc's radii are scaled up to reach its end (SVG F.6.6): a radius of 1e-30 makes the other about 5e29
	StrokeStyle style;
	style.width = 1.5f;
	style.join = affresco::LineJoin::round;
	style.cap = affresco::LineCap::round;
	style.dash = {3.0f, 0.5f, 0.0f};
	const auto finite = [](const std::vector<MeshVertex>& mesh) {
		return std::ranges::all_of(mesh, [](const MeshVertex& vertex) {
			return std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) && vertex.coverage >= 0.0f &&
			       vertex.coverage <= 1.0f;
		});
	};
	for(const std::vector<VectorPath::Contour>& contours :
	    {VectorPath::parse("M0 0 A 1e-30 1 30 0 0 1 1").flatten({{0.01f, 0.01f}, {3.0f, 5.0f}}),
	     VectorPath::parse("M0 0 L1e6 0 L1e6 1").flatten({{1e30f, 1e30f}, {0.0f, 0.0f}})}) {
		std::vector<MeshVertex> stroke, fill;
		affresco::appendStroke(contours, style, stroke);
		affresco::appendFill(contours, FillRule::evenOdd, fill);
		CHECK(finite(stroke));
		CHECK(finite(fill));
	}
}

TEST_CASE("render: a curve scaled near the float range flattens into at most 64 lines") {
	// Finite points, but a bend whose step count is past int's range (a view box a billionth of a unit wide)
	const VectorPath path = VectorPath::parse("M0 0 C1e6 0 0 1e6 1e6 1e6");
	const std::vector<VectorPath::Contour> contours = path.flatten({{1e12f, 1e12f}, {0.0f, 0.0f}});
	REQUIRE(contours.size() == 1);
	CHECK(contours[0].points.size() == 65);
}

TEST_CASE("render: SVG icons decode as images, within the limits") {
	constexpr std::string_view icon = R"svg(<?xml version="1.0"?>
<svg xmlns="http://www.w3.org/2000/svg" width="12" height="8" viewBox="0 0 24 16">
	<rect width="24" height="16" fill="#ff0000"/>
	<g transform="translate(12 0)"><path d="M0 0 H12 V16 H0 Z" fill="blue" opacity="0.5"/></g>
</svg>)svg";
	const auto bytes = std::as_bytes(std::span(icon));
	const std::span<const std::uint8_t> data(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
	const auto bitmap = affresco::Bitmap::decode(data, {});
	REQUIRE(bitmap);
	CHECK(bitmap->width() == 24); // twice its size, for crisp icons
	CHECK(bitmap->height() == 16);
	const auto pixels = bitmap->pixels();
	const std::size_t left = (std::size_t{8} * 24 + 4) * 4, right = (std::size_t{8} * 24 + 20) * 4;
	CHECK(pixels[left] == 255);
	CHECK(pixels[left + 3] == 255);
	CHECK(pixels[right + 2] > 100); // blue over red, half
	CHECK(pixels[right] > 100);

	constexpr std::string_view deep = R"(<svg width="4" height="4">)";
	std::string nested(deep);
	for(int level = 0; level < 100; ++level) nested += "<g>";
	const std::span<const std::uint8_t> deepData(reinterpret_cast<const std::uint8_t*>(nested.data()), nested.size());
	CHECK_THROWS_AS((void)affresco::Bitmap::decode(deepData, {}), std::runtime_error);
}

TEST_CASE("format: a path node applies its properties, and refuses broken ones naming the place") {
	const ManualClock clock;
	const auto headless = createHeadlessWindow(200, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(markScreen);
	screen.build(*headless);
	headless->render();

	const auto* mark = dynamic_cast<const affresco::PathView*>(screen.find("mark"));
	REQUIRE(mark);
	CHECK(mark->path() == VectorPath::parse("M4 12 L10 18 L20 6"));
	CHECK(mark->viewBox() == affresco::Rect{0.0f, 0.0f, 24.0f, 24.0f});
	CHECK(mark->fillRule() == FillRule::evenOdd);
	CHECK(mark->strokeColor() == affresco::Color::parseHex("#7c3aed").value_or(affresco::Color{}));
	CHECK(mark->strokeStyle().width == 3.0f);
	CHECK(mark->strokeStyle().join == affresco::LineJoin::round);
	CHECK(mark->strokeStyle().cap == affresco::LineCap::square);
	CHECK(mark->strokeStyle().dash == std::vector<float>{4.0f, 2.0f});

	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#mark/d": "M0 0 L"}})", &error));
	CHECK(error.find(".d") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#mark/viewBox": "0 0 -1 4"}})", &error));
	CHECK(error.find("viewBox") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#mark/strokeJoin": "pointy"}})", &error));
	CHECK(error.find("strokeJoin") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#mark/strokeDash": [1, -2]}})", &error));
	CHECK(error.find("strokeDash") != std::string::npos);

	REQUIRE(screen.applyRevision(R"({"set": {"#mark/d": "M0 0 H24"}, "remove": ["#mark/strokeDash"]})"));
	headless->render();
	CHECK(mark->path() == VectorPath::parse("M0 0 H24"));
	CHECK(mark->strokeStyle().dash.empty());
}
