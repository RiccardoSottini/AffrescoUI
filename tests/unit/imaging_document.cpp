/// @file
/// @brief The imaging module's documents: layers, groups, masks, clipping, adjustment layers, the CPU compositor and
/// the history of both pixels and structure.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Composite.hpp"
#include "affresco/imaging/Document.hpp"

#include <doctest/doctest.h>

#include <cmath>

using namespace affresco::imaging;

namespace {

/// @brief Fills a whole layer's image with one colour, as one step.
/// @param document The document.
/// @param image The image.
/// @param pixel The colour.
void fill(Document& document, std::uint64_t image, const Pixel& pixel) {
	document.begin("Fill");
	const Image& target = *document.image(image);
	for(int row = 0; row < target.rows(); ++row)
		for(int column = 0; column < target.columns(); ++column)
			document.replaceTile(image, column, row, Tile::filled(target.format(), pixel));
	document.commit();
}

/// @brief The flattened document's colour at a pixel.
/// @param document The document.
/// @param x Across.
/// @param y Down.
[[nodiscard]] Pixel flatAt(const Document& document, int x, int y) {
	const auto bytes = flatten(document, 0, x, y, 1, 1);
	return {unitFromByte(bytes[0]), unitFromByte(bytes[1]), unitFromByte(bytes[2]), unitFromByte(bytes[3])};
}

/// @brief Whether two colours are within 2/255.
/// @param left One.
/// @param right The other.
[[nodiscard]] bool near(const Pixel& left, const Pixel& right) {
	constexpr float tolerance = 2.0f / 255.0f;
	return std::abs(left.red - right.red) <= tolerance && std::abs(left.green - right.green) <= tolerance &&
	       std::abs(left.blue - right.blue) <= tolerance && std::abs(left.alpha - right.alpha) <= tolerance;
}

} // namespace

TEST_CASE("imaging document: layers are added, listed, moved, deleted, and undone") {
	Document document(300, 200);
	const std::uint64_t first = document.structure().active;
	const std::uint64_t second = document.addPixelLayer("Paint");
	const std::uint64_t group = document.addGroup("Group");
	CHECK(document.listed().front().first == group);
	document.moveLayer(first, group, 0);
	CHECK(document.layer(first)->parent == group);
	CHECK(depthOf(document.structure(), first) == 1);
	CHECK(document.listed().size() == 3);
	document.deleteLayer(group);
	CHECK(document.layer(first) == nullptr);
	CHECK(document.image(document.layer(second)->pixels) != nullptr);
	CHECK(document.undo());
	CHECK(document.layer(first) != nullptr);
	CHECK(document.undo());
	CHECK(document.layer(first)->parent == 0);
	document.goTo(0);
	CHECK(document.structure().layers.size() == 1);
	document.goTo(document.history().steps().size());
	CHECK(document.structure().layers.size() == 1); // the group went with the first layer
	CHECK_THROWS_AS(document.moveLayer(first, second, 0), affresco::DocumentError);
}

TEST_CASE("imaging document: groups nest at most two deep, layers at most to the limit") {
	Limits limits;
	limits.layers = 4;
	Document document(64, 64, limits);
	const std::uint64_t outer = document.addGroup("Outer");
	const std::uint64_t inner = document.addGroup("Inner");
	document.moveLayer(inner, outer, 0);
	const std::uint64_t third = document.addGroup("Third");
	CHECK_THROWS_AS(document.moveLayer(third, inner, 0), affresco::DocumentError);
	CHECK_THROWS_AS(document.moveLayer(outer, outer, 0), affresco::DocumentError);
	CHECK_THROWS_AS(document.addPixelLayer("Fifth"), affresco::DocumentError);
	CHECK(document.structure().layers.size() == 4);
}

TEST_CASE("imaging document: the compositor blends, masks, clips and adjusts") {
	Document document(300, 40);
	const std::uint64_t base = document.activeLayer()->pixels;
	fill(document, base, {1.0f, 0.5f, 0.0f, 1.0f});
	const std::uint64_t top = document.addPixelLayer("Top");
	fill(document, document.layer(top)->pixels, {0.5f, 0.5f, 0.5f, 1.0f});
	document.changeLayer(top, "Blend Mode", [](Layer& layer) { layer.blend = BlendMode::multiply; });
	CHECK(near(flatAt(document, 10, 10), {0.5f, 0.25f, 0.0f, 1.0f}));
	document.changeLayer(top, "Opacity", [](Layer& layer) { layer.opacity = 0.0f; });
	CHECK(near(flatAt(document, 10, 10), {1.0f, 0.5f, 0.0f, 1.0f}));
	document.undo();

	// A mask hiding the right half's tiles: only the first tile shows the layer
	document.addMask(top);
	const std::uint64_t mask = document.layer(top)->mask;
	document.begin("Mask");
	document.replaceTile(mask, 1, 0, nullptr);
	document.commit();
	CHECK(near(flatAt(document, 10, 10), {0.5f, 0.25f, 0.0f, 1.0f}));
	CHECK(near(flatAt(document, 290, 10), {1.0f, 0.5f, 0.0f, 1.0f}));

	// An inverting adjustment clipped to the masked layer inverts only where it shows
	const std::uint64_t adjustment = document.addAdjustment("Invert", Adjustment{});
	document.changeLayer(adjustment, "Clipping Mask", [](Layer& layer) { layer.clipped = true; });
	CHECK(near(flatAt(document, 10, 10), {0.5f, 0.75f, 1.0f, 1.0f}));
	CHECK(near(flatAt(document, 290, 10), {1.0f, 0.5f, 0.0f, 1.0f}));
	document.changeLayer(top, "Hide", [](Layer& layer) { layer.visible = false; });
	CHECK(near(flatAt(document, 10, 10), {1.0f, 0.5f, 0.0f, 1.0f}));
}

