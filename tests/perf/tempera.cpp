/// @file
/// @brief The "tempera" workloads: a 20,000 by 20,000 project of modules/imaging opened (its manifest read and the
/// fitted view composited from the tiles it inflates), panned at 100% (a 1280 by 800 view composited at a new place),
/// and painted (a brush stroke across it, committed as a step). CPU only: the GPU compositor draws the same tiles.
/// The same stroke is also played as a screen plays it, on a 4,000 by 4,000 image of an "images" section: through the
/// image store and the "image" step's "pointer" operation, then the snapshot the screen's state receives. On that image,
/// a painted layer is also turned and scaled by the "transformLayer" operation, then the step undone.
///
/// With AFFRESCOUI_PERF_BUDGETS set, a run over its budget fails: open 8 s, a pan 60 ms, a stroke 20 ms, a stroke
/// through the store 20 ms, a transform 2 s (measured on macOS: 1.1 s, 15 ms, 1 ms, 1.2 ms and 0.66 s).

#include "Bench.hpp"
#include "ImageDocuments.hpp"
#include "ImageOperations.hpp"

#include <affresco/imaging/Brush.hpp>
#include <affresco/imaging/Canvas.hpp>
#include <affresco/imaging/Composite.hpp>
#include <affresco/imaging/Project.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

namespace img = affresco::imaging;
using img::Document, img::Pixel, img::PixelFormat, img::Tile, img::tileSize;

constexpr int side = 20'000;                      ///< The image's width and height.
constexpr int viewWidth = 1280, viewHeight = 800; ///< The view, in surface pixels.
constexpr double openBudget = 8000.0;             ///< Opening, in milliseconds.
constexpr double panBudget = 60.0;                ///< A pan, in milliseconds.
constexpr double paintBudget = 20.0;              ///< A stroke, in milliseconds.
constexpr double storeBudget = 20.0;              ///< A stroke through the store, in milliseconds.
constexpr double transformBudget = 2000.0;        ///< A transform and its undo, in milliseconds.
constexpr int storeSide = 4000;                   ///< The store's image's width and height.

/// @brief Fails the run if a step took longer than its budget and budgets are on.
/// @param name The workload.
/// @param start When the step started.
/// @param budget Its budget, in milliseconds.
void checkBudget(const std::string& name, std::chrono::steady_clock::time_point start, double budget) {
	const double taken = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	if(std::getenv("AFFRESCOUI_PERF_BUDGETS") != nullptr && taken > budget)
		throw std::runtime_error(name + ": " + std::to_string(taken) + " ms, over its budget of " +
		                         std::to_string(budget) + " ms");
}

/// @brief The project file: a background filled with four colours in a checker of tiles (tiles shared, so building
/// it takes little memory), and a transparent layer above it to paint on.
/// @return The file's bytes.
std::shared_ptr<const std::vector<std::uint8_t>> projectFile() {
	Document document(side, side);
	const std::array<std::shared_ptr<const Tile>, 4> tiles{Tile::filled(PixelFormat::rgba8, {0.9f, 0.8f, 0.6f, 1.0f}),
	                                                       Tile::filled(PixelFormat::rgba8, {0.2f, 0.4f, 0.7f, 1.0f}),
	                                                       Tile::filled(PixelFormat::rgba8, {0.3f, 0.6f, 0.3f, 1.0f}),
	                                                       Tile::filled(PixelFormat::rgba8, {0.7f, 0.3f, 0.2f, 1.0f})};
	const auto background = std::make_shared<img::Image>(side, side, PixelFormat::rgba8, document.limits());
	for(int row = 0; row < background->rows(); ++row)
		for(int column = 0; column < background->columns(); ++column)
			background->setTile(column, row, tiles[static_cast<std::size_t>((column + row) % 4)]);
	static_cast<void>(document.addImageLayer("Background", background));
	static_cast<void>(document.addPixelLayer("Paint"));
	return std::make_shared<const std::vector<std::uint8_t>>(img::saveProject(document, std::nullopt));
}

/// @brief An opened project and what its workloads reuse.
struct Opened {
	std::shared_ptr<const std::vector<std::uint8_t>> file = projectFile(); ///< The project file.
	std::unique_ptr<Document> document;                                    ///< It, opened.
	std::unique_ptr<img::Compositor> compositor;                           ///< The document's compositor.
	std::vector<Pixel> pixels = std::vector<Pixel>(img::tilePixels);       ///< One tile, composited.
	std::size_t next = 0;                                                  ///< Picks the next place.

	/// @brief Opens the file and composites the view that fits the whole image.
	void open() {
		compositor.reset();
		document = img::loadProject(file, img::Limits{}).document;
		compositor = std::make_unique<img::Compositor>(*document);
		const double zoom = std::min(static_cast<double>(viewWidth) / side, static_cast<double>(viewHeight) / side);
		const img::Image& image = *document->image(document->activeLayer()->pixels);
		const int level = img::canvasLevel(zoom, image.levels());
		for(int row = 0; row < image.rows(level); ++row)
			for(int column = 0; column < image.columns(level); ++column)
				compositor->composite({level, column, row}, pixels);
	}

	/// @brief Composites a 100% view at a new place: the tiles it covers.
	void pan() {
		next = (next + 7919) % 100'003; // a prime stride: a new place every time
		const int x = static_cast<int>(next * 37 % static_cast<std::size_t>(side - viewWidth));
		const int y = static_cast<int>(next * 13 % static_cast<std::size_t>(side - viewHeight));
		for(int row = y / tileSize; row <= (y + viewHeight - 1) / tileSize; ++row)
			for(int column = x / tileSize; column <= (x + viewWidth - 1) / tileSize; ++column)
				compositor->composite({0, column, row}, pixels);
	}

