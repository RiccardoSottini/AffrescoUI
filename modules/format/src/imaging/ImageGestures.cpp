/// @file
/// @brief format::editing: the gestures on the canvas, each pointer event becoming a stroke, a selection, a fill, a
/// gradient, a pick or a change of view; and the two edits that move pixels about, moving a layer and cropping.

#include "ImageDocuments.hpp"

#include <affresco/imaging/Composite.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

namespace affresco::format::editing {

namespace {

using imaging::Point;

/// @brief How near the first corner a click closes a polygon, in surface pixels.
constexpr double closeDistance = 6.0;
/// @brief The smallest and largest zoom the gestures set.
constexpr double minimumZoom = 1.0 / 64.0, maximumZoom = 64.0;

/// @brief The image the paint gestures change: the selection in quick mask, otherwise the active layer's pixels.
/// @param image The image; its message says why when there is none.
/// @return The image's id, or std::nullopt.
[[nodiscard]] std::optional<std::uint64_t> paintTarget(ImageDocument& image) {
	const imaging::Document& document = image.canvas->document();
	if(image.settings.quickMask) return imaging::Document::selectionId;
	const imaging::Layer* layer = document.activeLayer();
	if(layer == nullptr || layer->kind != imaging::LayerKind::pixel) {
		image.message = "Choose a pixel layer";
		return std::nullopt;
	}
	if(const auto why = document.paintable(layer->pixels)) {
		image.message = *why;
		return std::nullopt;
	}
	return layer->pixels;
}

/// @brief Zooms by a factor, keeping a document point where it is on the screen.
/// @param image The image.
/// @param at The point.
/// @param factor The factor.
void zoomAt(ImageDocument& image, Point at, double factor) {
	imaging::CanvasView& view = image.view;
	const double zoom = std::clamp(view.zoom * factor, minimumZoom, maximumZoom);
	view.x = withinReach(at.x + (view.x - at.x) * view.zoom / zoom);
	view.y = withinReach(at.y + (view.y - at.y) * view.zoom / zoom);
	view.zoom = zoom;
}

/// @brief What a press of the clone stamp does before it paints: a right click sets the source, and with no source
/// set it only says how to set one.
/// @param image The image.
/// @param at Where.
/// @return Whether the press was handled here, so no stroke starts.
bool pressCloneSource(ImageDocument& image, Point at) {
	ToolSettings& settings = image.settings;
	if(image.press.right) {
		settings.cloneSource = at;
		image.cloneOffset.reset();
		image.message = "Clone source set";
		return true;
	}
	if(!settings.cloneSource) {
		image.message = "Right-click to set the clone source";
		return true;
	}
	return false;
}

/// @brief The step name of a stroke of the paint tools, as the History panel shows it.
/// @param settings The tool settings.
/// @return It.
[[nodiscard]] const char* strokeName(const ToolSettings& settings) noexcept {
	if(settings.quickMask) return "Quick Mask";
	if(settings.gesture == Gesture::erase) return "Eraser";
	return settings.gesture == Gesture::clone ? "Clone Stamp" : "Brush";
}

/// @brief Starts a brush, eraser or clone stroke.
/// @param image The image.
/// @param at Where.
/// @return Whether the screen should hear of it.
bool startStroke(ImageDocument& image, Point at) {
	const ToolSettings& settings = image.settings;
	if(settings.gesture == Gesture::clone && pressCloneSource(image, at)) return true;
	const auto target = paintTarget(image);
	if(!target) return true;
	const auto mode = settings.gesture == Gesture::erase  ? imaging::StrokeMode::erase
	                  : settings.gesture == Gesture::clone ? imaging::StrokeMode::clone
	                                                 : imaging::StrokeMode::paint;
	image.stroke = std::make_unique<imaging::Stroke>(image.canvas->document(), *target, settings.tip, mode,
	                                                   settings.foreground, strokeName(settings));
	if(mode == imaging::StrokeMode::clone && settings.cloneSource) {
		// The offset is kept between strokes, so that the source follows the brush as in other editors
		if(!image.cloneOffset)
			image.cloneOffset = Point{settings.cloneSource->x - at.x, settings.cloneSource->y - at.y};
		image.stroke->setCloneOffset(image.cloneOffset->x, image.cloneOffset->y);
	}
	image.stroke->add({at.x, at.y, image.pressure});
	return false;
}

/// @brief Adds a point to the lasso or polygon, or closes the polygon at its first corner.
/// @param image The image.
/// @param at Where.
/// @return Whether the screen should hear of it.
bool addCorner(ImageDocument& image, Point at) {
	std::vector<Point>& path = image.path;
	if(image.settings.gesture == Gesture::polygonLasso && path.size() >= 3) {
		const double distance = std::hypot(at.x - path.front().x, at.y - path.front().y) * image.view.zoom;
		if(distance <= closeDistance) {
			imaging::Document& document = image.canvas->document();
			imaging::select(document, "Polygonal Lasso",
			                imaging::polygonCoverage(document.width(), document.height(), path, image.limits),
			                image.settings.mode);
			path.clear();
			return true;
		}
	}
	if(image.settings.gesture == Gesture::lasso) path.clear();
	if(path.size() < image.limits.polygonPoints) path.push_back(at);
	return image.settings.gesture == Gesture::polygonLasso;
}

/// @brief Picks the foreground colour (the background's with the right button) from the flattened document.
/// @param image The image.
/// @param at Where.
/// @return Whether a colour was picked.
bool pickColour(ImageDocument& image, Point at) {
	const imaging::Document& document = image.canvas->document();
	const int x = static_cast<int>(std::floor(at.x)), y = static_cast<int>(std::floor(at.y));
	if(x < 0 || y < 0 || x >= document.width() || y >= document.height()) return false;
	imaging::Pixel colour = imaging::Sampler(document, 0).at(x, y);
	colour.alpha = 1.0f;
	(image.press.right ? image.settings.background : image.settings.foreground) = colour;
	return true;
}

/// @brief Selects with the magic wand from the pixel clicked.
/// @param document The document.
/// @param settings The tool settings: the sampling and the selection mode.
/// @param at Where, in document pixels.
/// @return Whether it was on the document, so the selection changed.
bool clickWand(imaging::Document& document, const ToolSettings& settings, Point at) {
	const int x = static_cast<int>(std::floor(at.x)), y = static_cast<int>(std::floor(at.y));
	if(x < 0 || y < 0 || x >= document.width() || y >= document.height()) return false;
	imaging::select(document, "Magic Wand", imaging::wandCoverage(document, x, y, settings.sample), settings.mode);
	return true;
}

/// @brief Fills with the paint bucket from the pixel clicked.
/// @param image The image; its message says why when nothing can be filled.
/// @param at Where, in document pixels.
void clickFill(ImageDocument& image, Point at) {
	const auto target = paintTarget(image);
	if(!target) return;
	const ToolSettings& settings = image.settings;
	imaging::fillAt(image.canvas->document(), *target, static_cast<int>(std::floor(at.x)),
	                static_cast<int>(std::floor(at.y)), settings.foreground, settings.sample, settings.opacity);
}

/// @brief What a tool does when a button goes down.
/// @param image The image.
/// @param at Where, in document pixels.
/// @return Whether the screen should hear of it.
bool pressTool(ImageDocument& image, Point at) {
	switch(image.settings.gesture) {
	case Gesture::paint:
	case Gesture::erase:
	case Gesture::clone:
		return startStroke(image, at);
	case Gesture::lasso:
	case Gesture::polygonLasso:
		return addCorner(image, at);
	case Gesture::magicWand:
		return clickWand(image.canvas->document(), image.settings, at);
	case Gesture::fill:
		clickFill(image, at);
		return true;
	case Gesture::sample:
		return pickColour(image, at);
	case Gesture::zoom:
		zoomAt(image, at, image.press.right ? 0.5 : 2.0);
		return true;
	default:
		return false;
	}
}

/// @brief What a tool does as the pointer moves with a button down.
/// @param image The image.
/// @param event The event.
/// @return Whether the screen should hear of it.
bool dragTool(ImageDocument& image, const imaging::CanvasPointer& event) {
	const Point at{static_cast<float>(event.x), static_cast<float>(event.y)};
	switch(image.settings.gesture) {
	case Gesture::paint:
	case Gesture::erase:
	case Gesture::clone:
		if(image.stroke) image.stroke->add({at.x, at.y, image.pressure});
		return false;
	case Gesture::lasso:
		if(image.path.size() < image.limits.polygonPoints) image.path.push_back(at);
		return false;
	case Gesture::pan: {
		const Press& press = image.press;
		image.view.x = withinReach(press.view.x - (event.pointer.position.x - press.surfaceX) / press.view.zoom);
		image.view.y = withinReach(press.view.y - (event.pointer.position.y - press.surfaceY) / press.view.zoom);
		return true;
	}
	case Gesture::sample:
		return pickColour(image, at);
	default:
		return false;
	}
}

/// @brief A marquee's selection, or none for a click without a drag.
/// @param document The document.
/// @param settings The tool settings: the marquee's shape and the selection mode.
/// @param from Where the button went down.
/// @param at Where it went up.
void selectShape(imaging::Document& document, const ToolSettings& settings, Point from, Point at) {
	if(std::abs(at.x - from.x) < 1.0f && std::abs(at.y - from.y) < 1.0f) {
		if(settings.mode == imaging::SelectionMode::replace) imaging::deselect(document);
		return;
	}
	if(settings.gesture == Gesture::selectRectangle)
		imaging::select(document, "Rectangular Marquee",
		                imaging::rectangleCoverage(document.width(), document.height(), from, at), settings.mode);
	else
		imaging::select(document, "Elliptical Marquee",
		                imaging::ellipseCoverage(document.width(), document.height(), from, at), settings.mode);
}

/// @brief Draws the gradient from the press to a point.
/// @param image The image.
/// @param at Where the button went up.
void drawGradient(ImageDocument& image, Point at) {
	const auto target = paintTarget(image);
	if(!target) return;
	const ToolSettings& settings = image.settings;
	imaging::Gradient gradient;
	if(settings.gradient) gradient = *settings.gradient;
	else gradient.stops = {{0.0f, settings.foreground}, {1.0f, settings.background}};
	imaging::drawGradient(image.canvas->document(), *target, gradient, settings.shape, image.press.start, at,
	                      settings.opacity);
}

/// @brief Selects inside the lasso's path, if it has an area, and clears the path.
/// @param image The image.
void closeLasso(ImageDocument& image) {
	imaging::Document& document = image.canvas->document();
	if(image.path.size() >= 3)
		imaging::select(document, "Lasso",
		                imaging::polygonCoverage(document.width(), document.height(), image.path, image.limits),
		                image.settings.mode);
	image.path.clear();
}

/// @brief Crops to the rectangle dragged from the press to a point.
/// @param image The image.
/// @param at Where the button went up.
void cropToDrag(ImageDocument& image, Point at) {
	const Point from = image.press.start;
	const float left = std::min(from.x, at.x), top = std::min(from.y, at.y);
	crop(image, static_cast<int>(std::lround(left)), static_cast<int>(std::lround(top)),
	     static_cast<int>(std::lround(std::max(from.x, at.x) - left)),
	     static_cast<int>(std::lround(std::max(from.y, at.y) - top)));
}

/// @brief What a tool does when the button goes up.
/// @param image The image.
/// @param at Where, in document pixels.
/// @return Whether the screen should hear of it.
bool releaseTool(ImageDocument& image, Point at) {
	const Point from = image.press.start;
	switch(image.settings.gesture) {
	case Gesture::paint:
	case Gesture::erase:
	case Gesture::clone:
		if(!image.stroke) return false;
		image.stroke->finish();
		image.stroke.reset();
		return true;
	case Gesture::selectRectangle:
	case Gesture::selectEllipse:
		selectShape(image.canvas->document(), image.settings, from, at);
		return true;
	case Gesture::lasso:
		closeLasso(image);
		return true;
	case Gesture::gradient:
		drawGradient(image, at);
		return true;
	case Gesture::move:
		moveActiveLayer(image.canvas->document(), static_cast<int>(std::lround(at.x - from.x)),
		                static_cast<int>(std::lround(at.y - from.y)));
		return true;
	case Gesture::crop:
		cropToDrag(image, at);
		return true;
	default:
		return false;
	}
}

/// @brief Where a tile lands in another image, and that image's size in tiles.
struct Placement {
	int left = 0;    ///< Where the tile's left edge lands, in the other image.
	int top = 0;     ///< Where its top edge lands.
	int columns = 0; ///< The other image's columns.
	int rows = 0;    ///< The other image's rows.
};

/// @brief A rectangle of an image, in pixels.
struct Area {
	int left = 0;   ///< Its left edge.
	int top = 0;    ///< Its top edge.
	int width = 0;  ///< Its width.
	int height = 0; ///< Its height.
};

/// @brief Copies a tile's pixels, placed at an offset, onto the tiles of another image they land on.
/// @param tile The source tile.
/// @param placement Where it lands, and the other image's size in tiles.
/// @param destinationOf The other image's tile to write at a column and row (of the same format).
void copyPlaced(const imaging::Tile& tile, const Placement& placement,
                const std::function<imaging::Tile&(int, int)>& destinationOf) {
	constexpr int side = imaging::tileSize;
	const auto pixelBytes = imaging::bytesPerPixel(tile.format());
	const int left = placement.left, top = placement.top;
	const int lastColumn = std::min(placement.columns - 1, (left + side - 1) / side);
	const int lastRow = std::min(placement.rows - 1, (top + side - 1) / side);
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
/// @param part The part, inside the image.
/// @param limits The limits.
[[nodiscard]] std::shared_ptr<imaging::Image> cropImage(const imaging::Image& image, const Area& part,
                                                        const imaging::Limits& limits) {
	auto copy = std::make_shared<imaging::Image>(part.width, part.height, image.format(), limits);
	const bool mask = image.format() == imaging::PixelFormat::mask8;
	std::map<std::pair<int, int>, bool> started;
	const auto destinationOf = [&](int column, int row) -> imaging::Tile& {
		if(mask && !started[{column, row}])
			copy->setTile(column, row, imaging::Tile::filled(imaging::PixelFormat::mask8, {1.0f, 1.0f, 1.0f, 1.0f}));
		started[{column, row}] = true;
		return copy->writableTile(column, row);
	};
	for(const auto& [place, tile] : tilesOf(image))
		copyPlaced(*tile,
		           {place.first * imaging::tileSize - part.left, place.second * imaging::tileSize - part.top,
		            copy->columns(), copy->rows()},
		           destinationOf);
	return copy;
}

} // namespace

double withinReach(double coordinate) noexcept {
	constexpr double farthest = 1e9;
	return std::clamp(coordinate, -farthest, farthest);
}

bool applyPointer(ImageDocument& image, const imaging::CanvasPointer& event) {
	const Point at{static_cast<float>(event.x), static_cast<float>(event.y)};
	Press& press = image.press;
	try {
		switch(event.pointer.kind) {
		case affresco::PointerKind::scroll:
			zoomAt(image, at, std::pow(1.1, static_cast<double>(event.pointer.scroll.y)));
			return true;
		case affresco::PointerKind::press:
			press = {true,
			         event.pointer.button == affresco::MouseButton::Right,
			         at,
			         at,
			         static_cast<double>(event.pointer.position.x),
			         static_cast<double>(event.pointer.position.y),
			         image.view};
			image.message.clear();
			return pressTool(image, at);
		case affresco::PointerKind::move:
			if(!press.down) return false;
			press.last = at;
			return dragTool(image, event);
		case affresco::PointerKind::release:
			if(!press.down) return false;
			press.down = false;
			return releaseTool(image, at);
		}
	} catch(const DocumentError& error) {
		image.stroke.reset();
		image.path.clear();
		image.message = error.what();
		return true;
	}
	return false;
}

void moveActiveLayer(imaging::Document& document, int dx, int dy) {
	if(dx == 0 && dy == 0) return;
	const imaging::Layer* layer = document.activeLayer();
	if(layer == nullptr || layer->kind != imaging::LayerKind::pixel)
		throw DocumentError("Choose a pixel layer to move");
	if(layer->locks.position || layer->locks.all) throw DocumentError("The layer's position is locked");
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
			copyPlaced(*tile,
			           {place.first * imaging::tileSize + dx, place.second * imaging::tileSize + dy, columns, rows},
			           destinationOf);
	} catch(...) {
		document.cancel();
		throw;
	}
	document.commit();
}

void crop(ImageDocument& image, int x, int y, int width, int height) {
	const imaging::Document& old = image.canvas->document();
	const int left = std::clamp(x, 0, old.width()), top = std::clamp(y, 0, old.height());
	width = std::min(width, old.width() - left);
	height = std::min(height, old.height() - top);
	if(width < 1 || height < 1) throw DocumentError("The crop is empty");
	auto cropped = std::make_unique<imaging::Document>(width, height, image.limits, old.format());
	imaging::Structure structure = old.structure();
	structure.selection = false;
	for(auto& [id, layerImage] : structure.images)
		layerImage = cropImage(*layerImage, {left, top, width, height}, image.limits);
	cropped->changeStructure("Crop", [&](imaging::Structure& replaced) { replaced = structure; });
	cropped->clearHistory();
	replaceDocument(image, std::move(cropped));
}

} // namespace affresco::format::editing
