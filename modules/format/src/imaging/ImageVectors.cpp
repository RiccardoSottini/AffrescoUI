/// @file
/// @brief format::editing: type and shape layers (drawn from their content whenever it changes), the document's paths,
/// and the vector gestures (text, shape, pen, path-select) that make and change them.

#include "ImageOperations.hpp"

#include "affresco/imaging/Shapes.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <cmath>
#include <string>

namespace affresco::format::editing {

namespace {

namespace im = affresco::imaging;
using imaging::Point;

/// @brief The text sizes type is drawn from: the smallest one at least as large as the text, so a document loads a
/// few fonts whatever sizes its type takes.
constexpr std::array<float, 7> rasterSizes{16.0f, 32.0f, 64.0f, 96.0f, 128.0f, 192.0f, 256.0f};
/// @brief How near an anchor a press grabs it, in screen pixels.
constexpr double grabDistance = 8.0;

/// @brief The size a text of a size is drawn from.
/// @param size The text's size.
[[nodiscard]] float rasterSizeOf(float size) noexcept {
	for(const float raster : rasterSizes)
		if(raster >= size) return raster;
	return rasterSizes.back();
}

/// @brief The active layer if it holds text or a shape, else a DocumentError.
/// @param image The image.
/// @param with The arguments ("id" names another layer).
[[nodiscard]] const im::Layer& contentLayer(const ImageDocument& image, const json& with) {
	const im::Layer* layer = image.canvas->document().layer(layerOf(image, with));
	if(layer == nullptr || layer->content.kind == im::ContentKind::none)
		throw DocumentError("Choose a type or shape layer");
	return *layer;
}

/// @brief A short name for a type layer: its first line, at most 32 bytes (not cutting a character).
/// @param text The text.
[[nodiscard]] std::string typeLayerName(const std::string& text) {
	std::string name = text.substr(0, text.find('\n'));
	if(name.size() > 32) {
		std::size_t cut = 32;
		while(cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0U) == 0x80U) --cut;
		name.resize(cut);
	}
	return name.empty() ? std::string("Text") : name;
}

/// @brief The name of a new shape layer.
/// @param content Its content.
[[nodiscard]] std::string shapeLayerName(const im::LayerContent& content) {
	std::string name(im::shapeKindNames()[static_cast<std::size_t>(content.shape)]);
	name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
	return name + " 1";
}

/// @brief Makes a type layer, active, drawn from a content.
/// @param image The image.
/// @param content The content (kind text).
void makeTypeLayer(ImageDocument& image, im::LayerContent content) {
	content.kind = im::ContentKind::text;
	if(content.text.empty()) content.text = "Text";
	image.canvas->document().addContentLayer(typeLayerName(content.text), content, drawContent(image, content));
}

/// @brief Makes a shape layer, active, drawn from a content.
/// @param image The image.
/// @param content The content (kind shape).
void makeShapeLayer(ImageDocument& image, im::LayerContent content) {
	content.kind = im::ContentKind::shape;
	image.canvas->document().addContentLayer(shapeLayerName(content), content, drawContent(image, content));
}

/// @brief The bounds of a layer's content: {left, top, right, bottom} (text laid out with its font).
/// @param image The image.
/// @param content The content.
[[nodiscard]] std::array<float, 4> boundsOf(const ImageDocument& image, const im::LayerContent& content) {
	if(content.kind == im::ContentKind::text) {
		const auto fonts = image.fonts.lock();
		if(fonts && *fonts)
			if(const auto font = (*fonts)(content.font, rasterSizeOf(content.size), {content.bold, content.italic}))
				return im::textBounds(content, *font);
		return {content.origin.x, content.origin.y, content.origin.x + content.size, content.origin.y + content.size};
	}
	std::vector<Point> outline = im::shapeOutline(content);
	outline.push_back({content.origin.x, content.origin.y});
	if(content.shape == im::ShapeKind::line) outline.push_back({content.corner.x, content.corner.y});
	std::array<float, 4> box{outline[0].x, outline[0].y, outline[0].x, outline[0].y};
	for(const Point point : outline)
		box = {std::min(box[0], point.x), std::min(box[1], point.y), std::max(box[2], point.x),
		       std::max(box[3], point.y)};
	return box;
}

/// @brief The topmost visible type layer whose text is under a point.
/// @param image The image.
/// @param at The point.
/// @return Its id, or 0.
[[nodiscard]] std::uint64_t typeLayerAt(const ImageDocument& image, Point at) {
	const im::Document& document = image.canvas->document();
	for(const auto& [id, depth] : document.listed()) {
		const im::Layer& layer = *document.layer(id);
		if(!layer.visible || layer.content.kind != im::ContentKind::text) continue;
		const auto box = boundsOf(image, layer.content);
		if(at.x >= box[0] && at.x <= box[2] && at.y >= box[1] && at.y <= box[3]) return id;
	}
	return 0;
}

/// @brief Changes the document's paths as one step.
/// @param image The image.
/// @param name The step's name.
/// @param change The change.
/// @param coalesce Whether it joins a step of the same name just before.
void changePaths(ImageDocument& image, const std::string& name, const std::function<void(im::Structure&)>& change,
                 bool coalesce = false) {
	settle(image);
	image.canvas->document().changeStructure(name, change, coalesce);
}

/// @brief The active path of a structure, or a DocumentError.
/// @param structure The structure.
[[nodiscard]] im::VectorPath& activePath(im::Structure& structure) {
	if(structure.activePath < 0 || static_cast<std::size_t>(structure.activePath) >= structure.paths.size())
		throw DocumentError("Choose a path");
	return structure.paths[static_cast<std::size_t>(structure.activePath)];
}

/// @brief The active path of a document, or a DocumentError.
/// @param document The document.
[[nodiscard]] const im::VectorPath& activePath(const im::Document& document) {
	const im::Structure& structure = document.structure();
	if(structure.activePath < 0 || static_cast<std::size_t>(structure.activePath) >= structure.paths.size())
		throw DocumentError("Choose a path");
	return structure.paths[static_cast<std::size_t>(structure.activePath)];
}

/// @brief A new path named after the paths there ("Path 1", "Path 2"...), made the active one.
/// @param structure The structure.
/// @param name Its name, or "" for the next free one.
void addPath(im::Structure& structure, std::string name) {
	if(structure.paths.size() >= im::maxPaths)
		throw DocumentError("A document has at most " + std::to_string(im::maxPaths) + " paths");
	if(name.empty()) name = "Path " + std::to_string(structure.paths.size() + 1);
	structure.paths.push_back({std::move(name), {}, false});
	structure.activePath = static_cast<int>(structure.paths.size()) - 1;
}

/// @brief The index argument of a path ("index"), or the active one.
/// @param image The image.
/// @param with The arguments.
[[nodiscard]] int pathIndexOf(const ImageDocument& image, const json& with) {
	const im::Structure& structure = image.canvas->document().structure();
	const int index = wholeOf(with, "index", structure.activePath);
	if(index < 0 || static_cast<std::size_t>(index) >= structure.paths.size()) throw DocumentError("No such path");
	return index;
}

/// @brief addText: adds a type layer.
/// @param image The image.
/// @param with text, x, y, size, font, bold, italic, align, leading, fill (the foreground's when absent).
void addText(ImageDocument& image, const json& with, Window*) {
	settle(image);
	im::LayerContent content = image.settings.vector;
	content.fill = image.settings.foreground;
	im::readContent(with, content);
	makeTypeLayer(image, content);
}

/// @brief addShape: adds a shape layer.
/// @param image The image.
/// @param with shape, x, y, x2, y2, radius, sides, points, closed, fill (the foreground's when absent), stroke,
/// strokeWidth.
void addShape(ImageDocument& image, const json& with, Window*) {
	settle(image);
	im::LayerContent content = image.settings.vector;
	content.fill = image.settings.foreground;
	im::readContent(with, content);
	makeShapeLayer(image, content);
}

/// @brief editContent: changes a type or shape layer's content (text, font, size, colours, box...) and draws it again;
/// typing and sliders coalesce into one step.
/// @param image The image.
/// @param with id, and the content's keys.
void editContent(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const im::Layer& layer = contentLayer(image, with);
	im::LayerContent content = layer.content;
	json changes = with;
	changes.erase("id");
	changes.erase("kind");
	im::readContent(changes, content);
	const std::uint64_t id = layer.id;
	// The layer is read before the change: the change replaces the structure it lives in
	const bool renaming = content.kind == im::ContentKind::text && layer.name == typeLayerName(layer.content.text) &&
	                      layer.name != typeLayerName(content.text);
	im::Document& document = image.canvas->document();
	document.setContent(id, content.kind == im::ContentKind::text ? "Edit Type" : "Edit Shape", content,
	                    drawContent(image, content), true);
	if(renaming) {
		const std::string name = typeLayerName(content.text);
		document.changeLayer(id, "Edit Type", [&](im::Layer& changed) { changed.name = name; }, true);
	}
}

/// @brief rasterize: makes a type or shape layer a pixel layer of the same pixels.
/// @param image The image.
/// @param with id.
void rasterize(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const im::Layer& layer = contentLayer(image, with);
	image.canvas->document().setContent(
	    layer.id, layer.content.kind == im::ContentKind::text ? "Rasterize Type" : "Rasterize Layer", {}, nullptr);
}

/// @brief newPath: adds an empty path, the active one, which the pen then draws.
/// @param image The image.
/// @param with name.
void newPath(ImageDocument& image, const json& with, Window*) {
	const std::string name = nameOf(with, "name");
	changePaths(image, "New Path", [&](im::Structure& structure) { addPath(structure, name); });
}

/// @brief selectPath: makes a path the active one (-1 for none: the paths hide).
/// @param image The image.
/// @param with index.
void selectPath(ImageDocument& image, const json& with, Window*) {
	im::Document& document = image.canvas->document();
	const int index = wholeOf(with, "index", -1);
	const int count = static_cast<int>(document.structure().paths.size());
	settle(image);
	document.changeStructure("Select Path",
	                         [&](im::Structure& structure) { structure.activePath = index < count ? index : -1; });
}

/// @brief deletePath: deletes a path.
/// @param image The image.
/// @param with index (the active path's when absent).
void deletePath(ImageDocument& image, const json& with, Window*) {
	const int index = pathIndexOf(image, with);
	changePaths(image, "Delete Path", [&](im::Structure& structure) {
		structure.paths.erase(structure.paths.begin() + index);
		if(structure.activePath >= static_cast<int>(structure.paths.size()) || structure.activePath == index)
			structure.activePath = -1;
		else if(structure.activePath > index) --structure.activePath;
	});
}

/// @brief renamePath: renames a path.
/// @param image The image.
/// @param with index, name.
void renamePath(ImageDocument& image, const json& with, Window*) {
	const int index = pathIndexOf(image, with);
	const std::string name = nameOf(with, "name");
	if(name.empty()) throw DocumentError("A path has a name");
	changePaths(image, "Rename Path",
	            [&](im::Structure& structure) { structure.paths[static_cast<std::size_t>(index)].name = name; });
}

/// @brief setPath: replaces the active path's anchors, or closes or opens it.
/// @param image The image.
/// @param with points, closed.
void setPath(ImageDocument& image, const json& with, Window*) {
	const json points = with.contains("points") ? with["points"] : json();
	const std::vector<im::PathPoint> read =
	    points.is_null() ? std::vector<im::PathPoint>{} : im::pointsFromJson(points);
	changePaths(image, "Edit Path", [&](im::Structure& structure) {
		im::VectorPath& path = activePath(structure);
		if(!points.is_null()) path.points = read;
		path.closed = flagOf(with, "closed", path.closed);
	});
}

/// @brief The active path flattened, closed or not.
/// @param image The image.
[[nodiscard]] std::vector<Point> flattenedPath(const ImageDocument& image) {
	const im::VectorPath& path = activePath(image.canvas->document());
	if(path.points.empty()) throw DocumentError("The path has no anchors");
	return im::flattenPath(path.points, path.closed);
}

/// @brief The pixel layer a path is stroked or filled on, or a DocumentError.
/// @param image The image.
[[nodiscard]] std::uint64_t pathTarget(const ImageDocument& image) {
	const im::Document& document = image.canvas->document();
	const im::Layer* layer = document.activeLayer();
	if(layer == nullptr || layer->kind != im::LayerKind::pixel) throw DocumentError("Choose a pixel layer");
	if(const auto problem = document.paintable(layer->pixels)) throw DocumentError(*problem);
	return layer->pixels;
}

/// @brief strokePath: strokes the active path on the active layer with the foreground colour.
/// @param image The image.
/// @param with width (the brush's size when absent).
void strokePath(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const std::vector<Point> points = flattenedPath(image);
	const std::uint64_t target = pathTarget(image);
	im::Document& document = image.canvas->document();
	const float width = std::clamp(static_cast<float>(numberOf(with, "width", image.settings.tip.size)), 1.0f, 1000.0f);
	const bool closed = activePath(document).closed;
	im::fillCoverage(document, target,
	                 im::strokeCoverage(document.width(), document.height(), points, closed, width, image.limits),
	                 image.settings.foreground, 1.0f, "Stroke Path");
}

/// @brief fillPath: fills the active path on the active layer with the foreground colour.
/// @param image The image.
void fillPath(ImageDocument& image, const json&, Window*) {
	settle(image);
	const std::vector<Point> points = flattenedPath(image);
	if(points.size() < 3) throw DocumentError("A path fills with three anchors or more");
	const std::uint64_t target = pathTarget(image);
	im::Document& document = image.canvas->document();
	im::fillCoverage(document, target, im::polygonCoverage(document.width(), document.height(), points, image.limits),
	                 image.settings.foreground, 1.0f, "Fill Path");
}

/// @brief pathToSelection: makes a selection of the active path's inside.
/// @param image The image.
/// @param with mode (replace, add, subtract, intersect).
void pathToSelection(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const std::vector<Point> points = flattenedPath(image);
	if(points.size() < 3) throw DocumentError("A path makes a selection with three anchors or more");
	im::Document& document = image.canvas->document();
	im::select(document, "Make Selection",
	           im::polygonCoverage(document.width(), document.height(), points, image.limits),
	           selectionModeOf(textOf(with, "mode"), im::SelectionMode::replace));
}

/// @brief pathToShape: makes a shape layer of the active path, filled with the foreground colour.
/// @param image The image.
void pathToShape(ImageDocument& image, const json&, Window*) {
	settle(image);
	const im::VectorPath& path = activePath(image.canvas->document());
	if(path.points.size() < 2) throw DocumentError("The path needs two anchors or more");
	im::LayerContent content = image.settings.vector;
	content.shape = im::ShapeKind::path;
	content.points = path.points;
	content.closed = true;
	content.fill = image.settings.foreground;
	makeShapeLayer(image, content);
}

/// @brief The anchor of the active path near a point (within grabDistance on screen).
/// @param image The image.
/// @param at The point.
/// @return Its index, or -1.
[[nodiscard]] int anchorNear(const ImageDocument& image, Point at) {
	const im::Structure& structure = image.canvas->document().structure();
	if(structure.activePath < 0 || static_cast<std::size_t>(structure.activePath) >= structure.paths.size()) return -1;
	const auto& points = structure.paths[static_cast<std::size_t>(structure.activePath)].points;
	const double reach = grabDistance / std::max(image.view.zoom, 1e-6);
	for(std::size_t index = 0; index < points.size(); ++index)
		if(std::hypot(points[index].anchor.x - at.x, points[index].anchor.y - at.y) <= reach)
			return static_cast<int>(index);
	return -1;
}

/// @brief pen's press: closes the active path at its first anchor, else adds an anchor (a new path if none is
/// active or the active one is closed).
/// @param image The image.
/// @param at Where.
bool pressPen(ImageDocument& image, Point at) {
	const int near = anchorNear(image, at);
	const im::Structure& structure = image.canvas->document().structure();
	const bool open = structure.activePath >= 0 &&
	                  static_cast<std::size_t>(structure.activePath) < structure.paths.size() &&
	                  !structure.paths[static_cast<std::size_t>(structure.activePath)].closed;
	if(open && near == 0 && structure.paths[static_cast<std::size_t>(structure.activePath)].points.size() > 2) {
		changePaths(image, "Close Path", [](im::Structure& changed) { activePath(changed).closed = true; });
		image.pathAnchor = -2; // a drag pulls nothing
		return true;
	}
	changePaths(image, "Add Anchor Point", [&](im::Structure& changed) {
		if(!open) addPath(changed, "Work Path");
		im::VectorPath& path = activePath(changed);
		if(path.points.size() >= im::maxPathPoints)
			throw DocumentError("A path has at most " + std::to_string(im::maxPathPoints) + " anchors");
		const im::VectorPoint anchor{at.x, at.y};
		path.points.push_back({anchor, anchor, anchor});
	});
	image.pathAnchor = static_cast<int>(activePath(image.canvas->document()).points.size()) - 1;
	return true;
}

/// @brief pen's drag: pulls the new anchor's handles, symmetric about it (a smooth point).
/// @param image The image.
/// @param at Where.
bool dragPen(ImageDocument& image, Point at) {
	if(image.pathAnchor < 0) return false;
	const auto index = static_cast<std::size_t>(image.pathAnchor);
	image.canvas->document().changeStructure(
	    "Add Anchor Point",
	    [&](im::Structure& changed) {
		    im::VectorPath& path = activePath(changed);
		    if(index >= path.points.size()) return;
		    im::PathPoint& point = path.points[index];
		    point.out = {at.x, at.y};
		    point.in = {2.0f * point.anchor.x - at.x, 2.0f * point.anchor.y - at.y};
	    },
	    true);
	return true;
}

/// @brief path-select's drag: moves the anchor pressed on (with its handles), or the whole active path.
/// @param image The image.
/// @param at Where.
bool dragPathSelect(ImageDocument& image, Point at) {
	const Point last = image.pathLast; // where the previous move left the pointer
	const float dx = at.x - last.x, dy = at.y - last.y;
	image.pathLast = at;
	if(dx == 0.0f && dy == 0.0f) return false;
	const int anchor = image.pathAnchor;
	image.canvas->document().changeStructure(
	    "Move Path",
	    [&](im::Structure& changed) {
		    im::VectorPath& path = activePath(changed);
		    for(std::size_t index = 0; index < path.points.size(); ++index) {
			    if(anchor >= 0 && static_cast<std::size_t>(anchor) != index) continue;
			    for(im::VectorPoint* point :
				    {&path.points[index].anchor, &path.points[index].in, &path.points[index].out}) {
				    point->x += dx;
				    point->y += dy;
			    }
		    }
	    },
	    image.pathMoved);
	image.pathMoved = true;
	return true;
}

/// @brief The corners of a shape dragged from a press: Shift keeps a square (or circle), Alt draws from the centre.
/// @param image The image.
[[nodiscard]] std::pair<Point, Point> shapeBox(const ImageDocument& image) {
	return {image.press.boxFrom, image.press.boxTo};
}

} // namespace

