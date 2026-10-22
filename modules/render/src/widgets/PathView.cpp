/// @file
/// @brief PathView: fitting a path's view box to the panel, building its meshes when its size changes, drawing them.

#include "affresco/render/PathView.hpp"
#include "affresco/render/Painter.hpp"

#include <algorithm>
#include <utility>

namespace affresco {

PathView::PathView() { setColor(colors::transparent); }

void PathView::changed() {
	builtFor_.reset();
	invalidate();
}

void PathView::setPath(VectorPath path) {
	path_ = std::move(path);
	pathData_.clear();
	changed();
}

void PathView::setPathData(std::string_view data) {
	if(data == pathData_ && !data.empty()) return;
	setPath(VectorPath::parse(data));
	pathData_ = data;
}

void PathView::setViewBox(std::optional<Rect> viewBox) {
	if(viewBox == viewBox_) return;
	viewBox_ = viewBox;
	changed();
}

void PathView::setFit(ImageFit fit) {
	if(fit == fit_) return;
	fit_ = fit;
	changed();
}

void PathView::setFillColor(Color color) {
	if(color == fillColor_) return;
	fillColor_ = color;
	changed();
}

void PathView::setFillRule(FillRule rule) {
	if(rule == fillRule_) return;
	fillRule_ = rule;
	changed();
}

void PathView::setStrokeColor(Color color) {
	if(color == strokeColor_) return;
	strokeColor_ = color;
	changed();
}

void PathView::setStrokeStyle(StrokeStyle style) {
	if(style == strokeStyle_) return;
	strokeStyle_ = std::move(style);
	changed();
}

PathTransform PathView::transformFor(Vec2 size) const noexcept {
	if(!viewBox_ || !(viewBox_->width() > 0.0f) || !(viewBox_->height() > 0.0f)) return {};
	const Rect& box = *viewBox_;
	Vec2 scale{size.x / box.width(), size.y / box.height()};
	if(fit_ != ImageFit::fill) {
		const float even = fit_ == ImageFit::cover ? std::max(scale.x, scale.y) : std::min(scale.x, scale.y);
		scale = {even, even};
	}
	// Centred: the box's middle lands on the panel's middle
	const Vec2 offset{(size.x - box.width() * scale.x) / 2.0f - box.left * scale.x,
	                  (size.y - box.height() * scale.y) / 2.0f - box.top * scale.y};
	return {scale, offset};
}

// The stroke scales with an even fit; stretched unevenly it would be uneven, so it stays in window units
void PathView::build(Vec2 size) const {
	if(builtFor_ == size) return;
	builtFor_ = size;
	fillMesh_.clear();
	strokeMesh_.clear();
	const PathTransform transform = transformFor(size);
	const std::vector<VectorPath::Contour> contours = path_.flatten(transform);
	if(fillColor_.a > 0.0f) appendFill(contours, fillRule_, fillMesh_);
	if(strokeColor_.a <= 0.0f) return;
	StrokeStyle style = strokeStyle_;
	if(viewBox_ && fit_ != ImageFit::fill) {
		style.width *= transform.scale.x;
		for(float& length : style.dash) length *= transform.scale.x;
	}
	appendStroke(contours, style, strokeMesh_);
}

void PathView::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& box = frame();
	build({box.width(), box.height()});
	if(!fillMesh_.empty()) painter.fillMesh(fillMesh_, {box.left, box.top}, fillColor_);
	if(!strokeMesh_.empty()) painter.fillMesh(strokeMesh_, {box.left, box.top}, strokeColor_);
}

} // namespace affresco