	/// @brief Paints a 40-pixel stroke 1,000 pixels long, at a new place, on the top layer, and commits it.
	void paint() {
		next = (next + 7919) % 100'003;
		const auto x = static_cast<float>(next * 37 % static_cast<std::size_t>(side - 1200));
		const auto y = static_cast<float>(next * 13 % static_cast<std::size_t>(side - 200)) + 100.0f;
		img::BrushTip tip;
		tip.size = 40.0f;
		tip.normalize(document->limits());
		img::Stroke stroke(*document, document->activeLayer()->pixels, tip, img::StrokeMode::paint,
		                   {0.1f, 0.1f, 0.1f, 1.0f}, "Brush");
		for(int i = 0; i <= 100; ++i) stroke.add({.x = x + static_cast<float>(i) * 10.0f, .y = y, .pressure = 1.0f});
		stroke.finish();
	}
};

/// @brief An image of an "images" section, as a screen's store makes it, painted through the "pointer" operation.
struct Stored {
	format::editing::ImageDocuments images;          ///< The store.
	format::editing::ImageDocument* image = nullptr; ///< Its image.
	/// @brief The "pointer" operation, as the "image" step runs it.
	const format::editing::ImageOperation* pointer = format::editing::findImageOperation("pointer");
	std::size_t next = 0; ///< Picks the next place.

	/// @brief Makes the image, with a brush as its tool.
	Stored() {
		const json entry = {{"width", storeSide},
		                    {"height", storeSide},
		                    {"into", "state.picture"},
		                    {"tools", {{"brush", {{"gesture", "paint"}, {"size", 40}, {"hardness", 100}}}}}};
		image = &images.image("picture", entry, "images.picture");
		if(pointer == nullptr) throw std::runtime_error("no \"pointer\" operation");
	}

	/// @brief Plays a 40-pixel stroke 1,000 pixels long at a new place, as one "pointer" step, then takes the
	/// snapshot the screen's state receives.
	void paint() {
		next = (next + 7919) % 100'003;
		const auto x = static_cast<double>(next * 37 % static_cast<std::size_t>(storeSide - 1200));
		const auto y = static_cast<double>(next * 13 % static_cast<std::size_t>(storeSide - 200)) + 100.0;
		json points = json::array({{{"phase", "press"}, {"x", x}, {"y", y}}});
		for(int i = 1; i <= 100; ++i) points.push_back({{"phase", "move"}, {"x", x + i * 10.0}, {"y", y}});
		points.push_back({{"phase", "release"}, {"x", x + 1000.0}, {"y", y}});
		pointer->run(*image, {{"points", points}}, nullptr);
		if(format::editing::snapshot(*image).empty()) throw std::runtime_error("an empty snapshot");
	}

	/// @brief Turns the painted layer by 33 degrees at 80% of its size, as one "transformLayer" step, then undoes it
	/// (painting a stroke first when nothing is painted yet).
	void transform() {
		if(next == 0) paint();
		run("transformLayer", {{"scaleX", 80}, {"scaleY", 80}, {"angle", 33}});
		run("undo", json::object());
	}

	/// @brief Runs an operation of the "image" step on the image.
	/// @param name The operation.
	/// @param with Its arguments.
	void run(const std::string& name, const json& with) {
		const format::editing::ImageOperation* operation = format::editing::findImageOperation(name);
		if(operation == nullptr) throw std::runtime_error("no \"" + name + "\" operation");
		operation->run(*image, with, nullptr);
	}
};

} // namespace

std::vector<Workload> temperaWorkloads(Context& /*context*/) {
	const auto opened = std::make_shared<Opened>();
	opened->open();
	std::vector<Workload> workloads;
	workloads.push_back({.name = "tempera/open-20k", .unit = "ms", .runs = 3, .step = [opened] {
		                     const auto start = std::chrono::steady_clock::now();
		                     opened->open();
		                     checkBudget("tempera/open-20k", start, openBudget);
	                     }});
	workloads.push_back({.name = "tempera/pan-20k", .unit = "ms", .runs = 21, .step = [opened] {
		                     const auto start = std::chrono::steady_clock::now();
		                     opened->pan();
		                     checkBudget("tempera/pan-20k", start, panBudget);
	                     }});
	workloads.push_back({.name = "tempera/paint-20k", .unit = "ms", .runs = 21, .step = [opened] {
		                     const auto start = std::chrono::steady_clock::now();
		                     opened->paint();
		                     checkBudget("tempera/paint-20k", start, paintBudget);
	                     }});
	const auto stored = std::make_shared<Stored>();
	workloads.push_back({.name = "tempera/paint-store", .unit = "ms", .runs = 21, .step = [stored] {
		                     const auto start = std::chrono::steady_clock::now();
		                     stored->paint();
		                     checkBudget("tempera/paint-store", start, storeBudget);
	                     }});
	workloads.push_back({.name = "tempera/transform-store", .unit = "ms", .runs = 9, .step = [stored] {
		                     const auto start = std::chrono::steady_clock::now();
		                     stored->transform();
		                     checkBudget("tempera/transform-store", start, transformBudget);
	                     }});
	return workloads;
}

} // namespace affresco::bench
