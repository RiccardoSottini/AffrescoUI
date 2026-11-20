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

void PathView::setFillGradient(std::optional<Gradient> gradient) {
	if(gradient == fillGradient_) return;
	fillGradient_ = std::move(gradient);
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
	gradientBands_.clear();
	const PathTransform transform = transformFor(size);
	const std::vector<VectorPath::Contour> contours = path_.flatten(transform);
	if(fillGradient_) {
		appendFill(contours, fillRule_, fillMesh_);
		appendGradientFill(fillMesh_, *fillGradient_, {0.0f, 0.0f, size.x, size.y}, gradientBands_);
		fillMesh_.clear();
	} else if(fillColor_.a > 0.0f) appendFill(contours, fillRule_, fillMesh_);
	if(strokeColor_.a <= 0.0f) return;
	StrokeStyle style = strokeStyle_;
	if(viewBox_ && fit_ != ImageFit::fill) {
		style.width *= transform.scale.x;
		for(float& length : style.dash) length *= transform.scale.x;
	}
	appendStroke(contours, style, strokeMesh_);
}

// A translucent path is drawn as a group (Panel::setPaintOpacity()): its mesh's quads overlap at joins, edges and
// crossings, which would show darker drawn one by one. Only when the group holds nothing else and is drawn untransformed
float PathView::groupOpacity() const {
	const float fill = fillMesh_.empty() ? 0.0f : fillColor_.a, stroke = strokeMesh_.empty() ? 0.0f : strokeColor_.a;
	const float most = std::max(fill, stroke);
	if(!(most > 0.0f && most < 1.0f) || !gradientBands_.empty() || !children().empty() || color().a > 0.0f ||
	   borderWidth() > 0.0f || shadow().isVisible())
		return 1.0f;
	for(const Panel* panel = this; panel; panel = panel->parent())
		if(panel->isTransformed()) return 1.0f; // a transformed subtree is drawn directly, not through a texture
	return most;
}

void PathView::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& box = frame();
	build({box.width(), box.height()});
	const float group = groupOpacity();
	setPaintOpacity(group);
	const auto inGroup = [group](Color color) {
		color.a = std::min(color.a / group, 1.0f);
		return color;
	};
	for(const GradientBand& band : gradientBands_) painter.fillMesh(band.quads, {box.left, box.top}, band.color);
	if(!fillMesh_.empty()) painter.fillMesh(fillMesh_, {box.left, box.top}, inGroup(fillColor_));
	if(!strokeMesh_.empty()) painter.fillMesh(strokeMesh_, {box.left, box.top}, inGroup(strokeColor_));
}

} // namespace affresco
