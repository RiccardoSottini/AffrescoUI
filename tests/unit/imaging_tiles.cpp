/// @file
/// @brief The imaging module's tile store (affresco/imaging/Image.hpp) and its undo by tile differences
/// (affresco/imaging/History.hpp): sparse tiles, copy on write, half floats, lazy sources and their cache, mip levels,
/// versions, and steps that keep only the tiles they changed within a memory budget.

#include <affresco/imaging/History.hpp>
#include <affresco/imaging/Image.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace affresco::imaging;

namespace {

/// @brief A source that paints each tile one colour from its place, and counts its loads.
class CountingSource : public TileSource {
public:
	/// @brief Gives levels up to and including a level.
	/// @param levels The highest level it gives.
	explicit CountingSource(int levels) : levels_(levels) {}
	/// @brief See TileSource.
	[[nodiscard]] bool providesLevel(int level) const override { return level <= levels_; }
	/// @brief See TileSource.
	[[nodiscard]] std::shared_ptr<const Tile> load(const TileKey& key) override {
		++loads;
		const float shade = static_cast<float>((key.column + key.row) % 4) / 4.0f;
		return Tile::filled(PixelFormat::rgba8, {shade, 0.5f, static_cast<float>(key.level) / 8.0f, 1.0f});
	}
	int loads = 0; ///< How many tiles it loaded.

private:
	int levels_; ///< See the constructor.
};

/// @brief An image id resolver over a map.
/// @param images The images by id.
[[nodiscard]] std::function<Image*(std::uint64_t)> resolver(std::map<std::uint64_t, Image*>& images) {
	return [&images](std::uint64_t id) -> Image* {
		const auto found = images.find(id);
		return found != images.end() ? found->second : nullptr;
	};
}

/// @brief Paints a square of pixels as one step, recording its tiles.
/// @param image The image (id 1).
/// @param left The square's left.
/// @param top Its top.
/// @param side Its side.
/// @param pixel The colour.
/// @return The step.
[[nodiscard]] HistoryStep paint(Image& image, int left, int top, int side, const Pixel& pixel) {
	TileRecorder recorder;
	for(int y = top; y < top + side; ++y)
		for(int x = left; x < left + side; ++x) {
			recorder.touch(1, image, x / tileSize, y / tileSize);
			image.setPixel(x, y, pixel);
		}
	HistoryStep step;
	step.name = "Paint";
	step.tiles = recorder.finish([&image](std::uint64_t) -> const Image* { return &image; });
	return step;
}

} // namespace

TEST_CASE("imaging tiles: half floats convert both ways") {
	for(const float value : {0.0f, 1.0f, -2.5f, 0.333251953125f, 65504.0f, 6.103515625e-05f, 5.9604645e-08f})
		CHECK(floatFromHalf(halfFromFloat(value)) == value);
	CHECK(std::isinf(floatFromHalf(halfFromFloat(1e6f))));
	CHECK(std::isnan(floatFromHalf(halfFromFloat(std::nanf("")))));
	CHECK(floatFromHalf(halfFromFloat(0.1f)) == doctest::Approx(0.1f).epsilon(1e-3));
	CHECK(byteFromUnit(-1.0f) == 0);
	CHECK(byteFromUnit(2.0f) == 255);
	CHECK(byteFromUnit(0.5f) == 128);
}

TEST_CASE("imaging tiles: a tile holds each format") {
	Tile color(PixelFormat::rgba8), wide(PixelFormat::rgba16f), mask(PixelFormat::mask8);
	CHECK(color.bytes().size() == tilePixels * 4);
	CHECK(wide.bytes().size() == tilePixels * 8);
	CHECK(mask.bytes().size() == tilePixels);
	CHECK(color.empty());
	color.setPixel(3, 7, {1.0f, 0.5f, 0.0f, 1.0f});
	CHECK(color.pixel(3, 7).green == doctest::Approx(128.0f / 255.0f));
	CHECK_FALSE(color.empty());
	CHECK_FALSE(color.uniform());
	wide.setPixel(255, 255, {4.0f, -1.0f, 0.25f, 0.5f});
	CHECK(wide.pixel(255, 255) == Pixel{4.0f, -1.0f, 0.25f, 0.5f});
	mask.setPixel(0, 0, {0.0f, 0.0f, 0.0f, 1.0f});
	CHECK(mask.pixel(0, 0).alpha == 1.0f);
	const auto filled = Tile::filled(PixelFormat::rgba8, {0.0f, 0.0f, 1.0f, 1.0f});
	REQUIRE(filled->uniform());
	CHECK(filled->uniform()->blue == 1.0f);
	CHECK(convertTile(mask, PixelFormat::rgba8)->pixel(0, 0) == Pixel{1.0f, 1.0f, 1.0f, 1.0f});
	CHECK_THROWS_AS(Tile(PixelFormat::rgba8, std::vector<std::uint8_t>(10)), std::invalid_argument);
}

