#pragma once

/// @file
/// @brief FontCache: fonts by file, size and fallbacks, within a bound; the least recently used that nothing uses go
/// first.

#include "affresco/render/Font.hpp"

#include <cstddef>
#include <filesystem>
#include <list>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace affresco {

/// @brief Loads fonts and keeps them for the next time the same file, size and fallbacks are asked for (e.g. by the
/// next revision of a screen), within a bound.
///
/// **The bound.** It keeps at most Limits::fonts fonts and Limits::bytes bytes: their atlas pages, glyphs and shaped
/// text (Font::memoryBytes()), and their font files, each counted once since fonts loaded here share them. When a
/// load goes past either, the least recently used fonts are dropped until it is back within both. A font something
/// else still holds (a screen's "fonts", a panel, a window) is in use and is never dropped, so the fonts in use can go
/// past the bound; once they are no longer used, the next load drops them.
///
/// **System fonts.** A cache can add fallbacks after every font's own: the system's fonts for scripts and symbols
/// (systemFonts()), when the host asks for them. The default constructor does when the environment variable
/// AFFRESCOUI_SYSTEM_FONTS is 1; otherwise no system font folder is read.
///
/// Main thread only, like the fonts.
class FontCache {
public:
	/// @brief How much a cache keeps of the fonts nothing else uses.
	struct Limits {
		std::size_t fonts = 32;                     ///< Fonts.
		std::size_t bytes = std::size_t{128} << 20; ///< Bytes of their atlases, glyphs, shaped text and files.
	};

	/// @brief A cache with the default limits, adding systemFonts() if AFFRESCOUI_SYSTEM_FONTS is 1.
	FontCache();
	/// @brief A cache.
	/// @param limits Its limits.
	/// @param systemFallbacks Fallbacks added after every font's own; one that can't be read or used is left out.
	explicit FontCache(Limits limits, std::vector<std::filesystem::path> systemFallbacks = {});
	/// @brief Drops its fonts; those still in use live on.
	~FontCache();
	FontCache(const FontCache&) = delete;            ///< Owns its fonts.
	FontCache& operator=(const FontCache&) = delete; ///< Owns its fonts.

	/// @brief Returns the font for a file, size and fallbacks: kept from before, or loaded now.
	/// @param path As Font::load().
	/// @param pixelHeight As Font::load().
	/// @param fallbacks As Font::load().
	/// @return The font.
	/// @throws std::runtime_error As Font::load(); the cache is then as it was.
	[[nodiscard]] std::shared_ptr<const Font> load(const std::filesystem::path& path, float pixelHeight,
	                                               std::span<const std::filesystem::path> fallbacks = {});

	/// @brief How many fonts it keeps now.
	[[nodiscard]] std::size_t size() const noexcept { return fonts_.size(); }
	/// @brief How many bytes the fonts it keeps hold, as the bound counts them.
	[[nodiscard]] std::size_t bytes() const noexcept;
	/// @brief Its limits.
	[[nodiscard]] const Limits& limits() const noexcept { return limits_; }

	/// @brief The system's fonts for the scripts and symbols common fonts lack (Chinese, Japanese, Korean, Arabic,
	/// Devanagari, symbols and colour emoji in COLR form), those of a fixed list that exist on this machine.
	/// @return Their files, in the order they are asked.
	[[nodiscard]] static std::vector<std::filesystem::path> systemFonts();

private:
	/// @brief A font and what it was loaded from.
	struct Entry {
		std::string key;                  ///< File, size and fallbacks.
		std::shared_ptr<const Font> font; ///< The font.
	};

	/// @brief Drops the least recently used fonts nothing else uses, until the cache is within its limits.
	void trim();

	Limits limits_;                                      ///< See limits().
	std::vector<std::filesystem::path> systemFallbacks_; ///< See the constructor.
	std::unique_ptr<detail::FontFiles> files_; ///< The files its fonts share. Declared before fonts_: freed after.
	std::list<Entry> fonts_;                   ///< The most recently used first.
};

} // namespace affresco
