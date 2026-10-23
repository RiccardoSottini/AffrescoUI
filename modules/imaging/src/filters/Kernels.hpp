#pragma once

/// @file
/// @brief The filters' kernels on a square region of premultiplied RGBA floats: a tile and its halo. Private to the
/// imaging module.

#include <array>
#include <cstdint>
#include <vector>

namespace affresco::imaging::kernels {

/// @brief The radii of three box blurs that together approximate a Gaussian.
/// @param sigma The Gaussian's standard deviation.
[[nodiscard]] std::array<int, 3> gaussianBoxes(float sigma) noexcept;

/// @brief Blurs a region in place; afterwards its centre (the region less the boxes' radii) holds the blur.
/// @param region side × side RGBA floats.
/// @param side The region's side.
/// @param sigma The standard deviation.
/// @param scratch Scratch, resized as needed.
void gaussianBlur(std::vector<float>& region, int side, float sigma, std::vector<float>& scratch);

/// @brief The motion blur of a region's centre tile.
/// @param region side × side RGBA floats, where side is tileSize + 2 × halo.
/// @param halo The region's halo.
/// @param angle The direction, degrees anticlockwise from across.
/// @param distance The length of the smear.
/// @param out tileSize × tileSize RGBA floats.
void motionBlur(const std::vector<float>& region, int halo, float angle, float distance, std::vector<float>& out);

/// @brief A random number from -1 to 1 for a pixel's channel, the same every time for the same arguments.
/// @param seed The seed.
/// @param x The pixel across.
/// @param y The pixel down.
/// @param channel The channel.
[[nodiscard]] float noise(std::uint32_t seed, int x, int y, int channel) noexcept;

} // namespace affresco::imaging::kernels