TEST_CASE("imaging tiles: an image is sparse, bounded and copy-on-write") {
	Image image(1000, 600, PixelFormat::rgba8);
	CHECK(image.columns() == 4);
	CHECK(image.rows() == 3);
	CHECK(image.levels() == 3);
	CHECK(image.tile({0, 1, 1}) == nullptr);
	CHECK(image.memoryBytes() == 0);
	image.setPixel(300, 300, {1.0f, 0.0f, 0.0f, 1.0f});
	image.setPixel(5000, 5, {1.0f, 0.0f, 0.0f, 1.0f}); // outside: nothing
	CHECK(image.writtenTiles().size() == 1);
	CHECK(image.pixel(300, 300).red == 1.0f);
	CHECK(image.pixel(-1, 0) == Pixel{});

	const std::shared_ptr<const Tile> kept = image.tile({0, 1, 1});
	const Image copy = image;                           // shares the tile
	image.setPixel(301, 300, {0.0f, 1.0f, 0.0f, 1.0f}); // copies it first
	CHECK(image.tile({0, 1, 1}) != kept);
	CHECK(kept->pixel(301 % tileSize, 300 % tileSize).alpha == 0.0f);
	CHECK(copy.pixel(301, 300).alpha == 0.0f);
	CHECK(copy.tile({0, 1, 1}) == kept);

	CHECK_THROWS_AS(Image(0, 10, PixelFormat::rgba8), std::invalid_argument);
	CHECK_THROWS_AS(Image(40000, 10, PixelFormat::rgba8), std::invalid_argument);
	Limits small;
	small.pixels = 1000;
	CHECK_THROWS_AS(Image(100, 100, PixelFormat::rgba8, small), std::invalid_argument);
	CHECK_THROWS_AS(static_cast<void>(image.writableTile(4, 0)), std::out_of_range);
	CHECK_THROWS_AS(image.setTile(0, 0, std::make_shared<Tile>(PixelFormat::mask8)), std::invalid_argument);
}

TEST_CASE("imaging tiles: versions change with the tile and its mips") {
	Image image(2048, 2048, PixelFormat::rgba8);
	CHECK(image.levels() == 4);
	CHECK(image.version({0, 5, 5}) == 0);
	image.setPixel(5 * tileSize, 5 * tileSize, {1.0f, 1.0f, 1.0f, 1.0f});
	const std::uint64_t first = image.version({0, 5, 5});
	CHECK(first != 0);
	CHECK(image.version({1, 2, 2}) != 0);
	CHECK(image.version({3, 0, 0}) != 0);
	CHECK(image.version({1, 0, 0}) == 0);
	image.setPixel(5 * tileSize + 1, 5 * tileSize, {1.0f, 1.0f, 1.0f, 1.0f});
	CHECK(image.version({0, 5, 5}) > first);
}

TEST_CASE("imaging tiles: mips are alpha-weighted and made again after a write") {
	Image image(512, 512, PixelFormat::rgba8);
	image.writableTile(0, 0) = *Tile::filled(PixelFormat::rgba8, {1.0f, 0.0f, 0.0f, 1.0f});
	const auto mip = image.tile({1, 0, 0});
	REQUIRE(mip);
	CHECK(mip->pixel(10, 10) == Pixel{1.0f, 0.0f, 0.0f, 1.0f});
	CHECK(mip->pixel(200, 10).alpha == 0.0f); // the transparent tile to its right
	// Half covered: the colour stays red, not darkened by the transparent pixels; the alpha halves
	Tile& tile = image.writableTile(0, 0);
	for(int y = 0; y < tileSize; ++y) tile.setPixel(1, y, {});
	const auto again = image.tile({1, 0, 0});
	CHECK(again != mip);
	CHECK(again->pixel(0, 10).red == 1.0f);
	CHECK(again->pixel(0, 10).alpha == doctest::Approx(0.5f).epsilon(0.01));
	CHECK(image.tile({2, 0, 0}) == nullptr); // beyond the levels
}

