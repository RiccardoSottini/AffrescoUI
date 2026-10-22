/// @file
/// @brief The imaging module's brushes (stamps, flow, spacing, pressure, eraser, clone, masks) and fills (gradient,
/// paint bucket, Edit > Fill).

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Brush.hpp"
#include "affresco/imaging/Gradient.hpp"

#include <doctest/doctest.h>

using namespace affresco::imaging;

namespace {

/// @brief A hard tip of a size.
/// @param size Its diameter.
[[nodiscard]] BrushTip hardTip(float size) {
	BrushTip tip;
	tip.size = size, tip.hardness = 1.0f, tip.pressureSize = false;
	return tip;
}

/// @brief The colour the strokes paint unless they say otherwise.
constexpr Pixel red{1.0f, 0.0f, 0.0f, 1.0f};

/// @brief Lays a stroke's points and finishes it.
/// @param stroke The stroke.
/// @param points Its points.
/// @return The stamps laid.
std::size_t lay(Stroke& stroke, std::initializer_list<StrokePoint> points) {
	for(const StrokePoint& point : points) stroke.add(point);
	stroke.finish();
	return stroke.stamps();
}

/// @brief Paints a dab or a line in one stroke.
/// @param document The document.
/// @param target The image.
/// @param tip The tip.
/// @param points The stroke's points.
/// @param colour The colour.
/// @return The stamps laid.
std::size_t stroke(Document& document, std::uint64_t target, const BrushTip& tip,
                   std::initializer_list<StrokePoint> points, const Pixel& colour = red) {
	Stroke stroke(document, target, tip, StrokeMode::paint, colour, "Brush");
	return lay(stroke, points);
}

/// @brief Erases a dab or a line in one stroke.
/// @param document The document.
/// @param target The image.
/// @param tip The tip.
/// @param points The stroke's points.
/// @return The stamps laid.
std::size_t erase(Document& document, std::uint64_t target, const BrushTip& tip,
                  std::initializer_list<StrokePoint> points) {
	Stroke stroke(document, target, tip, StrokeMode::erase, red, "Brush");
	return lay(stroke, points);
}

} // namespace

TEST_CASE("imaging brush: stamps are round, soft by their hardness, and spaced along a stroke") {
	CHECK(stampCoverage(0.0f, 10.0f, 1.0f) == 1.0f);
	CHECK(stampCoverage(10.5f, 10.0f, 1.0f) == 0.0f);
	CHECK(stampCoverage(5.0f, 10.0f, 0.0f) == doctest::Approx(0.5f));
	Document document(300, 128);
	const std::uint64_t pixels = document.activeLayer()->pixels;
	CHECK(stroke(document, pixels, hardTip(20.0f), {{50.0f, 50.0f}, {150.0f, 50.0f}}) == 21);
	const Image& image = *document.image(pixels);
	CHECK(image.pixel(100, 50) == Pixel{1.0f, 0.0f, 0.0f, 1.0f});
	CHECK(image.pixel(100, 58).alpha == 1.0f);
	CHECK(image.pixel(100, 62).alpha == 0.0f);
	CHECK(image.pixel(165, 50).alpha == 0.0f);
	CHECK(document.history().steps().size() == 1);
	BrushTip pressed = hardTip(20.0f);
	pressed.pressureSize = true;
	stroke(document, pixels, pressed, {{250.0f, 100.0f, 0.5f}});
	CHECK(image.pixel(250, 100).alpha == 1.0f);
	CHECK(image.pixel(258, 100).alpha == 0.0f);
}

TEST_CASE("imaging brush: flow builds up to the opacity, and never past it") {
	Document document(64, 64);
	const std::uint64_t pixels = document.activeLayer()->pixels;
	BrushTip tip = hardTip(10.0f);
	tip.opacity = 0.5f, tip.flow = 0.2f, tip.spacing = 0.01f;
	Stroke one(document, pixels, tip, StrokeMode::paint, {0.0f, 0.0f, 1.0f, 1.0f}, "Brush");
	one.add({30.0f, 30.0f});
	const Image& image = *document.image(pixels);
	CHECK(image.pixel(30, 30).alpha == doctest::Approx(0.1f).epsilon(0.05));
	for(int dab = 0; dab < 30; ++dab) one.add({30.0f + static_cast<float>(dab % 2) * 0.2f, 30.0f});
	CHECK(image.pixel(30, 30).alpha > 0.45f);
	CHECK(image.pixel(30, 30).alpha <= 0.51f);
	one.finish();
	stroke(document, pixels, tip, {{30.0f, 30.0f}}, {0.0f, 0.0f, 1.0f, 1.0f});
	CHECK(image.pixel(30, 30).alpha > 0.51f); // a new stroke lays more
}

