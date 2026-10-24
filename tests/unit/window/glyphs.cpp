/// @file
/// @brief Tests that need a window: how text reaches the GPU (render's gl::QuadRenderer). Colour emoji share the text's
/// batch, the atlas textures of a font that is gone are freed, and a picture at a scale of 2 draws glyphs rasterised
/// at 2.
///
/// Built into affrescoui_window_tests: it draws with its own renderer in a hidden window's context.

#include "gl/QuadRenderer.hpp"

#include <affresco/render/Application.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <affresco/render/Bitmap.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace {

/// @brief Roboto at 16 px with the Twemoji subset as its fallback.
/// @return The font.
std::shared_ptr<const affresco::Font> robotoWithEmoji() {
	const std::array fallback{std::filesystem::path(AFFRESCOUI_TEST_FONTS_DIR) / "TwemojiMozilla-Subset.ttf"};
	return affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 16.0f, fallback);
}

/// @brief Lays a window's tree out (rendering a frame), then draws it again with a renderer.
/// @param window The window, its context current.
/// @param renderer The renderer.
void draw(affresco::Window& window, affresco::gl::QuadRenderer& renderer) {
	window.render();
	glfwMakeContextCurrent(window.nativeHandle());
	renderer.draw(window.root(), {}, nullptr, window.width(), window.height());
}

} // namespace

TEST_CASE("render: colour emoji share the batch of the text around them: no draw call more") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("glyphs", 400, 120);
	glfwMakeContextCurrent(window.nativeHandle());
	affresco::gl::QuadRenderer renderer;

	const auto font = robotoWithEmoji();
	affresco::Label& label = window.root().add<affresco::Label>("Ship it, and again, then text");
	label.setFont(font);
	label.setSize(affresco::px(380), affresco::px(40));
	draw(window, renderer);
	const std::size_t plain = renderer.drawCalls();
	CHECK(plain >= 1);
	CHECK(font->atlasPages(affresco::AtlasKind::color) == 0);

	label.setText("Ship it 🚀 ✅, and again 🎨 👨‍👩‍👧, then text");
	draw(window, renderer);
	CHECK(font->atlasPages(affresco::AtlasKind::color) == 1);
	CHECK(renderer.drawCalls() == plain);
}

TEST_CASE("render: the atlas textures of a font that is gone are freed by the next frame") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("glyphs", 300, 80);
	glfwMakeContextCurrent(window.nativeHandle());
	affresco::gl::QuadRenderer renderer;

	affresco::Label& label = window.root().add<affresco::Label>("Hello 🎨");
	label.setFont(robotoWithEmoji());
	label.setSize(affresco::px(280), affresco::px(40));
	draw(window, renderer);
	CHECK(renderer.fontsOnGpu() == 1);

	label.setFont(robotoWithEmoji()); // another font: the first one has no owner left
	draw(window, renderer);
	CHECK(renderer.fontsOnGpu() == 1);
}

namespace {

/// @brief The box of an image's ink: the pixels darker than mid grey.
/// @param image Opaque RGBA, top row first.
/// @return Its left, top, right and bottom edges, in pixels (right and bottom exclusive).
affresco::Rect inkOf(const affresco::Bitmap& image) {
	affresco::Rect ink{static_cast<float>(image.width()), static_cast<float>(image.height()), 0.0f, 0.0f};
	const std::span<const std::uint8_t> pixels = image.pixels();
	for(int y = 0; y < image.height(); ++y)
		for(int x = 0; x < image.width(); ++x) {
			const std::size_t at =
			    (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width()) + static_cast<std::size_t>(x)) *
			    4;
			if(pixels[at] >= 128) continue;
			ink = {std::min(ink.left, static_cast<float>(x)), std::min(ink.top, static_cast<float>(y)),
			       std::max(ink.right, static_cast<float>(x + 1)), std::max(ink.bottom, static_cast<float>(y + 1))};
		}
	return ink;
}

} // namespace

TEST_CASE("render: a picture at a scale of 2 draws its text from glyphs rasterised at 2, where they are at 1") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("glyphs", 300, 80);
	glfwMakeContextCurrent(window.nativeHandle());

	const auto font = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 16.0f);
	affresco::Label& label = window.root().add<affresco::Label>("Crisp on Retina");
	label.setFont(font);
	label.setColor(affresco::colors::white);
	label.setTextColor(affresco::colors::black);
	label.setSize(affresco::px(280), affresco::px(40));

	const auto one = window.renderToImage(label, 1.0f);
	const auto two = window.renderToImage(label, 2.0f);
	REQUIRE(one);
	REQUIRE(two);
	CHECK(two->width() == 2 * one->width());
	CHECK(font->rasterAt(2.0f).atlasVersion(0) > 1); // '?' when it was made, then the label's glyphs

	// The text lies where it lies at 1, within a pixel of the picture at 1
	const affresco::Rect a = inkOf(*one), b = inkOf(*two);
	REQUIRE(a.right > a.left);
	CHECK(std::abs(b.left / 2.0f - a.left) <= 1.0f);
	CHECK(std::abs(b.top / 2.0f - a.top) <= 1.0f);
	CHECK(std::abs(b.right / 2.0f - a.right) <= 1.0f);
	CHECK(std::abs(b.bottom / 2.0f - a.bottom) <= 1.0f);
}
