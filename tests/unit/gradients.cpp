/// @file
/// @brief Gradient fills: how a recorder cuts a linear gradient into bands and a radial one into cells, with the
/// gradient's colours at their corners, and the "gradient" property a screen's nodes read.

#include "Headless.hpp"
#include "paint/PaintRecorder.hpp"
#include "paint/RetainedGeometry.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

using affresco::Color;
using affresco::CornerRadii;
using affresco::Gradient;
using affresco::Rect;
using affresco::detail::PaintRecord;
using affresco::detail::PaintRecorder;
using affresco::detail::QuadKind;
using affresco::detail::Vertex;

namespace {

/// @brief A colour's bytes, as a vertex holds them.
/// @param color The colour.
/// @return Its red, green, blue and alpha bytes.
std::array<std::uint8_t, 4> bytes(Color color) { return color.toBytes(); }

/// @brief A vertex's colour bytes.
/// @param vertex The vertex.
/// @return Its red, green, blue and alpha bytes.
std::array<std::uint8_t, 4> bytes(const Vertex& vertex) { return {vertex.r, vertex.g, vertex.b, vertex.a}; }

/// @brief Records a gradient filling a rectangle.
/// @param rect The rectangle.
/// @param radii Its corners.
/// @param gradient The gradient.
/// @param clip What clips the drawing.
/// @return The record.
PaintRecord recorded(const Rect& rect, const CornerRadii& radii, const Gradient& gradient,
                     const Rect& clip = {0.0f, 0.0f, 400.0f, 400.0f}) {
	PaintRecord record;
	PaintRecorder recorder(record, clip, clip);
	recorder.fillGradient(rect, radii, gradient);
	return record;
}

/// @brief Tells whether every vertex at a place has a colour.
/// @param record The record.
/// @param on Whether a vertex is at the place.
/// @param color The colour.
/// @return true if at least one vertex is there and all of them have the colour.
template <typename On> bool allAt(const PaintRecord& record, const On& on, Color color) {
	bool any = false;
	for(const Vertex& vertex : record.vertices) {
		if(!on(vertex)) continue;
		any = true;
		if(bytes(vertex) != bytes(color)) return false;
	}
	return any;
}

/// @brief A screen with a card whose background is a gradient, and an accent colour for it to name.
constexpr std::string_view bannerScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"colors": { "accent": "#3366ff" },
	"root": { "type": "panel", "children": [
		{ "id": "banner", "width": 200, "height": 80, "color": "#101820",
		  "gradient": { "kind": "radial", "stops": ["@accent", { "at": 0.75, "color": "transparent" }] } }
	] }
})json";

/// @brief Applies a revision that should fail, and returns its error.
/// @param screen The screen.
/// @param revision The revision.
/// @return The error text.
std::string rejection(affresco::Screen& screen, std::string_view revision) {
	std::string error;
	CHECK_FALSE(screen.applyRevision(revision, &error));
	return error;
}

} // namespace

TEST_CASE(
    "render: a square linear gradient is one plain band between each two stops, the stops' colours on its edges") {
	const Gradient gradient{
	    .angle = 180.0,
	    .stops = {{0.0, affresco::colors::black}, {0.5, affresco::colors::red}, {1.0, affresco::colors::white}}};
	const PaintRecord record = recorded({0.0f, 0.0f, 100.0f, 100.0f}, {}, gradient);
	CHECK(record.vertices.size() == 2 * affresco::detail::verticesPerQuad);
	CHECK_FALSE(record.shapes);
	CHECK(std::ranges::all_of(record.vertices, [](const Vertex& vertex) {
		return vertex.kind == static_cast<std::uint16_t>(QuadKind::plain);
	}));
	CHECK(allAt(record, [](const Vertex& vertex) { return vertex.y == 0.0f; }, affresco::colors::black));
	CHECK(allAt(record, [](const Vertex& vertex) { return vertex.y == 50.0f; }, affresco::colors::red));
	CHECK(allAt(record, [](const Vertex& vertex) { return vertex.y == 100.0f; }, affresco::colors::white));
}

TEST_CASE("render: a linear gradient's angle turns it, and its line reaches the corners as in CSS") {
	const Gradient across{.angle = 90.0, .stops = {{0.0, affresco::colors::black}, {1.0, affresco::colors::white}}};
	const PaintRecord sideways = recorded({0.0f, 0.0f, 200.0f, 100.0f}, {}, across);
	CHECK(sideways.vertices.size() == affresco::detail::verticesPerQuad);
	CHECK(allAt(sideways, [](const Vertex& vertex) { return vertex.x == 0.0f; }, affresco::colors::black));
	CHECK(allAt(sideways, [](const Vertex& vertex) { return vertex.x == 200.0f; }, affresco::colors::white));

	// At 135 degrees the start is the top left corner and the end the bottom right one
	const Gradient diagonal{.angle = 135.0, .stops = {{0.0, affresco::colors::black}, {1.0, affresco::colors::white}}};
	const PaintRecord corners = recorded({0.0f, 0.0f, 100.0f, 100.0f}, {}, diagonal);
	CHECK(allAt(
	    corners, [](const Vertex& vertex) { return vertex.x == 0.0f && vertex.y == 0.0f; }, affresco::colors::black));
	CHECK(allAt(
	    corners, [](const Vertex& vertex) { return vertex.x == 100.0f && vertex.y == 100.0f; },
	    affresco::colors::white));
}

