/// @file
/// @brief The images a screen edits: the "images" section (good, malformed, and limits that try to rise), the store,
/// each family of the "image" step's operations, the snapshot and the thumbnails, painting through the pointer, and
/// undo and redo.

#include "Headless.hpp"
#include "ImageOperations.hpp"
#include "SectionRegistry.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/imaging/Composite.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
namespace editing = affresco::format::editing;

namespace {

/// @brief The error the sections give a document with this "images" section, or "".
/// @param images The section.
/// @return The error.
std::string imagesError(const json& images) {
	affresco::format::SectionRegistry sections;
	affresco::format::registerBuiltInSections(sections);
	try {
		sections.validate({{"version", 2}, {"root", json::object()}, {"images", images}});
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return "";
}

/// @brief Runs an operation on an image.
/// @param image The image.
/// @param name The operation.
/// @param with Its arguments.
/// @param window The window, if any.
void run(editing::ImageDocument& image, std::string_view name, const json& with = json::object(),
         affresco::Window* window = nullptr) {
	const editing::ImageOperation* operation = editing::findImageOperation(name);
	REQUIRE_MESSAGE(operation != nullptr, name);
	operation->run(image, with, window);
}

/// @brief One pixel of the flattened image.
/// @param image The image.
/// @param x Across.
/// @param y Down.
/// @return Its red byte.
int redAt(const editing::ImageDocument& image, int x, int y) {
	return affresco::imaging::flatten(image.canvas->document(), 0, x, y, 1, 1).at(0);
}

/// @brief A screen with one image, its snapshot in state.photo, and actions of image steps.
/// @param actions The actions.
/// @return The document.
std::string imageScreen(const std::string& actions) {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "photo": null, "error": "" },
		"images": { "photo": { "width": 64, "height": 48, "into": "state.photo",
		                       "tools": { "brush": { "gesture": "paint", "size": 9, "hardness": 100 },
		                                  "box": { "gesture": "select-rectangle" } } } },
		"actions": )json" +
	       actions + R"json(,
		"root": { "id": "root", "children": [] }
	})json";
}

} // namespace

TEST_CASE("format: the images section reads images, tools and lowered limits") {
	CHECK(imagesError({{"photo", {{"width", 640}, {"height", 480}}}}).empty());
	CHECK(imagesError({{"photo",
	                    {{"width", 64},
	                     {"height", 64},
	                     {"format", "rgba16f"},
	                     {"background", "transparent"},
	                     {"into", "state.photo"},
	                     {"tools", {{"pen", {{"gesture", "paint"}, {"size", 4}}}, {"wand", {{"gesture", "magic-wand"}}}}},
	                     {"tool", "wand"},
	                     {"limits", {{"side", 1000}, {"undoSteps", 10}}}}}})
	          .empty());

	const auto refused = [](const json& entry, const std::string& place) {
		const std::string error = imagesError({{"photo", entry}});
		CHECK_MESSAGE(error.find(place) != std::string::npos, error);
	};
	refused({{"height", 4}}, "images.photo: an image needs a \"width\"");
	refused({{"width", 0}, {"height", 4}}, "images.photo.width");
	refused({{"width", 4}, {"height", 4}, {"colour", "red"}}, "images.photo.colour: unknown property");
	refused({{"width", 4}, {"height", 4}, {"into", "photo"}}, "images.photo.into");
	refused({{"width", 4}, {"height", 4}, {"tools", {{"pen", {{"gesture", "spray"}}}}}}, "images.photo.tools.pen.gesture");
	refused({{"width", 4}, {"height", 4}, {"tools", {{"pen", {{"size", 4}}}}}}, "images.photo.tools.pen");
	refused({{"width", 4}, {"height", 4}, {"tools", {{"pen", {{"gesture", "paint"}, {"tip", 1}}}}}},
	        "images.photo.tools.pen.tip: unknown tool option");
	refused({{"width", 4}, {"height", 4}, {"tools", {{"pen", {{"gesture", "paint"}, {"preset", "Nope"}}}}}},
	        "images.photo.tools.pen: Unknown brush");
	refused({{"width", 4}, {"height", 4}, {"tool", "pen"}}, "images.photo.tool");
	refused({{"width", 4}, {"height", 4}, {"limits", {{"wishes", 3}}}}, "images.photo.limits.wishes");
	// A document only lowers the library's limits
	refused({{"width", 4}, {"height", 4}, {"limits", {{"side", 40000}}}},
	        "images.photo.limits.side: a document only lowers the library's limit (32768)");
	refused({{"width", 4}, {"height", 4}, {"limits", {{"layers", 0}}}}, "images.photo.limits.layers");
	refused({{"width", 4}, {"height", 4}, {"limits", {{"undoSteps", 1.5}}}}, "images.photo.limits.undoSteps");
	refused({{"width", 200}, {"height", 4}, {"limits", {{"side", 100}}}}, "images.photo.width");
	refused({{"width", 100}, {"height", 100}, {"limits", {{"pixels", 50}}}}, "images.photo: at most 50 pixels");

	json many = json::object();
	for(int i = 0; i <= static_cast<int>(editing::maxImages); ++i) many["i" + std::to_string(i)] = {{"width", 1}, {"height", 1}};
	CHECK(imagesError(many).find("images: at most") != std::string::npos);
	CHECK(imagesError({{"a/b", {{"width", 1}, {"height", 1}}}}).find("images.a/b") != std::string::npos);
}

