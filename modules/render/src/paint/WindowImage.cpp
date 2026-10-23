/// @file
/// @brief Window::renderToImage(): one panel's subtree drawn from scratch into an image (thumbnails, previews).

#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Window.hpp"

#include "paint/Renderer.hpp"
#include "platform/Platform.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace affresco {

namespace {

/// @brief A rectangle of an image, in pixels from its top left corner.
struct PixelArea {
	int left = 0;   ///< Its left edge.
	int top = 0;    ///< Its top edge.
	int width = 0;  ///< Its width.
	int height = 0; ///< Its height.
};

/// @brief Copies a rectangle of tightly packed RGB pixels as opaque RGBA.
/// @param rgb The whole image, top row first.
/// @param imageWidth Its width, in pixels.
/// @param area The rectangle: left, top, width and height in pixels, inside the image.
/// @return Its pixels, top row first.
std::vector<std::uint8_t> cropToRgba(std::span<const std::uint8_t> rgb, int imageWidth, const PixelArea& area) {
	std::vector<std::uint8_t> rgba(static_cast<std::size_t>(area.width) * static_cast<std::size_t>(area.height) * 4);
	std::size_t out = 0;
	for(int row = 0; row < area.height; ++row) {
		std::size_t in = (static_cast<std::size_t>(area.top + row) * static_cast<std::size_t>(imageWidth) +
		                  static_cast<std::size_t>(area.left)) *
		                 3;
		for(int column = 0; column < area.width; ++column, in += 3) {
			rgba[out++] = rgb[in];
			rgba[out++] = rgb[in + 1];
			rgba[out++] = rgb[in + 2];
			rgba[out++] = 255;
		}
	}
	return rgba;
}

} // namespace

std::shared_ptr<const Bitmap> Window::renderToImage(Panel& panel, float scale) {
	if(!renderer_ || !(scale > 0.0f)) return nullptr;
	runDeferred();
	native_->makeCurrent(); // the image is created in it
	updateLayout();
	const Rect area = panel.bounds();
	if(!panel.isShown() || area.width() <= 0.0f || area.height() <= 0.0f) return nullptr;

	// The frame reaches past the window if the panel does, so that nothing of it is clipped by the window's edges
	const int frameWidth = std::max(width_, static_cast<int>(std::ceil(area.right)));
	const int frameHeight = std::max(height_, static_cast<int>(std::ceil(area.bottom)));
	const auto pixels = [scale](float units) { return static_cast<int>(std::lround(units * scale)); };
	const PixelArea crop{std::max(0, pixels(area.left)), std::max(0, pixels(area.top)),
	                     std::max(1, pixels(area.width())), std::max(1, pixels(area.height()))};
	const platform::PixelSize size{std::max(pixels(static_cast<float>(frameWidth)), crop.left + crop.width),
	                               std::max(pixels(static_cast<float>(frameHeight)), crop.top + crop.height)};
	const Bitmap::Limits limits;
	if(crop.width > limits.side || crop.height > limits.side || size.width > limits.side * 2 ||
	   size.height > limits.side * 2)
		throw std::invalid_argument("renderToImage: the image would be larger than " + std::to_string(limits.side) +
		                            " pixels a side");

	const detail::Frame frame{panel, {}, nullptr, frameWidth, frameHeight, layoutPass_, paintGeneration_, {}, false};
	const std::vector<std::uint8_t> rgb = renderer_->drawFromScratch(frame, size);
	return Bitmap::fromPixels(crop.width, crop.height, cropToRgba(rgb, size.width, crop), limits);
}

} // namespace affresco
