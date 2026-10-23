/// @file
/// @brief format::editing: the "image" step's edits: select, feather, fillSelection, crop, and the filters with a live
/// preview (previewFilter, applyFilter, cancelFilter).

#include "ImageOperations.hpp"

#include <algorithm>
#include <cmath>

namespace affresco::format::editing {

namespace {

/// @brief The most tiles a filter preview computes per call: what is visible first, the rest on applyFilter.
constexpr std::size_t previewTiles = 256;

/// @brief The coverage of a polygon argument, its points at most the limits' polygonPoints.
/// @param image The image.
/// @param with points: [{x, y}].
/// @return The coverage.
/// @throws DocumentError If there is no list of points, or too long a one.
[[nodiscard]] imaging::Coverage polygonOf(const ImageDocument& image, const json& with) {
	const auto found = with.find("points");
	if(found == with.end() || !found->is_array() || found->size() > image.limits.polygonPoints)
		throw DocumentError("A polygon needs a list of at most " + std::to_string(image.limits.polygonPoints) +
		                    " points");
	std::vector<imaging::Point> points;
	points.reserve(found->size());
	for(const json& point : *found)
		points.push_back({static_cast<float>(withinReach(numberOf(point, "x", 0.0))),
		                  static_cast<float>(withinReach(numberOf(point, "y", 0.0)))});
	const imaging::Document& document = image.canvas->document();
	return imaging::polygonCoverage(document.width(), document.height(), points, image.limits);
}

/// @brief The coverage of a colour range argument.
/// @param image The image; its foreground is the colour when none is given.
/// @param with color, fuzziness (0 to 255), allLayers.
/// @return The coverage.
/// @throws DocumentError If the colour isn't one.
[[nodiscard]] imaging::Coverage colourRangeOf(const ImageDocument& image, const json& with) {
	const auto colour = colorOf(textOf(with, "color", hexOf(image.settings.foreground)));
	if(!colour) throw DocumentError("Not a colour");
	const int fuzziness = std::clamp(wholeOf(with, "fuzziness", 40), 0, 255);
	return imaging::colorRangeCoverage(image.canvas->document(), *colour, fuzziness, flagOf(with, "allLayers", false));
}

/// @brief select: selects by a shape, a wand click or a colour range; or all, none, the inverse.
/// @param image The image.
/// @param with kind ("all", "none", "invert", "rectangle", "ellipse", "polygon", "wand", "colorRange"), mode, and the
/// shape's from and to, points, x and y, color and fuzziness.
/// @throws DocumentError If the kind is unknown or its arguments malformed.
void select(ImageDocument& image, const json& with, Window*) {
	settle(image);
	imaging::Document& document = image.canvas->document();
	const std::string kind = textOf(with, "kind");
	const auto mode = selectionModeOf(textOf(with, "mode"), image.settings.mode);
	const int width = document.width(), height = document.height();
	if(kind == "all") return imaging::selectAll(document);
	if(kind == "none") return imaging::deselect(document);
	if(kind == "invert") return imaging::invertSelection(document);
	if(kind == "rectangle" || kind == "ellipse") {
		const imaging::Point from = pointOf(with, "from"), to = pointOf(with, "to");
		const bool box = kind == "rectangle";
		return imaging::select(document, box ? "Rectangular Marquee" : "Elliptical Marquee",
		                       box ? imaging::rectangleCoverage(width, height, from, to)
		                           : imaging::ellipseCoverage(width, height, from, to),
		                       mode);
	}
	if(kind == "polygon") return imaging::select(document, "Polygonal Lasso", polygonOf(image, with), mode);
	if(kind == "wand")
		return imaging::select(
		    document, "Magic Wand",
		    imaging::wandCoverage(document, wholeOf(with, "x", 0), wholeOf(with, "y", 0), image.settings.sample), mode);
	if(kind == "colorRange") return imaging::select(document, "Color Range", colourRangeOf(image, with), mode);
	throw DocumentError("Unknown selection " + kind);
}

/// @brief feather: softens the selection's edge.
/// @param image The image.
/// @param with radius (pixels, 4 by default, at most the filterRadius limit).
void feather(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const double radius = std::clamp(numberOf(with, "radius", 4.0), 0.0, static_cast<double>(image.limits.filterRadius));
	imaging::featherSelection(image.canvas->document(), static_cast<float>(radius));
}

/// @brief The image the filters and fill change: the active pixel layer's, or its mask's when "mask" is true.
/// @param image The image.
/// @param with mask.
/// @return Its id.
/// @throws DocumentError If it can't be changed.
[[nodiscard]] std::uint64_t filterTarget(const ImageDocument& image, const json& with) {
	const imaging::Document& document = image.canvas->document();
	const imaging::Layer* layer = document.activeLayer();
	if(layer == nullptr || layer->kind != imaging::LayerKind::pixel) throw DocumentError("Filters change a pixel layer");
	const std::uint64_t target = flagOf(with, "mask", false) && layer->mask != 0 ? layer->mask : layer->pixels;
	if(const auto why = document.paintable(target)) throw DocumentError(*why);
	return target;
}

/// @brief fillSelection: fills the selection (or all) of the active layer with a colour at the fill opacity.
/// @param image The image.
/// @param with color (the foreground by default), mask.
/// @throws DocumentError If the colour isn't one or the layer can't be painted.
void fillSelection(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const auto colour = colorOf(textOf(with, "color", hexOf(image.settings.foreground)));
	if(!colour) throw DocumentError("Not a colour");
	imaging::fillSelection(image.canvas->document(), filterTarget(image, with), *colour, image.settings.opacity);
}

/// @brief crop: keeps a rectangle of the image (and forgets the history).
/// @param image The image.
/// @param with x, y, width, height.
void cropImage(ImageDocument& image, const json& with, Window*) {
	settle(image);
	crop(image, wholeOf(with, "x", 0), wholeOf(with, "y", 0), wholeOf(with, "width", 0), wholeOf(with, "height", 0));
}

/// @brief A filter argument.
/// @param image The image, for its limits.
/// @param with kind, radius, amount (percent), threshold, angle, distance, monochrome, seed.
/// @return The filter, within the limits.
/// @throws DocumentError If the kind is unknown.
[[nodiscard]] imaging::Filter filterOf(const ImageDocument& image, const json& with) {
	imaging::Filter filter;
	const std::string kind = textOf(with, "kind", "gaussian-blur");
	const auto parsed = imaging::filterKindOf(kind);
	if(!parsed) throw DocumentError("Unknown filter " + kind);
	filter.kind = *parsed;
	filter.radius = static_cast<float>(numberOf(with, "radius", filter.radius));
	filter.amount = static_cast<float>(numberOf(with, "amount", filter.amount * 100.0) / 100.0);
	filter.threshold = wholeOf(with, "threshold", filter.threshold);
	filter.angle = static_cast<float>(numberOf(with, "angle", filter.angle));
	filter.distance = static_cast<float>(numberOf(with, "distance", filter.distance));
	filter.monochrome = flagOf(with, "monochrome", filter.monochrome);
	filter.seed = static_cast<std::uint32_t>(std::clamp(numberOf(with, "seed", filter.seed), 0.0, 4294967295.0));
	filter.normalize(image.limits);
	return filter;
}

/// @brief previewFilter: previews a filter over what the view shows; a step in progress until applied or cancelled.
/// @param image The image.
/// @param with The filter (filterOf()), name (the step's), mask.
/// @throws DocumentError If the filter or the target is wrong.
void previewFilter(ImageDocument& image, const json& with, Window*) {
	if(image.stroke) settle(image);
	image.preview.reset();
	image.previewed = filterOf(image, with);
	auto& document = image.canvas->document();
	image.preview = std::make_unique<imaging::FilterRun>(document, filterTarget(image, with), image.previewed,
	                                                     nameOf(with, "name", "Filter"));
	const imaging::CanvasView& view = image.view;
	const double halfWidth = view.width / 2.0 / view.zoom, halfHeight = view.height / 2.0 / view.zoom;
	const int left = std::max(0, static_cast<int>(view.x - halfWidth));
	const int top = std::max(0, static_cast<int>(view.y - halfHeight));
	const int right = std::min(document.width(), static_cast<int>(std::ceil(view.x + halfWidth)));
	const int bottom = std::min(document.height(), static_cast<int>(std::ceil(view.y + halfHeight)));
	if(right > left && bottom > top) image.preview->runRegion(left, top, right - left, bottom - top, previewTiles);
}

/// @brief applyFilter: finishes the filter previewed (or, given a kind, previews it first) as one step.
/// @param image The image.
/// @param with As previewFilter, or nothing to apply the preview.
/// @param window Unused.
/// @throws DocumentError If there is nothing to apply.
void applyFilter(ImageDocument& image, const json& with, Window* window) {
	if(!image.preview || with.contains("kind")) previewFilter(image, with, window);
	image.preview->finish();
	image.preview.reset();
}

/// @brief cancelFilter: drops the preview, leaving the layer as it was.
/// @param image The image.
void cancelFilter(ImageDocument& image, const json&, Window*) {
	if(image.preview) image.preview->cancel();
	image.preview.reset();
}

} // namespace

std::vector<ImageOperation> editOperations() {
	return {
	    {"select",
	     {"kind", "mode", "from", "to", "points", "x", "y", "color", "fuzziness", "allLayers"},
	     "Selects all, none, the inverse, a rectangle, an ellipse, a polygon, by magic wand or by colour range.",
	     select},
	    {"feather", {"radius"}, "Softens the selection's edge.", feather},
	    {"fillSelection", {"color", "mask"}, "Fills the selection of the active layer with a colour.", fillSelection},
	    {"crop", {"x", "y", "width", "height"}, "Keeps a rectangle of the image, and forgets the history.", cropImage},
	    {"previewFilter",
	     {"kind", "radius", "amount", "threshold", "angle", "distance", "monochrome", "seed", "name", "mask"},
	     "Previews a filter on the active layer until it is applied or cancelled.",
	     previewFilter},
	    {"applyFilter",
	     {"kind", "radius", "amount", "threshold", "angle", "distance", "monochrome", "seed", "name", "mask"},
	     "Applies the filter previewed, or the one given, as one step.",
	     applyFilter},
	    {"cancelFilter", {}, "Drops the filter's preview.", cancelFilter},
	};
}

} // namespace affresco::format::editing
