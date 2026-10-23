/// @file
/// @brief RasterView's overlays: what the tools are doing, drawn over the image (the crop being dragged, the marquee
/// or the lasso being drawn, the brush's outline). The selection's ants and the quick mask are the canvas surface's.

#include "RasterView.hpp"

#include "affresco/render/Painter.hpp"
#include "affresco/render/VectorPath.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace affresco::format {

namespace {

constexpr int ellipsePoints = 96;                  ///< The points of an elliptical marquee's outline.
constexpr float dashLength = 4.0f;                 ///< An outline's dashes and gaps, in window units.
constexpr Color underline = Color{0, 0, 0, 0.75f}; ///< Under an outline's dashes, so it shows on any image.
constexpr float smallestBrush = 3.0f;              ///< The brush outline's smallest diameter, in window units.

/// @brief A rectangle between two points, whichever way it was dragged.
/// @param from One corner.
/// @param to The other.
/// @return It.
[[nodiscard]] Rect between(Vec2 from, Vec2 to) noexcept {
	return {std::min(from.x, to.x), std::min(from.y, to.y), std::max(from.x, to.x), std::max(from.y, to.y)};
}

/// @brief Strokes an outline twice: solid and dark, then dashed in a colour, so it shows on any image.
/// @param painter The painter.
/// @param contour The outline, in window units.
/// @param color The dashes' colour.
void strokeOutline(Painter& painter, const VectorPath::Contour& contour, Color color) {
	if(contour.points.size() < 2) return;
	const std::span<const VectorPath::Contour> contours(&contour, 1);
	std::vector<MeshVertex> mesh;
	appendStroke(contours, StrokeStyle{.width = 1.0f}, mesh);
	painter.fillMesh(mesh, {}, underline);
	mesh.clear();
	appendStroke(contours, StrokeStyle{.width = 1.0f, .dash = {dashLength, dashLength}}, mesh);
	painter.fillMesh(mesh, {}, color);
}

/// @brief Adds a point to a contour unless it repeats the last one (a contour never holds two equal points in a
/// row).
/// @param contour The contour.
/// @param point The point.
void addPoint(VectorPath::Contour& contour, Vec2 point) {
	if(contour.points.empty() || contour.points.back().x != point.x || contour.points.back().y != point.y)
		contour.points.push_back(point);
}

} // namespace

void RasterView::paint(Painter& painter) const {
	SurfaceView::paint(painter);
	const editing::ImageDocument* shown = image();
	if(shown == nullptr || shown->canvas != shown_.lock()) return;
	using editing::Gesture;
	const Gesture gesture = shown->settings.gesture;
	if(shown->press.down && !shown->press.pan && gesture == Gesture::crop) paintCrop(painter, *shown);
	paintOutline(painter, *shown);
	if(hovered_ && !shown->spaceHeld && !shown->press.pan &&
	   (gesture == Gesture::paint || gesture == Gesture::erase || gesture == Gesture::clone))
		paintBrush(painter, *shown);
}

Vec2 RasterView::toWindow(const editing::ImageDocument& image, double x, double y) const noexcept {
	const auto [surfaceX, surfaceY] = image.view.toSurface(x, y);
	const double pixels = scale();
	return {static_cast<float>(frame().left + surfaceX / pixels), static_cast<float>(frame().top + surfaceY / pixels)};
}

void RasterView::paintCrop(Painter& painter, const editing::ImageDocument& image) const {
	const Rect& all = frame();
	const editing::Press& press = image.press;
	const Rect kept =
	    between(toWindow(image, press.start.x, press.start.y), toWindow(image, press.last.x, press.last.y));
	painter.fillRect({all.left, all.top, all.right, kept.top}, look_.shade);
	painter.fillRect({all.left, kept.bottom, all.right, all.bottom}, look_.shade);
	painter.fillRect({all.left, kept.top, kept.left, kept.bottom}, look_.shade);
	painter.fillRect({kept.right, kept.top, all.right, kept.bottom}, look_.shade);
	const Color thirds = look_.overlay.withAlpha(look_.overlay.a * 0.6f);
	for(const float third : {1.0f / 3.0f, 2.0f / 3.0f}) {
		const float x = std::round(kept.left + kept.width() * third);
		const float y = std::round(kept.top + kept.height() * third);
		painter.fillRect({x, kept.top, x + 1.0f, kept.bottom}, thirds);
		painter.fillRect({kept.left, y, kept.right, y + 1.0f}, thirds);
	}
	painter.strokeRoundedRect(kept, {}, 1.0f, look_.overlay);
}

void RasterView::paintOutline(Painter& painter, const editing::ImageDocument& image) const {
	using editing::Gesture;
	const editing::Press& press = image.press;
	const Gesture gesture = image.settings.gesture;
	VectorPath::Contour contour;
	if(press.down && !press.pan && (gesture == Gesture::selectRectangle || gesture == Gesture::selectEllipse)) {
		const Rect box =
		    between(toWindow(image, press.start.x, press.start.y), toWindow(image, press.last.x, press.last.y));
		contour.closed = true;
		if(gesture == Gesture::selectRectangle) {
			for(const Vec2 corner : {Vec2{box.left, box.top}, Vec2{box.right, box.top}, Vec2{box.right, box.bottom},
			                         Vec2{box.left, box.bottom}})
				addPoint(contour, corner);
		} else {
			const Vec2 centre{(box.left + box.right) / 2.0f, (box.top + box.bottom) / 2.0f};
			for(int at = 0; at < ellipsePoints; ++at) {
				const double angle = 2.0 * std::numbers::pi * at / ellipsePoints;
				addPoint(contour, {centre.x + static_cast<float>(std::cos(angle)) * box.width() / 2.0f,
				                   centre.y + static_cast<float>(std::sin(angle)) * box.height() / 2.0f});
			}
		}
	} else if(!image.path.empty() && (gesture == Gesture::lasso || gesture == Gesture::polygonLasso)) {
		for(const imaging::Point& point : image.path) addPoint(contour, toWindow(image, point.x, point.y));
		if(gesture == Gesture::polygonLasso && hovered_) addPoint(contour, pointer_); // the edge being placed
	}
	strokeOutline(painter, contour, look_.overlay);
}

void RasterView::paintBrush(Painter& painter, const editing::ImageDocument& image) const {
	const float diameter =
	    std::max(smallestBrush, static_cast<float>(image.settings.tip.size * image.view.zoom / scale()));
	const Rect circle{pointer_.x - diameter / 2.0f, pointer_.y - diameter / 2.0f, pointer_.x + diameter / 2.0f,
	                  pointer_.y + diameter / 2.0f};
	painter.strokeRoundedRect(circle, CornerRadii::all(diameter / 2.0f), 1.0f, underline);
	const Rect inner{circle.left + 1.0f, circle.top + 1.0f, circle.right - 1.0f, circle.bottom - 1.0f};
	if(inner.width() > 1.0f)
		painter.strokeRoundedRect(inner, CornerRadii::all(inner.width() / 2.0f), 1.0f, look_.overlay);
}

} // namespace affresco::format
