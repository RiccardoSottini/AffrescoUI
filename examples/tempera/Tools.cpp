/// @file
/// @brief Tempera's tools on the canvas: each pointer event becomes a stroke, a selection, a fill, a gradient, a pick
/// or a change of view; and the two edits that move pixels about, moving a layer and cropping.

#include "tempera/Tempera.hpp"

#include <affresco/imaging/Composite.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

namespace tempera {

namespace {

using imaging::Point;

/// @brief How near the first corner a click closes a polygon, in surface pixels.
constexpr double closeDistance = 6.0;
/// @brief The smallest and largest zoom the tools set.
constexpr double minimumZoom = 1.0 / 64.0, maximumZoom = 64.0;

/// @brief The image the paint tools change: the selection in quick mask, otherwise the active layer's pixels.
/// @param session The session; its message says why when there is none.
/// @return The image's id, or std::nullopt.
[[nodiscard]] std::optional<std::uint64_t> paintTarget(Session& session) {
	const imaging::Document& document = session.canvas->document();
	if(session.settings.quickMask) return imaging::Document::selectionId;
	const imaging::Layer* layer = document.activeLayer();
	if(layer == nullptr || layer->kind != imaging::LayerKind::pixel) {
		session.message = "Choose a pixel layer";
		return std::nullopt;
	}
	if(const auto why = document.paintable(layer->pixels)) {
		session.message = *why;
		return std::nullopt;
	}
	return layer->pixels;
}

/// @brief Zooms by a factor, keeping a document point where it is on the screen.
/// @param session The session.
/// @param at The point.
/// @param factor The factor.
void zoomAt(Session& session, Point at, double factor) {
	imaging::CanvasView& view = session.view;
	const double zoom = std::clamp(view.zoom * factor, minimumZoom, maximumZoom);
	view.x = withinReach(at.x + (view.x - at.x) * view.zoom / zoom);
	view.y = withinReach(at.y + (view.y - at.y) * view.zoom / zoom);
	view.zoom = zoom;
}

/// @brief Starts a brush, eraser or clone stroke.
/// @param session The session.
/// @param at Where.
/// @return Whether the screen should hear of it.
bool startStroke(Session& session, Point at) {
	ToolSettings& settings = session.settings;
	if(settings.tool == Tool::clone && session.press.right) {
		settings.cloneSource = at;
		session.cloneOffset.reset();
		session.message = "Clone source set";
		return true;
	}
	if(settings.tool == Tool::clone && !settings.cloneSource) {
		session.message = "Right-click to set the clone source";
		return true;
	}
	const auto target = paintTarget(session);
	if(!target) return true;
	const auto mode = settings.tool == Tool::eraser  ? imaging::StrokeMode::erase
	                  : settings.tool == Tool::clone ? imaging::StrokeMode::clone
	                                                 : imaging::StrokeMode::paint;
	const char* name = settings.quickMask              ? "Quick Mask"
	                   : settings.tool == Tool::eraser ? "Eraser"
	                   : settings.tool == Tool::clone  ? "Clone Stamp"
	                                                   : "Brush";
	session.stroke = std::make_unique<imaging::Stroke>(session.canvas->document(), *target, settings.tip, mode,
	                                                   settings.foreground, name);
	if(mode == imaging::StrokeMode::clone) {
		if(!session.cloneOffset)
			session.cloneOffset = Point{settings.cloneSource->x - at.x, settings.cloneSource->y - at.y};
		session.stroke->setCloneOffset(session.cloneOffset->x, session.cloneOffset->y);
	}
	session.stroke->add({at.x, at.y, session.pressure});
	return false;
}

/// @brief Adds a point to the lasso or polygon, or closes the polygon at its first corner.
/// @param session The session.
/// @param at Where.
/// @return Whether the screen should hear of it.
bool addCorner(Session& session, Point at) {
	std::vector<Point>& path = session.path;
	if(session.settings.tool == Tool::polygon && path.size() >= 3) {
		const double distance = std::hypot(at.x - path.front().x, at.y - path.front().y) * session.view.zoom;
		if(distance <= closeDistance) {
			imaging::Document& document = session.canvas->document();
			imaging::select(document, "Polygonal Lasso",
			                imaging::polygonCoverage(document.width(), document.height(), path, session.limits),
			                session.settings.mode);
			path.clear();
			return true;
		}
	}
	if(session.settings.tool == Tool::lasso) path.clear();
	if(path.size() < session.limits.polygonPoints) path.push_back(at);
	return session.settings.tool == Tool::polygon;
}

/// @brief Picks the foreground colour (the background's with the right button) from the flattened document.
/// @param session The session.
/// @param at Where.
/// @return Whether a colour was picked.
bool pickColour(Session& session, Point at) {
	const imaging::Document& document = session.canvas->document();
	const int x = static_cast<int>(std::floor(at.x)), y = static_cast<int>(std::floor(at.y));
	if(x < 0 || y < 0 || x >= document.width() || y >= document.height()) return false;
	imaging::Pixel colour = imaging::Sampler(document, 0).at(x, y);
	colour.alpha = 1.0f;
	(session.press.right ? session.settings.background : session.settings.foreground) = colour;
	return true;
}

/// @brief What a tool does when a button goes down.
/// @param session The session.
/// @param at Where, in document pixels.
/// @return Whether the screen should hear of it.
bool pressTool(Session& session, Point at) {
	imaging::Document& document = session.canvas->document();
	const ToolSettings& settings = session.settings;
	switch(settings.tool) {
	case Tool::brush:
	case Tool::eraser:
	case Tool::clone:
		return startStroke(session, at);
	case Tool::lasso:
	case Tool::polygon:
		return addCorner(session, at);
	case Tool::wand: {
		const int x = static_cast<int>(std::floor(at.x)), y = static_cast<int>(std::floor(at.y));
		if(x < 0 || y < 0 || x >= document.width() || y >= document.height()) return false;
		imaging::select(document, "Magic Wand", imaging::wandCoverage(document, x, y, settings.sample), settings.mode);
		return true;
	}
	case Tool::fill: {
		const auto target = paintTarget(session);
		if(target)
			imaging::fillAt(document, *target, static_cast<int>(std::floor(at.x)), static_cast<int>(std::floor(at.y)),
			                settings.foreground, settings.sample, settings.opacity);
		return true;
	}
	case Tool::eyedropper:
		return pickColour(session, at);
	case Tool::zoom:
		zoomAt(session, at, session.press.right ? 0.5 : 2.0);
		return true;
	default:
		return false;
	}
}

/// @brief What a tool does as the pointer moves with a button down.
/// @param session The session.
/// @param event The event.
/// @return Whether the screen should hear of it.
bool dragTool(Session& session, const imaging::CanvasPointer& event) {
	const Point at{static_cast<float>(event.x), static_cast<float>(event.y)};
	switch(session.settings.tool) {
	case Tool::brush:
	case Tool::eraser:
	case Tool::clone:
		if(session.stroke) session.stroke->add({at.x, at.y, session.pressure});
		return false;
	case Tool::lasso:
		if(session.path.size() < session.limits.polygonPoints) session.path.push_back(at);
		return false;
	case Tool::hand: {
		const Press& press = session.press;
		session.view.x = withinReach(press.view.x - (event.pointer.position.x - press.surfaceX) / press.view.zoom);
		session.view.y = withinReach(press.view.y - (event.pointer.position.y - press.surfaceY) / press.view.zoom);
		return true;
	}
	case Tool::eyedropper:
		return pickColour(session, at);
	default:
		return false;
	}
}

/// @brief A marquee's selection, or none for a click without a drag.
/// @param session The session.
/// @param at Where the button went up.
void selectShape(Session& session, Point at) {
	imaging::Document& document = session.canvas->document();
	const Point from = session.press.start;
	const ToolSettings& settings = session.settings;
	if(std::abs(at.x - from.x) < 1.0f && std::abs(at.y - from.y) < 1.0f) {
		if(settings.mode == imaging::SelectionMode::replace) imaging::deselect(document);
		return;
	}
	if(settings.tool == Tool::marquee)
		imaging::select(document, "Rectangular Marquee",
		                imaging::rectangleCoverage(document.width(), document.height(), from, at), settings.mode);
	else
		imaging::select(document, "Elliptical Marquee",
		                imaging::ellipseCoverage(document.width(), document.height(), from, at), settings.mode);
}

/// @brief Draws the gradient from the press to a point.
/// @param session The session.
/// @param at Where the button went up.
void drawGradient(Session& session, Point at) {
	const auto target = paintTarget(session);
	if(!target) return;
	const ToolSettings& settings = session.settings;
	imaging::Gradient gradient;
	if(settings.gradient) gradient = *settings.gradient;
	else gradient.stops = {{0.0f, settings.foreground}, {1.0f, settings.background}};
	imaging::drawGradient(session.canvas->document(), *target, gradient, settings.shape, session.press.start, at,
	                      settings.opacity);
}

/// @brief What a tool does when the button goes up.
/// @param session The session.
/// @param at Where, in document pixels.
/// @return Whether the screen should hear of it.
bool releaseTool(Session& session, Point at) {
	const Point from = session.press.start;
	switch(session.settings.tool) {
	case Tool::brush:
	case Tool::eraser:
	case Tool::clone:
		if(!session.stroke) return false;
		session.stroke->finish();
		session.stroke.reset();
		return true;
	case Tool::marquee:
	case Tool::ellipse:
		selectShape(session, at);
		return true;
	case Tool::lasso: {
		imaging::Document& document = session.canvas->document();
		if(session.path.size() >= 3)
			imaging::select(document, "Lasso",
			                imaging::polygonCoverage(document.width(), document.height(), session.path, session.limits),
			                session.settings.mode);
		session.path.clear();
		return true;
	}
	case Tool::gradient:
		drawGradient(session, at);
		return true;
	case Tool::move:
		moveActiveLayer(session.canvas->document(), static_cast<int>(std::lround(at.x - from.x)),
		                static_cast<int>(std::lround(at.y - from.y)));
		return true;
	case Tool::crop: {
		const float left = std::min(from.x, at.x), top = std::min(from.y, at.y);
		crop(session, static_cast<int>(std::lround(left)), static_cast<int>(std::lround(top)),
		     static_cast<int>(std::lround(std::max(from.x, at.x) - left)),
		     static_cast<int>(std::lround(std::max(from.y, at.y) - top)));
		return true;
	}
	default:
		return false;
	}
}

/// @brief Copies a tile's pixels, placed at an offset, onto the tiles of another image they land on.
/// @param tile The source tile.
/// @param left Where its left edge lands, in the other image.
/// @param top Where its top edge lands.
/// @param columns The other image's columns.
/// @param rows The other image's rows.
/// @param destinationOf The other image's tile to write at a column and row (of the same format).
void copyPlaced(const imaging::Tile& tile, int left, int top, int columns, int rows,
                const std::function<imaging::Tile&(int, int)>& destinationOf) {
	constexpr int side = imaging::tileSize;
	const auto pixelBytes = imaging::bytesPerPixel(tile.format());
	const int lastColumn = std::min(columns - 1, (left + side - 1) / side);
	const int lastRow = std::min(rows - 1, (top + side - 1) / side);
	for(int row = std::max(0, top / side); row <= lastRow; ++row)
		for(int column = std::max(0, left / side); column <= lastColumn; ++column) {
			const int originX = column * side, originY = row * side;
			const int fromX = std::max(left, originX), toX = std::min(left + side, originX + side);
			const int fromY = std::max(top, originY), toY = std::min(top + side, originY + side);
			if(fromX >= toX || fromY >= toY) continue;
			const auto source = tile.bytes();
			const auto destination = destinationOf(column, row).bytes();
			const auto length = static_cast<std::size_t>(toX - fromX) * pixelBytes;
			for(int y = fromY; y < toY; ++y) {
				const auto from = static_cast<std::size_t>((y - top) * side + (fromX - left)) * pixelBytes;
				const auto to = static_cast<std::size_t>((y - originY) * side + (fromX - originX)) * pixelBytes;
				std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(from), length,
				            destination.begin() + static_cast<std::ptrdiff_t>(to));
			}
		}
}

/// @brief An image's tiles, the transparent ones (or a mask's white ones) left out.
/// @param image The image.
[[nodiscard]] std::map<std::pair<int, int>, std::shared_ptr<const imaging::Tile>> tilesOf(const imaging::Image& image) {
	std::map<std::pair<int, int>, std::shared_ptr<const imaging::Tile>> tiles;
	for(int row = 0; row < image.rows(); ++row)
		for(int column = 0; column < image.columns(); ++column)
			if(auto tile = image.tile({0, column, row})) tiles.emplace(std::pair{column, row}, std::move(tile));
	return tiles;
}

/// @brief A part of an image as a new image; a mask's absent tiles stay absent (white).
/// @param image The image.
/// @param left The part's left.
/// @param top Its top.
/// @param width Its width.
/// @param height Its height.
/// @param limits The limits.
[[nodiscard]] std::shared_ptr<imaging::Image> cropImage(const imaging::Image& image, int left, int top, int width,
                                                        int height, const imaging::Limits& limits) {
	auto copy = std::make_shared<imaging::Image>(width, height, image.format(), limits);
	const bool mask = image.format() == imaging::PixelFormat::mask8;
	std::map<std::pair<int, int>, bool> started;
	const auto destinationOf = [&](int column, int row) -> imaging::Tile& {
		if(mask && !started[{column, row}])
			copy->setTile(column, row, imaging::Tile::filled(imaging::PixelFormat::mask8, {1.0f, 1.0f, 1.0f, 1.0f}));
		started[{column, row}] = true;
		return copy->writableTile(column, row);
	};
	for(const auto& [place, tile] : tilesOf(image))
		copyPlaced(*tile, place.first * imaging::tileSize - left, place.second * imaging::tileSize - top,
		           copy->columns(), copy->rows(), destinationOf);
	return copy;
}

} // namespace

