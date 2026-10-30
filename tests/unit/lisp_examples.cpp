/// @file
/// @brief Each example holds its app twice, in a json and a lisp folder: both load as exactly the same document, and
/// the lisp folder's app is in the Lisp notation.

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

using affresco::Screen;

TEST_CASE("lisp examples: every example is there in both notations, and loads as the same document") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int apps = 0;
	for(const auto& entry : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_regular_file(entry.path() / "json" / "app.json")) continue;
		CAPTURE(entry.path().filename().string());
		REQUIRE(std::filesystem::is_regular_file(entry.path() / "lisp" / "app.json"));
		const Screen written = Screen::fromApp(entry.path() / "lisp", {}, Screen::History::InMemory);
		REQUIRE(written.app() != nullptr);
		CHECK(written.app()->entry.ends_with(".lisp"));
		CHECK(written.documentJson() ==
		      Screen::fromApp(entry.path() / "json", {}, Screen::History::InMemory).documentJson());
		++apps;
	}
	CHECK(apps >= 14);
}
