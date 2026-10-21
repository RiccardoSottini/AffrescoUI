#pragma once

/// @file
/// @brief SVG icons: a small subset of SVG, read and rasterised into a Bitmap with the vector path meshes.

#include "affresco/render/Bitmap.hpp"

#include <cstdint>
#include <memory>
#include <span>

namespace affresco::vector {

/// @brief Tells whether bytes look like an SVG document: text starting with a tag, with an svg element early on.
/// @param bytes The bytes.
[[nodiscard]] bool looksLikeSvg(std::span<const std::uint8_t> bytes);

/// @brief Rasterises an SVG document at twice its size (at most 2048 pixels a side), for crisp icons.
///
/// The subset icons use: svg (width, height, viewBox), g, path, rect (rx, ry), circle, ellipse, line, polyline and
/// polygon; fill, stroke and their opacities, fill-rule, stroke-width, -linejoin, -linecap, -miterlimit and
/// -dasharray, as attributes or in a style attribute, inherited; translate, scale and axis-aligned matrix transforms.
/// Colours are hex, a few names, or currentColor (white, so the icon can be tinted). Group opacity multiplies into its
/// children's. What else it holds (gradients, text, defs, rotations) is skipped. Bounded: 1 MiB, 4096 elements, 32
/// levels deep, and a bound on the pixels drawn.
/// @param bytes The document.
/// @param limits What the image may be.
/// @return The image.
/// @throws std::runtime_error If it isn't an SVG document within the limits.
[[nodiscard]] std::shared_ptr<const Bitmap> rasterizeSvg(std::span<const std::uint8_t> bytes,
                                                         const Bitmap::Limits& limits);

} // namespace affresco::vector