double withinReach(double coordinate) noexcept {
	constexpr double farthest = 1e9;
	return std::clamp(coordinate, -farthest, farthest);
}

bool applyPointer(Session& session, const imaging::CanvasPointer& event) {
	const Point at{static_cast<float>(event.x), static_cast<float>(event.y)};
	Press& press = session.press;
	try {
		switch(event.pointer.kind) {
		case affresco::PointerKind::scroll:
			zoomAt(session, at, std::pow(1.1, static_cast<double>(event.pointer.scroll.y)));
			return true;
		case affresco::PointerKind::press:
			press = {true,
			         event.pointer.button == affresco::MouseButton::Right,
			         at,
			         at,
			         static_cast<double>(event.pointer.position.x),
			         static_cast<double>(event.pointer.position.y),
			         session.view};
			session.message.clear();
			return pressTool(session, at);
		case affresco::PointerKind::move:
			if(!press.down) return false;
			press.last = at;
			return dragTool(session, event);
		case affresco::PointerKind::release:
			if(!press.down) return false;
			press.down = false;
			return releaseTool(session, at);
		}
	} catch(const affresco::DocumentError& error) {
		session.stroke.reset();
		session.path.clear();
		session.message = error.what();
		return true;
	}
	return false;
}

void moveActiveLayer(imaging::Document& document, int dx, int dy) {
	if(dx == 0 && dy == 0) return;
	const imaging::Layer* layer = document.activeLayer();
	if(layer == nullptr || layer->kind != imaging::LayerKind::pixel)
		throw affresco::DocumentError("Choose a pixel layer to move");
	if(layer->locks.position || layer->locks.all) throw affresco::DocumentError("The layer's position is locked");
	const std::uint64_t target = layer->pixels;
	const imaging::Image& image = *document.image(target);
	const auto before = tilesOf(image);
	const int columns = image.columns(), rows = image.rows();
	const auto destinationOf = [&](int column, int row) -> imaging::Tile& {
		return document.editTile(target, column, row);
	};
	document.begin("Move");
	try {
		for(const auto& [place, tile] : before) document.replaceTile(target, place.first, place.second, nullptr);
		for(const auto& [place, tile] : before)
			copyPlaced(*tile, place.first * imaging::tileSize + dx, place.second * imaging::tileSize + dy, columns,
			           rows, destinationOf);
	} catch(...) {
		document.cancel();
		throw;
	}
	document.commit();
}

void crop(Session& session, int x, int y, int width, int height) {
	const imaging::Document& old = session.canvas->document();
	const int left = std::clamp(x, 0, old.width()), top = std::clamp(y, 0, old.height());
	width = std::min(width, old.width() - left);
	height = std::min(height, old.height() - top);
	if(width < 1 || height < 1) throw affresco::DocumentError("The crop is empty");
	auto cropped = std::make_unique<imaging::Document>(width, height, session.limits, old.format());
	imaging::Structure structure = old.structure();
	structure.selection = false;
	for(auto& [id, image] : structure.images) image = cropImage(*image, left, top, width, height, session.limits);
	cropped->changeStructure("Crop", [&](imaging::Structure& replaced) { replaced = structure; });
	cropped->clearHistory();
	replaceDocument(session, std::move(cropped));
}

} // namespace tempera