TEST_CASE("format: an image without tools names every gesture after itself, and lowered limits hold") {
	editing::ImageDocuments store;
	const json definition = {{"width", 32}, {"height", 16}, {"limits", {{"layers", 3}}}};
	editing::ImageDocument& image = store.image("photo", definition, "images.photo");
	CHECK(&store.image("photo", definition, "images.photo") == &image); // the same definition, the same image
	CHECK(store.find("photo") == &image);
	CHECK(image.definition.tools.size() == editing::gestureNames().size());
	CHECK(image.settings.tool == "paint");
	CHECK(image.limits.layers == 3);
	run(image, "setTool", {{"tool", "magic-wand"}});
	CHECK(image.settings.gesture == editing::Gesture::magicWand);
	CHECK_THROWS(run(image, "setTool", {{"tool", "spray"}}));
	run(image, "addLayer");
	run(image, "addLayer");
	CHECK_THROWS(run(image, "addLayer")); // a fourth layer is beyond the limit
	CHECK(editing::findImageOperation("paintEverything") == nullptr);
}

TEST_CASE("format: the snapshot has the keys a screen reads, and thumbnails are named after the image") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = store.image("photo", {{"width", 20}, {"height", 10}}, "images.photo");
	const json snapshot = editing::snapshot(image);
	for(const char* key : {"width", "height", "format", "profile", "display", "proof", "revision", "active", "layers",
	                       "history", "position", "dropped", "canUndo", "canRedo", "memory", "selection", "bounds",
	                       "foreground", "background", "tool", "gesture", "options", "view", "message", "previewing"})
		CHECK_MESSAGE(snapshot.contains(key), key);
	CHECK(snapshot["width"] == 20);
	CHECK(snapshot["gesture"] == "paint");
	REQUIRE(snapshot["layers"].size() == 1);
	const json& layer = snapshot["layers"][0];
	CHECK(layer["name"] == "Background");
	CHECK(layer["thumbnail"] == "photo/layer/" + layer["id"].get<std::string>());
	for(const char* key : {"size", "hardness", "spacing", "opacity", "flow", "mode", "tolerance", "contiguous",
	                       "allLayers", "fillOpacity", "shape", "quickMask"})
		CHECK_MESSAGE(snapshot["options"].contains(key), key);

	affresco::ImageCache cache;
	editing::updateThumbnails(image, cache);
	CHECK(cache.named("photo/navigator") != nullptr);
	CHECK(cache.named(layer["thumbnail"].get<std::string>()) != nullptr);
}

TEST_CASE("format: the pointer paints through the current tool, and undo and redo take it back and again") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = store.image("photo", {{"width", 64}, {"height", 32}}, "images.photo");
	run(image, "setOptions", {{"size", 9}, {"hardness", 100}, {"foreground", "#000000"}});
	CHECK(redAt(image, 30, 16) == 255);
	run(image, "pointer",
	    {{"points",
	      {{{"phase", "press"}, {"x", 10}, {"y", 16}},
	       {{"phase", "move"}, {"x", 50}, {"y", 16}},
	       {{"phase", "release"}, {"x", 50}, {"y", 16}}}}});
	CHECK(redAt(image, 30, 16) < 64);
	CHECK(editing::snapshot(image)["history"].back()["label"] == "Brush");
	run(image, "undo");
	CHECK(redAt(image, 30, 16) == 255);
	CHECK(editing::snapshot(image)["canRedo"] == true);
	run(image, "redo");
	CHECK(redAt(image, 30, 16) < 64);
	run(image, "history", {{"position", 0}});
	CHECK(redAt(image, 30, 16) == 255);
	run(image, "clearHistory");
	CHECK(editing::snapshot(image)["canRedo"] == false);

	// One event at a time, as a view would send them; an eraser leaves the background layer transparent
	run(image, "setTool", {{"tool", "erase"}});
	for(const char* phase : {"press", "move", "release"})
		run(image, "pointer", {{"phase", phase}, {"x", phase == std::string("press") ? 5 : 20}, {"y", 5}});
	CHECK(affresco::imaging::flatten(image.canvas->document(), 0, 12, 5, 1, 1).at(3) < 255);
	CHECK_THROWS(run(image, "pointer", {{"points", json(4097, json::object())}}));
}

