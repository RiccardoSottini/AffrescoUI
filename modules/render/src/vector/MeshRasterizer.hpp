#pragma once

/// @file
/// @brief vector::Canvas: pixels that meshes of quads (fills and strokes) are drawn into on the CPU, for images such
/// as SVG icons that become a Bitmap.

#include "affresco/render/Color.hpp"
#include "affresco/render/Painter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace affresco::vector {

/// @brief Premultiplied RGBA pixels that meshes are blended into (source over), with a bound on the work.
///
/// Each quad is two triangles; a pixel takes a triangle's colour times the coverage interpolated at its centre, so
/// the meshes' fringes anti-alias the edges. Pixels on an edge two triangles share go to one of them only.
class Canvas {
public:
	/// @brief A transparent canvas.
	/// @param width Its width, at least 1.
	/// @param height Its height, at least 1.
	/// @param workLimit The most pixels visited (each triangle visits its bounding box); past it, nothing is drawn.
	Canvas(int width, int height, std::size_t workLimit);

	/// @brief Draws a mesh.
	/// @param quads Its quads, 4 vertices each, in pixel units.
	/// @param color Its colour (not premultiplied).
	void fill(std::span<const MeshVertex> quads, Color color);

	/// @brief Its pixels, not premultiplied: red, green, blue, alpha, top row first.
	[[nodiscard]] std::vector<std::uint8_t> straightPixels() const;

	/// @brief Its width.
	[[nodiscard]] int width() const noexcept { return width_; }
	/// @brief Its height.
	[[nodiscard]] int height() const noexcept { return height_; }

private:
	/// @brief Draws a triangle.
	/// @param corners Its corners.
	/// @param color Its colour, premultiplied, in [0, 1].
	void triangle(const std::array<MeshVertex, 3>& corners, const std::array<float, 4>& color);
	/// @brief Blends a colour into a pixel.
	/// @param index The pixel's first byte.
	/// @param color The colour, premultiplied, in [0, 1].
	/// @param coverage How much of it.
	void blend(std::size_t index, const std::array<float, 4>& color, float coverage) noexcept;

	int width_;                        ///< See width().
	int height_;                       ///< See height().
	std::size_t workLeft_;             ///< Pixels that may still be visited.
	std::vector<std::uint8_t> pixels_; ///< Premultiplied RGBA.
};

} // namespace affresco::vector
