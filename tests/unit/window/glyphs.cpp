/// @file
/// @brief Tests that need a window: how text reaches the GPU (render's gl::QuadRenderer). Colour emoji share the text's
/// batch, the atlas textures of a font that is gone are freed, a picture at a scale of 2 draws glyphs rasterised at 2,
/// glyphs fitted to pixels at 1 make no draw call more than at 2, and a turned text at 1 draws the same pixels whatever
/// its font drew before.
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

TEST_CASE("render: glyphs fitted to quarters of a pixel at 1 make no draw call more than at 2") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("glyphs", 600, 400);
	glfwMakeContextCurrent(window.nativeHandle());
	affresco::gl::QuadRenderer renderer;

	// Three fonts, every letter, each label starting a fraction of a pixel further right
	const std::filesystem::path fonts = AFFRESCOUI_FONTS_DIR;
	const std::array names{"Roboto-Regular.ttf", "JetBrainsMono-Regular.ttf", "NotoSansArabic-Subset.ttf"};
	const std::array texts{"The quick brown fox jumps over the lazy dog: 0123456789",
	                       "PACK MY BOX WITH FIVE DOZEN LIQUOR JUGS, then {[(fit)]}",
	                       "\u0645\u0631\u062d\u0628\u0627 \u0628\u0627\u0644\u0639\u0627\u0644\u0645 abc xyz"};
	for(std::size_t row = 0; row < 18; ++row) {
		affresco::Label& label = window.root().add<affresco::Label>(texts[row % texts.size()]);
		label.setFont(affresco::Font::load(fonts / names[row % names.size()], 13.0f + static_cast<float>(row % 3)));
		label.setPosition(affresco::px(3.0f + 0.37f * static_cast<float>(row)),
		                  affresco::px(2.0f + 21.3f * static_cast<float>(row)));
		label.setSize(affresco::px(590), affresco::px(20));
	}
	window.render();

	std::array<std::size_t, 2> calls{};
	for(const int scale : {1, 2}) {
		glfwMakeContextCurrent(window.nativeHandle());
		glViewport(0, 0, window.width() * scale, window.height() * scale);
		for(int frame = 0; frame < 2; ++frame) // the second frame draws what the first has rasterised
			renderer.draw(window.root(), {}, nullptr, window.width(), window.height());
		calls[static_cast<std::size_t>(scale - 1)] = renderer.drawCalls();
	}
	CHECK(calls[0] >= 1);
	CHECK(calls[0] == calls[1]);
}

namespace {

/// @brief Draws a turned, scaled label in a fresh font into an image, after the font has drawn another text first.
/// @param window The window, its context current.
/// @param before What the font draws first, in another label (empty: nothing).
/// @param scale Pixels per window unit.
/// @return The turned label's picture; nullptr if it can't be drawn.
std::shared_ptr<const affresco::Bitmap> turnedText(affresco::Window& window, const char* before, float scale) {
	const auto font = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 15.0f);
	window.root().removeChildren();
	affresco::Panel& frame = window.root().add<affresco::Panel>();
	frame.setSize(affresco::px(320), affresco::px(160));
	affresco::Label& first = frame.add<affresco::Label>(before);
	first.setFont(font);
	first.setSize(affresco::px(300), affresco::px(20));
	affresco::Panel& holder = window.root().add<affresco::Panel>();
	holder.setSize(affresco::px(320), affresco::px(160));
	holder.setColor(affresco::colors::white);
	affresco::Label& label = holder.add<affresco::Label>("Pinned to the sidebar, quick brown fox");
	label.setFont(font);
	label.setTextColor(affresco::colors::black);
	label.setPosition(affresco::px(20), affresco::px(60));
	label.setSize(affresco::px(280), affresco::px(24));
	label.setTransform(affresco::Affine2::compose({0.3f, 0.0f}, 7.0f, {1.13f, 1.13f}, {0.0f, 0.0f}));
	window.render();
	if(before[0] != '\0') static_cast<void>(window.renderToImage(frame, scale)); // its glyphs go in first
	return window.renderToImage(holder, scale);
}

} // namespace

// A turned or scaled text samples its texels between their centres, and how the GPU interpolates them there, to the
// last bit, follows where its glyphs lie in the atlas: at 1 those places are kept from the start (reserveDrawn()).
// A raster's (1.25 and up) are taken as text draws, so there the same text can differ by a level or two
TEST_CASE("render: text turned and scaled at 1 draws the same pixels whatever its font drew before") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("glyphs", 340, 340);
	glfwMakeContextCurrent(window.nativeHandle());
	const auto alone = turnedText(window, "", 1.0f);
	const auto after = turnedText(window, "Wizard jumps over the lazy glyph: QUICK 0123456789 kxv", 1.0f);
	REQUIRE(alone);
	REQUIRE(after);
	REQUIRE(alone->pixels().size() == after->pixels().size());
	CHECK(std::ranges::equal(alone->pixels(), after->pixels()));
}
