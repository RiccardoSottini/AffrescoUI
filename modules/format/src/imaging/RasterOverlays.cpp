/// @file
/// @brief RasterView's overlays: what the tools are doing, drawn over the image (the crop being dragged, the marquee
/// or the lasso being drawn, the brush's outline). The selection's ants and the quick mask are the canvas surface's.

#include "RasterView.hpp"

#include "affresco/render/Painter.hpp"
#include "affresco/render/VectorPath.hpp"
#include "affresco/imaging/Shapes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>
#include <utility>
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
	StrokeStyle style; // one unit wide, solid
	appendStroke(contours, style, mesh);
	painter.fillMesh(mesh, {}, underline);
	mesh.clear();
	style.dash = {dashLength, dashLength};
	appendStroke(contours, style, mesh);
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

/// @brief A marquee's closed outline: a rectangle's corners, or points around the ellipse inside it.
/// @param box The marquee, in window units.
/// @param ellipse Whether it is elliptical.
/// @return The outline.
[[nodiscard]] VectorPath::Contour marqueeContour(const Rect& box, bool ellipse) {
	VectorPath::Contour contour;
	contour.closed = true;
	if(!ellipse) {
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
	return contour;
}

/// @brief Strokes a path's line once, solid, 1.5 units wide.
/// @param painter The painter.
/// @param contour The line, in window units (nothing is drawn under two points).
/// @param color Its colour.
void strokePathLine(Painter& painter, const VectorPath::Contour& contour, Color color) {
	if(contour.points.size() < 2) return;
	const std::span<const VectorPath::Contour> contours(&contour, 1);
	std::vector<MeshVertex> mesh;
	StrokeStyle style;
	style.width = 1.5f;
	appendStroke(contours, style, mesh);
	painter.fillMesh(mesh, {}, color);
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
	paintPaths(painter, *shown);
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
	    between(toWindow(image, press.boxFrom.x, press.boxFrom.y), toWindow(image, press.boxTo.x, press.boxTo.y));
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
	const bool ellipse = gesture == Gesture::selectEllipse ||
	                     (gesture == Gesture::shape && image.settings.vector.shape == imaging::ShapeKind::ellipse);
	if(press.down && !press.pan &&
	   (gesture == Gesture::selectRectangle || gesture == Gesture::selectEllipse || gesture == Gesture::shape)) {
		const Rect box =
		    between(toWindow(image, press.boxFrom.x, press.boxFrom.y), toWindow(image, press.boxTo.x, press.boxTo.y));
		contour = marqueeContour(box, ellipse);
	} else if(!image.path.empty() && (gesture == Gesture::lasso || gesture == Gesture::polygonLasso)) {
		for(const imaging::Point& point : image.path) addPoint(contour, toWindow(image, point.x, point.y));
		if(gesture == Gesture::polygonLasso && hovered_) addPoint(contour, pointer_); // the edge being placed
	}
	strokeOutline(painter, contour, look_.overlay);
}

void RasterView::paintContentBox(Painter& painter, const editing::ImageDocument& image) const {
	using editing::Gesture;
	const Gesture gesture = image.settings.gesture;
	const bool vectorTool = gesture == Gesture::text || gesture == Gesture::shape || gesture == Gesture::pen ||
	                        gesture == Gesture::pathSelect || gesture == Gesture::move;
	const imaging::Layer* layer = image.canvas->document().activeLayer();
	if(!vectorTool || layer == nullptr || layer->content.kind == imaging::ContentKind::none || image.press.down) return;
	const auto box = editing::contentBounds(image, layer->content);
	VectorPath::Contour contour;
	contour.closed = true;
	for(const auto& [x, y] : {std::pair{box[0], box[1]}, {box[2], box[1]}, {box[2], box[3]}, {box[0], box[3]}})
		addPoint(contour, toWindow(image, x, y));
	strokeOutline(painter, contour, look_.overlay);
}

void RasterView::paintPaths(Painter& painter, const editing::ImageDocument& image) const {
	paintContentBox(painter, image);
	const imaging::Structure& structure = image.canvas->document().structure();
	if(structure.activePath < 0 || static_cast<std::size_t>(structure.activePath) >= structure.paths.size()) return;
	const imaging::VectorPath& path = structure.paths[static_cast<std::size_t>(structure.activePath)];
	VectorPath::Contour contour;
	contour.closed = path.closed;
	for(const imaging::Point& point : imaging::flattenPath(path.points, path.closed))
		addPoint(contour, toWindow(image, point.x, point.y));
	if(!path.closed && !path.points.empty() && hovered_ && image.settings.gesture == editing::Gesture::pen &&
	   !image.press.down)
		addPoint(contour, pointer_); // the segment the next anchor would add
	strokePathLine(painter, contour, look_.overlay);
	paintAnchors(painter, image, path);
}

void RasterView::paintAnchors(Painter& painter, const editing::ImageDocument& image,
                              const imaging::VectorPath& path) const {
	constexpr float anchorSide = 7.0f;
	for(std::size_t index = 0; index < path.points.size(); ++index) {
		const imaging::PathPoint& point = path.points[index];
		const Vec2 at = toWindow(image, point.anchor.x, point.anchor.y);
		if(index + 1 == path.points.size() || std::cmp_equal(index, image.pathAnchor))
			for(const imaging::VectorPoint handle : {point.in, point.out}) {
				if(handle == point.anchor) continue;
				const Vec2 end = toWindow(image, handle.x, handle.y);
				VectorPath::Contour arm;
				addPoint(arm, at);
				addPoint(arm, end);
				strokeOutline(painter, arm, look_.overlay);
				painter.fillRoundedRect({end.x - 3.0f, end.y - 3.0f, end.x + 3.0f, end.y + 3.0f}, CornerRadii::all(3.0f),
				                        look_.overlay);
			}
		const Rect square{std::round(at.x - anchorSide / 2.0f), std::round(at.y - anchorSide / 2.0f),
		                  std::round(at.x + anchorSide / 2.0f), std::round(at.y + anchorSide / 2.0f)};
		painter.fillRect(square, index + 1 == path.points.size() ? look_.overlay : Color{1, 1, 1, 1});
		painter.strokeRoundedRect(square, {}, 1.0f, underline);
	}
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
