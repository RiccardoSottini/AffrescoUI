/// @file
/// @brief Gradients and fills: a colour per pixel laid over a layer's tiles as they were, by a coverage and the
/// selection, each tile replaced whole.

#include "affresco/imaging/Gradient.hpp"

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Blend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace affresco::imaging {

namespace {

constexpr std::array<std::string_view, 5> shapeNames{"linear", "radial", "angle", "reflected",
                                                     "diamond"}; ///< By GradientShape.

/// @brief A tile to fill.
struct Place {
	int column = 0;                 ///< Its column.
	int row = 0;                    ///< Its row.
	const Tile* coverage = nullptr; ///< How much of each pixel, or null for all of it.
};

/// @brief A pixel with a colour laid over it: blended, or on a mask its grey moved towards the colour's.
/// @param under The pixel.
/// @param laid The colour, its alpha the opacity laid.
/// @param mask Whether the image is a mask.
/// @return The pixel after.
[[nodiscard]] Pixel laidOver(const Pixel& under, const Pixel& laid, bool mask) noexcept {
	if(!mask) return blendPixels(under, laid, BlendMode::normal, 1.0f);
	const float grey = clamped(0.2126f * laid.red + 0.7152f * laid.green + 0.0722f * laid.blue);
	const float value = under.alpha + (grey - under.alpha) * laid.alpha;
	return {value, value, value, value};
}

/// @brief The opacity laid at a pixel of a tile, by its coverage and then its selection (in that order, as floats).
/// @param alpha The colour's opacity.
/// @param place The tile.
/// @param selection The selection's tile, or null for no selection.
/// @param at The pixel's index in the tile.
/// @return The opacity.
[[nodiscard]] float coveredAt(float alpha, const Place& place, const Tile* selection, std::size_t at) noexcept {
	if(place.coverage) alpha *= unitFromByte(place.coverage->bytes()[at]);
	if(selection) alpha *= unitFromByte(selection->bytes()[at]);
	return alpha;
}

/// @brief A tile of an image with a colour per pixel laid over it, within the image.
/// @param image The image.
/// @param place The tile.
/// @param selection The selection's tile, or null for no selection.
/// @param source The colour at a pixel (x, y), its alpha the opacity laid.
/// @return The new tile.
template <typename Source>
[[nodiscard]] std::shared_ptr<Tile> laidTile(const Image& image, const Place& place, const Tile* selection,
                                             const Source& source) {
	const bool mask = image.format() == PixelFormat::mask8;
	const std::shared_ptr<const Tile> original = image.tile({0, place.column, place.row});
	auto tile = original ? std::make_shared<Tile>(*original) : std::make_shared<Tile>(image.format());
	const int width = std::min(tileSize, image.width() - place.column * tileSize);
	const int height = std::min(tileSize, image.height() - place.row * tileSize);
	for(int y = 0; y < height; ++y)
		for(int x = 0; x < width; ++x) {
			Pixel laid = source(place.column * tileSize + x, place.row * tileSize + y);
			laid.alpha = coveredAt(laid.alpha, place, selection,
			                       static_cast<std::size_t>(y) * tileSize + static_cast<std::size_t>(x));
			if(laid.alpha <= 0.0f) continue;
			tile->setPixel(x, y, laidOver(tile->pixel(x, y), laid, mask));
		}
	return tile;
}

/// @brief Lays a colour per pixel over tiles of a layer as one step.
/// @param document The document.
/// @param target The image.
/// @param name The step's name.
/// @param places The tiles.
/// @param source The colour at a pixel (x, y), its alpha the opacity laid.
/// @throws DocumentError when the target can't be painted.
template <typename Source>
void lay(Document& document, std::uint64_t target, const std::string& name, const std::vector<Place>& places,
         const Source& source) {
	if(const auto refused = document.paintable(target); refused) throw DocumentError(*refused);
	const Image* image = document.image(target);
	if(image == nullptr) throw DocumentError("imaging: no image to fill");
	const bool selected = document.hasSelection() && target != Document::selectionId;
	document.begin(name);
	for(const Place& place : places) {
		const std::shared_ptr<const Tile> selection =
		    selected ? document.selection().tile({0, place.column, place.row}) : nullptr;
		if(selected && !selection) continue;
		auto tile = laidTile(*image, place, selection.get(), source);
		document.replaceTile(target, place.column, place.row, tile->empty() ? nullptr : std::move(tile));
	}
	document.commit(target == Document::selectionId ? std::optional<bool>(selectsAnything(document.selection()))
	                                                : std::nullopt);
}

/// @brief Every tile of a layer.
/// @param document The document.
/// @param target The image.
[[nodiscard]] std::vector<Place> everywhere(const Document& document, std::uint64_t target) {
	std::vector<Place> places;
	const Image* image = document.image(target);
	if(image == nullptr) return places;
	for(int row = 0; row < image->rows(); ++row)
		for(int column = 0; column < image->columns(); ++column) places.push_back({column, row, nullptr});
	return places;
}

} // namespace

std::span<const std::string_view> gradientShapeNames() noexcept { return shapeNames; }

std::span<const GradientPreset> gradientPresets() {
	static const std::array<GradientPreset, 4> presets{{
	    {"Black, White", Gradient{}},
	    {"Spectrum", Gradient{{{0.0f, {1, 0, 0, 1}},
		                       {1.0f / 6, {1, 1, 0, 1}},
		                       {2.0f / 6, {0, 1, 0, 1}},
		                       {3.0f / 6, {0, 1, 1, 1}},
		                       {4.0f / 6, {0, 0, 1, 1}},
		                       {5.0f / 6, {1, 0, 1, 1}},
		                       {1.0f, {1, 0, 0, 1}}}}},
	    {"Copper",
		 Gradient{
	         {{0.0f, {0.59f, 0.29f, 0.16f, 1}}, {0.6f, {0.98f, 0.82f, 0.68f, 1}}, {1.0f, {0.42f, 0.17f, 0.08f, 1}}}}},
	    {"Black, Transparent", Gradient{{{0.0f, {0, 0, 0, 1}}, {1.0f, {0, 0, 0, 0}}}}},
	}};
	return presets;
}

float gradientPosition(GradientShape shape, Point from, Point to, float x, float y) noexcept {
	const float alongX = to.x - from.x, alongY = to.y - from.y;
	const float length = std::hypot(alongX, alongY);
	if(!(length > 1e-3f) || !std::isfinite(length)) return 1.0f;
	const float offsetX = x - from.x, offsetY = y - from.y;
	const float along = (offsetX * alongX + offsetY * alongY) / (length * length);
	const float across = (offsetY * alongX - offsetX * alongY) / (length * length);
	switch(shape) {
	case GradientShape::linear:
		return clamped(along);
	case GradientShape::radial:
		return clamped(std::hypot(along, across));
	case GradientShape::reflected:
		return clamped(std::abs(along));
	case GradientShape::diamond:
		return clamped(std::abs(along) + std::abs(across));
	case GradientShape::angle: {
		const float turn = std::atan2(-across, along) / (2.0f * std::numbers::pi_v<float>);
		return turn < 0.0f ? turn + 1.0f : turn;
	}
	}
	return 0.0f;
}

void drawGradient(Document& document, std::uint64_t target, const Gradient& gradient, GradientShape shape, Point from,
                  Point to, float opacity) {
	Gradient normalized = gradient;
	normalized.normalize();
	const float laid = clamped(std::isfinite(opacity) ? opacity : 1.0f);
	lay(document, target, "Gradient", everywhere(document, target), [&](int x, int y) {
		Pixel colour = normalized.at(
		    gradientPosition(shape, from, to, static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f));
		colour.alpha *= laid;
		return colour;
	});
}

void fillCoverage(Document& document, std::uint64_t target, const Coverage& coverage, const Pixel& colour,
                  float opacity, const std::string& name) {
	const Image* image = document.image(target);
	std::vector<Place> places;
	for(const auto& [place, tile] : coverage)
		if(image != nullptr && image->contains({0, place.first, place.second}))
			places.push_back({place.first, place.second, tile.get()});
	Pixel laid = clamped(colour);
	laid.alpha *= clamped(std::isfinite(opacity) ? opacity : 1.0f);
	lay(document, target, name, places, [&](int, int) { return laid; });
}

void fillAt(Document& document, std::uint64_t target, int x, int y, const Pixel& colour, const SampleOptions& options,
            float opacity) {
	if(const auto refused = document.paintable(target); refused) throw DocumentError(*refused);
	fillCoverage(document, target, wandCoverage(document, x, y, options), colour, opacity, "Paint Bucket");
}

void fillSelection(Document& document, std::uint64_t target, const Pixel& colour, float opacity) {
	Pixel laid = clamped(colour);
	laid.alpha *= clamped(std::isfinite(opacity) ? opacity : 1.0f);
	lay(document, target, "Fill", everywhere(document, target), [&](int, int) { return laid; });
}

} // namespace affresco::imaging
