#pragma once

/// @file
/// @brief gl::GlState and gl::SurfaceGuard: the OpenGL state a surface may change, saved before it draws and restored
/// after, so the window's drawing carries on as if it hadn't.

#include <glad/glad.h>

#include <array>

namespace affresco::gl {

/// @brief The OpenGL state the window's drawing relies on, as one value.
struct GlState {
	std::array<GLint, 2> framebuffers{};      ///< Draw and read framebuffer bindings.
	std::array<GLint, 4> viewport{};          ///< The viewport.
	std::array<GLint, 4> scissorBox{};        ///< The scissor box.
	std::array<GLint, 4> blendFunctions{};    ///< Source and destination, colour then alpha.
	std::array<GLint, 2> blendEquations{};    ///< Colour and alpha.
	std::array<GLint, 4> stencilFunction{};   ///< Function, reference, value mask, write mask (front faces).
	std::array<GLint, 3> stencilOperations{}; ///< Stencil fail, depth fail, pass (front faces).
	std::array<GLint, 4> colorMask{};         ///< Red, green, blue, alpha writes.
	std::array<GLfloat, 4> clearColor{};      ///< The clear colour.
	std::array<GLint, 2> textures{};          ///< The 2D textures on units 0 and 1.
	std::array<GLboolean, 5> enabled{};       ///< Scissor, blend, depth and stencil tests, face culling.
	GLint depthFunction = 0;                  ///< The depth test.
	GLboolean depthMask = GL_TRUE;            ///< Depth writes.
	GLfloat clearDepth = 1.0f;                ///< The clear depth.
	GLint clearStencil = 0;                   ///< The clear stencil value.
	GLint frontFace = 0;                      ///< Which winding faces front.
	GLint cullFace = 0;                       ///< Which faces are culled.
	GLint program = 0;                        ///< The program in use.
	GLint vertexArray = 0;                    ///< The vertex array bound.
	GLint arrayBuffer = 0;                    ///< The array buffer bound.
	GLint activeTexture = 0;                  ///< The active texture unit.
	GLint unpackAlignment = 0;                ///< Pixel upload row alignment.

	/// @brief Reads the current context's state.
	/// @return It.
	[[nodiscard]] static GlState capture();
	/// @brief Sets the current context's state to this.
	void restore() const;
	/// @brief Equal when every member is.
	bool operator==(const GlState&) const noexcept = default;
};

/// @brief Saves the state on construction and restores it on destruction (exceptions included).
class SurfaceGuard {
public:
	SurfaceGuard() : saved_(GlState::capture()) {}
	~SurfaceGuard() { saved_.restore(); }
	SurfaceGuard(const SurfaceGuard&) = delete;
	SurfaceGuard& operator=(const SurfaceGuard&) = delete;

private:
	GlState saved_; ///< The state before.
};

} // namespace affresco::gl
