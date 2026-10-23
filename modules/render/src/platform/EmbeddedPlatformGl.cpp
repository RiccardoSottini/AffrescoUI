/// @file
/// @brief gl::SavedState and gl::EmbeddedTarget: the OpenGL side of an embedding. What the library changes in the
/// host's context is saved first and restored after; its frames go into a texture of its own, drawn over the host's
/// framebuffer with premultiplied alpha.

#include "platform/EmbeddedPlatform.hpp"

#include "gl/Shader.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace affresco::gl {

namespace {

/// @brief A triangle that covers the viewport, its corners from gl_VertexID, and the texture's coordinates. After
/// shaderPrelude(), like every shader, so that it compiles on OpenGL ES too.
constexpr const char* compositeVertex = R"(out vec2 coordinates;
void main() {
	vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
	coordinates = corner;
	gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

/// @brief Samples the texture as it is: its colours are already multiplied by alpha.
constexpr const char* compositeFragment = R"(in vec2 coordinates;
uniform sampler2D picture;
out vec4 color;
void main() { color = texture(picture, coordinates); }
)";

/// @brief Enables or disables a capability.
/// @param capability The capability.
/// @param enabled Whether it is enabled.
void setEnabled(GLenum capability, GLboolean enabled) {
	if(enabled != 0) glEnable(capability);
	else glDisable(capability);
}

/// @brief Reads one integer of state.
/// @param name The state's name.
/// @return Its value.
GLint integer(GLenum name) noexcept {
	GLint value = 0;
	glGetIntegerv(name, &value);
	return value;
}

} // namespace

SavedState::SavedState() noexcept
    : drawFramebuffer_(integer(GL_DRAW_FRAMEBUFFER_BINDING)), readFramebuffer_(integer(GL_READ_FRAMEBUFFER_BINDING)),
      renderbuffer_(integer(GL_RENDERBUFFER_BINDING)), blendSourceRgb_(integer(GL_BLEND_SRC_RGB)),
      blendDestinationRgb_(integer(GL_BLEND_DST_RGB)), blendSourceAlpha_(integer(GL_BLEND_SRC_ALPHA)),
      blendDestinationAlpha_(integer(GL_BLEND_DST_ALPHA)), blendEquationRgb_(integer(GL_BLEND_EQUATION_RGB)),
      blendEquationAlpha_(integer(GL_BLEND_EQUATION_ALPHA)), program_(integer(GL_CURRENT_PROGRAM)),
      vertexArray_(integer(GL_VERTEX_ARRAY_BINDING)), arrayBuffer_(integer(GL_ARRAY_BUFFER_BINDING)),
      activeTexture_(integer(GL_ACTIVE_TEXTURE)) {
	glGetIntegerv(GL_VIEWPORT, viewport_.data());
	glGetIntegerv(GL_SCISSOR_BOX, scissorBox_.data());
	for(GLint unit = 0; unit < 2; ++unit) {
		glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
		textures_[static_cast<std::size_t>(unit)] = integer(GL_TEXTURE_BINDING_2D);
	}
	packAlignment_ = integer(GL_PACK_ALIGNMENT);
	unpackAlignment_ = integer(GL_UNPACK_ALIGNMENT);
	unpackRowLength_ = integer(GL_UNPACK_ROW_LENGTH);
	packBuffer_ = integer(GL_PIXEL_PACK_BUFFER_BINDING);
	unpackBuffer_ = integer(GL_PIXEL_UNPACK_BUFFER_BINDING);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor_.data());
	glGetBooleanv(GL_COLOR_WRITEMASK, colorMask_.data());
	blend_ = glIsEnabled(GL_BLEND);
	scissor_ = glIsEnabled(GL_SCISSOR_TEST);
	depth_ = glIsEnabled(GL_DEPTH_TEST);
	stencil_ = glIsEnabled(GL_STENCIL_TEST);
	cull_ = glIsEnabled(GL_CULL_FACE);
