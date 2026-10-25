/// @file
/// @brief The fuzzing README lists every fuzz target that is built: its table is how a reader learns what is fuzzed.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_CASE("fuzz: the README's table has a row for every target") {
	const std::filesystem::path folder = std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY).parent_path() / "tests/fuzz";
	std::ifstream file(folder / "README.md");
	REQUIRE(file);
	const std::string readme{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	int targets = 0;
	for(const auto& entry : std::filesystem::directory_iterator(folder)) {
		const std::string name = entry.path().stem().string();
		if(entry.path().extension() != ".cpp" || name == "Themes") continue; // Themes.cpp is built into every target
		++targets;
		CHECK_MESSAGE(readme.contains("| `" + name + "` |"), name);
	}
	CHECK(targets > 50);
}
