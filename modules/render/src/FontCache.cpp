/// @file
/// @brief FontCache: a bounded cache of fonts, and the system's fallback fonts.

#include "affresco/render/FontCache.hpp"

#include "text/FontFace.hpp"

#include <cstdlib>
#include <string_view>
#include <system_error>

namespace affresco {

namespace {

/// @brief Reads an environment variable.
/// @param name Its name.
/// @return Its value, or an empty string.
std::string environment(const char* name) {
#ifdef _MSC_VER
	char* value = nullptr;
	std::size_t length = 0;
	if(_dupenv_s(&value, &length, name) != 0 || !value) return {};
	std::string copy(value);
	std::free(value);
	return copy;
#else
	const char* value = std::getenv(name);
	return value ? value : "";
#endif
}

/// @brief The fonts systemFonts() looks for, for each platform. Colour emoji only in COLR form (Segoe UI Emoji):
/// Apple Color Emoji and Noto Color Emoji are bitmap fonts, which need libpng.
/// @return Their candidate files, existing or not.
std::vector<std::filesystem::path> systemFontCandidates() {
#ifdef _WIN32
	const std::string windows = environment("WINDIR");
	const std::filesystem::path fonts = std::filesystem::path(windows.empty() ? "C:\\Windows" : windows) / "Fonts";
	return {fonts / "msyh.ttc",    fonts / "YuGothR.ttc",  fonts / "malgun.ttf",  fonts / "Nirmala.ttf",
	        fonts / "segoeui.ttf", fonts / "seguisym.ttf", fonts / "seguiemj.ttf"};
#else
#ifdef __APPLE__
	const std::filesystem::path system = "/System/Library/Fonts";
	return {system / "Hiragino Sans GB.ttc", system / "AppleSDGothicNeo.ttc",
	        system / "GeezaPro.ttc",         system / "Kohinoor.ttc",
	        system / "Apple Symbols.ttf",    system / "Supplemental/Arial Unicode.ttf"};
#else
	const std::filesystem::path noto = "/usr/share/fonts/truetype/noto", cjk = "/usr/share/fonts/opentype/noto";
	return {cjk / "NotoSansCJK-Regular.ttc",       "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
	        noto / "NotoSansArabic-Regular.ttf",   noto / "NotoSansDevanagari-Regular.ttf",
	        noto / "NotoSansSymbols2-Regular.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
#endif
#endif
}

/// @brief The key of a font in the cache.
/// @param path Its file.
/// @param pixelHeight Its size.
/// @param fallbacks Its fallbacks.
/// @param style Its style.
/// @return File, size, style and fallbacks, in one string.
std::string keyOf(const std::filesystem::path& path, float pixelHeight,
                  std::span<const std::filesystem::path> fallbacks, FontStyle style) {
	std::string key = path.lexically_normal().string() + "@" + std::to_string(pixelHeight);
	if(style.bold) key += "+bold";
	if(style.italic) key += "+italic";
	for(const std::filesystem::path& fallback : fallbacks) key += "|" + fallback.lexically_normal().string();
	return key;
}

} // namespace

FontCache::FontCache()
    : FontCache(Limits{},
	            environment("AFFRESCOUI_SYSTEM_FONTS") == "1" ? systemFonts() : std::vector<std::filesystem::path>{}) {}

FontCache::FontCache(Limits limits, std::vector<std::filesystem::path> systemFallbacks)
    : limits_(limits), systemFallbacks_(std::move(systemFallbacks)), files_(std::make_unique<detail::FontFiles>()) {}

FontCache::~FontCache() = default;

std::shared_ptr<const Font> FontCache::load(const std::filesystem::path& path, float pixelHeight,
                                            std::span<const std::filesystem::path> fallbacks, FontStyle style) {
	const std::string key = keyOf(path, pixelHeight, fallbacks, style);
	for(auto entry = fonts_.begin(); entry != fonts_.end(); ++entry)
		if(entry->key == key) {
			fonts_.splice(fonts_.begin(), fonts_, entry);
			return entry->font;
		}

	std::shared_ptr<const Font> font = Font::load(path, pixelHeight, fallbacks, systemFallbacks_, *files_, style);
	fonts_.push_front({key, font});
	trim(); // the new font is in use: the caller holds it
	return font;
}

std::size_t FontCache::bytes() const noexcept {
	std::size_t total = files_->bytes();
	for(const Entry& entry : fonts_) total += entry.font->memoryBytes();
	return total;
}

void FontCache::trim() {
	// From the least recently used; a font held elsewhere stays (use_count: the cache's own share is one)
	auto entry = fonts_.end();
	while(entry != fonts_.begin() && (fonts_.size() > limits_.fonts || bytes() > limits_.bytes)) {
		--entry;
		if(entry->font.use_count() == 1) entry = fonts_.erase(entry);
	}
}

std::vector<std::filesystem::path> FontCache::systemFonts() {
	std::vector<std::filesystem::path> found;
	for(std::filesystem::path& candidate : systemFontCandidates()) {
		std::error_code error;
		if(std::filesystem::is_regular_file(candidate, error)) found.push_back(std::move(candidate));
	}
	return found;
}

} // namespace affresco
