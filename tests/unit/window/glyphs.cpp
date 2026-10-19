/// @file
/// @brief Tests that need a window: how text reaches the GPU (render's gl::QuadRenderer). Colour emoji share the text's
/// batch, and the atlas textures of a font that is gone are freed.
///
/// Built into affrescoui_window_tests: it draws with its own renderer in a hidden window's context.

#include "gl/QuadRenderer.hpp"

#include <affresco/render/Application.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <array>
#include <filesystem>
#include <memory>

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