std::shared_ptr<imaging::Image> drawContent(const ImageDocument& image, const imaging::LayerContent& content) {
	const im::Document& document = image.canvas->document();
	if(content.kind == im::ContentKind::shape)
		return im::renderShape(content, document.width(), document.height(), document.format(), image.limits);
	if(content.kind != im::ContentKind::text) throw DocumentError("A layer's content is text or a shape");
	const auto fonts = image.fonts.lock();
	std::shared_ptr<const Font> font;
	if(fonts && *fonts) font = (*fonts)(content.font, rasterSizeOf(content.size), {content.bold, content.italic});
	if(!font) throw DocumentError("No font draws type here: the screen declares no fonts");
	return im::renderText(content, *font, document.width(), document.height(), document.format(), image.limits);
}

std::array<float, 4> contentBounds(const ImageDocument& image, const imaging::LayerContent& content) {
	return boundsOf(image, content);
}

bool pressVector(ImageDocument& image, imaging::Point at) {
	image.pathMoved = false;
	switch(image.settings.gesture) {
	case Gesture::text: {
		image.typing = true;
		if(const std::uint64_t found = typeLayerAt(image, at)) {
			image.canvas->document().setActive(found);
			return true;
		}
		im::LayerContent content = image.settings.vector;
		content.fill = image.settings.foreground;
		content.origin = {std::round(at.x), std::round(at.y - content.size * 0.5f)};
		makeTypeLayer(image, content);
		return true;
	}
	case Gesture::pen:
		return pressPen(image, at);
	case Gesture::pathSelect:
		image.pathAnchor = anchorNear(image, at);
		image.pathLast = at;
		return false;
	default:
		return false;
	}
}