TEST_CASE("imaging tiles: a source loads lazily, within the cache budget") {
	Limits limits;
	limits.cacheBytes = 8 * tilePixels * 4; // eight tiles
	const auto source = std::make_shared<CountingSource>(2);
	Image image(20000, 20000, PixelFormat::rgba8, limits, source);
	CHECK(image.levels() == 8);
	CHECK(source->loads == 0);
	CHECK(image.pixel(10, 10).green == doctest::Approx(0.5f).epsilon(0.01));
	CHECK(source->loads == 1);
	static_cast<void>(image.tile({0, 0, 0}));
	CHECK(source->loads == 1); // cached
	for(int column = 0; column < 20; ++column) static_cast<void>(image.tile({0, column, 3}));
	CHECK(image.cachedBytes() <= limits.cacheBytes);
	static_cast<void>(image.tile({0, 0, 0}));
	CHECK(source->loads == 22); // evicted, loaded again
	// Levels it gives come from it; above, made from below
	CHECK(image.tile({2, 1, 1})->pixel(0, 0).blue == doctest::Approx(0.25f).epsilon(0.01));
	CHECK(image.tile({3, 0, 0}));
	// A written tile's mips stop coming from the source
	image.setPixel(0, 0, {0.0f, 0.0f, 0.0f, 0.0f});
	CHECK(image.tile({1, 0, 0})->pixel(0, 0).blue == 0.0f);
	CHECK(image.tile({0, 0, 0})->pixel(0, 0).alpha == 0.0f);
	CHECK(image.tile({0, 0, 0})->pixel(1, 0).alpha == 1.0f); // the rest of the source tile stayed
}

TEST_CASE("imaging tiles: a step keeps only the tiles it changed, and undoes and redoes them") {
	Image image(1024, 1024, PixelFormat::rgba8);
	std::map<std::uint64_t, Image*> images{{1, &image}};
	History history;
	history.push(paint(image, 10, 10, 20, {1.0f, 0.0f, 0.0f, 1.0f}));
	CHECK(history.steps().back().tiles.size() == 1);
	history.push(paint(image, 250, 250, 20, {0.0f, 0.0f, 1.0f, 1.0f})); // across four tiles
	CHECK(history.steps().back().tiles.size() == 4);
	CHECK(image.pixel(255, 255).blue == 1.0f);

	const HistoryStep* step = history.undo();
	REQUIRE(step);
	applyTiles(*step, false, resolver(images));
	CHECK(image.pixel(255, 255).alpha == 0.0f);
	CHECK(image.pixel(15, 15).red == 1.0f);
	CHECK(history.canRedo());
	step = history.redo();
	REQUIRE(step);
	applyTiles(*step, true, resolver(images));
	CHECK(image.pixel(255, 255).blue == 1.0f);

	// Undo twice, then a new step drops the one undone
	applyTiles(*history.undo(), false, resolver(images));
	applyTiles(*history.undo(), false, resolver(images));
	CHECK(image.pixel(15, 15).alpha == 0.0f);
	CHECK(history.undo() == nullptr);
	history.push(paint(image, 600, 600, 4, {0.0f, 1.0f, 0.0f, 1.0f}));
	CHECK(history.steps().size() == 1);
	CHECK_FALSE(history.canRedo());
}

TEST_CASE("imaging tiles: a stroke that touches a tile without changing it keeps nothing") {
	Image image(256, 256, PixelFormat::rgba8);
	TileRecorder recorder;
	recorder.touch(1, image, 0, 0);
	CHECK(recorder.touched(1, 0, 0));
	CHECK_FALSE(recorder.touched(1, 0, 1));
	CHECK(recorder.finish([&image](std::uint64_t) -> const Image* { return &image; }).empty());
}

TEST_CASE("imaging tiles: the history drops its oldest steps beyond its budget") {
	Image image(4096, 256, PixelFormat::rgba8);
	History history(3 * (tilePixels * 4 + 1024), 100); // about three tiles
	for(int index = 0; index < 8; ++index) history.push(paint(image, index * tileSize, 0, 4, {1.0f, 1.0f, 1.0f, 1.0f}));
	CHECK(history.memoryBytes() <= 3 * (tilePixels * 4 + 1024));
	CHECK(history.steps().size() == 3);
	CHECK(history.dropped() == 5);
	CHECK(history.position() == 3);
	History counted(std::size_t{1} << 30, 2);
	for(int index = 0; index < 5; ++index)
		counted.push(paint(image, 0, 0, 1, {0.1f * static_cast<float>(index), 0.0f, 0.0f, 1.0f}));
	CHECK(counted.steps().size() == 2);
	// One step beyond the whole budget is still kept, so it can be undone
	History tiny(10, 100);
	tiny.push(paint(image, 0, 0, 1, {1.0f, 0.0f, 0.0f, 1.0f}));
	CHECK(tiny.steps().size() == 1);
}

TEST_CASE("imaging tiles: undo restores tiles that came from a source") {
	const auto source = std::make_shared<CountingSource>(0);
	Image image(1024, 1024, PixelFormat::rgba8, {}, source);
	std::map<std::uint64_t, Image*> images{{1, &image}};
	const Pixel before = image.pixel(5, 5);
	const HistoryStep step = paint(image, 0, 0, 10, {0.0f, 0.0f, 0.0f, 0.0f});
	CHECK(image.pixel(5, 5).alpha == 0.0f);
	applyTiles(step, false, resolver(images));
	CHECK(image.pixel(5, 5) == before);
	CHECK(step.memoryBytes() >= 2 * tilePixels * 4);
}
