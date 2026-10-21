/// @file
/// @brief Tests that need a window: a cached subtree (Panel::setCached()) is drawn into a texture once, then as one
/// quad per frame, and drawn into it again when something in it changes.
///
/// Built into affrescoui_window_tests: it draws with its own renderer in a hidden window's context.

#include "Headless.hpp"
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

/// @brief Takes, refreshes and draws a whole frame of a window with a renderer.
/// @param window The window.
/// @param renderer The renderer, current in the window's context.
void drawFrame(affresco::Window& window, affresco::gl::QuadRenderer& renderer) {
	const affresco::detail::Frame frame = affresco::detail::takeFrame(window);
	const affresco::detail::Refreshed refreshed = renderer.refresh(frame);
	renderer.draw(frame, std::nullopt, refreshed.shapes, refreshed.color);
}

} // namespace

TEST_CASE("render: a cached subtree is one quad per frame, drawn again only when it changes") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("cache", 600, 200);
	glfwMakeContextCurrent(window.nativeHandle());
	affresco::gl::QuadRenderer renderer;

	const auto font = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 16.0f);
	affresco::Panel& group = window.root().add<affresco::Panel>();
	group.setSize(affresco::px(600.0f), affresco::px(100.0f));
	group.setColor(affresco::Color::hex(0x1e293b));
	std::array<affresco::Label*, 6> labels{};
	for(int i = 0; i < 6; ++i) {
		affresco::Label& label = group.add<affresco::Label>("Label " + std::to_string(i));
		label.setFont(font);
		label.setPosition(affresco::px(static_cast<float>(i) * 100.0f), affresco::px(20.0f));
		label.setSize(affresco::px(90.0f), affresco::px(40.0f));
		label.setColor(affresco::Color::hex(0xe2e8f0));
		labels.at(static_cast<std::size_t>(i)) = &label;
	}

	drawFrame(window, renderer);
	const std::size_t directQuads = renderer.quadsDrawn();
	CHECK(directQuads > 6);

	group.setCached(true);
	drawFrame(window, renderer); // drawn into its texture, then as one quad
	const std::size_t firstCalls = renderer.drawCalls();
	CHECK(renderer.quadsDrawn() < directQuads);
	CHECK(renderer.quadsDrawn() >= 1);

	drawFrame(window, renderer); // unchanged: only the quad
	const std::size_t steadyCalls = renderer.drawCalls();
	CHECK(steadyCalls < firstCalls);
	CHECK(renderer.quadsDrawn() < directQuads);

	labels[3]->setTextColor(affresco::Color::hex(0xf97316)); // something inside changes
	drawFrame(window, renderer);
	CHECK(renderer.drawCalls() > steadyCalls);

	drawFrame(window, renderer);
	CHECK(renderer.drawCalls() == steadyCalls);

	group.setCached(false);
	drawFrame(window, renderer);
	CHECK(renderer.quadsDrawn() == directQuads);
}