bool dragVector(ImageDocument& image, imaging::Point at) {
	switch(image.settings.gesture) {
	case Gesture::pen:
		return dragPen(image, at);
	case Gesture::pathSelect:
		(void)activePath(image.canvas->document());
		return dragPathSelect(image, at);
	default:
		return false;
	}
}

bool releaseVector(ImageDocument& image, imaging::Point) {
	if(image.settings.gesture != Gesture::shape) return false;
	const auto [from, to] = shapeBox(image);
	im::LayerContent content = image.settings.vector;
	if(std::abs(to.x - from.x) < 2.0f && std::abs(to.y - from.y) < 2.0f) {
		// A click: a shape of 100 by 100 pixels there, as the counterpart's dialog would make
		content.origin = {from.x, from.y};
		content.corner = {from.x + 100.0f, from.y + 100.0f};
	} else {
		content.origin = {from.x, from.y};
		content.corner = {to.x, to.y};
	}
	content.fill = image.settings.foreground;
	if(content.shape == im::ShapeKind::path) content.shape = im::ShapeKind::rectangle;
	if(content.shape == im::ShapeKind::line && content.strokeWidth <= 0.0f) content.strokeWidth = 4.0f;
	if(content.shape == im::ShapeKind::line && content.stroke.alpha <= 0.0f) content.stroke = image.settings.foreground;
	makeShapeLayer(image, content);
	return true;
}

