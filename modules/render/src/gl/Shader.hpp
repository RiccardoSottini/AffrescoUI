#pragma once

/// @file
/// @brief Shader program creation, and the one prelude every shader starts with.
///
/// Shader bodies are written once, in the common subset of GLSL 4.10 core and GLSL ES 3.00: no implicit conversions
/// between int and float, no features newer than either, `layout(location)` only on vertex inputs and fragment
/// outputs. shaderPrelude() adds what differs, the `#version` line and, on OpenGL ES, the default precision.

#include "gl/GlObjects.hpp"

#include <string_view>

namespace affresco::gl {

/// @brief A stage of a shader program.
enum class ShaderStage {
	vertex,   ///< The vertex shader.
	fragment, ///< The fragment shader.
};

/// @brief The lines a shader of this build's backend starts with: `#version 410 core` (AFFRESCOUI_GL_BACKEND=gl41),
/// or `#version 300 es` and the default precision (gles3).
/// @param stage The shader's stage: on OpenGL ES, fragment shaders have no default float precision.
/// @return The prelude, ending with a newline.
[[nodiscard]] std::string_view shaderPrelude(ShaderStage stage) noexcept;

/// @brief Compiles and links a vertex and fragment shader, in the current context.
/// @param vertexSource GLSL vertex shader, starting with shaderPrelude(ShaderStage::vertex).
/// @param fragmentSource GLSL fragment shader, starting with shaderPrelude(ShaderStage::fragment).
/// @return The program.
/// @throws std::runtime_error With the driver's log, if compiling or linking fails.
[[nodiscard]] Program linkProgram(std::string_view vertexSource, std::string_view fragmentSource);

} // namespace affresco::gl
