/// @file
/// @brief Tests that need a window: GPU surfaces (render's gl::Surfaces). The window's GL state is the same after a
/// surface as before it, whatever the surface changed; a GL error disables the surface with one logged error; the
/// limits on passes hold.
///
/// Built into affrescoui_window_tests: it draws surfaces in a hidden window's context.

#include "gl/SurfaceGuard.hpp"
#include "gl/SurfaceState.hpp"
#include "gl/Surfaces.hpp"

#include <affresco/core/Log.hpp>
#include <affresco/render/Application.hpp>
#include <affresco/render/Window.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using affresco::PassFormat;
using affresco::SurfaceFrame;

/// @brief A surface that draws with a function.
class FunctionSurface : public affresco::GpuSurface {
public:
	/// @brief A surface.
	/// @param draw What its render() does.
	explicit FunctionSurface(std::function<void(SurfaceFrame&)> draw) : draw_(std::move(draw)) {}
	/// @brief Calls the function.
	void render(SurfaceFrame& frame) override { draw_(frame); }

private:
	std::function<void(SurfaceFrame&)> draw_; ///< What render() does.
};

/// @brief A panel's surface state, 64 by 64 units at scale 1, drawing with a function.
/// @param draw What its render() does.
/// @return The state.
std::shared_ptr<affresco::detail::SurfaceState> surfaceState(std::function<void(SurfaceFrame&)> draw) {
	auto state = std::make_shared<affresco::detail::SurfaceState>();
	state->name = "test";
	state->factory = [draw = std::move(draw)] { return std::make_unique<FunctionSurface>(draw); };
	state->size = {64.0f, 64.0f};
	return state;
}

/// @brief Changes all the state it can, as a careless surface would.
/// @param frame The frame (a pass is made and left bound).
void changeEverything(SurfaceFrame& frame) {
	const affresco::SurfacePass pass = frame.pass("scratch", {32, 32, PassFormat::rgba16f});
	glBindFramebuffer(GL_FRAMEBUFFER, pass.framebuffer);
	glViewport(1, 2, 3, 4);
	glScissor(5, 6, 7, 8);
	for(const GLenum capability :
	    std::array<GLenum, 5>{GL_SCISSOR_TEST, GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE})
		glEnable(capability);
	glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ZERO);
	glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_MAX);
	glStencilFunc(GL_EQUAL, 3, 0x0Fu);
	glStencilOp(GL_INCR, GL_DECR, GL_INVERT);
	glStencilMask(0x0Fu);
	glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
	glClearColor(0.1f, 0.2f, 0.3f, 0.4f);
	glDepthFunc(GL_GREATER);
	glDepthMask(GL_FALSE);
	glFrontFace(GL_CW);
	glCullFace(GL_FRONT);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, pass.texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	GLuint vertexArray = 0;
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);
}

/// @brief Collects the errors logged while it lives.
class LoggedErrors {
public:
	/// @brief Collects the errors from now on.
	LoggedErrors() {
		affresco::setLogSink([this](affresco::LogLevel level, std::string_view message) {
			if(level == affresco::LogLevel::Error) messages.emplace_back(message);
		});
	}
	/// @brief Puts the default sink back, so that nothing calls this any more.
	~LoggedErrors() { affresco::setLogSink(nullptr); }
	LoggedErrors(const LoggedErrors&) = delete;
	LoggedErrors& operator=(const LoggedErrors&) = delete;

	std::vector<std::string> messages; ///< The errors, oldest first.
};

/// @brief A hidden window whose context is current, with state a frame of the library might have set.
/// @param app The application.
/// @return The window.
affresco::Window& hiddenWindow(affresco::Application& app) {
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("surfaces", 200, 120);
	glfwMakeContextCurrent(window.nativeHandle());
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_SCISSOR_TEST);
	glScissor(10, 10, 50, 50);
	glViewport(0, 0, 200, 120);
	return window;
}

/// @brief Whether a surface that asks for these passes fails, and with which logged error.
/// @param passes The passes it asks for, in order.
/// @return The errors logged.
std::vector<std::string> placeWithPasses(const std::vector<std::pair<std::string, affresco::PassSpec>>& passes) {
	affresco::gl::Surfaces surfaces;
	const LoggedErrors errors;
	const auto state = surfaceState([&passes](SurfaceFrame& frame) {
		for(const auto& [name, spec] : passes) static_cast<void>(frame.pass(name, spec));
	});
	const GLuint texture = surfaces.place(state);
	CHECK((texture == 0) == state->failed);
	return errors.messages;
}

} // namespace

TEST_CASE("surfaces: the window's GL state is the same after a surface as before, whatever it changed") {
	affresco::Application app;
	static_cast<void>(hiddenWindow(app));
	affresco::gl::Surfaces surfaces;
	const affresco::gl::GlState before = affresco::gl::GlState::capture();

	const auto state = surfaceState(changeEverything);
	CHECK(surfaces.place(state) != 0);
	CHECK_FALSE(state->failed);
	CHECK(affresco::gl::GlState::capture() == before);
	CHECK(glGetError() == GL_NO_ERROR);

	++state->version; // drawn again, with its pass kept
	CHECK(surfaces.place(state) != 0);
	CHECK(affresco::gl::GlState::capture() == before);
}

