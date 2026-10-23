/// @file
/// @brief Window::renderToImage() and snapshots (Panel::setSnapshot()): one panel's subtree drawn from scratch into
/// an image (thumbnails, previews).

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
#include <utility>
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
	refreshSnapshots(); // the images it may show, as the next frame draws them
	if(!panel.isShown()) return nullptr;
	return drawImage(panel, scale, {static_cast<float>(width_), static_cast<float>(height_)});
}

std::shared_ptr<const Bitmap> Window::drawImage(Panel& panel, float scale, Vec2 least) {
	const Rect area = panel.bounds();
	if(area.width() <= 0.0f || area.height() <= 0.0f) return nullptr;

	// The frame reaches past its least size if the panel does, so that nothing of it is clipped by the frame's edges
	const int frameWidth = static_cast<int>(std::ceil(std::max(least.x, area.right)));
	const int frameHeight = static_cast<int>(std::ceil(std::max(least.y, area.bottom)));
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

/* Snapshots */

void Window::layoutSnapshot(Panel& panel, bool shown) const {
	const auto width = static_cast<float>(width_), height = static_cast<float>(height_);
	// Unclipped, so that the whole source is drawn: a clip far past any image Bitmap::Limits allows
	constexpr float far = 1.0e6f;
	panel.layout(Rect{0.0f, 0.0f, width, height}, Rect{-far, -far, far, far}, Vec2{width, height}, shown);
}

bool Window::snapshotChanged(const Panel& panel) const noexcept {
	const Panel::Snapshot& snapshot = *panel.snapshot_;
	return !snapshot.drawn || panel.paintStale_ || panel.paintBelow_ || panel.layoutPass_ > snapshot.pass ||
	       paintGeneration_ != snapshot.generation;
}

// Shown only while it is drawn: hidden, nothing can hit, focus, read or draw it on the window
std::shared_ptr<const Bitmap> Window::drawSnapshot(Panel& panel) {
	layingOut_ = true; // the frame's own layouts, which ask for no other
	layoutSnapshot(panel, true);
	std::shared_ptr<const Bitmap> image;
	try {
		image = drawImage(panel, panel.snapshot_->scale, {});
	} catch(const std::invalid_argument&) {
		image = nullptr; // past Bitmap::Limits: no image, as for a source with no area
	}
	layoutSnapshot(panel, false);
	layingOut_ = false;
	return image;
}

void Window::clearPaintMarks(Panel& panel) noexcept {
	panel.paintStale_ = panel.paintBelow_ = false;
	for(const auto& child : panel.children_) clearPaintMarks(*child);
}

// Every image is drawn before any is given: a handler may change the tree, and with it the list of snapshots. What
// the images change is laid out for this frame, and doesn't make a snapshot drawn before stale, so a snapshot showing
// another's image (or its own) is drawn again only when something else in it changes. One the handlers made is drawn
// in the next frame
void Window::refreshSnapshots() {
	if(snapshots_.empty() || !renderer_) return;
	std::vector<std::pair<Panel::SnapshotHandler, std::shared_ptr<const Bitmap>>> images;
	for(Panel* panel : snapshots_) {
		if(!snapshotChanged(*panel)) continue;
		images.emplace_back(panel->snapshot_->onImage, drawSnapshot(*panel));
		panel->snapshot_->drawn = true;
	}
	if(images.empty()) return;
	for(auto& [handler, image] : images)
		if(handler) handler(std::move(image));
	updateLayout();
	for(Panel* panel : snapshots_) {
		Panel::Snapshot& snapshot = *panel->snapshot_;
		if(!snapshot.drawn) continue;
		clearPaintMarks(*panel);
		snapshot.pass = layoutPass_;
		snapshot.generation = paintGeneration_;
	}
}

} // namespace affresco