bool typingInto(const ImageDocument& image) {
	const im::Layer* layer = image.canvas->document().activeLayer();
	return image.typing && image.settings.gesture == Gesture::text && layer != nullptr &&
	       layer->content.kind == im::ContentKind::text && !layer->locks.all;
}

void typeText(ImageDocument& image, std::string_view added) {
	if(!typingInto(image)) return;
	const im::Layer& layer = *image.canvas->document().activeLayer();
	std::string text = layer.content.text;
	if(added.empty()) {
		while(!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0U) == 0x80U) text.pop_back();
		if(!text.empty()) text.pop_back();
	} else {
		text += added;
	}
	editContent(image, {{"text", text}}, nullptr);
}

json pathsJson(const imaging::Document& document) {
	json rows = json::array();
	const im::Structure& structure = document.structure();
	for(std::size_t index = 0; index < structure.paths.size(); ++index) {
		const im::VectorPath& path = structure.paths[index];
		rows.push_back({{"id", std::to_string(index)},
		                {"index", index},
		                {"name", path.name},
		                {"label", path.name},
		                {"closed", path.closed},
		                {"anchors", path.points.size()},
		                {"active", static_cast<int>(index) == structure.activePath}});
	}
	return rows;
}

std::vector<ImageOperation> vectorOperations() {
	return {
	    {"addText",
		 {"text", "x", "y", "size", "font", "bold", "italic", "align", "leading", "fill"},
		 "Adds a type layer.",
		 addText},
	    {"addShape",
		 {"shape", "x", "y", "x2", "y2", "radius", "sides", "points", "closed", "fill", "stroke", "strokeWidth"},
		 "Adds a shape layer.",
		 addShape},
	    {"editContent",
		 {"id",    "text",    "x",     "y",      "x2",    "y2",     "size",   "font", "bold",   "italic",
		  "align", "leading", "shape", "radius", "sides", "points", "closed", "fill", "stroke", "strokeWidth"},
		 "Changes a type or shape layer's content, and draws it again.",
		 editContent},
	    {"rasterize", {"id"}, "Makes a type or shape layer a pixel layer of the same pixels.", rasterize},
	    {"newPath", {"name"}, "Adds an empty path, the active one, for the pen.", newPath},
	    {"selectPath", {"index"}, "Makes a path the active one (-1 for none).", selectPath},
	    {"deletePath", {"index"}, "Deletes a path (the active one when no index).", deletePath},
	    {"renamePath", {"index", "name"}, "Renames a path.", renamePath},
	    {"setPath", {"points", "closed"}, "Replaces the active path's anchors, or closes or opens it.", setPath},
	    {"strokePath",
		 {"width"},
		 "Strokes the active path on the active layer with the foreground colour.",
		 strokePath},
	    {"fillPath", {}, "Fills the active path on the active layer with the foreground colour.", fillPath},
	    {"pathToSelection", {"mode"}, "Makes a selection of the active path's inside.", pathToSelection},
	    {"pathToShape", {}, "Makes a shape layer of the active path.", pathToShape},
	};
}

} // namespace affresco::format::editing