TEST_CASE("render: two stops at one place change colour sharply, with no band between them") {
	const Gradient flag{.angle = 90.0,
	                    .stops = {{0.0, affresco::colors::red},
	                              {0.5, affresco::colors::red},
	                              {0.5, affresco::colors::blue},
	                              {1.0, affresco::colors::blue}}};
	const PaintRecord record = recorded({0.0f, 0.0f, 100.0f, 40.0f}, {}, flag);
	REQUIRE(record.vertices.size() == 2 * affresco::detail::verticesPerQuad);
	for(std::size_t index = 0; index < affresco::detail::verticesPerQuad; ++index) {
		CHECK(bytes(record.vertices[index]) == bytes(affresco::colors::red));
		CHECK(bytes(record.vertices[index + affresco::detail::verticesPerQuad]) == bytes(affresco::colors::blue));
	}
}

TEST_CASE("render: a gradient to transparent fades its colour, not black, and skips what is all transparent") {
	// Top to bottom, as a document's gradient goes when it gives no angle
	const Gradient fade{.angle = 180.0,
	                    .stops = {{0.0, affresco::colors::red}, {1.0, affresco::colors::transparent}}};
	const PaintRecord record = recorded({0.0f, 0.0f, 50.0f, 50.0f}, {}, fade);
	CHECK(allAt(record, [](const Vertex& vertex) { return vertex.y == 50.0f; }, affresco::colors::red.withAlpha(0.0f)));

	const Gradient clear{.stops = {{0.0, affresco::colors::transparent}, {1.0, affresco::colors::transparent}}};
	CHECK(recorded({0.0f, 0.0f, 50.0f, 50.0f}, {}, clear).vertices.empty());
}

TEST_CASE("render: rounded corners make a gradient shape quarters, anti-aliased by distance like a rounded fill") {
	const Gradient gradient{.stops = {{0.0, affresco::colors::black}, {1.0, affresco::colors::white}}};
	const PaintRecord record = recorded({10.0f, 10.0f, 110.0f, 110.0f}, CornerRadii::all(8.0f), gradient);
	CHECK(record.shapes);
	REQUIRE_FALSE(record.vertices.empty());
	CHECK(std::ranges::all_of(record.vertices, [](const Vertex& vertex) {
		return vertex.kind == static_cast<std::uint16_t>(QuadKind::shape) && vertex.radius == 8 * 16;
	}));
	// A unit past the edges for the anti-aliased ramp: there the shape's distance is negative
	const auto corner = std::ranges::find_if(record.vertices,
	                                         [](const Vertex& vertex) { return vertex.x == 9.0f && vertex.y == 9.0f; });
	REQUIRE(corner != record.vertices.end());
	CHECK(corner->insideX == -1.0f);
	CHECK(corner->insideY == -1.0f);
	CHECK(bytes(*corner) == bytes(affresco::colors::black));
}

TEST_CASE("render: a gradient is clipped, keeping the colours of the whole rectangle") {
	const Gradient gradient{.angle = 90.0, .stops = {{0.0, affresco::colors::black}, {1.0, affresco::colors::white}}};
	const PaintRecord record = recorded({0.0f, 0.0f, 100.0f, 20.0f}, {}, gradient, {0.0f, 0.0f, 50.0f, 400.0f});
	REQUIRE(record.vertices.size() == affresco::detail::verticesPerQuad);
	CHECK(std::ranges::all_of(record.vertices, [](const Vertex& vertex) { return vertex.x <= 50.0f; }));
	CHECK(allAt(
	    record, [](const Vertex& vertex) { return vertex.x == 50.0f; },
	    Color::mix(affresco::colors::black, affresco::colors::white, 0.5f)));

	CHECK(recorded({0.0f, 0.0f, 0.0f, 20.0f}, {}, gradient).vertices.empty());
	CHECK(recorded({0.0f, 0.0f, 100.0f, 20.0f}, {}, gradient, {200.0f, 0.0f, 300.0f, 20.0f}).vertices.empty());
}

