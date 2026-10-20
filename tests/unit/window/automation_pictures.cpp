/// @file
/// @brief The automation module's pictures, which need OpenGL: a golden image recorded, matched, and a frame that
/// differs written with its diff; a capture saved.

#include <affresco/automation/Scenarios.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

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
	std::ofstream(folder.path() / "box.screen.json") << R"({"version": 1, "window": {"width": 64, "height": 48},
		"root": {"id": "root", "color": "#336699", "children": [{"id": "box", "color": "#ffffff", "width": 16, "height": 16}]}})";
	return automation::ScenarioFile::parse(R"({"version": 1, "screen": "box.screen.json", "scenarios": [
		{"name": "box", "steps": [
			{"expect": {"image": "first"}},
			{"revision": {"message": "Red", "set": {"#box/color": "#ff0000"}}},
			{"expect": {"image": "first"}},
			{"capture": "red"}
		]}]})",
	                                       folder.path() / "box.scenarios.json");
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
		CHECK(box->pictures[0].outcome == automation::Picture::Outcome::Same);
		CHECK(box->pictures[1].outcome == automation::Picture::Outcome::Differs);
		CHECK(box->pictures[1].changedPixels == 16 * 16);
		CHECK(std::filesystem::exists(options.output / "box-first.diff.png"));
		REQUIRE(box->failures.size() == 1);
		CHECK(box->failures[0].step == 3);
		CHECK(box->failures[0].problem.starts_with("image box-first: 256 pixels differ (see "));
	}
}