TEST_CASE("format: the layer operations add and change and move and mask and delete layers") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = store.image("photo", {{"width", 16}, {"height", 16}}, "images.photo");
	const auto layers = [&] { return editing::snapshot(image)["layers"]; };
	run(image, "addLayer", {{"name", "Ink"}});
	run(image, "addLayer", {{"kind", "group"}});
	run(image, "addLayer", {{"kind", "levels"}});
	CHECK(layers().size() == 4);
	CHECK_THROWS(run(image, "addLayer", {{"kind", "hologram"}}));
	CHECK_THROWS(run(image, "addLayer", {{"name", std::string(300, 'x')}}));
	const std::string ink = layers()[2]["id"];
	run(image, "setLayer", {{"id", ink}, {"name", "Ink 2"}, {"opacity", 50}, {"blend", "multiply"}, {"visible", false}});
	json changed = layers()[2];
	CHECK(changed["name"] == "Ink 2");
	CHECK(changed["visible"] == false);
	CHECK(changed["blend"] == "multiply");
	CHECK_THROWS(run(image, "setLayer", {{"id", ink}, {"blend", "sparkle"}}));
	run(image, "moveLayer", {{"id", ink}, {"by", 1}});
	CHECK(layers()[1]["id"] == ink);
	run(image, "duplicateLayer", {{"id", ink}});
	CHECK(layers().size() == 5);
	run(image, "selectLayer", {{"id", ink}});
	CHECK(editing::snapshot(image)["active"] == ink);
	run(image, "addMask", {{"id", ink}});
	CHECK(layers()[2]["mask"] != false);
	run(image, "deleteMask", {{"id", ink}});
	run(image, "deleteLayer", {{"id", ink}});
	CHECK(layers().size() == 4);
	CHECK_THROWS(run(image, "deleteLayer", {{"id", "999"}}));
}

TEST_CASE("format: the image edits select and feather and fill and filter and crop and set the view and profiles") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = store.image("photo", {{"width", 40}, {"height", 30}}, "images.photo");
	run(image, "select", {{"kind", "rectangle"}, {"from", {{"x", 0}, {"y", 0}}}, {"to", {{"x", 20}, {"y", 30}}}});
	CHECK(editing::snapshot(image)["selection"] == true);
	run(image, "feather", {{"radius", 1e9}}); // bounded by the filter radius limit
	run(image, "select", {{"kind", "rectangle"}, {"from", {{"x", 0}, {"y", 0}}}, {"to", {{"x", 20}, {"y", 30}}}});
	run(image, "fillSelection", {{"color", "#000000"}});
	CHECK(redAt(image, 5, 15) == 0);
	CHECK(redAt(image, 35, 15) == 255);
	CHECK_THROWS(run(image, "select", {{"kind", "lasso"}}));
	CHECK_THROWS(run(image, "select", {{"kind", "rectangle"}}));
	run(image, "select", {{"kind", "none"}});
	CHECK(editing::snapshot(image)["selection"] == false);

	run(image, "previewFilter", {{"kind", "gaussian-blur"}, {"radius", 3}});
	CHECK(editing::snapshot(image)["previewing"] == true);
	run(image, "cancelFilter");
	CHECK(editing::snapshot(image)["previewing"] == false);
	run(image, "applyFilter", {{"kind", "gaussian-blur"}, {"radius", 3}});
	const int blurred = redAt(image, 20, 15);
	CHECK(blurred > 0);
	CHECK(blurred < 255);
	CHECK_THROWS(run(image, "applyFilter", {{"kind", "teleport"}}));

	run(image, "crop", {{"x", 10}, {"y", 5}, {"width", 15}, {"height", 1000000}});
	CHECK(editing::snapshot(image)["width"] == 15);
	CHECK(editing::snapshot(image)["height"] == 25);
	CHECK_THROWS(run(image, "crop", {{"x", 100}, {"y", 0}, {"width", 5}, {"height", 5}}));

	run(image, "zoom", {{"to", 1e9}, {"x", 3}, {"y", 4}});
	CHECK(image.view.zoom == doctest::Approx(64.0));
	CHECK(image.view.x == doctest::Approx(3.0));
	run(image, "fit");
	run(image, "setProfile", {{"working", "displayP3"}, {"proof", "gray"}});
	CHECK(editing::snapshot(image)["proof"] != "");
	CHECK_THROWS(run(image, "setProfile", {{"working", "cmyk"}}));
	CHECK_THROWS(run(image, "setOptions", {{"foreground", "red-ish"}}));
	run(image, "setOptions", {{"swap", true}});
	CHECK(editing::snapshot(image)["foreground"] == "#ffffff");
}

