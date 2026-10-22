/// @file
/// @brief Locales: the one the computer's user chose (CFLocale on macOS, the user's locale name on Windows, the
/// environment elsewhere).

#include "affresco/format/Locale.hpp"

#include "Locale.hpp"

#include "affresco/core/Portable.hpp"

#include <array>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace affresco::format {

std::string systemLocale() {
#if defined(__APPLE__)
	std::array<char, 64> name{};
	if(CFLocaleRef locale = CFLocaleCopyCurrent()) {
		const auto identifier = static_cast<CFStringRef>(CFLocaleGetValue(locale, kCFLocaleIdentifier));
		const bool read = identifier && CFStringGetCString(identifier, name.data(), static_cast<CFIndex>(name.size()),
		                                                   kCFStringEncodingUTF8);
		CFRelease(locale);
		if(read) return detail::normalizeTag(name.data());
	}
#elif defined(_WIN32)
	std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> name{};
	if(GetUserDefaultLocaleName(name.data(), static_cast<int>(name.size())) > 0) {
		std::string tag;
		for(const wchar_t character : name) {
			if(character == 0 || character > 0x7F) break;
			tag += static_cast<char>(character);
		}
		return detail::normalizeTag(tag);
	}
#endif
	for(const char* variable : {"LC_ALL", "LC_MESSAGES", "LANG"})
		if(const auto value = environmentVariable(variable); value && !value->empty())
			return detail::normalizeTag(*value);
	return {};
}

} // namespace affresco::format
