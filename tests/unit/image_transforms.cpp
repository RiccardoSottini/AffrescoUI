/// @file
/// @brief The "image" step's geometry and merges (format/src/imaging/ImageTransforms.cpp): canvas and image size,
/// turning and mirroring the canvas, a layer's transform, merging down and visible, flattening, deleting hidden
/// layers, and copy, cut, paste and clear; what forgets the history and what is one undoable step.

#include "ImageOperations.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/imaging/Composite.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
namespace editing = affresco::format::editing;

namespace {

/// @brief Runs an operation on an image.
/// @param image The image.
/// @param name The operation.
/// @param with Its arguments.
void run(editing::ImageDocument& image, std::string_view name, const json& with = json::object()) {
	const editing::ImageOperation* operation = editing::findImageOperation(name);
	REQUIRE_MESSAGE(operation != nullptr, name);
	operation->run(image, with, nullptr);
}

/// @brief One pixel of the flattened image.
/// @param image The image.
/// @param x Across.
/// @param y Down.
/// @return Its red, green, blue and alpha bytes.
std::vector<std::uint8_t> at(const editing::ImageDocument& image, int x, int y) {
	return affresco::imaging::flatten(image.canvas->document(), 0, x, y, 1, 1);
}

/// @brief An image of 40 × 30 on white, its left 10 columns black (on a layer of their own, "Ink", the active one).
/// @param store The store.
/// @return The image.
editing::ImageDocument& inked(editing::ImageDocuments& store) {
	editing::ImageDocument& image = store.image("photo", {{"width", 40}, {"height", 30}}, "images.photo");
	run(image, "addLayer", {{"name", "Ink"}});
	run(image, "select", {{"kind", "rectangle"}, {"from", {{"x", 0}, {"y", 0}}}, {"to", {{"x", 10}, {"y", 30}}}});
	run(image, "fillSelection", {{"color", "#000000"}});
	run(image, "select", {{"kind", "none"}});
	return image;
}

/// @brief The layers' names, top first.
/// @param image The image.
/// @return The names.
std::vector<std::string> names(const editing::ImageDocument& image) {
	std::vector<std::string> found;
	for(const json& layer : editing::snapshot(image)["layers"]) found.push_back(layer["name"]);
	return found;
}

} // namespace

TEST_CASE("format: canvas size places the image by its anchor and forgets the history") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = inked(store);
	run(image, "canvasSize", {{"width", 60}, {"height", 30}, {"anchor", "right"}});
	CHECK(editing::snapshot(image)["width"] == 60);
	CHECK(editing::snapshot(image)["canUndo"] == false);
	CHECK(at(image, 5, 5)[3] == 0);    // the new part, transparent
	CHECK(at(image, 25, 5)[0] == 0);   // the ink, 20 to the right
	CHECK(at(image, 35, 5)[0] == 255); // the white background
	CHECK(names(image) == std::vector<std::string>{"Ink", "Background"});
	CHECK_THROWS(run(image, "canvasSize", {{"width", 0}}));
	CHECK_THROWS(run(image, "canvasSize", {{"width", 1 << 30}}));
	CHECK_THROWS(run(image, "canvasSize", {{"width", 70}, {"anchor", "middle-earth"}}));
	run(image, "canvasSize", {{"width", 20}, {"height", 10}, {"anchor", "top-left"}});
	CHECK(editing::snapshot(image)["height"] == 10);
}

TEST_CASE("format: image size resamples, keeping the proportions when one side is given") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = inked(store);
	run(image, "imageSize", {{"width", 20}});
	CHECK(editing::snapshot(image)["width"] == 20);
	CHECK(editing::snapshot(image)["height"] == 15);
	CHECK(at(image, 2, 7)[0] == 0);
	CHECK(at(image, 15, 7)[0] == 255);
	run(image, "imageSize", {{"width", 80}, {"height", 60}});
	CHECK(editing::snapshot(image)["height"] == 60);
	const int edge = at(image, 20, 30)[0];
	CHECK(edge > 0); // between the ink and the white, smoothly
	CHECK(edge < 255);
	CHECK_THROWS(run(image, "imageSize", {{"height", -4}}));
}

TEST_CASE("format: the canvas turns by quarters and mirrors; a flip and a half turn can be undone") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = inked(store);
	run(image, "rotateCanvas", {{"angle", 90}});
	CHECK(editing::snapshot(image)["width"] == 30);
	CHECK(editing::snapshot(image)["height"] == 40);
	CHECK(at(image, 15, 2)[0] == 0); // the left edge is now the top
	CHECK(at(image, 15, 30)[0] == 255);
	run(image, "rotateCanvas", {{"angle", -90}});
	CHECK(editing::snapshot(image)["width"] == 40);
	CHECK(at(image, 2, 15)[0] == 0);
	CHECK_THROWS(run(image, "rotateCanvas", {{"angle", 45}}));
	run(image, "flipCanvas");
	CHECK(at(image, 2, 15)[0] == 255);
	CHECK(at(image, 37, 15)[0] == 0);
	CHECK(editing::snapshot(image)["canUndo"] == true);
	run(image, "undo");
	CHECK(at(image, 2, 15)[0] == 0);
	run(image, "rotateCanvas", {{"angle", 180}});
	CHECK(at(image, 37, 15)[0] == 0);
	CHECK_THROWS(run(image, "flipCanvas", {{"direction", "diagonal"}}));
}