TEST_CASE("imaging brush: eraser, clone, the selection, masks, quick mask, cancel and locks") {
	Document document(512, 256);
	const std::uint64_t pixels = document.activeLayer()->pixels;
	const Image& image = *document.image(pixels);
	fillSelection(document, pixels, {0.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
	erase(document, pixels, hardTip(10.0f), {{20.0f, 20.0f}});
	CHECK(image.pixel(20, 20).alpha == 0.0f);
	CHECK(image.pixel(40, 20).alpha == 1.0f);
	stroke(document, pixels, hardTip(10.0f), {{20.0f, 60.0f}});
	{
		Stroke clone(document, pixels, hardTip(10.0f), StrokeMode::clone, {}, "Clone Stamp");
		clone.setCloneOffset(-20.0f, 0.0f);
		clone.add({40.0f, 60.0f});
		clone.finish();
	}
	CHECK(image.pixel(40, 60) == Pixel{1.0f, 0.0f, 0.0f, 1.0f}); // the red dab, copied
	select(document, "Marquee", rectangleCoverage(512, 256, {300.0f, 0.0f}, {512.0f, 256.0f}), SelectionMode::replace);
	stroke(document, pixels, hardTip(40.0f), {{300.0f, 100.0f}});
	CHECK(image.pixel(305, 100) == Pixel{1.0f, 0.0f, 0.0f, 1.0f});
	CHECK(image.pixel(295, 100) == Pixel{0.0f, 1.0f, 0.0f, 1.0f});
	{
		Stroke abandoned(document, pixels, hardTip(40.0f), StrokeMode::erase, {}, "Eraser");
		abandoned.add({400.0f, 100.0f});
		CHECK(image.pixel(400, 100).alpha == 0.0f);
	}
	CHECK(image.pixel(400, 100).alpha == 1.0f);
	deselect(document);
	document.addMask(document.activeLayer()->id);
	const std::uint64_t mask = document.activeLayer()->mask;
	stroke(document, mask, hardTip(10.0f), {{100.0f, 100.0f}}, {0.0f, 0.0f, 0.0f, 1.0f});
	CHECK(document.image(mask)->pixel(100, 100).alpha == 0.0f);
	CHECK(document.image(mask)->pixel(120, 100).alpha == 1.0f);
	stroke(document, Document::selectionId, hardTip(10.0f), {{50.0f, 50.0f}}, {1.0f, 1.0f, 1.0f, 1.0f});
	CHECK(document.hasSelection());
	CHECK(document.selection().pixel(50, 50).alpha == 1.0f);
	document.changeLayer(document.activeLayer()->id, "Lock", [](Layer& layer) { layer.locks.pixels = true; });
	CHECK_THROWS_AS(Stroke(document, pixels, hardTip(10.0f), StrokeMode::paint, {}, "Brush"), affresco::DocumentError);
	CHECK_FALSE(brushPresets().empty());
}

TEST_CASE("imaging gradient: stops, shapes, presets, and the gradient and bucket tools") {
	Gradient fade{{{1.0f, {0.0f, 0.0f, 0.0f, 0.0f}}, {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}}}};
	fade.normalize();
	CHECK(fade.stops.front().position == 0.0f);
	CHECK(fade.at(0.5f) == Pixel{0.5f, 0.0f, 0.0f, 0.5f});
	Gradient many;
	many.stops.assign(40, {0.5f, {1.0f, 1.0f, 1.0f, 1.0f}});
	many.normalize();
	CHECK(many.stops.size() == Gradient::maxStops);
	CHECK(gradientPosition(GradientShape::linear, {0, 0}, {100, 0}, 25.0f, 80.0f) == doctest::Approx(0.25f));
	CHECK(gradientPosition(GradientShape::radial, {0, 0}, {100, 0}, 0.0f, 50.0f) == doctest::Approx(0.5f));
	CHECK(gradientPosition(GradientShape::reflected, {50, 0}, {100, 0}, 25.0f, 0.0f) == doctest::Approx(0.5f));
	CHECK(gradientPosition(GradientShape::diamond, {0, 0}, {100, 0}, 25.0f, 25.0f) == doctest::Approx(0.5f));
	CHECK(gradientPosition(GradientShape::angle, {0, 0}, {100, 0}, 0.0f, -10.0f) == doctest::Approx(0.25f));
	CHECK(gradientPresets().size() == 4);

	Document document(300, 100);
	const std::uint64_t pixels = document.activeLayer()->pixels;
	drawGradient(document, pixels, Gradient{}, GradientShape::linear, {0.0f, 0.0f}, {300.0f, 0.0f}, 1.0f);
	const Image& image = *document.image(pixels);
	CHECK(image.pixel(150, 50).red == doctest::Approx(0.5f).epsilon(0.02));
	CHECK(image.pixel(0, 50).red < 0.01f);
	CHECK(document.history().steps().back().name == "Gradient");
	document.undo();
	fillSelection(document, pixels, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
	stroke(document, pixels, hardTip(20.0f), {{50.0f, 50.0f}, {250.0f, 50.0f}}, {0.0f, 0.0f, 0.0f, 1.0f});
	fillAt(document, pixels, 5, 5, {0.0f, 0.0f, 1.0f, 1.0f}, {}, 1.0f);
	CHECK(image.pixel(5, 5) == Pixel{0.0f, 0.0f, 1.0f, 1.0f});
	CHECK(image.pixel(100, 50) == Pixel{0.0f, 0.0f, 0.0f, 1.0f});
	CHECK(image.pixel(5, 95) == Pixel{0.0f, 0.0f, 1.0f, 1.0f}); // connected around the line's ends
	CHECK(document.history().steps().back().name == "Paint Bucket");
}