TEST_CASE("imaging document: a group composites its layers first, then with its own mode") {
	Document document(64, 64);
	fill(document, document.activeLayer()->pixels, {0.5f, 0.5f, 0.5f, 1.0f});
	const std::uint64_t group = document.addGroup("Group");
	document.changeLayer(group, "Blend Mode", [](Layer& layer) { layer.blend = BlendMode::multiply; });
	const std::uint64_t inside = document.addPixelLayer("Inside");
	document.moveLayer(inside, group, 0);
	fill(document, document.layer(inside)->pixels, {1.0f, 0.0f, 1.0f, 1.0f});
	const std::uint64_t screen = document.addPixelLayer("Screen");
	document.moveLayer(screen, group, 1);
	fill(document, document.layer(screen)->pixels, {0.0f, 1.0f, 0.0f, 1.0f});
	document.changeLayer(screen, "Blend Mode", [](Layer& layer) { layer.blend = BlendMode::screen; });
	// Inside the group: magenta screened with green is white; multiplied onto grey, grey
	CHECK(near(flatAt(document, 5, 5), {0.5f, 0.5f, 0.5f, 1.0f}));
	document.changeLayer(group, "Blend Mode", [](Layer& layer) { layer.blend = BlendMode::normal; });
	CHECK(near(flatAt(document, 5, 5), {1.0f, 1.0f, 1.0f, 1.0f}));
}

TEST_CASE("imaging document: pixel steps undo, cancel, and dragged sliders make one step") {
	Document document(64, 64);
	const std::uint64_t pixels = document.activeLayer()->pixels;
	document.begin("Brush");
	document.editTile(pixels, 0, 0).setPixel(3, 3, {1.0f, 0.0f, 0.0f, 1.0f});
	document.commit();
	document.begin("Brush");
	document.editTile(pixels, 0, 0).setPixel(3, 3, {0.0f, 1.0f, 0.0f, 1.0f});
	document.cancel();
	CHECK(document.image(pixels)->pixel(3, 3).red == 1.0f);
	CHECK(document.history().steps().size() == 1); // the brush (a new document has no steps)
	const std::uint64_t layer = document.structure().active;
	for(const float opacity : {0.9f, 0.8f, 0.7f})
		document.changeLayer(layer, "Opacity", [&](Layer& changed) { changed.opacity = opacity; }, true);
	CHECK(document.history().steps().size() == 2);
	document.undo();
	CHECK(document.activeLayer()->opacity == 1.0f);
	document.undo();
	CHECK(document.image(pixels)->pixel(3, 3).alpha == 0.0f);
	document.redo();
	CHECK(document.image(pixels)->pixel(3, 3).red == 1.0f);
	CHECK_THROWS_AS(static_cast<void>(document.editTile(pixels, 0, 0)), std::logic_error);
}

TEST_CASE("imaging document: locks refuse what they lock") {
	Document document(64, 64);
	const std::uint64_t layer = document.structure().active;
	document.changeLayer(layer, "Lock", [](Layer& changed) { changed.locks.pixels = true; });
	CHECK(document.paintable(document.layer(layer)->pixels).has_value());
	document.changeLayer(layer, "Lock All", [](Layer& changed) { changed.locks.all = true; });
	document.changeLayer(layer, "Rename", [](Layer& changed) { changed.name = "Renamed"; });
	CHECK(document.layer(layer)->name == "Layer 1");
	CHECK_THROWS_AS(document.deleteLayer(layer), affresco::DocumentError);
	CHECK_FALSE(document.paintable(Document::selectionId).has_value());
}

TEST_CASE("imaging document: thumbnails fit the image into a square") {
	Document document(512, 256);
	fill(document, document.activeLayer()->pixels, {0.0f, 0.0f, 1.0f, 1.0f});
	const auto square = thumbnail(document, 0, 32);
	CHECK(square.size() == 32 * 32 * 4);
	CHECK(square[(2 * 32 + 5) * 4 + 3] == 0);     // above the image: transparent
	CHECK(square[(16 * 32 + 5) * 4 + 2] == 255);  // within it: blue
	CHECK(thumbnail(document, document.activeLayer()->pixels, 1000).size() == 256 * 256 * 4);
}
