/// @file
/// @brief What a screen may include (Screen.cpp, resolveIncludes()): a plain file of at most 16 MiB, and for an app a
/// file in its own folder or the "shared" folder beside it, never another file of the person's.

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

/// @brief Writes a file, making its folder.
/// @param path The file.
/// @param text What it holds.
void write(const std::filesystem::path& path, const std::string& text) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream(path) << text;
}

/// @brief Writes an app whose entry screen includes a file.
/// @param folder The app's folder.
/// @param include The include, as the screen names it.
void writeApp(const std::filesystem::path& folder, const std::string& include) {
	write(folder / "app.json", R"({ "format": "affrescoui-app", "version": 1, "id": "parts", "name": "Parts" })");
	write(folder / "index.screen.json", R"({ "format": "affrescoui-screen", "version": 2, "include": [")" + include +
	                                        R"("], "root": { "type": "label", "id": "hello", "text": "hello" } })");
}

/// @brief Why an app doesn't open.
/// @param folder The app's folder.
/// @return The error, or "" if it opens.
std::string refusal(const std::filesystem::path& folder) {
	try {
		const affresco::Screen screen = affresco::Screen::fromApp(folder, {});
		return "";
	} catch(const affresco::ScreenError& error) {
		return error.what();
	}
}

} // namespace

TEST_CASE("app includes: an app includes its own files and the shared folder's, never another file of the person's") {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "affrescoui-app-includes-test";
	std::filesystem::remove_all(root);
	const std::string part = R"({ "strings": { "en": { "hi": "hi" } } })";
	write(root / "apps" / "shared" / "kit.json", part);
	write(root / "apps" / "app" / "own.json", part);
	write(root / "private.json", part); // another file of the person's
	const std::filesystem::path app = root / "apps" / "app";

	for(const std::string include : {"own.json", "../shared/kit.json"}) {
		writeApp(app, include);
		CHECK_MESSAGE(refusal(app).empty(), include);
	}
	for(const std::string& include : {std::string("../../private.json"), (root / "private.json").generic_string()}) {
		writeApp(app, include);
		CHECK_MESSAGE(refusal(app).contains("outside the app's folder"), include);
	}
	// A link out is followed before it is checked, and a "shared" that is a link isn't the shared folder
	std::filesystem::create_symlink(root / "private.json", app / "link.json");
	writeApp(app, "link.json");
	CHECK(refusal(app).contains("outside the app's folder"));
	std::filesystem::remove_all(root / "apps" / "shared");
	std::filesystem::create_directory_symlink(root, root / "apps" / "shared");
	writeApp(app, "../shared/private.json");
	CHECK(refusal(app).contains("outside the app's folder"));
	// An include inside an included file is held to the same folders
	std::filesystem::remove(root / "apps" / "shared");
	write(app / "own.json", R"({ "include": ["../../private.json"] })");
	writeApp(app, "own.json");
	CHECK(refusal(app).contains("outside the app's folder"));
	std::filesystem::remove_all(root);
}

TEST_CASE("app includes: an include that isn't a plain file, or is over 16 MiB, is refused before it is read") {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "affrescoui-include-size-test";
	std::filesystem::remove_all(root);
	write(root / "big.json", std::string((16U << 20) + 1, ' '));
	const auto open = [&](const std::string& include) {
		try {
			write(root / "screen.json", R"({ "format": "affrescoui-screen", "version": 2, "include": [")" + include +
			                                R"("], "root": { "type": "label", "id": "x", "text": "x" } })");
			const affresco::Screen screen = affresco::Screen::fromFile(root / "screen.json", {});
			return std::string();
		} catch(const affresco::ScreenError& error) {
			return std::string(error.what());
		}
	};
	CHECK(open("big.json").contains("over 16 MiB"));
	CHECK(open(".").contains("is not a file"));
#ifndef _WIN32
	CHECK(open("/dev/zero").contains("is not a file"));
#endif
	std::filesystem::remove_all(root);
}