TEST_CASE(
    "render: a radial gradient is cells of about 12 units, its first colour at the centre, its last at the corners") {
	const Gradient glow{.kind = Gradient::Kind::Radial,
	                    .stops = {{0.0, affresco::colors::white}, {1.0, affresco::colors::black}}};
	const PaintRecord record = recorded({0.0f, 0.0f, 48.0f, 24.0f}, {}, glow);
	CHECK(record.vertices.size() == 4 * 2 * affresco::detail::verticesPerQuad);
	CHECK(allAt(
	    record, [](const Vertex& vertex) { return vertex.x == 24.0f && vertex.y == 12.0f; }, affresco::colors::white));
	for(const Vertex& vertex : record.vertices)
		if(vertex.x == 0.0f && vertex.y == 0.0f) CHECK(std::abs(int{vertex.r}) <= 1);

	// A large one has at most 12 cells a side
	const PaintRecord large = recorded({0.0f, 0.0f, 400.0f, 400.0f}, {}, glow);
	CHECK(large.vertices.size() == 12 * 12 * affresco::detail::verticesPerQuad);
}

TEST_CASE("format: \"gradient\" fills a node's background, names colours, and goes when it is removed") {
	const affresco::detail::ManualClock clock;
	const auto headless = affresco::detail::createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(bannerScreen);
	affresco::Window& window = *headless;
	screen.build(window);
	window.render();

	affresco::Panel* banner = screen.find("banner");
	REQUIRE(banner);
	REQUIRE(banner->gradient());
	CHECK(banner->gradient()->kind == Gradient::Kind::Radial);
	REQUIRE(banner->gradient()->stops.size() == 2);
	CHECK(banner->gradient()->stops[0] == affresco::GradientStop{0.0, Color::parseHex("#3366ff").value_or(Color{})});
	CHECK(banner->gradient()->stops[1] == affresco::GradientStop{0.75, affresco::colors::transparent});

	// A list of colours spreads them evenly, from top to bottom
	REQUIRE(screen.applyRevision(R"({"set": {"#banner/gradient": ["#000000", "#808080", "#ffffff"]}})"));
	window.render();
	REQUIRE(banner->gradient());
	CHECK(banner->gradient()->kind == Gradient::Kind::Linear);
	CHECK(banner->gradient()->angle == 180.0);
	REQUIRE(banner->gradient()->stops.size() == 3);
	CHECK(banner->gradient()->stops[1].at == 0.5);

	REQUIRE(screen.applyRevision(R"({"set": {"#banner/gradient": {"angle": 450, "stops": ["#000000"]}}})"));
	window.render();
	REQUIRE(banner->gradient());
	CHECK(banner->gradient()->angle == 90.0);
	CHECK(banner->gradient()->stops.size() == 2);

	REQUIRE(screen.applyRevision(R"({"remove": ["#banner/gradient"]})"));
	window.render();
	CHECK_FALSE(banner->gradient());
}

TEST_CASE("format: malformed gradients are rejected, naming the place") {
	const affresco::detail::ManualClock clock;
	const auto headless = affresco::detail::createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(bannerScreen);
	screen.build(*headless);
	headless->render();

	const auto rejects = [&](std::string_view revision, std::string_view expected) {
		const std::string error = rejection(screen, revision);
		INFO(error);
		CHECK(error.find(expected) != std::string::npos);
	};
	rejects(R"({"set": {"#banner/gradient": []}})", "gradient: expected a list of colours");
	rejects(R"({"set": {"#banner/gradient": "#ff0000"}})", "gradient: expected a list of colours or {kind");
	rejects(R"({"set": {"#banner/gradient": {"kind": "conic", "stops": ["#000"]}}})", "gradient.kind: expected");
	rejects(R"({"set": {"#banner/gradient": {"spread": 2, "stops": ["#000"]}}})", "gradient.spread: unknown");
	rejects(R"({"set": {"#banner/gradient": {"angle": 90}}})", R"(gradient: expected "stops")");
	rejects(R"({"set": {"#banner/gradient": {"stops": [{"at": 1.5, "color": "#000"}]}}})",
	        "gradient.stops[0].at: expected a position");
	rejects(R"({"set": {"#banner/gradient": {"stops": [{"at": 0.5}]}}})", R"(gradient.stops[0]: expected "color")");
	rejects(R"({"set": {"#banner/gradient": ["#000", "not a colour"]}})", "gradient[1]");
	rejects(R"({"set": {"#banner/gradient": {"stops": [{"at": 0, "color": "#000", "mid": 1}]}}})",
	        "gradient.stops[0].mid: unknown");

	std::string many = R"({"set": {"#banner/gradient": [)";
	for(int index = 0; index < 33; ++index) many += index == 0 ? R"("#000")" : R"(, "#000")";
	rejects(many + "]}}", "gradient: at most 32 stops");
}
