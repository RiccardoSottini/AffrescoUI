/// @file
/// @brief Tests that need a window: images of one panel's subtree (Window::renderToImage(), what slide thumbnails
/// and a presenter's audience view are made from), and second windows that share the first one's OpenGL context and
/// font (Application::createWindow(..., sharing), Screen::createWindow(app, sharing)).
///
/// Built into affrescoui_window_tests (tests/unit/CMakeLists.txt): drawing needs an OpenGL context, so a display or
/// Xvfb. The windows are never shown.

#include <affresco/affresco.hpp>

#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <cstdint>
#include <memory>

namespace {

/// @brief A screen with a dark ground, a red board with a white square in its corner, and a blue panel outside it.
constexpr const char* boardScreen = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"window": { "title": "second", "width": 200, "height": 120 },
	"root": { "type": "panel", "id": "main", "color": "#202020", "children": [
		{ "type": "row", "width": "100%", "height": 60, "children": [
			{ "type": "panel", "id": "board", "color": "#ff0000", "width": 100, "height": 60, "children": [
				{ "type": "panel", "id": "corner", "color": "#ffffff", "width": 20, "height": 20 }
			] },
			{ "type": "panel", "id": "side", "color": "#0000ff", "width": 50, "height": 60 }
		] }
	] }
})json";

/// @brief A pixel of an image.
/// @param image The image.
/// @param x Its column.
/// @param y Its row, from the top.
/// @return Its red, green and blue, packed as 0xRRGGBB.
std::uint32_t pixel(const affresco::Bitmap& image, int x, int y) {
	const std::size_t at =
	    (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width()) + static_cast<std::size_t>(x)) * 4;
	const auto rgba = image.pixels();
	return (std::uint32_t{rgba[at]} << 16) | (std::uint32_t{rgba[at + 1]} << 8) | std::uint32_t{rgba[at + 2]};
}

} // namespace

TEST_CASE("render to image: one panel's subtree, at its size times the scale, without its siblings") {
	affresco::Application app({nullptr, true});
	affresco::Screen screen = affresco::Screen::fromString(boardScreen);
	affresco::Window& window = screen.createWindow(app);

	const auto image = window.renderToImage(screen.get("board"), 2.0f);
	REQUIRE(image);
	CHECK(image->width() == 200);
	CHECK(image->height() == 120);
	CHECK(pixel(*image, 10, 10) == 0xffffff);   // the corner
	CHECK(pixel(*image, 150, 100) == 0xff0000); // the board
	CHECK(pixel(*image, 199, 119) == 0xff0000); // its last pixel: nothing of the blue panel next to it

	const auto side = window.renderToImage(screen.get("side"), 0.5f);
	REQUIRE(side);
	CHECK(side->width() == 25);
	CHECK(pixel(*side, 12, 15) == 0x0000ff);
}

TEST_CASE("render to image: what changed since is drawn, and the window's own frames are as before") {
	affresco::Application app({nullptr, true});
	affresco::Screen screen = affresco::Screen::fromString(boardScreen);
	affresco::Window& window = screen.createWindow(app);
	const auto before = window.renderToImage(screen.get("board"), 1.0f);
	REQUIRE(before);
	CHECK(pixel(*before, 50, 30) == 0xff0000);

	screen.get("board").setColor(affresco::Color{0.0f, 1.0f, 0.0f, 1.0f});
	const auto after = window.renderToImage(screen.get("board"), 1.0f);
	REQUIRE(after);
	CHECK(pixel(*after, 50, 30) == 0x00ff00);
	CHECK(pixel(*after, 5, 5) == 0xffffff);
	window.render(); // a frame of the window's own, from its retained geometry, still draws
	CHECK(window.frameStatistics().drawCalls > 0);
}

TEST_CASE("render to image: nothing for a hidden panel, nor in a headless window") {
	affresco::Application app({nullptr, true});
	affresco::Screen screen = affresco::Screen::fromString(boardScreen);
	affresco::Window& window = screen.createWindow(app);
	screen.get("board").setVisible(false);
	CHECK_FALSE(window.renderToImage(screen.get("board"), 1.0f));
	CHECK_FALSE(window.renderToImage(screen.get("side"), 0.0f));
}

TEST_CASE("second window: shares the first one's context objects and default font") {
	affresco::Application app({nullptr, true});
	affresco::Screen first = affresco::Screen::fromString(boardScreen);
	affresco::Window& main = first.createWindow(app);
	affresco::Window& audience = app.createWindow("audience", 160, 90, main);
	CHECK(audience.defaultFont() == main.defaultFont());

	// A texture made in the first window's context is a texture in the second's
	glfwMakeContextCurrent(main.nativeHandle());
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glfwMakeContextCurrent(audience.nativeHandle());
	CHECK(glIsTexture(texture) == GL_TRUE);
	glfwMakeContextCurrent(main.nativeHandle());
	glDeleteTextures(1, &texture);
}

TEST_CASE("second window: a screen built into it shows an image of a panel of the first") {
	affresco::Application app({nullptr, true});
	affresco::Screen first = affresco::Screen::fromString(boardScreen);
	affresco::Window& main = first.createWindow(app);
	affresco::Screen second = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 1,
		"window": { "title": "audience", "width": 100, "height": 60 },
		"root": { "type": "panel", "children": [
			{ "type": "image", "id": "slide", "source": "slide", "fit": "fill", "width": "100%", "height": "100%" }
		] }
	})json");
	affresco::Window& audience = second.createWindow(app, main);
	CHECK(audience.width() == 100);

	second.setImage("slide", main.renderToImage(first.get("board"), 1.0f));
	audience.render();
	const auto shown = audience.renderToImage(audience.root(), 1.0f);
	REQUIRE(shown);
	CHECK(pixel(*shown, 50, 40) == 0xff0000);
	CHECK(pixel(*shown, 5, 5) == 0xffffff);
}
