#pragma once

#include "gl/GlObjects.hpp"

#include <string_view>

namespace sgl::gl {

// Compiles and links a vertex + fragment shader pair; throws std::runtime_error with the driver log
[[nodiscard]] Program linkProgram(std::string_view vertexSource, std::string_view fragmentSource);

} // namespace sgl::gl
