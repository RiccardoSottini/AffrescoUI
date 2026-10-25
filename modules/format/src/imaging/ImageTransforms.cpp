/// @file
/// @brief format::editing: the "image" step's geometry and merges: canvasSize, imageSize, rotateCanvas, flipCanvas,
/// transformLayer, mergeDown, mergeVisible, flatten, deleteHidden, and the clipboard's copy, cut, paste and clear.
///
/// A change of the image's size makes a new document and forgets the history, as crop does; a change that keeps the
/// size (a flip, a half turn, a layer's transform, the merges) is one undoable step.

#include "ImageOperations.hpp"

#include "affresco/imaging/Composite.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

namespace affresco::format::editing {

namespace {

using imaging::Pixel;
using imaging::tileSize;

/// @brief Any offset past this many pixels is the same as this one, so a sum can't overflow.
constexpr double farthest = 1e7;
/// @brief The most samples a pixel of a shrunk image averages along each axis.
constexpr int mostSamples = 4;
/// @brief The smallest and largest scale of a layer's transform, in percent (negative flips it).
constexpr double leastScale = 1.0, mostScale = 10000.0;

/// @brief Where a pixel of a new image comes from in an old one, between continuous coordinates (a pixel's centre is
/// at +0.5): x' = a x + b y + c, y' = d x + e y + f.
struct Mapping {
	double a = 1.0; ///< x' per x.
	double b = 0.0; ///< x' per y.
	double c = 0.0; ///< x' at the origin.
	double d = 0.0; ///< y' per x.
	double e = 1.0; ///< y' per y.
	double f = 0.0; ///< y' at the origin.

	/// @brief The point of the old image a point of the new one shows.
	/// @param x Across, in the new image.
	/// @param y Down, in the new image.
	/// @return The point in the old one.
	[[nodiscard]] std::pair<double, double> operator()(double x, double y) const noexcept {
		return {a * x + b * y + c, d * x + e * y + f};
	}
};

/// @brief What an image's absent tiles hold: transparent, or a layer mask's white (which reveals).
/// @param format The image's format.
/// @return The pixel.
[[nodiscard]] Pixel absentOf(imaging::PixelFormat format) noexcept {
	return format == imaging::PixelFormat::mask8 ? Pixel{1.0f, 1.0f, 1.0f, 1.0f} : Pixel{};
}

/// @brief Reads an image's pixels, keeping the tile it read last; outside the image, its absent pixel.
class Reader {
public:
	/// @brief A reader of an image, which must outlive it.
	/// @param image The image.
	explicit Reader(const imaging::Image& image) : image_(image), absent_(absentOf(image.format())) {}

	/// @brief One pixel.
	/// @param x Across.
	/// @param y Down.
	/// @return It.
	[[nodiscard]] Pixel at(int x, int y) {
		if(x < 0 || y < 0 || x >= image_.width() || y >= image_.height()) return absent_;
		const int column = x / tileSize, row = y / tileSize;
		if(column != column_ || row != row_) {
			tile_ = image_.tile({0, column, row});
			column_ = column;
			row_ = row;
		}
		return tile_ ? tile_->pixel(x % tileSize, y % tileSize) : absent_;
	}

