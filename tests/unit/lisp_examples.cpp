/// @file
/// @brief Each example holds its app twice, in a json and a lisp folder: both load as exactly the same document, and
/// the lisp folder's app is in the Lisp notation.

#include <affresco/affresco.hpp>
#include <affresco/format/App.hpp>
#include <affresco/notation/Notation.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using affresco::Screen;

TEST_CASE("lisp examples: every example is there in both notations, and loads as the same document") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int apps = 0;
	for(const auto& entry : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_regular_file(entry.path() / "json" / "app.json")) continue;
		CAPTURE(entry.path().filename().string());
		REQUIRE(std::filesystem::is_regular_file(entry.path() / "lisp" / "app.lisp"));
		const Screen written = Screen::fromApp(entry.path() / "lisp", {}, Screen::History::InMemory);
		REQUIRE(written.app() != nullptr);
		CHECK(written.app()->entry.ends_with(".lisp"));
		CHECK(written.documentJson() ==
		      Screen::fromApp(entry.path() / "json", {}, Screen::History::InMemory).documentJson());
		++apps;
	}
	CHECK(apps >= 14);
}

TEST_CASE("lisp examples: a lisp folder holds Lisp, but for scenarios and a signed catalogue") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int documents = 0;
	for(const auto& example : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_directory(example.path() / "lisp")) continue;
		for(const auto& file : std::filesystem::recursive_directory_iterator(example.path() / "lisp")) {
			if(!file.is_regular_file() || file.path().extension() != ".json") {
				documents += file.is_regular_file() ? 1 : 0;
				continue;
			}
			CAPTURE(file.path().string());
			const std::string relative = file.path().lexically_relative(example.path() / "lisp").generic_string();
			CHECK((relative.ends_with(".scenarios.json") || relative.starts_with("catalog/")));
		}
	}
	CHECK(documents > 100);
}

TEST_CASE("lisp examples: a file found by its name reads the same in either notation") {
	namespace notation = affresco::notation;
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	const std::filesystem::path written = examples / "forge" / "json" / "features" / "palette";
	const std::filesystem::path lisp = examples / "forge" / "lisp" / "features" / "palette";
	CHECK(notation::namedDocument(written, "feature").filename() == "feature.json");
	CHECK(notation::namedDocument(lisp, "feature").filename() == "feature.lisp");
	CHECK(notation::namedDocument(lisp, "missing").filename() ==
	      "missing.json"); // the problem names what was looked for
	CHECK(notation::isNamedDocument("a/app.lisp", "app"));
	CHECK(notation::isNamedDocument("a/app.json", "app"));
	CHECK_FALSE(notation::isNamedDocument("a/app.txt", "app"));
	CHECK_FALSE(notation::isNamedDocument("a/myapp.lisp", "app"));
	const auto read = [](const std::filesystem::path& file) {
		std::ifstream stream(file, std::ios::binary);
		const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		return notation::readDocument(text, file);
	};
	CHECK(read(notation::namedDocument(lisp, "feature")) == read(notation::namedDocument(written, "feature")));
	CHECK(read(examples / "tempera" / "lisp" / "sorter" / "sorter.adapter.lisp") ==
	      read(examples / "tempera" / "json" / "sorter" / "sorter.adapter.json"));
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
	const std::filesystem::path assets = std::filesystem::absolute("/a/flow/assets"); // on Windows, with its drive
	CHECK(affresco::format::notationAssets("/a/flow/json") == assets);
	CHECK(affresco::format::notationAssets("/a/flow/lisp/") == assets);
	CHECK(affresco::format::notationAssets("/a/flow/json/.") == assets);
	CHECK(affresco::format::notationAssets("/a/flow").empty());
	CHECK(affresco::format::notationAssets("").empty());
	CHECK(affresco::format::notationAssets("/a/flow/parts").empty());
}
