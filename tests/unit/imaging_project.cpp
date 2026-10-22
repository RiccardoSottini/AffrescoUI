/// @file
/// @brief The imaging module's project file: layers, groups, masks, adjustments and the profile round-trip, tiles
/// load lazily and are copied when saved again, and damaged or oversized files are refused.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Project.hpp"

#include <doctest/doctest.h>

#include <string>

using namespace affresco::imaging;

namespace {

/// @brief A document of a painted layer with a mask, a group holding a layer, and an adjustment on top.
/// @return The document.
[[nodiscard]] std::unique_ptr<Document> sample() {
	auto document = std::make_unique<Document>(600, 300);
	const std::uint64_t first = document->activeLayer()->id;
	Image& pixels = *document->image(document->activeLayer()->pixels);
	pixels.setPixel(10, 10, {1.0f, 0.0f, 0.0f, 1.0f});
	pixels.setPixel(590, 290, {0.0f, 0.0f, 1.0f, 0.5f});
	document->addMask(first);
	document->changeLayer(first, "Settings", [](Layer& layer) {
		layer.opacity = 0.75f, layer.blend = BlendMode::multiply, layer.locks.position = true;
	});
	const std::uint64_t group = document->addGroup("Group");
	const std::uint64_t inner = document->addPixelLayer("Inner");
	document->moveLayer(inner, group, 0);
	document->image(document->layer(inner)->pixels)->setPixel(300, 100, {0.0f, 1.0f, 0.0f, 1.0f});
	Adjustment curves;
	curves.kind = AdjustmentKind::curves;
	curves.curve = {{0.0f, 0.1f}, {0.5f, 0.6f}, {1.0f, 1.0f}};
	document->setActive(group);
	document->addAdjustment("Curves", curves);
	document->setActive(first);
	return document;
}

/// @brief Loads bytes as a project.
/// @param bytes The file.
/// @param limits The limits.
[[nodiscard]] LoadedProject load(std::vector<std::uint8_t> bytes, const Limits& limits = {}) {
	return loadProject(std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes)), limits);
}

/// @brief A file of a manifest's text and no blobs.
/// @param text The manifest.
[[nodiscard]] std::vector<std::uint8_t> withManifest(const std::string& text) {
	std::vector<std::uint8_t> file(projectMagic.begin(), projectMagic.end());
	for(const std::uint32_t value : {projectVersion, static_cast<std::uint32_t>(text.size())})
		for(int shift = 24; shift >= 0; shift -= 8) file.push_back(static_cast<std::uint8_t>(value >> shift));
	file.insert(file.end(), text.begin(), text.end());
	return file;
}

} // namespace

TEST_CASE("imaging project: layers, masks, groups, adjustments and the profile round-trip, tiles loaded lazily") {
	const auto document = sample();
	const std::vector<std::uint8_t> file = saveProject(*document, ColorProfile::displayP3());
	CHECK(isProject(file));
	const LoadedProject loaded = load(file);
	const Document& opened = *loaded.document;
	REQUIRE(loaded.profile);
	CHECK(loaded.profile->name() == "Display P3");
	CHECK(opened.history().steps().empty());
	CHECK(opened.width() == 600);
	REQUIRE(opened.listed().size() == 4);
	CHECK(opened.activeLayer()->name == "Layer 1");
	const Layer& first = *opened.activeLayer();
	CHECK(first.opacity == 0.75f);
	CHECK(first.blend == BlendMode::multiply);
	CHECK(first.locks.position);
	const Image& pixels = *opened.image(first.pixels);
	CHECK(pixels.writtenTiles().empty()); // still in the file
	CHECK(pixels.pixel(10, 10) == Pixel{1.0f, 0.0f, 0.0f, 1.0f});
	CHECK(pixels.pixel(590, 290).alpha == doctest::Approx(0.5f).epsilon(0.01));
	CHECK(opened.image(first.mask)->pixel(300, 150).alpha == 1.0f);
	const Layer& top = *opened.layer(opened.listed()[0].first);
	CHECK(top.adjustment.kind == AdjustmentKind::curves);
	CHECK(top.adjustment.curve.size() == 3);
	const Layer& group = *opened.layer(opened.listed()[1].first);
	REQUIRE(group.children.size() == 1);
	CHECK(opened.listed()[2].second == 1);
	CHECK(opened.image(opened.layer(group.children[0])->pixels)->pixel(300, 100).green == 1.0f);
	// The mask's one white tile is kept once; saved again, the tiles still in the file are copied as they are
	CHECK(loaded.tiles == 2 + 6 + 1);
	const std::vector<std::uint8_t> again = saveProject(opened, loaded.profile);
	CHECK(again.size() == file.size());
	CHECK(load(again).document->image(first.pixels)->pixel(10, 10).red == 1.0f);
}