TEST_CASE("surfaces: a GL error disables the surface with one logged error, and leaves the state as it was") {
	affresco::Application app;
	static_cast<void>(hiddenWindow(app));
	affresco::gl::Surfaces surfaces;
	const affresco::gl::GlState before = affresco::gl::GlState::capture();
	const LoggedErrors errors;

	const auto state = surfaceState([](SurfaceFrame& frame) {
		changeEverything(frame);
		glEnable(GL_RGBA); // not a capability: GL_INVALID_ENUM
	});
	CHECK(surfaces.place(state) == 0);
	CHECK(state->failed);
	REQUIRE(errors.messages.size() == 1);
	CHECK(errors.messages.front() == "AffrescoUI: surface 'test' disabled: an OpenGL error");
	CHECK(affresco::gl::GlState::capture() == before);
	CHECK(glGetError() == GL_NO_ERROR);

	++state->version; // once failed, it stays off, without another error
	CHECK(surfaces.place(state) == 0);
	CHECK(errors.messages.size() == 1);

	const auto healthy = surfaceState([](SurfaceFrame&) {}); // the next surface is not affected
	CHECK(surfaces.place(healthy) != 0);
}

TEST_CASE("surfaces: an exception from render() disables the surface") {
	affresco::Application app;
	static_cast<void>(hiddenWindow(app));
	affresco::gl::Surfaces surfaces;
	const affresco::gl::GlState before = affresco::gl::GlState::capture();
	const LoggedErrors errors;
	const auto state = surfaceState([](SurfaceFrame&) { throw std::runtime_error("no luck"); });
	CHECK(surfaces.place(state) == 0);
	CHECK(state->failed);
	REQUIRE(errors.messages.size() == 1);
	CHECK(errors.messages.front() == "AffrescoUI: surface 'test' disabled: no luck");
	CHECK(affresco::gl::GlState::capture() == before);
}

TEST_CASE("surfaces: the pass limits hold") {
	affresco::Application app;
	static_cast<void>(hiddenWindow(app));
	using Passes = std::vector<std::pair<std::string, affresco::PassSpec>>;

	SUBCASE("eight passes, and asking for the same one again, are fine") {
		Passes passes;
		for(int index = 0; index < 8; ++index)
			passes.emplace_back("pass" + std::to_string(index), affresco::PassSpec{16, 16});
		passes.emplace_back("pass0", affresco::PassSpec{16, 16});
		CHECK(placeWithPasses(passes).empty());
	}
	SUBCASE("a ninth pass fails") {
		Passes passes;
		for(int index = 0; index < 9; ++index)
			passes.emplace_back("pass" + std::to_string(index), affresco::PassSpec{16, 16});
		const std::vector<std::string> errors = placeWithPasses(passes);
		REQUIRE(errors.size() == 1);
		CHECK(errors.front() == "AffrescoUI: surface 'test' disabled: a surface has at most 8 passes");
	}
	SUBCASE("a side over 4096 pixels fails, as does an empty one") {
		CHECK(placeWithPasses({{"wide", {4097, 16}}}).size() == 1);
		CHECK(placeWithPasses({{"empty", {0, 16}}}).size() == 1);
		CHECK(placeWithPasses({{"widest", {4096, 16}}}).empty());
	}
	SUBCASE("over 256 MiB in all fails") {
		const affresco::PassSpec half{4096, 4096, PassFormat::rgba16f}; // 128 MiB
		const std::vector<std::string> errors = placeWithPasses({{"one", half}, {"two", half}});
		REQUIRE(errors.size() == 1);
		CHECK(errors.front() == "AffrescoUI: surface 'test' disabled: a surface's passes are over 256 MiB");
	}
}

TEST_CASE("surfaces: a picture at a fixed scale draws its surfaces at that scale, whatever the display's") {
	affresco::Application app;
	affresco::Window& window = hiddenWindow(app);
	std::vector<float> scales;
	auto& view = window.root().add<affresco::SurfaceView>();
	view.setSurface("probe", [&scales] {
		return std::make_unique<FunctionSurface>([&scales](SurfaceFrame& frame) { scales.push_back(frame.scale); });
	});
	view.setSize(affresco::px(40.0f), affresco::px(40.0f));
	const std::filesystem::path picture = std::filesystem::temp_directory_path() / "affrescoui-surface-scale.ppm";
	REQUIRE(window.saveScreenshot(picture, 2));
	REQUIRE_FALSE(scales.empty());
	CHECK(scales.back() == 2.0f);
	REQUIRE(window.saveScreenshot(picture, 1));
	CHECK(scales.back() == 1.0f);
	std::filesystem::remove(picture);
}