TEST_CASE("format: the image files are made and saved and opened and reverted through the file dialogs") {
	editing::ImageDocuments store;
	editing::ImageDocument& image = store.image("photo", {{"width", 8}, {"height", 8}}, "images.photo");
	run(image, "newImage", {{"width", 12}, {"height", 6}, {"background", "#000000"}});
	CHECK(editing::snapshot(image)["width"] == 12);
	CHECK(redAt(image, 3, 3) == 0);
	CHECK_THROWS(run(image, "newImage", {{"width", 100000}}));
	CHECK_THROWS(run(image, "revert")); // nothing saved yet
	CHECK_THROWS(run(image, "open"));   // opening needs a window
	run(image, "save");                 // without a window, only kept for revert
	run(image, "fillSelection", {{"color", "#ffffff"}});
	CHECK(redAt(image, 3, 3) == 255);
	run(image, "revert");
	CHECK(redAt(image, 3, 3) == 0);
	CHECK_THROWS(run(image, "save", {{"format", "tiff"}}));
	CHECK_THROWS(run(image, "save", {{"format", "png"}, {"extension", "../x"}}));

	const ManualClock clock;
	const auto window = createHeadlessWindow(100, 100, clock);
	const std::filesystem::path file = std::filesystem::temp_directory_path() / "affrescoui_image_documents.png";
	std::vector<affresco::FileDialogKind> asked;
	window->setFileDialogHandler([&](const affresco::FileDialog& dialog) {
		asked.push_back(dialog.kind);
		return std::vector<std::string>{file.string()};
	});
	run(image, "save", {{"format", "png"}}, window.get());
	run(image, "newImage", {{"width", 3}, {"height", 3}});
	run(image, "open", {}, window.get());
	CHECK(asked.size() == 2);
	CHECK(editing::snapshot(image)["width"] == 12);
	CHECK(redAt(image, 3, 3) == 0);
	std::filesystem::remove(file);
}

TEST_CASE("format: the image step runs operations, publishes the snapshot, and hands errors to onError") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(imageScreen(R"json({
		"show": [{ "image": "snapshot" }],
		"paint": [{ "image": "pointer", "of": "photo", "with": { "points": [
			{ "phase": "press", "x": 5, "y": 5 }, { "phase": "move", "x": 40, "y": 5 }, { "phase": "release", "x": 40, "y": 5 }] } }],
		"undo": [{ "image": "undo" }],
		"box": [{ "image": "setTool", "with": { "tool": "box" } }],
		"broken": [{ "image": "setTool", "with": { "tool": "spray" }, "onError": [{ "set": "state.error", "to": "{{error}}" }] }],
		"loud": [{ "image": "setTool", "with": { "tool": "spray" } }],
		"stranger": [{ "image": "snapshot", "of": "drawing" }],
		"nonsense": [{ "image": "dance" }] })json"));
	screen.build(*window);
	window->render();
	affresco::Panel* root = screen.find("root");
	REQUIRE(root != nullptr);
	const auto state = [&] { return json::parse(screen.stateJson()); };

	screen.runAction("show", *root);
	CHECK(state()["photo"]["width"] == 64);
	CHECK(state()["photo"]["tool"] == "brush");
	CHECK(state()["photo"]["options"]["size"] == 9);
	screen.runAction("paint", *root);
	CHECK(state()["photo"]["canUndo"] == true);
	CHECK(state()["photo"]["history"].back()["label"] == "Brush");
	screen.runAction("undo", *root);
	CHECK(state()["photo"]["canUndo"] == false);
	screen.runAction("box", *root);
	CHECK(state()["photo"]["gesture"] == "select-rectangle");
	screen.runAction("broken", *root);
	CHECK(state()["error"].get<std::string>().find("Unknown tool spray") != std::string::npos);

	const auto error = [&](const char* action) {
		std::string logged;
		affresco::setLogSink([&](affresco::LogLevel, std::string_view message) { logged += message; });
		screen.runAction(action, *root);
		affresco::setLogSink(nullptr);
		return logged;
	};
	CHECK(error("loud").find("setTool: Unknown tool spray") != std::string::npos);
	CHECK(error("stranger").find(".of") != std::string::npos);
	CHECK(error("nonsense").find("unknown image operation") != std::string::npos);
}
