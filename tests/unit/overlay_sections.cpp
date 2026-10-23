/// @file
/// @brief A document's "overlays" (format's Reconciler::addOverlays()): nodes laid over the window above the root,
/// e.g. from an included add-on, and the "persist" lists an include joins (Screen's include merge).

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with a row of two labels and a corner badge laid over it.
constexpr const char* overlaidScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": { "wide": true, "open": true },
	"actions": { "close": [{ "set": "state.open", "to": false }] },
	"root": { "type": "row", "id": "root", "children": [
		{ "type": "label", "id": "first", "text": "One", "width": 100 },
		{ "type": "label", "id": "second", "text": "Two", "width": 100, "visible": "{{state.wide}}" }
	] },
	"overlays": {
		"badge": { "type": "panel", "id": "badge", "width": 40, "height": 20, "anchors": { "right": 8, "bottom": 8 },
			"children": [{ "type": "label", "id": "badge-text", "text": "3" }] },
		"sheet": { "type": "panel", "id": "sheet", "width": 80, "height": 60,
			"overlay": { "placement": "center", "open": "state.open" } }
	}
})json";

/// @brief A screen built in a headless window.
struct Overlaid {
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param screen The screen.
	explicit Overlaid(affresco::Screen built)
	    : window(createHeadlessWindow(400, 300, clock)), screen(std::move(built)) {
		screen.build(*window);
		window->render();
	}
};

/// @brief A folder of its own for one test, removed after it.
struct Folder {
	/// @brief Creates it.
	/// @param name Its name in the temporary folder.
	explicit Folder(const std::string& name) : path(std::filesystem::temp_directory_path() / name) {
		std::filesystem::create_directories(path);
	}
	Folder(const Folder&) = delete;
	Folder& operator=(const Folder&) = delete;
	/// @brief Removes it and what is in it.
	~Folder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}

	/// @brief Writes a file in it.
	/// @param name The file.
	/// @param content What it holds.
	void write(const std::string& name, const std::string& content) const { std::ofstream(path / name) << content; }

	std::filesystem::path path; ///< The folder.
};

} // namespace

TEST_CASE("overlay sections: each is built after the root's children, over the window, and found by its id") {
	Overlaid overlaid(affresco::Screen::fromString(overlaidScreen));
	affresco::Panel& root = overlaid.window->root();
	affresco::Panel* row = overlaid.screen.find("root");
	REQUIRE(row);
	CHECK(row->children().size() == 2); // the row's labels: the overlays are not in it

	affresco::Panel* badge = overlaid.screen.find("badge");
	REQUIRE(badge);
	CHECK(badge->isOverlay());
	CHECK(badge->parent() == &root);
	CHECK(badge->frame().right == doctest::Approx(392)); // placed by its anchors in the window
	CHECK(badge->frame().bottom == doctest::Approx(292));
	CHECK(overlaid.screen.find("badge-text"));

	affresco::Panel* sheet = overlaid.screen.find("sheet");
	REQUIRE(sheet);
	CHECK(sheet->frame().left == doctest::Approx(160)); // its own placement wins
	CHECK(sheet->isVisible());
	overlaid.screen.runAction("close", overlaid.window->root(), "{}");
	overlaid.window->render();
	CHECK(overlaid.screen.find("sheet") == sheet); // the same panel, closed
	CHECK_FALSE(sheet->isVisible());
}

TEST_CASE("overlay sections: a revision changes one like a screen, and an id inside it is a place") {
	Overlaid overlaid(affresco::Screen::fromString(overlaidScreen));
	const affresco::RevisionResult result =
	    overlaid.screen.submitRevision(R"({"author": "studio", "set": {"#badge-text/text": "4"}})");
	REQUIRE(result.status == affresco::RevisionStatus::Applied);
	overlaid.window->render();
	CHECK(overlaid.screen.versions().document()["overlays"]["badge"]["children"][0]["text"] == "4");

	const affresco::RevisionResult removed =
	    overlaid.screen.submitRevision(R"({"author": "studio", "remove": ["/overlays/sheet"]})");
	REQUIRE(removed.status == affresco::RevisionStatus::Applied);
	overlaid.window->render();
	CHECK(overlaid.screen.find("sheet") == nullptr);
	CHECK(overlaid.screen.find("badge"));
}

TEST_CASE("overlay sections: one from an include joins the screen's, and its persist list joins the screen's") {
	const Folder folder("affrescoui-unit-overlays");
	folder.write("addon.json", R"json({
		"state": { "addonOpen": false, "count": 1 },
		"persist": ["state.addonOpen", "state.count"],
		"overlays": { "addon": { "type": "panel", "id": "addon", "width": 20, "height": 20 } }
	})json");
	folder.write("app.screen.json", R"json({
		"format": "affrescoui-screen", "version": 2,
		"include": ["addon.json"],
		"state": { "count": 0, "notes": "" },
		"persist": ["state.notes", "state.count"],
		"root": { "type": "panel", "id": "root" }
	})json");
	std::ifstream in(folder.path / "app.screen.json");
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

	Overlaid overlaid(affresco::Screen::fromString(text, folder.path));
	const json& document = overlaid.screen.versions().document();
	CHECK(document["persist"] == json::array({"state.addonOpen", "state.count", "state.notes"}));
	CHECK(document["state"]["count"] == 0); // the screen's own entries win
	CHECK(overlaid.screen.find("addon"));
}
