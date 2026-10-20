/// @file
/// @brief Tests that need a window: a frame drawn only where it changed (render's gl::QuadRenderer with a region) makes
/// no more draw calls than a full frame, one per texture switch, and draws only the quads in the region.
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

TEST_CASE("render: a frame drawn where it changed makes no more draw calls, and draws fewer quads") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("regions", 600, 200);
	glfwMakeContextCurrent(window.nativeHandle());
	affresco::gl::QuadRenderer renderer;

	const auto font = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 16.0f);
	std::array<affresco::Label*, 6> labels{};
	for(int i = 0; i < 6; ++i) {
		affresco::Label& label = window.root().add<affresco::Label>("Label " + std::to_string(i));
		label.setFont(font);
		label.setPosition(affresco::px(static_cast<float>(i) * 100.0f), affresco::px(20.0f));
		label.setSize(affresco::px(90.0f), affresco::px(40.0f));
		label.setColor(affresco::Color::hex(0xe2e8f0));
		label.setCornerRadii(affresco::CornerRadii::all(6.0f));
		labels.at(static_cast<std::size_t>(i)) = &label;
	}

	// Taking the frame lays it out without the window drawing it: only this renderer refreshes the geometry
	const affresco::detail::Frame first = affresco::detail::takeFrame(window);
	const affresco::detail::Refreshed all = renderer.refresh(first);
	renderer.draw(first, std::nullopt, all.shapes, all.color);
	const std::size_t fullCalls = renderer.drawCalls(), fullQuads = renderer.quadsDrawn();
	CHECK(fullCalls >= 1);

	labels[3]->setTextColor(affresco::Color::hex(0xf97316)); // one label's look
	const affresco::detail::Frame next = affresco::detail::takeFrame(window);
	const affresco::detail::Refreshed changed = renderer.refresh(next);
	REQUIRE(changed.damage);
	const affresco::Rect area = changed.damage.value_or(affresco::Rect{});
	CHECK(area.left >= 300.0f);
	CHECK(area.right <= 390.0f);
	CHECK(changed.shapes == all.shapes); // the shaders of the whole frame, so its pixels match a full frame's
	renderer.draw(next, changed.damage, changed.shapes, changed.color);
	CHECK(renderer.drawCalls() <= fullCalls);
	CHECK(renderer.quadsDrawn() < fullQuads / 2);
	CHECK(renderer.quadsDrawn() > 0);
}
