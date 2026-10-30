/// @file
/// @brief Each example holds its app twice, in a json and a lisp folder: both load as exactly the same document, and
/// the lisp folder's app is in the Lisp notation.

#include <affresco/affresco.hpp>
#include <affresco/format/App.hpp>

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

TEST_CASE("lisp examples: the two notations share one copy of an example's assets") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int assets = 0;
	for(const auto& example : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_directory(example.path() / "lisp")) continue;
		assets += std::filesystem::is_directory(example.path() / "assets") ? 1 : 0;
		for(const std::string notation : {"json", "lisp"})
			for(const auto& file : std::filesystem::recursive_directory_iterator(example.path() / notation)) {
				const std::string extension = file.path().extension().string();
				CAPTURE(file.path().string());
				// Documents and the scripts of the JSON folder, and the catalogue's line-ending rule: no picture or
				// sound
				CHECK((!file.is_regular_file() || extension == ".json" || extension == ".lisp" || extension == ".py" ||
				       file.path().filename() == ".gitattributes"));
			}
	}
	CHECK(assets >= 5);
}

TEST_CASE("lisp examples: only a json or a lisp folder reads the assets folder beside it") {
	CHECK(affresco::format::notationAssets("/a/flow/json") == std::filesystem::path("/a/flow/assets"));
	CHECK(affresco::format::notationAssets("/a/flow/lisp/") == std::filesystem::path("/a/flow/assets"));
	CHECK(affresco::format::notationAssets("/a/flow/json/.") == std::filesystem::path("/a/flow/assets"));
	CHECK(affresco::format::notationAssets("/a/flow").empty());
	CHECK(affresco::format::notationAssets("").empty());
	CHECK(affresco::format::notationAssets("/a/flow/parts").empty());
}
