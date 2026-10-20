/// @file
/// @brief Tests that need a window: how images and icons reach the GPU (render's gl::QuadRenderer, gl::ImageTextures).
/// Icons from a fallback font and small images share the text's batch; a large image has a texture of its own; an
/// image's texture is freed once the image is gone. The draw calls are reported with MESSAGE.
///
/// Built into affrescoui_window_tests: it draws with its own renderer in a hidden window's context.

#include "gl/QuadRenderer.hpp"

#include <affresco/render/Application.hpp>
#include <affresco/render/Bitmap.hpp>
#include <affresco/render/ImageView.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace {

/// @brief Lays a window's tree out (rendering a frame), then draws it again with a renderer.
/// @param window The window, its context current.
/// @param renderer The renderer.
void draw(affresco::Window& window, affresco::gl::QuadRenderer& renderer) {
	window.render();
	glfwMakeContextCurrent(window.nativeHandle());
	renderer.draw(window.root(), {}, nullptr, window.width(), window.height());
}

/// @brief An image of one colour.
/// @param width Its width.
/// @param height Its height.
/// @return The image.
std::shared_ptr<const affresco::Bitmap> plainImage(int width, int height) {
	const std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 200);
	return affresco::Bitmap::fromPixels(width, height, pixels, {});
}

/// @brief Adds an image view.
/// @param window The window.
/// @param bitmap Its image.
/// @param x Its left.
void addImage(affresco::Window& window, std::shared_ptr<const affresco::Bitmap> bitmap, float x) {
	auto& view = window.root().add<affresco::ImageView>();
	view.setBitmap(std::move(bitmap));
	view.setPosition(affresco::px(x), affresco::px(60.0f));
	view.setSize(affresco::px(40.0f), affresco::px(40.0f));
}

} // namespace

TEST_CASE("render: icons and small images share the text's batch; a large image adds one; textures go with images") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("visuals", 480, 120);
	glfwMakeContextCurrent(window.nativeHandle());
	affresco::gl::QuadRenderer renderer;

	const std::filesystem::path fonts = AFFRESCOUI_FONTS_DIR;
	const std::array fallback{fonts / "Lucide.ttf"};
	const auto font = affresco::Font::load(fonts / "Roboto-Regular.ttf", 16.0f, fallback);
	auto& label = window.root().add<affresco::Label>("Search the board");
	label.setFont(font);
	label.setSize(affresco::px(300), affresco::px(30));
	draw(window, renderer);
	const std::size_t text = renderer.drawCalls();

	label.setIcon("search");
	draw(window, renderer);
	const std::size_t icon = renderer.drawCalls();

	const auto small = plainImage(32, 32), other = plainImage(64, 16);
	addImage(window, small, 10.0f);
	addImage(window, other, 60.0f);
	addImage(window, small, 110.0f);
	draw(window, renderer);
	const std::size_t images = renderer.drawCalls();

	auto large = plainImage(300, 200);
	addImage(window, large, 160.0f);
	draw(window, renderer);
	const std::size_t withLarge = renderer.drawCalls();

	MESSAGE("draw calls: text ", text, ", with an icon ", icon, ", with three small images ", images,
	        ", with a large image ", withLarge);
	CHECK(text == 2);      // the root's background (a white texture: no text yet), then the font's page
	CHECK(icon == text);   // the icon comes from the text font's fallback: the same atlas page
	CHECK(images == text); // both small images are in one image page, sampled next to the text's page
	CHECK(withLarge == images + 1);

	// Gone with its image: the large one's texture is freed by the next frame
	window.root().removeChildren(window.root().children().size() - 1);
	large.reset();
	draw(window, renderer);
	CHECK(renderer.drawCalls() == images);
}
