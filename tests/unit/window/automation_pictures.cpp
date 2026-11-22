/// @file
/// @brief The automation module's pictures, which need OpenGL: a golden image recorded, matched, and a frame that
/// differs written with its diff; a capture saved; pictures at a display scale of 1.25 and 1.5.

#include "Headless.hpp" // drawFromScratch

#include <affresco/automation/Scenarios.hpp>
#include <affresco/render/Application.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace automation = affresco::automation;

namespace {

/// @brief A folder for one test, removed after it.
class Folder {
public:
	Folder()
	    : path_(std::filesystem::temp_directory_path() /
	            ("affrescoui-pictures-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
		std::filesystem::create_directories(path_ / "goldens");
		std::filesystem::create_directories(path_ / "out");
	}
	~Folder() {
		std::error_code error;
		std::filesystem::remove_all(path_, error);
	}
	Folder(const Folder&) = delete;
	Folder& operator=(const Folder&) = delete;
	Folder(Folder&&) = delete;
	Folder& operator=(Folder&&) = delete;

	/// @brief The folder.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_; ///< See path().
};

/// @brief Writes a small screen whose colour a revision step changes, and its scenarios.
/// @param folder Where.
/// @return The scenarios file.
automation::ScenarioFile write(const Folder& folder) {
	std::ofstream(folder.path() / "box.screen.json") << R"({"version": 2, "window": {"width": 64, "height": 48},
		"root": {"id": "root", "color": "#336699", "children": [{"id": "box", "color": "#ffffff", "width": 16, "height": 16}]}})";
	return automation::ScenarioFile::parse(R"({"version": 2, "screen": "box.screen.json", "scenarios": [
		{"name": "box", "steps": [
			{"expect": {"image": "first"}},
			{"revision": {"message": "Red", "set": {"#box/color": "#ff0000"}}},
			{"expect": {"image": "first"}},
			{"capture": "red"}
		]}]})",
	                                       folder.path() / "box.scenarios.json");
}

/// @brief The size a PNG's header gives.
/// @param path The file.
/// @return Width and height, or 0 by 0 if it isn't a PNG.
std::pair<std::uint32_t, std::uint32_t> pngSize(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	std::array<unsigned char, 24> header{};
	in.read(reinterpret_cast<char*>(header.data()), header.size()); // NOLINT: bytes of the file
	if(!in) return {0, 0};
	const auto bigEndian = [&header](std::size_t at) {
		return (std::uint32_t{header[at]} << 24) | (std::uint32_t{header[at + 1]} << 16) |
		       (std::uint32_t{header[at + 2]} << 8) | std::uint32_t{header[at + 3]};
	};
	return {bigEndian(16), bigEndian(20)}; // IHDR's width and height
}

/// @brief The size a binary PPM's header gives, as Window::saveScreenshot writes it.
/// @param path The file.
/// @return Width and height, or 0 by 0 if it can't be read.
std::pair<int, int> ppmSize(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	std::string magic;
	int width = 0;
	int height = 0;
	in >> magic >> width >> height;
	return magic == "P6" ? std::pair{width, height} : std::pair{0, 0};
}

} // namespace

TEST_CASE("automation: pictures are compared with golden images pixel for pixel") {
	const Folder folder;
	const automation::ScenarioFile file = write(folder);
	automation::Options options;
	options.goldens = folder.path() / "goldens";
	options.output = folder.path() / "out";

	SUBCASE("without golden images, comparing fails and says how to record them") {
		const automation::Report report = automation::run(file, options);
		REQUIRE(report.scenarios.at(0).pictures.size() == 3);
		CHECK(report.scenarios[0].pictures[0].outcome == automation::Picture::Outcome::Differs);
		CHECK(report.scenarios[0].pictures[0].problem.contains("record it with --update"));
	}

	SUBCASE("recorded once, then the same frame matches and a changed one differs") {
		options.policy = automation::Goldens::RecordMissing;
		automation::Report report = automation::run(file, options);
		const automation::ScenarioResult* box = &report.scenarios.at(0);
		REQUIRE(box->pictures.size() == 3);
		CHECK(box->pictures[0].outcome == automation::Picture::Outcome::Recorded);
		CHECK(box->pictures[0].name == "box-first");
		CHECK(std::filesystem::exists(options.goldens / "box-first.png"));
		CHECK(box->pictures[2].outcome == automation::Picture::Outcome::Saved);
		CHECK(std::filesystem::exists(options.output / "box-red.png"));

		report = automation::run(file, options);
		box = &report.scenarios.at(0);
		REQUIRE(box->pictures.size() == 3); // none if the window couldn't open
		CHECK(box->pictures[0].outcome == automation::Picture::Outcome::Same);
		CHECK(box->pictures[1].outcome == automation::Picture::Outcome::Differs);
		CHECK(box->pictures[1].changedPixels == 16 * 16);
		CHECK(std::filesystem::exists(options.output / "box-first.diff.png"));
		REQUIRE(box->failures.size() == 1);
		CHECK(box->failures[0].step == 3);
		CHECK(box->failures[0].problem.starts_with("image box-first: 256 pixels differ (see "));
	}
}

