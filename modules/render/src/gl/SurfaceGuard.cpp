/// @file
/// @brief gl::GlState: reading and setting the state a surface may change.

#include "gl/SurfaceGuard.hpp"

#include <cstddef>

namespace affresco::gl {

namespace {

constexpr std::array<GLenum, 5> switches{GL_SCISSOR_TEST, GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST,
                                         GL_CULL_FACE}; ///< GlState::enabled's capabilities, in order.

/// @brief Reads one integer.
/// @param name The state.
/// @return Its value.
GLint integer(GLenum name) {
	GLint value = 0;
	glGetIntegerv(name, &value);
	return value;
}

/// @brief Turns a capability on or off.
/// @param capability The capability.
/// @param on Whether it is on.
void enable(GLenum capability, GLboolean on) {
	if(on == GL_TRUE) glEnable(capability);
	else glDisable(capability);
}

} // namespace

GlState GlState::capture() {
	GlState state;
	state.framebuffers = {integer(GL_DRAW_FRAMEBUFFER_BINDING), integer(GL_READ_FRAMEBUFFER_BINDING)};
	glGetIntegerv(GL_VIEWPORT, state.viewport.data());
	glGetIntegerv(GL_SCISSOR_BOX, state.scissorBox.data());
	state.blendFunctions = {integer(GL_BLEND_SRC_RGB), integer(GL_BLEND_DST_RGB), integer(GL_BLEND_SRC_ALPHA),
	                        integer(GL_BLEND_DST_ALPHA)};
	state.blendEquations = {integer(GL_BLEND_EQUATION_RGB), integer(GL_BLEND_EQUATION_ALPHA)};
	state.stencilFunction = {integer(GL_STENCIL_FUNC), integer(GL_STENCIL_REF), integer(GL_STENCIL_VALUE_MASK),
	                         integer(GL_STENCIL_WRITEMASK)};
	state.stencilOperations = {integer(GL_STENCIL_FAIL), integer(GL_STENCIL_PASS_DEPTH_FAIL),
	                           integer(GL_STENCIL_PASS_DEPTH_PASS)};
	glGetIntegerv(GL_COLOR_WRITEMASK, state.colorMask.data());
	glGetFloatv(GL_COLOR_CLEAR_VALUE, state.clearColor.data());
	state.activeTexture = integer(GL_ACTIVE_TEXTURE);
	for(std::size_t unit = 0; unit < state.textures.size(); ++unit) {
		glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
		state.textures[unit] = integer(GL_TEXTURE_BINDING_2D);
	}
	glActiveTexture(static_cast<GLenum>(state.activeTexture));
	for(std::size_t index = 0; index < switches.size(); ++index) state.enabled[index] = glIsEnabled(switches[index]);
	state.depthFunction = integer(GL_DEPTH_FUNC);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &state.depthMask);
	glGetFloatv(GL_DEPTH_CLEAR_VALUE, &state.clearDepth);
	state.clearStencil = integer(GL_STENCIL_CLEAR_VALUE);
	state.frontFace = integer(GL_FRONT_FACE);
	state.cullFace = integer(GL_CULL_FACE_MODE);
	state.program = integer(GL_CURRENT_PROGRAM);
	state.vertexArray = integer(GL_VERTEX_ARRAY_BINDING);
	state.arrayBuffer = integer(GL_ARRAY_BUFFER_BINDING);
	state.unpackAlignment = integer(GL_UNPACK_ALIGNMENT);
	return state;
}

void GlState::restore() const {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(framebuffers[0]));
	glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(framebuffers[1]));
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
	glBlendFuncSeparate(static_cast<GLenum>(blendFunctions[0]), static_cast<GLenum>(blendFunctions[1]),
	                    static_cast<GLenum>(blendFunctions[2]), static_cast<GLenum>(blendFunctions[3]));
	glBlendEquationSeparate(static_cast<GLenum>(blendEquations[0]), static_cast<GLenum>(blendEquations[1]));
	glStencilFunc(static_cast<GLenum>(stencilFunction[0]), stencilFunction[1], static_cast<GLuint>(stencilFunction[2]));
	glStencilMask(static_cast<GLuint>(stencilFunction[3]));
	glStencilOp(static_cast<GLenum>(stencilOperations[0]), static_cast<GLenum>(stencilOperations[1]),
	            static_cast<GLenum>(stencilOperations[2]));
	glColorMask(static_cast<GLboolean>(colorMask[0]), static_cast<GLboolean>(colorMask[1]),
	            static_cast<GLboolean>(colorMask[2]), static_cast<GLboolean>(colorMask[3]));
	glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
	for(std::size_t unit = 0; unit < textures.size(); ++unit) {
		glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textures[unit]));
	}
	glActiveTexture(static_cast<GLenum>(activeTexture));
	for(std::size_t index = 0; index < switches.size(); ++index) enable(switches[index], enabled[index]);
	glDepthFunc(static_cast<GLenum>(depthFunction));
	glDepthMask(depthMask);
	glClearDepth(clearDepth);
	glClearStencil(clearStencil);
	glFrontFace(static_cast<GLenum>(frontFace));
	glCullFace(static_cast<GLenum>(cullFace));
	glUseProgram(static_cast<GLuint>(program));
	glBindVertexArray(static_cast<GLuint>(vertexArray));
	glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
	glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
}

} // namespace affresco::gl
