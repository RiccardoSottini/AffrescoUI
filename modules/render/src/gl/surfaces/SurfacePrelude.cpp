/// @file
/// @brief surfaceShaderPrelude(): the lines surfaces' shaders start with, those of this build's backend
/// (gl/Shader.hpp's shaderPrelude()): `#version 410 core`, or on OpenGL ES (WebGL 2, ANGLE) `#version 300 es` and the
/// precision.

#include "affresco/render/GpuSurface.hpp"

#include "gl/Shader.hpp"

namespace affresco {

// The fragment stage's, which on OpenGL ES also sets the precision: valid at the top of a vertex shader too
std::string_view surfaceShaderPrelude() noexcept { return gl::shaderPrelude(gl::ShaderStage::fragment); }

} // namespace affresco