TEST_CASE("automation: pictures at a display scale of 1.25 and 1.5 are whole pixels, and match a frame from scratch") {
	const Folder folder;
	// Next to the screen: a document names its fonts relative to itself
	std::filesystem::copy_file(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf",
	                           folder.path() / "Roboto-Regular.ttf");
	std::ofstream(folder.path() / "text.screen.json") << R"({"version": 2, "window": {"width": 640, "height": 400},
		"fonts": {"default": {"file": "Roboto-Regular.ttf", "size": 15}},
		"root": {"id": "root", "color": "#f8fafc", "children": [
			{"type": "label", "id": "text", "x": 16, "y": 16, "width": 400, "height": 24, "textColor": "#0f172a",
			 "text": "Pinned Crash Waffle illicit 0123456789"},
			{"id": "box", "x": 16, "y": 60, "width": 33, "height": 17, "color": "#336699", "rotate": 12}]}})";
	const automation::ScenarioFile file = automation::ScenarioFile::parse(
	    R"({"version": 2, "screen": "text.screen.json", "scenarios": [
		{"name": "at-125", "scale": 1.25, "steps": [{"expect": {"image": "first"}}]},
		{"name": "at-150", "scale": 1.5, "steps": [{"expect": {"image": "first"}}]}]})",
	    folder.path() / "text.scenarios.json");
	automation::Options options;
	options.goldens = folder.path() / "goldens";
	options.output = folder.path() / "out";
	options.policy = automation::Goldens::RecordMissing;

	const automation::Report report = automation::run(file, options);
	REQUIRE(report.scenarios.size() == 2);
	const std::array<std::pair<std::uint32_t, std::uint32_t>, 2> sizes = {std::pair{800u, 500u}, std::pair{960u, 600u}};
	for(std::size_t i = 0; i < sizes.size(); ++i) {
		const automation::ScenarioResult& scenario = report.scenarios[i];
		CAPTURE(scenario.name);
		REQUIRE(scenario.pictures.size() == 1);
		// Recorded, not "stale pixels": the frame from retained geometry is the frame from scratch
		CHECK(scenario.pictures[0].outcome == automation::Picture::Outcome::Recorded);
		CHECK(scenario.pictures[0].problem.empty());
		CHECK(pngSize(options.goldens / (scenario.name + "-first.png")) == sizes[i]);
	}
}

TEST_CASE("window: a picture at a fractional scale is the window's size times it, rounded; a bad scale draws nothing") {
	affresco::Application app(affresco::ApplicationOptions{nullptr, true});
	affresco::Window& window = app.createWindow("scales", 640, 400);
	const Folder folder;
	const std::filesystem::path picture = folder.path() / "out" / "picture.ppm";

	REQUIRE(window.saveScreenshot(picture, 1.25f));
	CHECK(ppmSize(picture) == std::pair{800, 500});
	CHECK(affresco::detail::drawFromScratch(window, 1.25f).size() == std::size_t{800} * 500 * 3);
	REQUIRE(window.saveScreenshot(picture, 1.5f));
	CHECK(ppmSize(picture) == std::pair{960, 600});
	REQUIRE(window.saveScreenshot(picture, 2));
	CHECK(ppmSize(picture) == std::pair{1280, 800});

	std::filesystem::remove(picture);
	for(const float scale :
	    {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
		CAPTURE(scale);
		CHECK_FALSE(window.saveScreenshot(picture, scale));
		CHECK(affresco::detail::drawFromScratch(window, scale).empty());
	}
	CHECK_FALSE(std::filesystem::exists(picture));
}