#ifndef AFFRESCOUI_GLES3 // OpenGL ES has no switch for sRGB writes
	srgb_ = glIsEnabled(GL_FRAMEBUFFER_SRGB);
#endif

	// What the renderer assumes: OpenGL's defaults, whatever the host left
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
#ifndef AFFRESCOUI_GLES3
	glDisable(GL_FRAMEBUFFER_SRGB);
#endif
	glDisable(GL_SCISSOR_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBlendEquation(GL_FUNC_ADD);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glActiveTexture(GL_TEXTURE0);
}

SavedState::~SavedState() {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer_));
	glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer_));
	glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer_));
	glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
	glScissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
	glBlendFuncSeparate(static_cast<GLenum>(blendSourceRgb_), static_cast<GLenum>(blendDestinationRgb_),
	                    static_cast<GLenum>(blendSourceAlpha_), static_cast<GLenum>(blendDestinationAlpha_));
	glBlendEquationSeparate(static_cast<GLenum>(blendEquationRgb_), static_cast<GLenum>(blendEquationAlpha_));
	glUseProgram(static_cast<GLuint>(program_));
	glBindVertexArray(static_cast<GLuint>(vertexArray_));
	glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer_));
	for(GLint unit = 0; unit < 2; ++unit) {
		glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textures_[static_cast<std::size_t>(unit)]));
	}
	glActiveTexture(static_cast<GLenum>(activeTexture_));
	glPixelStorei(GL_PACK_ALIGNMENT, packAlignment_);
	glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment_);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength_);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(packBuffer_));
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpackBuffer_));
	glClearColor(clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]);
	glColorMask(colorMask_[0], colorMask_[1], colorMask_[2], colorMask_[3]);
	setEnabled(GL_BLEND, blend_);
	setEnabled(GL_SCISSOR_TEST, scissor_);
	setEnabled(GL_DEPTH_TEST, depth_);
	setEnabled(GL_STENCIL_TEST, stencil_);
	setEnabled(GL_CULL_FACE, cull_);
#ifndef AFFRESCOUI_GLES3
	setEnabled(GL_FRAMEBUFFER_SRGB, srgb_);
#else
	static_cast<void>(srgb_); // kept on both backends, so the class is one layout wherever it is included
#endif
}

EmbeddedTarget::EmbeddedTarget()
    : texture_(Texture::create()), framebuffer_(Framebuffer::create()),
      program_(linkProgram(std::string(shaderPrelude(ShaderStage::vertex)) + compositeVertex,
	                       std::string(shaderPrelude(ShaderStage::fragment)) + compositeFragment)),
      vertexArray_(VertexArray::create()) {
	glUseProgram(program_.id());
	glUniform1i(glGetUniformLocation(program_.id(), "picture"), 0);
}

void EmbeddedTarget::resize(int width, int height) {
	if(width == width_ && height == height_) return;
	width_ = width;
	height_ = height;
	glBindTexture(GL_TEXTURE_2D, texture_.id());
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.id());
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_.id(), 0);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void EmbeddedTarget::composite(GLuint framebuffer, int width, int height) const {
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glViewport(0, 0, width, height);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(program_.id());
	glBindVertexArray(vertexArray_.id());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_.id());
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

std::vector<std::uint8_t> EmbeddedTarget::pixels() const {
	const auto rowSize = static_cast<std::size_t>(width_) * 4;
	const auto rows = static_cast<std::size_t>(height_);
	std::vector<std::uint8_t> result(rowSize * rows);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_.id());
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
	// OpenGL gives the bottom row first: flip to the usual top-down order
	for(std::size_t top = 0, bottom = rows - 1; rows > 0 && top < bottom; ++top, --bottom) {
		std::swap_ranges(result.begin() + static_cast<std::ptrdiff_t>(top * rowSize),
		                 result.begin() + static_cast<std::ptrdiff_t>((top + 1) * rowSize),
		                 result.begin() + static_cast<std::ptrdiff_t>(bottom * rowSize));
	}
	return result;
}

} // namespace affresco::gl
