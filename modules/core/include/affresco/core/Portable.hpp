#pragma once

/// @file
/// @brief The C library calls whose portable spelling differs between MSVC and POSIX, in one place: the environment
/// and broken-down time.
///
/// MSVC deprecates `getenv`, `gmtime` and `localtime` (warning C4996, an error with warnings as errors) and has no
/// `gmtime_r` or `localtime_r`; POSIX has no `_dupenv_s`, `gmtime_s` or `localtime_s`. The thread-safe call of each
/// platform is used: `gmtime` and `localtime` share one buffer between threads.

#include <cstdlib>
#include <ctime>
#include <memory>
#include <optional>
#include <string>

namespace affresco {

/// @brief Reads an environment variable.
/// @param name The variable's name (a C string: no null bytes).
/// @return Its value, in the bytes the C library gives (the ANSI code page on Windows); nothing if it is not set.
[[nodiscard]] inline std::optional<std::string> environmentVariable(const std::string& name) {
#ifdef _WIN32
	char* value = nullptr;
	std::size_t length = 0;
	if(_dupenv_s(&value, &length, name.c_str()) != 0 || !value) return std::nullopt;
	const std::unique_ptr<char, void (*)(void*)> freer(value, [](void* memory) { std::free(memory); });
	return std::string(value);
#else
	const char* value = std::getenv(name.c_str());
	if(!value) return std::nullopt;
	return std::string(value);
#endif
}

/// @brief Breaks a time down in UTC.
/// @param time Seconds since 1970.
/// @return The fields; all zero if the time can't be represented.
[[nodiscard]] inline std::tm utcTime(std::time_t time) noexcept {
	std::tm fields{};
#ifdef _WIN32
	if(gmtime_s(&fields, &time) != 0) fields = {};
#else
	if(!gmtime_r(&time, &fields)) fields = {};
#endif
	return fields;
}

/// @brief Breaks a time down in the local time zone.
/// @param time Seconds since 1970.
/// @return The fields; all zero if the time can't be represented.
[[nodiscard]] inline std::tm localTime(std::time_t time) noexcept {
	std::tm fields{};
#ifdef _WIN32
	if(localtime_s(&fields, &time) != 0) fields = {};
#else
	if(!localtime_r(&time, &fields)) fields = {};
#endif
	return fields;
}

} // namespace affresco
