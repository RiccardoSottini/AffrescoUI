/// @file
/// @brief ImageView: showing an image, fitted to the panel.

#include "affresco/render/ImageView.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/render/Painter.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace affresco {

ImageView::ImageView() { setColor(colors::transparent); }

void ImageView::setBitmap(const std::shared_ptr<const Bitmap>& bitmap) {
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

void ImageView::showData(std::string_view uri, const Bitmap::Limits& limits) {
	std::string source = "data:" + std::string(uri);
	if(source == source_) return;
	request_.cancel();
	source_ = std::move(source);
	std::shared_ptr<const Bitmap> bitmap;
	try {
		bitmap = Bitmap::fromDataUri(uri, limits);
	} catch(const std::runtime_error& error) {
		log(LogLevel::Warning, std::string("AffrescoUI: an inline image can't be shown: ") + error.what());
	}
	deliver(bitmap);
}

void ImageView::setHighlight(const std::optional<std::array<double, 4>>& region) {
	if(region == highlight_) return;
	highlight_ = region;
	invalidate();
}

void ImageView::setHighlightColor(Color color) {
	if(color == highlightColor_) return;
	highlightColor_ = color;
	invalidate();
}

Rect ImageView::highlighted(ImageFit fit, const Rect& frame, int width, int height,
                            const std::array<double, 4>& region) noexcept {
	if(width <= 0 || height <= 0) return {};
	const Rect image = fitted(fit, frame, width, height);
	// a region from a peer may be anything: kept near the image, so floats stay finite and the outline is clipped
	const auto within = [](double value, double size) { return std::clamp(value, -size, 2.0 * size); };
	const double w = width, h = height;
	const double left = within(region[0], w), top = within(region[1], h);
	const double right = within(region[0] + std::max(region[2], 0.0), w);
	const double bottom = within(region[1] + std::max(region[3], 0.0), h);
	if(!(right > left && bottom > top)) return {}; // and NaN is nothing
	const double scaleX = image.width() / w, scaleY = image.height() / h;
	return {image.left + static_cast<float>(left * scaleX), image.top + static_cast<float>(top * scaleY),
	        image.left + static_cast<float>(right * scaleX), image.top + static_cast<float>(bottom * scaleY)};
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
	if(!bitmap_) return;
	painter.drawImage(*bitmap_, fitted(fit_, frame(), bitmap_->width(), bitmap_->height()));
	if(!highlight_) return;
	const Rect region = highlighted(fit_, frame(), bitmap_->width(), bitmap_->height(), *highlight_);
	if(region.width() <= 0 || region.height() <= 0) return;
	constexpr float thickness = 3.0f; // outside the region, so what changed stays visible
	const Rect outer{region.left - thickness, region.top - thickness, region.right + thickness,
	                 region.bottom + thickness};
	painter.fillRect({outer.left, outer.top, outer.right, region.top}, highlightColor_);
	painter.fillRect({outer.left, region.bottom, outer.right, outer.bottom}, highlightColor_);
	painter.fillRect({outer.left, region.top, region.left, region.bottom}, highlightColor_);
	painter.fillRect({region.right, region.top, outer.right, region.bottom}, highlightColor_);
}

} // namespace affresco
