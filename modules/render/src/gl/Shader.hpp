#pragma once

/// @file
/// @brief Shader program creation.

#include "gl/GlObjects.hpp"

#include <string_view>

namespace sgl::gl {

/// @brief Compiles and links a vertex and fragment shader, in the current context.
/// @param vertexSource GLSL vertex shader.
/// @param fragmentSource GLSL fragment shader.
/// @return The program.
/// @throws std::runtime_error With the driver's log, if compiling or linking fails.
[[nodiscard]] Program linkProgram(std::string_view vertexSource, std::string_view fragmentSource);

} // namespace sgl::gl
