#pragma once

// Operations on the current OpenGL context, so that no code outside src/gl calls OpenGL directly

#include <cstdint>
#include <vector>

namespace sgl::gl {

using ProcAddressLoader = void* (*)(const char* name);

// Loads the OpenGL 4.1 core functions for the current context; false if the driver doesn't provide them
[[nodiscard]] bool loadFunctions(ProcAddressLoader loader);

// Starts a frame: viewport covering the whole framebuffer, cleared to transparent black
void beginFrame(int framebufferWidth, int framebufferHeight);

// Reads the back buffer as tightly packed 8-bit RGB, top row first
[[nodiscard]] std::vector<std::uint8_t> readBackBuffer(int width, int height);

} // namespace sgl::gl
