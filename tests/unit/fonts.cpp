/// @file
/// @brief Unit tests for render's FontCache: fonts kept by file, size and fallbacks within a bound, the least recently
/// used dropped first and those in use never; shared font files; system fallbacks that can't be used left out.
///
/// The fonts are the examples' (AFFRESCOUI_FONTS_DIR) and the rendering check's subsets (AFFRESCOUI_TEST_FONTS_DIR).

#include <affresco/render/Font.hpp>
#include <affresco/render/FontCache.hpp>

#include <doctest/doctest.h>

#include <array>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

using affresco::Font;
using affresco::FontCache;

namespace {

/// @brief The examples' fonts.
std::filesystem::path fonts() { return AFFRESCOUI_FONTS_DIR; }
/// @brief The rendering check's font subsets.
std::filesystem::path testFonts() { return AFFRESCOUI_TEST_FONTS_DIR; }

} // namespace

TEST_CASE("font cache: the same file, size and fallbacks give the same font, and files are shared") {
	FontCache cache(FontCache::Limits{});
	const auto first = cache.load(fonts() / "Roboto-Regular.ttf", 16.0f);
	CHECK(cache.load(fonts() / "Roboto-Regular.ttf", 16.0f) == first);
	CHECK(cache.load(fonts() / "./Roboto-Regular.ttf", 16.0f) == first); // the same path, written differently
	const auto larger = cache.load(fonts() / "Roboto-Regular.ttf", 17.0f);
	CHECK(larger != first);
	CHECK(cache.size() == 2);

	// The file is counted once, though both fonts use it
	const auto file = static_cast<std::size_t>(std::filesystem::file_size(fonts() / "Roboto-Regular.ttf"));
	CHECK(cache.bytes() == file + first->memoryBytes() + larger->memoryBytes());
}

TEST_CASE("font cache: many fonts and sizes stay within the bound, and a font in use is never dropped") {
	const FontCache::Limits limits{.fonts = 6, .bytes = std::size_t{24} << 20};
	FontCache cache(limits);
	const auto held = cache.load(fonts() / "Roboto-Regular.ttf", 20.0f);

	const std::array files{fonts() / "Roboto-Regular.ttf", fonts() / "JetBrainsMono-Regular.ttf"};
	for(int size = 8; size < 108; ++size) {
		(void)cache.load(files[static_cast<std::size_t>(size) % 2], static_cast<float>(size));
		CHECK(cache.size() <= limits.fonts);
		CHECK(cache.bytes() <= limits.bytes);
	}
	CHECK(cache.load(fonts() / "Roboto-Regular.ttf", 20.0f) == held); // kept: something holds it

	// Fonts in use go past the bound; once they are let go, the next load drops them
	std::vector<std::shared_ptr<const Font>> inUse;
	for(int size = 30; size < 40; ++size) inUse.push_back(cache.load(files[0], static_cast<float>(size)));
	CHECK(cache.size() > limits.fonts);
	inUse.clear();
	(void)cache.load(files[1], 9.5f);
	CHECK(cache.size() <= limits.fonts);
	CHECK(cache.bytes() <= limits.bytes);
}

TEST_CASE("font cache: an optional system font that can't be used is left out, and a font's own fallback can't") {
	FontCache cache(FontCache::Limits{}, {fonts() / "README.md", testFonts() / "NotoSansSC-Subset.ttf"});
	const auto font = cache.load(fonts() / "Roboto-Regular.ttf", 16.0f);
	CHECK(font->hasGlyph(U'你')); // from the second system font

	const std::array broken{fonts() / "README.md"};
	CHECK_THROWS_AS((void)cache.load(fonts() / "Roboto-Regular.ttf", 16.0f, broken), std::runtime_error);
	for(const std::filesystem::path& system : FontCache::systemFonts()) CHECK(std::filesystem::is_regular_file(system));
}
