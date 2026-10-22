/// @file
/// @brief surfaceShaderPrelude(): the GLSL version line of surfaces' shaders. One line, in a file of its own, so that
/// a build for OpenGL ES (WebGL 2, ANGLE) swaps it for gl/Shader.hpp's shaderPrelude().

#include "affresco/render/GpuSurface.hpp"

namespace affresco {

std::string_view surfaceShaderPrelude() noexcept { return "#version 410 core\n"; }

} // namespace affresco
