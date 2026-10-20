/// @file
/// @brief ImageView: showing an image, fitted to the panel.

#include "affresco/render/ImageView.hpp"
#include "affresco/render/Painter.hpp"

#include <algorithm>
#include <utility>

namespace affresco {

ImageView::ImageView() { setColor(colors::transparent); }

void ImageView::setBitmap(std::shared_ptr<const Bitmap> bitmap) {
	request_.cancel();
	source_.clear();
	deliver(bitmap);
}

void ImageView::showFile(ImageCache& cache, const std::filesystem::path& file) {
	std::string source = "file:" + file.string();
	if(source == source_) return;
	source_ = std::move(source);
	// The delivery may run at once, inside load(): the request returned is then empty
	request_ = cache.load(file, [this](const std::shared_ptr<const Bitmap>& bitmap) { deliver(bitmap); });
}

void ImageView::showNamed(ImageCache& cache, const std::string& name) {
	std::string source = "name:" + name;
	if(source == source_) return;
	source_ = std::move(source);
	request_ = cache.watch(name, [this](const std::shared_ptr<const Bitmap>& bitmap) { deliver(bitmap); });
}

void ImageView::deliver(const std::shared_ptr<const Bitmap>& bitmap) {
	if(bitmap == bitmap_) return;
	bitmap_ = bitmap;
	invalidate();
}

void ImageView::setFit(ImageFit fit) {
	if(fit == fit_) return;
	fit_ = fit;
	invalidate();
}

Rect ImageView::fitted(ImageFit fit, const Rect& frame, int width, int height) noexcept {
	if(fit == ImageFit::fill || width <= 0 || height <= 0) return frame;
	const float scaleX = frame.width() / static_cast<float>(width),
	            scaleY = frame.height() / static_cast<float>(height);
	const float scale = fit == ImageFit::cover ? std::max(scaleX, scaleY) : std::min(scaleX, scaleY);
	const float drawnWidth = static_cast<float>(width) * scale, drawnHeight = static_cast<float>(height) * scale;
	const float left = frame.left + (frame.width() - drawnWidth) / 2.0f;
	const float top = frame.top + (frame.height() - drawnHeight) / 2.0f;
	return {left, top, left + drawnWidth, top + drawnHeight};
}

void ImageView::paint(Painter& painter) const {
	Panel::paint(painter);
	if(bitmap_) painter.drawImage(*bitmap_, fitted(fit_, frame(), bitmap_->width(), bitmap_->height()));
}

} // namespace affresco