	/// @brief The colour at a continuous point, between the four nearest pixels.
	/// @param x Across.
	/// @param y Down.
	/// @return It, straight alpha.
	[[nodiscard]] Pixel between(double x, double y) {
		const double fromX = x - 0.5, fromY = y - 0.5;
		const double left = std::floor(fromX), top = std::floor(fromY);
		const double across = fromX - left, down = fromY - top;
		const int column = static_cast<int>(left), row = static_cast<int>(top);
		std::array<double, 4> sum{};
		const auto add = [&](const Pixel& pixel, double weight) {
			const double covered = pixel.alpha * weight;
			sum[0] += pixel.red * covered;
			sum[1] += pixel.green * covered;
			sum[2] += pixel.blue * covered;
			sum[3] += covered;
		};
		add(at(column, row), (1.0 - across) * (1.0 - down));
		add(at(column + 1, row), across * (1.0 - down));
		add(at(column, row + 1), (1.0 - across) * down);
		add(at(column + 1, row + 1), across * down);
		if(sum[3] <= 0.0) return {};
		return {static_cast<float>(sum[0] / sum[3]), static_cast<float>(sum[1] / sum[3]),
		        static_cast<float>(sum[2] / sum[3]), static_cast<float>(std::min(sum[3], 1.0))};
	}

private:
	const imaging::Image& image_;               ///< The image.
	Pixel absent_;                              ///< Outside it, and its absent tiles.
	int column_ = -1;                           ///< The column of the tile read last.
	int row_ = -1;                              ///< Its row.
	std::shared_ptr<const imaging::Tile> tile_; ///< It, or null when absent.
};

/// @brief How a new image samples an old one.
struct Sampling {
	Mapping mapping;     ///< Where each pixel comes from.
	int samples = 1;     ///< Samples per axis each pixel averages (more when shrinking).
	bool smooth = false; ///< Between pixels (scaling, turning), or the nearest (moving, flipping, quarter turns).
};

/// @brief Whether an image has a present tile under a rectangle of its pixels (one pixel more each side).
/// @param image The image.
/// @param left Its left edge.
/// @param top Its top edge.
/// @param right Its right edge.
/// @param bottom Its bottom edge.
/// @return Whether one is present.
[[nodiscard]] bool touches(const imaging::Image& image, double left, double top, double right, double bottom) {
	const auto columnOf = [&](double x) {
		return static_cast<int>(std::clamp(std::floor(x / tileSize), -1.0, static_cast<double>(image.columns())));
	};
	const auto rowOf = [&](double y) {
		return static_cast<int>(std::clamp(std::floor(y / tileSize), -1.0, static_cast<double>(image.rows())));
	};
	for(int row = std::max(0, rowOf(top - 1.0)); row <= std::min(image.rows() - 1, rowOf(bottom + 1.0)); ++row)
		for(int column = std::max(0, columnOf(left - 1.0));
		    column <= std::min(image.columns() - 1, columnOf(right + 1.0)); ++column)
			if(image.tile({0, column, row})) return true;
	return false;
}

/// @brief An image made from another through a sampling: tiles that would hold only the absent pixel stay absent.
/// @param source The old image.
/// @param width The new one's width.
/// @param height Its height.
/// @param format Its format.
/// @param sampling How it samples the old one.
/// @param limits Its limits.
/// @return It.
[[nodiscard]] std::shared_ptr<imaging::Image> remapped(const imaging::Image& source, int width, int height,
                                                       imaging::PixelFormat format, const Sampling& sampling,
                                                       const imaging::Limits& limits) {
	auto made = std::make_shared<imaging::Image>(width, height, format, limits);
	Reader reader(source);
	const Pixel absent = absentOf(format), outside = absentOf(source.format());
	const Mapping& mapping = sampling.mapping;
	const int samples = std::clamp(sampling.samples, 1, mostSamples);
	std::vector<Pixel> pixels(imaging::tilePixels);
	const auto sampleAt = [&](double x, double y) -> Pixel {
		if(samples == 1 && !sampling.smooth) {
			const auto [fromX, fromY] = mapping(x + 0.5, y + 0.5);
			return reader.at(static_cast<int>(std::floor(std::clamp(fromX, -farthest, farthest))),
			                 static_cast<int>(std::floor(std::clamp(fromY, -farthest, farthest))));
		}
		std::array<double, 4> sum{};
		for(int down = 0; down < samples; ++down)
			for(int across = 0; across < samples; ++across) {
				const auto [fromX, fromY] = mapping(x + (across + 0.5) / samples, y + (down + 0.5) / samples);
				const double clampedX = std::clamp(fromX, -farthest, farthest),
				             clampedY = std::clamp(fromY, -farthest, farthest);
				const Pixel pixel = sampling.smooth ? reader.between(clampedX, clampedY)
				                                    : reader.at(static_cast<int>(std::floor(clampedX)),
				                                                static_cast<int>(std::floor(clampedY)));
				sum[0] += pixel.red * pixel.alpha;
				sum[1] += pixel.green * pixel.alpha;
				sum[2] += pixel.blue * pixel.alpha;
				sum[3] += pixel.alpha;
			}
		if(sum[3] <= 0.0) return {};
		return {static_cast<float>(sum[0] / sum[3]), static_cast<float>(sum[1] / sum[3]),
		        static_cast<float>(sum[2] / sum[3]), static_cast<float>(sum[3] / (samples * samples))};
	};
	for(int row = 0; row < made->rows(); ++row)
		for(int column = 0; column < made->columns(); ++column) {
			const double left = column * tileSize, top = row * tileSize;
			// The old image's rectangle under the tile's corners: none of it present, the tile is the absent pixel
			std::array<std::pair<double, double>, 4> corners{mapping(left, top), mapping(left + tileSize, top),
			                                                 mapping(left, top + tileSize),
			                                                 mapping(left + tileSize, top + tileSize)};
			const auto [lowX, highX] = std::ranges::minmax(corners | std::views::keys);
			const auto [lowY, highY] = std::ranges::minmax(corners | std::views::values);
			if(outside == absent && !touches(source, lowX, lowY, highX, highY)) continue;
			bool plain = true;
			for(int y = 0; y < tileSize; ++y)
				for(int x = 0; x < tileSize; ++x) {
					const int atX = column * tileSize + x, atY = row * tileSize + y;
					const Pixel pixel = atX < width && atY < height ? sampleAt(atX, atY) : absent;
					pixels[static_cast<std::size_t>(y * tileSize + x)] = pixel;
					plain = plain && pixel == absent;
				}
			if(plain) continue;
			imaging::Tile& tile = made->writableTile(column, row);
			for(int y = 0; y < tileSize; ++y)
				for(int x = 0; x < tileSize; ++x)
					tile.setPixel(x, y, pixels[static_cast<std::size_t>(y * tileSize + x)]);
		}
	return made;
}

/// @brief Refuses a size beyond an image's limits.
/// @param image The image.
/// @param width The width.
/// @param height The height.
/// @throws DocumentError If it is beyond them.
void checkSize(const ImageDocument& image, int width, int height) {
	const imaging::Limits& limits = image.limits;
	if(width < 1 || height < 1 || width > limits.side || height > limits.side ||
	   static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > limits.pixels)
		throw DocumentError("An image is 1 to " + std::to_string(limits.side) + " pixels a side, and at most " +
		                    std::to_string(limits.pixels) + " pixels");
}

/// @brief Samples every image of the document anew: one step when the size stays, otherwise a new document (which
/// forgets the history). The selection goes.
/// @param image The image.
/// @param name The step's name.
/// @param width The new width.
/// @param height The new height.
/// @param sampling How the images are sampled.
/// @throws DocumentError If the size is beyond the limits.
void reshape(ImageDocument& image, const std::string& name, int width, int height, const Sampling& sampling) {
	settle(image);
	checkSize(image, width, height);
	imaging::Document& old = image.canvas->document();
	imaging::Structure structure = old.structure();
	structure.selection = false;
	for(auto& [id, layerImage] : structure.images)
		layerImage = remapped(*layerImage, width, height, layerImage->format(), sampling, image.limits);
	if(width == old.width() && height == old.height()) {
		old.changeStructure(name, [&](imaging::Structure& changed) { changed = structure; });
		return;
	}
	auto made = std::make_unique<imaging::Document>(width, height, image.limits, old.format());
	made->changeStructure(name, [&](imaging::Structure& replaced) { replaced = structure; });
	made->clearHistory();
	replaceDocument(image, std::move(made));
}

/// @brief How far across and down a name places the old image in a new canvas.
/// @param name "top-left", "top", "top-right", "left", "center", "right", "bottom-left", "bottom" or "bottom-right".
/// @return 0, 0.5 or 1 across and down.
/// @throws DocumentError If the name is unknown.
[[nodiscard]] std::pair<double, double> anchorOf(std::string_view name) {
	constexpr std::array<std::string_view, 9> names{"top-left", "top",         "top-right", "left",        "center",
	                                                "right",    "bottom-left", "bottom",    "bottom-right"};
	const auto found = std::ranges::find(names, name);
	if(found == names.end()) throw DocumentError("Unknown anchor " + std::string(name));
	const auto index = found - names.begin();
	return {static_cast<double>(index % 3) / 2.0, static_cast<double>(index / 3) / 2.0};
}

/// @brief canvasSize: makes the canvas larger or smaller, the image placed by an anchor; new parts are transparent.
/// @param image The image.
/// @param with width, height (the image's by default), anchor ("center" by default).
/// @throws DocumentError If the size is beyond the limits or the anchor unknown.
void canvasSize(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const imaging::Document& old = image.canvas->document();
	const int width = wholeOf(with, "width", old.width()), height = wholeOf(with, "height", old.height());
	checkSize(image, width, height);
	const auto [across, down] = anchorOf(textOf(with, "anchor", "center"));
	const double left = std::floor((width - old.width()) * across), top = std::floor((height - old.height()) * down);
	if(width == old.width() && height == old.height()) return;
	reshape(image, "Canvas Size", width, height, {{1.0, 0.0, -left, 0.0, 1.0, -top}});
}

/// @brief imageSize: resamples the image to a size; given one side only (the other absent or null), the other keeps
/// the proportions.
/// @param image The image.
/// @param with width, height.
/// @throws DocumentError If the size is beyond the limits.
void imageSize(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const imaging::Document& old = image.canvas->document();
	const auto given = [&with](const char* key) { return with.contains(key) && with[key].is_number(); };
	const bool across = given("width"), down = given("height");
	const auto proportional = [](int side, int to, int from) {
		return static_cast<int>(std::clamp(std::lround(static_cast<double>(side) * to / from), 1L, 1L << 30));
	};
	int width = wholeOf(with, "width", old.width()), height = wholeOf(with, "height", old.height());
	if(across && !down && width >= 1) height = proportional(old.height(), width, old.width());
	if(down && !across && height >= 1) width = proportional(old.width(), height, old.height());
	checkSize(image, width, height);
	if(width == old.width() && height == old.height()) return;
	const double scaleX = static_cast<double>(old.width()) / width, scaleY = static_cast<double>(old.height()) / height;
	const int samples = static_cast<int>(std::ceil(std::min(std::max(scaleX, scaleY), double{mostSamples})));
	reshape(image, "Image Size", width, height, {{scaleX, 0.0, 0.0, 0.0, scaleY, 0.0}, samples, true});
}

/// @brief rotateCanvas: turns the whole image clockwise by a quarter, a half or three quarters.
/// @param image The image.
/// @param with angle (degrees: 90, 180 or 270, or the same less 360).
/// @throws DocumentError If the angle isn't a quarter turn.
void rotateCanvas(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const imaging::Document& old = image.canvas->document();
	const double width = old.width(), height = old.height();
	const int angle = ((wholeOf(with, "angle", 90) % 360) + 360) % 360;
	if(angle == 0) return;
	if(angle == 90)
		return reshape(image, "Rotate Canvas", old.height(), old.width(), {{0.0, 1.0, 0.0, -1.0, 0.0, height}});
	if(angle == 180)
		return reshape(image, "Rotate Canvas", old.width(), old.height(), {{-1.0, 0.0, width, 0.0, -1.0, height}});
	if(angle == 270)
		return reshape(image, "Rotate Canvas", old.height(), old.width(), {{0.0, -1.0, width, 1.0, 0.0, 0.0}});
	throw DocumentError("A canvas turns by 90, 180 or 270 degrees");
}

/// @brief flipCanvas: mirrors the whole image.
/// @param image The image.
/// @param with direction ("horizontal", left to right, by default; or "vertical").
/// @throws DocumentError If the direction is unknown.
void flipCanvas(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const imaging::Document& old = image.canvas->document();
	const std::string direction = textOf(with, "direction", "horizontal");
	if(direction != "horizontal" && direction != "vertical") throw DocumentError("Unknown direction " + direction);
	const bool across = direction == "horizontal";
	reshape(image, across ? "Flip Canvas Horizontal" : "Flip Canvas Vertical", old.width(), old.height(),
	        {across ? Mapping{-1.0, 0.0, static_cast<double>(old.width()), 0.0, 1.0, 0.0}
	                : Mapping{1.0, 0.0, 0.0, 0.0, -1.0, static_cast<double>(old.height())}});
}

/// @brief The centre of the box around an image's pixels that aren't transparent, or nothing when there are none.
/// @param image The image.
/// @return The centre, which lands on a pixel's edge or centre.
[[nodiscard]] std::optional<std::pair<double, double>> contentCentre(const imaging::Image& image) {
	int left = image.width(), top = image.height(), right = 0, bottom = 0;
	for(int row = 0; row < image.rows(); ++row)
		for(int column = 0; column < image.columns(); ++column) {
			const auto tile = image.tile({0, column, row});
			if(!tile) continue;
			for(int y = 0; y < tileSize; ++y)
				for(int x = 0; x < tileSize; ++x) {
					const int atX = column * tileSize + x, atY = row * tileSize + y;
					if(atX >= image.width() || atY >= image.height() || tile->pixel(x, y).alpha <= 0.0f) continue;
					left = std::min(left, atX);
					top = std::min(top, atY);
					right = std::max(right, atX + 1);
					bottom = std::max(bottom, atY + 1);
				}
		}
	if(right <= left || bottom <= top) return std::nullopt;
	return std::pair{(left + right) / 2.0, (top + bottom) / 2.0};
}

/// @brief transformLayer: moves, scales, flips and turns a pixel layer (with its mask) about the centre of its
/// pixels, as one step.
/// @param image The image.
/// @param with id (the active layer by default), x and y (pixels), scaleX and scaleY (percent, 100 by default;
/// negative flips), angle (degrees, clockwise).
/// @throws DocumentError If the layer isn't a pixel layer, is locked, or a scale is under 1 %.
void transformLayer(ImageDocument& image, const json& with, Window*) {
	settle(image);
	imaging::Document& document = image.canvas->document();
	const std::uint64_t id = layerOf(image, with);
	const imaging::Layer layer = *document.layer(id);
	if(layer.kind != imaging::LayerKind::pixel) throw DocumentError("Transform a pixel layer");
	if(layer.locks.position || layer.locks.all) throw DocumentError("The layer's position is locked");
	if(const auto why = document.paintable(layer.pixels)) throw DocumentError(*why);
	const auto scaleOf = [&](std::string_view key) {
		const double scale = std::clamp(numberOf(with, key, 100.0), -mostScale, mostScale);
		if(std::abs(scale) < leastScale) throw DocumentError("A layer scales by at least 1 %");
		return scale / 100.0;
	};
	const double scaleX = scaleOf("scaleX"), scaleY = scaleOf("scaleY");
	const double degrees = std::fmod(numberOf(with, "angle", 0.0), 360.0);
	const double moveX = std::clamp(numberOf(with, "x", 0.0), -farthest, farthest);
	const double moveY = std::clamp(numberOf(with, "y", 0.0), -farthest, farthest);
	if(scaleX == 1.0 && scaleY == 1.0 && degrees == 0.0 && moveX == 0.0 && moveY == 0.0) return;
	const auto centre = contentCentre(*document.image(layer.pixels));
	const double centreX = centre ? centre->first : document.width() / 2.0;
	const double centreY = centre ? centre->second : document.height() / 2.0;
	// From the new image back to the old: p = C + S⁻¹ R⁻¹ (p' - C - T), R clockwise with y down
	const double radians = degrees * std::numbers::pi / 180.0, cosine = std::cos(radians), sine = std::sin(radians);
	const double toX = centreX + moveX, toY = centreY + moveY;
	Sampling sampling;
	sampling.mapping = {cosine / scaleX, sine / scaleX,   centreX - (cosine * toX + sine * toY) / scaleX,
	                    -sine / scaleY,  cosine / scaleY, centreY - (-sine * toX + cosine * toY) / scaleY};
	const bool quarter = std::fmod(degrees, 90.0) == 0.0;
	const bool whole = moveX == std::round(moveX) && moveY == std::round(moveY);
	sampling.smooth = !(quarter && whole && std::abs(scaleX) == 1.0 && std::abs(scaleY) == 1.0);
	sampling.samples =
	    static_cast<int>(std::ceil(std::min(1.0 / std::min(std::abs(scaleX), std::abs(scaleY)), double{mostSamples})));
	const auto remap = [&](std::uint64_t from) {
		const imaging::Image& old = *document.image(from);
		return remapped(old, old.width(), old.height(), old.format(), sampling, image.limits);
	};
	auto pixels = remap(layer.pixels);
	auto mask = layer.mask != 0 ? remap(layer.mask) : nullptr;
	document.changeStructure("Free Transform", [&](imaging::Structure& changed) {
		changed.images[layer.pixels] = pixels;
		if(mask) changed.images[layer.mask] = mask;
	});
}

/// @brief The list of a layer's siblings in a structure.
/// @param structure The structure.
/// @param parent The parent's id, or 0 for the top.
/// @return The list, the bottom first.
[[nodiscard]] std::vector<std::uint64_t>& siblingsOf(imaging::Structure& structure, std::uint64_t parent) {
	return parent == 0 ? structure.root : structure.layers.at(parent).children;
}

/// @brief Takes a layer (a group with its layers) and its images out of a structure.
/// @param structure The structure.
/// @param id The layer.
void removeLayer(imaging::Structure& structure, std::uint64_t id) {
	const std::function<void(std::uint64_t)> erase = [&](std::uint64_t at) {
		const imaging::Layer layer = structure.layers.at(at);
		for(const std::uint64_t child : layer.children) erase(child);
		if(layer.pixels != 0) structure.images.erase(layer.pixels);
		if(layer.mask != 0) structure.images.erase(layer.mask);
		structure.layers.erase(at);
	};
	auto& list = siblingsOf(structure, structure.layers.at(id).parent);
	list.erase(std::ranges::find(list, id));
	erase(id);
}

/// @brief Puts a new pixel layer with an image into a structure.
/// @param structure The structure.
/// @param name Its name.
/// @param pixels Its image.
/// @param parent Its parent's id, or 0 for the top.
/// @param index Its place among its siblings, the bottom 0.
/// @return Its id; it is the active layer.
std::uint64_t placeLayer(imaging::Structure& structure, const std::string& name, std::shared_ptr<imaging::Image> pixels,
                         std::uint64_t parent, std::size_t index) {
	imaging::Layer layer;
	layer.name = name;
	layer.pixels = structure.nextId++;
	structure.images.emplace(layer.pixels, std::move(pixels));
	layer.id = structure.nextId++;
	layer.parent = parent;
	auto& list = siblingsOf(structure, parent);
	list.insert(list.begin() + static_cast<std::ptrdiff_t>(std::min(index, list.size())), layer.id);
	structure.active = layer.id;
	const std::uint64_t id = layer.id;
	structure.layers.emplace(id, std::move(layer));
	return id;
}

/// @brief A colour laid under another, straight alpha.
/// @param top The colour on top.
/// @param under The one under it.
/// @return Both.
[[nodiscard]] Pixel over(const Pixel& top, const Pixel& under) noexcept {
	const float alpha = top.alpha + under.alpha * (1.0f - top.alpha);
	if(alpha <= 0.0f) return {};
	const float below = under.alpha * (1.0f - top.alpha);
	return {(top.red * top.alpha + under.red * below) / alpha, (top.green * top.alpha + under.green * below) / alpha,
	        (top.blue * top.alpha + under.blue * below) / alpha, alpha};
}

/// @brief The layers of a structure composited into one image of a document's size, as the compositor shows them.
/// @param document The document whose images the structure names.
/// @param structure The structure (its root is what is composited).
/// @param under A colour laid under everything, if any.
/// @return The image.
[[nodiscard]] std::shared_ptr<imaging::Image>
flattened(const imaging::Document& document, const imaging::Structure& structure, const std::optional<Pixel>& under) {
	imaging::Document alone(document.width(), document.height(), document.limits(), document.format());
	alone.changeStructure("", [&](imaging::Structure& changed) { changed = structure; });
	imaging::Compositor compositor(alone);
	auto made =
	    std::make_shared<imaging::Image>(document.width(), document.height(), document.format(), document.limits());
	std::vector<Pixel> pixels(imaging::tilePixels);
	for(int row = 0; row < made->rows(); ++row)
		for(int column = 0; column < made->columns(); ++column) {
			compositor.composite({0, column, row}, pixels);
			if(under)
				for(Pixel& pixel : pixels) pixel = over(pixel, *under);
			if(std::ranges::all_of(pixels, [](const Pixel& pixel) { return pixel.alpha <= 0.0f; })) continue;
			imaging::Tile& tile = made->writableTile(column, row);
			for(int y = 0; y < tileSize; ++y)
				for(int x = 0; x < tileSize; ++x)
					tile.setPixel(x, y, pixels[static_cast<std::size_t>(y * tileSize + x)]);
		}
	return made;
}

/// @brief Refuses a layer locked against every change.
/// @param layer The layer.
/// @throws DocumentError If it is.
void checkUnlocked(const imaging::Layer& layer) {
	if(layer.locks.all) throw DocumentError("The layer \"" + layer.name + "\" is locked");
}

/// @brief mergeDown: merges a layer into the pixel layer below it, which keeps its settings and mask.
/// @param image The image.
/// @param with id (the active layer by default).
/// @throws DocumentError If there is no pixel layer below, or either is locked.
void mergeDown(ImageDocument& image, const json& with, Window*) {
	settle(image);
	imaging::Document& document = image.canvas->document();
	const std::uint64_t id = layerOf(image, with);
	imaging::Structure alone = document.structure();
	const imaging::Layer above = alone.layers.at(id);
	const auto& list = siblingsOf(alone, above.parent);
	const auto place = std::ranges::find(list, id);
	if(place == list.begin()) throw DocumentError("There is no layer below to merge into");
	const imaging::Layer below = alone.layers.at(*(place - 1));
	if(below.kind != imaging::LayerKind::pixel) throw DocumentError("A layer merges down into a pixel layer");
	checkUnlocked(above);
	if(const auto why = document.paintable(below.pixels)) throw DocumentError(*why);
	// The two on their own, the one below as it is before its settings: those stay on the merged layer
	alone.root = {below.id, above.id};
	imaging::Layer& base = alone.layers.at(below.id);
	base.parent = 0;
	base.visible = true;
	base.opacity = 1.0f;
	base.blend = imaging::BlendMode::normal;
	base.maskEnabled = false;
	base.clipped = false;
	alone.layers.at(above.id).parent = 0;
	auto merged = flattened(document, alone, std::nullopt);
	document.changeStructure("Merge Down", [&](imaging::Structure& changed) {
		removeLayer(changed, id);
		changed.images[below.pixels] = merged;
		changed.active = below.id;
	});
}

/// @brief mergeVisible: merges every visible layer into one, where the top one was; hidden layers stay.
/// @param image The image.
/// @throws DocumentError If nothing is visible, or a visible layer is locked.
void mergeVisible(ImageDocument& image, const json&, Window*) {
	settle(image);
	imaging::Document& document = image.canvas->document();
	const imaging::Structure& structure = document.structure();
	std::vector<std::uint64_t> shown;
	std::size_t hiddenBelow = 0, place = 0;
	for(const std::uint64_t id : structure.root) {
		const imaging::Layer& layer = structure.layers.at(id);
		if(!layer.visible) {
			++hiddenBelow;
			continue;
		}
		checkUnlocked(layer);
		shown.push_back(id);
		place = hiddenBelow;
	}
	if(shown.empty()) throw DocumentError("No layer is visible");
	auto merged = flattened(document, structure, std::nullopt);
	document.changeStructure("Merge Visible", [&](imaging::Structure& changed) {
		for(const std::uint64_t id : shown) removeLayer(changed, id);
		placeLayer(changed, "Merged", merged, 0, place);
	});
}

/// @brief flatten: merges every visible layer into one, Background, and discards the hidden ones.
/// @param image The image.
/// @param with background (a colour laid under everything; none keeps the transparency).
/// @throws DocumentError If the colour isn't one.
void flatten(ImageDocument& image, const json& with, Window*) {
	settle(image);
	imaging::Document& document = image.canvas->document();
	std::optional<Pixel> under;
	if(const std::string text = textOf(with, "background"); !text.empty()) {
		under = colorOf(text);
		if(!under) throw DocumentError("Not a colour");
	}
	auto merged = flattened(document, document.structure(), under);
	document.changeStructure("Flatten Image", [&](imaging::Structure& changed) {
		while(!changed.root.empty()) removeLayer(changed, changed.root.back());
		placeLayer(changed, "Background", merged, 0, 0);
	});
}

/// @brief deleteHidden: deletes every hidden layer (a hidden group with its layers).
/// @param image The image.
/// @throws DocumentError If one is locked.
void deleteHidden(ImageDocument& image, const json&, Window*) {
	settle(image);
	imaging::Document& document = image.canvas->document();
	const imaging::Structure& structure = document.structure();
	std::vector<std::uint64_t> hidden;
	const std::function<void(const std::vector<std::uint64_t>&)> visit = [&](const std::vector<std::uint64_t>& list) {
		for(const std::uint64_t id : list) {
			const imaging::Layer& layer = structure.layers.at(id);
			if(!layer.visible) {
				checkUnlocked(layer);
				hidden.push_back(id);
			} else {
				visit(layer.children);
			}
		}
	};
	visit(structure.root);
	if(hidden.empty()) return;
	document.changeStructure("Delete Hidden Layers", [&](imaging::Structure& changed) {
		for(const std::uint64_t id : hidden) removeLayer(changed, id);
		if(!changed.layers.contains(changed.active)) changed.active = changed.root.empty() ? 0 : changed.root.back();
	});
}

/// @brief An image's pixels inside the document's selection (all of them without one), as a new image.
/// @param document The document.
/// @param source The image, the document's size.
/// @return The new image.
[[nodiscard]] std::shared_ptr<imaging::Image> selectedOf(const imaging::Document& document,
                                                         const imaging::Image& source) {
	auto made = std::make_shared<imaging::Image>(source.width(), source.height(), source.format(), document.limits());
	const imaging::Image& selection = document.selection();
	for(int row = 0; row < source.rows(); ++row)
		for(int column = 0; column < source.columns(); ++column) {
			auto tile = source.tile({0, column, row});
			if(!tile) continue;
			const auto chosen = document.hasSelection() ? selection.tile({0, column, row}) : nullptr;
			if(document.hasSelection() && !chosen) continue;
			const auto uniform = chosen ? chosen->uniform() : std::nullopt;
			if(!chosen || (uniform && uniform->alpha >= 1.0f)) {
				made->setTile(column, row, std::move(tile));
				continue;
			}
			imaging::Tile& written = made->writableTile(column, row);
			for(int y = 0; y < tileSize; ++y)
				for(int x = 0; x < tileSize; ++x) {
					Pixel pixel = tile->pixel(x, y);
					pixel.alpha *= chosen->pixel(x, y).alpha;
					written.setPixel(x, y, pixel);
				}
		}
	return made;
}

/// @brief The active layer's pixels, when it is a pixel layer that can be painted.
/// @param document The document.
/// @return Their image's id.
/// @throws DocumentError Otherwise.
[[nodiscard]] std::uint64_t activePixels(const imaging::Document& document) {
	const imaging::Layer* layer = document.activeLayer();
	if(layer == nullptr || layer->kind != imaging::LayerKind::pixel) throw DocumentError("Choose a pixel layer");
	return layer->pixels;
}

/// @brief Clears the active layer's pixels inside the selection (all of them without one), as one step.
/// @param document The document.
/// @param name The step's name.
/// @throws DocumentError If the layer can't be painted.
void clearSelected(imaging::Document& document, const std::string& name) {
	const std::uint64_t target = activePixels(document);
	if(const auto why = document.paintable(target)) throw DocumentError(*why);
	const imaging::Image& pixels = *document.image(target);
	const imaging::Image& selection = document.selection();
	document.begin(name);
	try {
		for(int row = 0; row < pixels.rows(); ++row)
			for(int column = 0; column < pixels.columns(); ++column) {
				if(!pixels.tile({0, column, row})) continue;
				const auto chosen = document.hasSelection() ? selection.tile({0, column, row}) : nullptr;
				if(document.hasSelection() && !chosen) continue;
				const auto uniform = chosen ? chosen->uniform() : std::nullopt;
				if(!chosen || (uniform && uniform->alpha >= 1.0f)) {
					document.replaceTile(target, column, row, nullptr);
					continue;
				}
				imaging::Tile& tile = document.editTile(target, column, row);
				for(int y = 0; y < tileSize; ++y)
					for(int x = 0; x < tileSize; ++x) {
						Pixel pixel = tile.pixel(x, y);
						pixel.alpha *= 1.0f - chosen->pixel(x, y).alpha;
						tile.setPixel(x, y, pixel);
					}
			}
	} catch(...) {
		document.cancel();
		throw;
	}
	document.commit();
}

/// @brief copy: keeps the selection's pixels (all without one) of the active layer, or of every layer, for paste.
/// @param image The image.
/// @param with merged (every visible layer as shown).
/// @throws DocumentError If the active layer isn't a pixel layer.
void copy(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const imaging::Document& document = image.canvas->document();
	if(flagOf(with, "merged", false))
		image.clipboard = selectedOf(document, *flattened(document, document.structure(), std::nullopt));
	else image.clipboard = selectedOf(document, *document.image(activePixels(document)));
}

/// @brief cut: copies, then clears what it copied, as one step.
/// @param image The image.
/// @param window Unused.
/// @throws DocumentError If the active layer isn't a pixel layer or can't be painted.
void cut(ImageDocument& image, const json&, Window* window) {
	settle(image);
	if(const auto why = image.canvas->document().paintable(activePixels(image.canvas->document())))
		throw DocumentError(*why);
	copy(image, json::object(), window);
	clearSelected(image.canvas->document(), "Cut");
}

/// @brief paste: adds what copy or cut kept as a new layer above the active one: in its place when the image has the
/// size it had, otherwise centred.
/// @param image The image.
/// @param with name ("Pasted" by default).
/// @throws DocumentError If nothing was copied, or the name is too long.
void paste(ImageDocument& image, const json& with, Window*) {
	settle(image);
	if(!image.clipboard) throw DocumentError("Nothing has been copied");
	imaging::Document& document = image.canvas->document();
	const imaging::Image& kept = *image.clipboard;
	const std::string name = nameOf(with, "name", "Pasted");
	std::shared_ptr<imaging::Image> pixels;
	if(kept.width() == document.width() && kept.height() == document.height() && kept.format() == document.format()) {
		pixels = std::make_shared<imaging::Image>(kept.width(), kept.height(), kept.format(), document.limits());
		for(int row = 0; row < kept.rows(); ++row)
			for(int column = 0; column < kept.columns(); ++column)
				if(auto tile = kept.tile({0, column, row})) pixels->setTile(column, row, std::move(tile));
	} else {
		const double left = std::floor((document.width() - kept.width()) / 2.0);
		const double top = std::floor((document.height() - kept.height()) / 2.0);
		pixels = remapped(kept, document.width(), document.height(), document.format(),
		                  {{1.0, 0.0, -left, 0.0, 1.0, -top}}, document.limits());
	}
	document.changeStructure("Paste", [&](imaging::Structure& changed) {
		std::uint64_t parent = 0;
		std::size_t index = changed.root.size();
		if(const auto active = changed.layers.find(changed.active); active != changed.layers.end()) {
			parent = active->second.parent;
			const auto& list = siblingsOf(changed, parent);
			index = static_cast<std::size_t>(std::ranges::find(list, changed.active) - list.begin()) + 1;
		}
		placeLayer(changed, name, pixels, parent, index);
	});
}

/// @brief clear: clears the selection's pixels (all without one) of the active layer.
/// @param image The image.
/// @throws DocumentError If the active layer isn't a pixel layer or can't be painted.
void clear(ImageDocument& image, const json&, Window*) {
	settle(image);
	clearSelected(image.canvas->document(), "Clear");
}

} // namespace

std::vector<ImageOperation> transformOperations() {
	return {
	    {"canvasSize",
		 {"width", "height", "anchor"},
		 "Makes the canvas larger or smaller around the image, and forgets the history.",
		 canvasSize},
	    {"imageSize", {"width", "height"}, "Resamples the image to a size, and forgets the history.", imageSize},
	    {"rotateCanvas",
		 {"angle"},
		 "Turns the image clockwise by 90, 180 or 270 degrees (a quarter turn forgets the history).",
		 rotateCanvas},
	    {"flipCanvas", {"direction"}, "Mirrors the image horizontally or vertically.", flipCanvas},
	    {"transformLayer",
		 {"id", "x", "y", "scaleX", "scaleY", "angle"},
		 "Moves, scales, flips and turns a pixel layer about its centre.",
		 transformLayer},
	    {"mergeDown", {"id"}, "Merges a layer into the pixel layer below it.", mergeDown},
	    {"mergeVisible", {}, "Merges the visible layers into one.", mergeVisible},
	    {"flatten", {"background"}, "Merges every visible layer into one and discards the hidden ones.", flatten},
	    {"deleteHidden", {}, "Deletes the hidden layers.", deleteHidden},
	    {"copy", {"merged"}, "Keeps the selection's pixels of the active layer, or of every layer, for paste.", copy},
	    {"cut", {}, "Copies the selection's pixels of the active layer and clears them.", cut},
	    {"paste", {"name"}, "Adds what was copied as a new layer above the active one.", paste},
	    {"clear", {}, "Clears the selection's pixels of the active layer.", clear},
	};
}

} // namespace affresco::format::editing