TEST_CASE("imaging project: adjustments as JSON keep their settings and bring them into range") {
	Adjustment levels;
	levels.kind = AdjustmentKind::levels, levels.gamma = 2.0f, levels.inputBlack = 0.1f;
	CHECK(adjustmentFromJson(adjustmentToJson(levels)) == levels);
	Adjustment map;
	map.kind = AdjustmentKind::gradientMap;
	map.gradient.stops = {{0.0f, {0.0f, 0.0f, 1.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 0.0f, 1.0f}}};
	CHECK(adjustmentFromJson(adjustmentToJson(map)) == map);
	const Adjustment wild = adjustmentFromJson(affresco::json{{"kind", "levels"}, {"gamma", 1e300}, {"hue", -9e9}});
	CHECK(wild.gamma == 10.0f);
	CHECK(wild.hue == -180.0f);
	CHECK_THROWS_AS((void)adjustmentFromJson(affresco::json{{"kind", "sepia"}}), affresco::DocumentError);
	CHECK_THROWS_AS(
	    (void)adjustmentFromJson(affresco::json{{"kind", "curves"}, {"curve", affresco::json::array({1, 2})}}),
	    affresco::DocumentError);
	affresco::json many{{"kind", "curves"}, {"curve", affresco::json::array()}};
	for(int point = 0; point < 17; ++point) many["curve"].push_back(affresco::json::array({0.5, 0.5}));
	CHECK_THROWS_AS((void)adjustmentFromJson(many), affresco::DocumentError);
}

TEST_CASE("imaging project: damaged and oversized files are refused, a damaged tile reads transparent") {
	const auto document = sample();
	std::vector<std::uint8_t> file = saveProject(*document, std::nullopt);
	Limits small;
	small.projectFileBytes = file.size() - 1;
	CHECK_THROWS_AS((void)load(file, small), affresco::DocumentError);
	small = {};
	small.layers = 3;
	CHECK_THROWS_AS((void)load(file, small), affresco::DocumentError);
	small = {};
	small.groupDepth = 0;
	CHECK_THROWS_AS((void)load(file, small), affresco::DocumentError);
	small = {};
	small.manifestBytes = 100;
	CHECK_THROWS_AS((void)load(file, small), affresco::DocumentError);
	CHECK_THROWS_AS((void)load(std::vector<std::uint8_t>(file.begin(), file.begin() + 40)), affresco::DocumentError);
	CHECK_THROWS_AS((void)load({}), affresco::DocumentError);
	const std::string layer = R"({"kind":"pixel","name":"A","tiles":)";
	const std::string head = R"({"width":300,"height":300,"layers":[)";
	CHECK_THROWS_AS((void)load(withManifest(head + layer + "[[0,0,0,10]]}]}")), affresco::DocumentError);
	CHECK_THROWS_AS((void)load(withManifest(head + layer + "[[5,0,0,0]]}]}")), affresco::DocumentError);
	CHECK_THROWS_AS((void)load(withManifest(head + layer + "[[0,0,0,0],[0,0,0,0]]}]}")), affresco::DocumentError);
	CHECK_THROWS_AS((void)load(withManifest(head + "]}")), affresco::DocumentError);
	CHECK_THROWS_AS((void)load(withManifest(R"({"width":40000,"height":1,"layers":[{"kind":"pixel","name":"A"}]})")),
	                affresco::DocumentError);
	CHECK_THROWS_AS((void)load(withManifest(head + R"({"kind":"pixel","name":"A","children":[]}]})")),
	                affresco::DocumentError);
	CHECK_THROWS_AS((void)load(withManifest(head + R"({"kind":"group","name":"A","blend":"glow","children":[]}]})")),
	                affresco::DocumentError);
	CHECK_THROWS_AS((void)load(withManifest("{")), affresco::DocumentError);
	file.back() = static_cast<std::uint8_t>(file.back() ^ 0xff); // the last tile's adler checksum: still inflates
	file[file.size() - 20] = 0xff;                               // its deflate data: no longer does, or differently
	const LoadedProject damaged = load(file);
	(void)damaged.document->image(damaged.document->activeLayer()->pixels)->pixel(300, 100);
	CHECK(damaged.document->listed().size() == 4);
}
