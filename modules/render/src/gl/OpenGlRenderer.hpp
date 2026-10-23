#pragma once

/// @file
/// @brief gl::createRenderer: the OpenGL 4.1 core implementation of detail::Renderer.

#include "paint/Renderer.hpp"

#include <memory>

namespace affresco::gl {

/// @brief Loads the OpenGL functions of the current context, and creates a renderer that draws into it with a
/// gl::QuadRenderer.
/// @param loader Looks the context's functions up.
/// @return The renderer.
/// @throws std::runtime_error If the functions can't be loaded or the shaders fail.
[[nodiscard]] std::unique_ptr<detail::Renderer> createRenderer(platform::ProcAddressLoader loader);

} // namespace affresco::gl
