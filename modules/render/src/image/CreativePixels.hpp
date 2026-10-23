#pragma once

/// @file
/// @brief What the creative controls (ColorPicker, GradientEditor, CurvesEditor) share: CPU bitmaps cached by their
/// inputs (the Painter has no gradients), keys for those caches, conversions and markers.

#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/ColorSpaces.hpp"
#include "affresco/render/Painter.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace affresco::detail {

/// @brief The widest and tallest bitmap a creative control builds; drawImage() stretches it to the control's size.
inline constexpr int maxCreativeBitmapSide = 256;

/// @brief Converts a 0..1 component to a byte.
/// @param component The component (clamped).
/// @return 0..255.
[[nodiscard]] inline std::uint8_t toByte(double component) noexcept {
	return static_cast<std::uint8_t>(std::lround(std::clamp(component, 0.0, 1.0) * 255.0));
}

/// @brief Folds a number into a cache key (FNV-1a over its bits).
/// @param key The key so far.
/// @param value The number.
/// @return The new key.
[[nodiscard]] inline std::uint64_t mixKey(std::uint64_t key, double value) noexcept {
	const auto bits = std::bit_cast<std::uint64_t>(value);
	for(int shift = 0; shift < 64; shift += 8) {
		key ^= (bits >> shift) & 0xffu;
		key *= 0x100000001b3ull;
	}
	return key;
}

/// @brief The start of a cache key.
inline constexpr std::uint64_t firstKey = 0xcbf29ce484222325ull;

/// @brief A colour as the ColorSpaces maths sees it.
/// @param color The colour.
/// @return Its red, green and blue.
[[nodiscard]] inline color::Rgb toRgb(Color color) noexcept { return {color.r, color.g, color.b}; }

/// @brief A colour from the ColorSpaces maths.
/// @param rgb Red, green and blue (clamped).
/// @param alpha The opacity.
/// @return The colour.
[[nodiscard]] inline Color fromRgb(color::Rgb rgb, float alpha = 1.0f) noexcept {
	const color::Rgb clamped = color::clampRgb(rgb);
	return {static_cast<float>(clamped.red), static_cast<float>(clamped.green), static_cast<float>(clamped.blue),
	        alpha};
}

/// @brief One cached bitmap, rebuilt only when its key or size changes, so drawing a frame allocates nothing.
class BitmapSlot {
public:
	/// @brief The bitmap for a key, building it when the key or the size differs from the last one.
	/// @tparam Fill Called as fill(x, y) for each pixel, returning a Color (not premultiplied).
	/// @param key What the pixels depend on.
	/// @param width Its width (clamped to 1..maxCreativeBitmapSide).
	/// @param height Its height (clamped likewise).
	/// @param fill Gives each pixel.
	/// @return The bitmap.
	template <typename Fill> const Bitmap& get(std::uint64_t key, int width, int height, Fill fill) {
		width = std::clamp(width, 1, maxCreativeBitmapSide);
		height = std::clamp(height, 1, maxCreativeBitmapSide);
		if(bitmap_ && key == key_ && bitmap_->width() == width && bitmap_->height() == height) return *bitmap_;
		pixels_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
		std::size_t at = 0;
		for(int y = 0; y < height; ++y) {
			for(int x = 0; x < width; ++x) {
				const Color pixel = fill(x, y);
				pixels_[at++] = toByte(pixel.r);
				pixels_[at++] = toByte(pixel.g);
				pixels_[at++] = toByte(pixel.b);
				pixels_[at++] = toByte(pixel.a);
			}
		}
		bitmap_ = Bitmap::fromPixels(width, height, pixels_, Bitmap::Limits{});
		key_ = key;
		return *bitmap_;
	}

private:
	std::uint64_t key_ = 0;                ///< What the bitmap was built for.
	std::shared_ptr<const Bitmap> bitmap_; ///< The bitmap, or nullptr before the first get().
	std::vector<std::uint8_t> pixels_;     ///< Reused while building, so a rebuild of the same size keeps its memory.
};

/// @brief The light or dark square of a checkerboard behind transparent colours.
/// @param x A pixel's column.
/// @param y Its row.
/// @param cell The side of a square, in pixels.
/// @return The square's colour.
[[nodiscard]] inline Color checker(int x, int y, int cell) noexcept {
	return ((x / cell + y / cell) % 2) == 0 ? Color{1.0f, 1.0f, 1.0f, 1.0f} : Color{0.8f, 0.8f, 0.8f, 1.0f};
}

/// @brief Draws a colour over another, both straight (not premultiplied).
/// @param top The colour on top.
/// @param bottom The opaque colour under it.
/// @return The opaque result.
[[nodiscard]] inline Color over(Color top, Color bottom) noexcept {
	return {top.r * top.a + bottom.r * (1.0f - top.a), top.g * top.a + bottom.g * (1.0f - top.a),
	        top.b * top.a + bottom.b * (1.0f - top.a), 1.0f};
}

/// @brief Draws a round marker: a white ring with a dark edge, readable on any colour.
/// @param painter Where to draw.
/// @param center Its centre, in window units.
/// @param radius Its radius.
inline void drawMarker(Painter& painter, Vec2 center, float radius) {
	const Rect outer{center.x - radius, center.y - radius, center.x + radius, center.y + radius};
	painter.strokeRoundedRect(outer, CornerRadii::all(radius), 1.0f, Color{0.0f, 0.0f, 0.0f, 0.6f});
	const Rect inner{outer.left + 1.0f, outer.top + 1.0f, outer.right - 1.0f, outer.bottom - 1.0f};
	painter.strokeRoundedRect(inner, CornerRadii::all(radius - 1.0f), 2.0f, colors::white);
}

/// @brief The text baseline that centres a line of a font vertically in a rectangle.
/// @param top The rectangle's top.
/// @param height Its height.
/// @param lineHeight The font's line height.
/// @param ascent The font's ascent.
/// @return The baseline's y.
[[nodiscard]] inline float centredBaseline(float top, float height, float lineHeight, float ascent) noexcept {
	return std::round(top + (height - lineHeight) / 2.0f + ascent);
}

} // namespace affresco::detail
