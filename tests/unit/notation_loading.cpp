/// @file
/// @brief Screens written in the Lisp notation load as the same document as their JSON: a real template converted
/// whole, and an app whose entry and one include are Lisp while its other files stay JSON.

#include <affresco/affresco.hpp>
#include <affresco/notation/Notation.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

using affresco::json;
using affresco::Screen;
using affresco::notation::Notation;

namespace {

/// @brief A copy of two example folders in a folder of its own, removed when the test ends.
class Copy {
public:
	/// @brief Copies `examples/<app>` and `examples/shared`, which its apps include from, side by side.
	/// @param app The app's folder name.
	explicit Copy(const std::string& app) {
		const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
		root_ =
		    std::filesystem::temp_directory_path() / ("affrescoui-notation-" + std::to_string(std::random_device()()));
		std::filesystem::create_directories(root_);
		std::filesystem::copy(examples / app, root_ / app, std::filesystem::copy_options::recursive);
		std::filesystem::copy(examples / "shared", root_ / "shared", std::filesystem::copy_options::recursive);
		app_ = root_ / app / "json";
	}
	Copy(const Copy&) = delete;            ///< One owner of the folder.
	Copy& operator=(const Copy&) = delete; ///< One owner of the folder.
	/// @brief Removes the folder.
	~Copy() {
		std::error_code ignored;
		std::filesystem::remove_all(root_, ignored);
	}

	/// @brief The app's JSON folder in the copy. @return It.
	[[nodiscard]] const std::filesystem::path& app() const noexcept { return app_; }
	/// @brief A data folder inside the copy, for the screens loaded from files. @return It.
	[[nodiscard]] std::filesystem::path data() const { return root_ / "data"; }

private:
	std::filesystem::path root_; ///< The folder of the copy.
	std::filesystem::path app_;  ///< The app's folder in it.
};

/// @brief Reads a file.
/// @param file The file.
/// @return Its value, in the notation its name says.
json readFile(const std::filesystem::path& file) {
	const std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return affresco::notation::readDocument(text.str(), file);
}

/// @brief Writes a value to a file, in the Lisp notation.
/// @param file The file.
/// @param value The value.
void writeLisp(const std::filesystem::path& file, const json& value) {
	std::ofstream(file, std::ios::binary) << affresco::notation::write(value, Notation::Lisp);
}

/// @brief Loads an app's entry screen as the host does, keeping nothing on disk.
/// @param folder The app's folder.
/// @return Its document as loaded (its includes, kits, models and seeds merged in), as JSON text.
std::string loadedApp(const std::filesystem::path& folder) {
	return Screen::fromApp(folder, {}, Screen::History::InMemory).documentJson();
}

} // namespace

TEST_CASE("notation: a template converted to the Lisp notation loads as the same document") {
	const Copy copy("sign-in");
	const std::filesystem::path jsonFile = copy.app() / "index.screen.json";
	const std::filesystem::path lisp = copy.app() / "plain.screen.lisp"; // not the app's entry: a screen on its own
	writeLisp(lisp, readFile(jsonFile));
	std::filesystem::copy_file(jsonFile, copy.app() / "plain.screen.json");

	const std::string fromJson =
	    Screen::fromFile(copy.app() / "plain.screen.json", copy.data(), Screen::History::InMemory).documentJson();
	CHECK(Screen::fromFile(lisp, copy.data(), Screen::History::InMemory).documentJson() == fromJson);
}

TEST_CASE("notation: an app may mix Lisp and JSON files, and loads as the same document") {
	const Copy copy("flow");
	const std::string before = loadedApp(copy.app());

	// The entry in Lisp, naming one include written in Lisp; the other includes, the models and seeds stay JSON
	json entry = readFile(copy.app() / "index.screen.json");
	for(json& include : entry["include"])
		if(include == "board.json") include = "board.lisp";
	writeLisp(copy.app() / "index.screen.lisp", entry);
	writeLisp(copy.app() / "board.lisp", readFile(copy.app() / "board.json"));
	std::filesystem::remove(copy.app() / "index.screen.json");
	std::filesystem::remove(copy.app() / "board.json");
	json manifest = readFile(copy.app() / "app.json");
	manifest["entry"] = "index.screen.lisp";
	std::ofstream(copy.app() / "app.json") << manifest.dump();

	CHECK(loadedApp(copy.app()) == before);
	// Opened by its entry's file, as the host opens it
	CHECK(Screen::fromFile(copy.app() / "index.screen.lisp", copy.data(), Screen::History::InMemory).documentJson() ==
	      before);
}

TEST_CASE("notation: a mistake in a Lisp include names the file, the line and the column") {
	const Copy copy("sign-in");
	const std::filesystem::path broken = copy.app() / "broken.lisp";
	std::ofstream(broken) << "{:styles {:card {:color red}}\n :tokens {:a 1}";
	json screen = readFile(copy.app() / "index.screen.json");
	screen["include"] = json::array({"broken.lisp"});
	writeLisp(copy.app() / "with-include.screen.lisp", screen);
	try {
		static_cast<void>(
		    Screen::fromFile(copy.app() / "with-include.screen.lisp", copy.data(), Screen::History::InMemory));
		FAIL("it loaded");
	} catch(const affresco::ScreenError& error) {
		CHECK(std::string(error.what())
		          .ends_with(
		              "broken.lisp: line 2, column 16: expected '}' to close the object opened at line 1, column 1"));
	}
}