TEST_CASE("format: a layer's transform moves, flips and scales its pixels about their centre, as one step") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = inked(store);
	run(image, "transformLayer", {{"x", 20}});
	CHECK(at(image, 5, 5)[0] == 255);
	CHECK(at(image, 25, 5)[0] == 0);
	run(image, "undo");
	run(image, "transformLayer", {{"scaleX", -100}}); // about the ink's own centre: it stays
	CHECK(at(image, 5, 5)[0] == 0);
	CHECK(at(image, 15, 5)[0] == 255);
	run(image, "transformLayer", {{"scaleX", 200}}); // twice as wide about x = 5: from -5 to 15
	CHECK(at(image, 12, 5)[0] == 0);
	CHECK(at(image, 18, 5)[0] == 255);
	run(image, "transformLayer", {{"angle", 90}});
	CHECK(editing::snapshot(image)["canUndo"] == true);
	CHECK_THROWS(run(image, "transformLayer", {{"scaleY", 0}}));
	run(image, "setLayer", {{"lockPosition", true}});
	CHECK_THROWS(run(image, "transformLayer", {{"x", 3}}));
	run(image, "addLayer", {{"kind", "group"}});
	CHECK_THROWS(run(image, "transformLayer", {{"x", 3}}));
}

TEST_CASE("format: merge down, merge visible and flatten keep what shows; delete hidden drops what doesn't") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = inked(store);
	run(image, "setLayer", {{"opacity", 50}});
	run(image, "mergeDown");
	CHECK(names(image) == std::vector<std::string>{"Background"});
	CHECK(at(image, 5, 5)[0] == doctest::Approx(128).epsilon(0.02));
	CHECK_THROWS(run(image, "mergeDown")); // nothing below
	run(image, "undo");
	CHECK(names(image).size() == 2);

	run(image, "addLayer", {{"name", "Hidden"}}); // above the active one, the background since the merge
	run(image, "setLayer", {{"visible", false}});
	run(image, "mergeVisible"); // where the topmost visible layer was
	CHECK(names(image) == std::vector<std::string>{"Merged", "Hidden"});
	CHECK(at(image, 5, 5)[0] == doctest::Approx(128).epsilon(0.02));
	run(image, "undo");
	run(image, "deleteHidden");
	CHECK(names(image) == std::vector<std::string>{"Ink", "Background"});
	run(image, "addLayer", {{"name", "Hidden"}});
	run(image, "setLayer", {{"visible", false}});
	run(image, "flatten", {{"background", "#ff0000"}});
	CHECK(names(image) == std::vector<std::string>{"Background"});
	CHECK(at(image, 35, 5)[0] == 255);
	CHECK_THROWS(run(image, "flatten", {{"background", "plaid"}}));
	run(image, "setLayer", {{"visible", false}});
	CHECK_THROWS(run(image, "mergeVisible"));
}

TEST_CASE("format: copy, cut, paste and clear move the selection's pixels through the image's clipboard") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = inked(store);
	CHECK_THROWS(run(image, "paste"));
	run(image, "select", {{"kind", "rectangle"}, {"from", {{"x", 0}, {"y", 0}}}, {"to", {{"x", 5}, {"y", 30}}}});
	run(image, "cut");
	CHECK(at(image, 2, 5)[0] == 255); // the white below shows
	CHECK(at(image, 7, 5)[0] == 0);
	run(image, "paste");
	CHECK(names(image).front() == "Pasted");
	CHECK(at(image, 2, 5)[0] == 0);
	run(image, "select", {{"kind", "none"}});
	run(image, "copy", {{"merged", true}});
	run(image, "canvasSize", {{"width", 80}, {"anchor", "left"}});
	run(image, "paste", {{"name", "Again"}});
	CHECK(names(image).front() == "Again");
	CHECK(at(image, 22, 5)[0] == 0); // centred: 20 from the left
	run(image, "clear");
	CHECK(at(image, 22, 5)[0] == 255);
	run(image, "addLayer", {{"kind", "group"}});
	CHECK_THROWS(run(image, "clear"));
	CHECK_THROWS(run(image, "copy"));
}

TEST_CASE("format: align lines a layer's pixels up with the selection's edges, or the image's") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = inked(store); // the ink: columns 0 to 10
	run(image, "alignLayer", {{"edge", "right"}});
	CHECK(at(image, 5, 5)[0] == 255);
	CHECK(at(image, 35, 5)[0] == 0); // columns 30 to 40
	CHECK(image.canvas->document().history().canUndo());
	run(image, "select", {{"kind", "rectangle"}, {"from", {{"x", 10}, {"y", 0}}}, {"to", {{"x", 30}, {"y", 30}}}});
	run(image, "alignLayer", {{"edge", "center"}});
	CHECK(at(image, 22, 5)[0] == 0); // columns 15 to 25
	CHECK(at(image, 12, 5)[0] == 255);
	CHECK_THROWS(run(image, "alignLayer", {{"edge", "diagonal"}}));
	run(image, "addLayer", {{"name", "Empty"}});
	run(image, "alignLayer", {{"edge", "top"}}); // nothing to line up: nothing changes
	run(image, "addLayer", {{"kind", "group"}});
	CHECK_THROWS(run(image, "alignLayer", {{"edge", "left"}}));
}
