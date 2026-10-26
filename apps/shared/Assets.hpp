#pragma once

/// @file
/// @brief Where the examples find their screens and fonts: the examples folder of the source tree they were built
/// from, or, in a downloaded release (tools/release), the "assets" folder next to the program, laid out the same way
/// (on macOS, in an application bundle, the bundle's Contents/Resources/assets).

#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>

#include <cstdint>
#endif

/// @brief The file of the running program.
/// @return Its path, or an empty one if the system doesn't say.
inline std::filesystem::path programFile() {
#ifdef _WIN32
	std::vector<wchar_t> path(32768);
	const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
	return length > 0 && length < path.size() ? std::filesystem::path(path.data()) : std::filesystem::path();
#endif
#ifdef __APPLE__
	std::uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size); // fails, giving the size it needs
	std::vector<char> path(size + 1);
	return _NSGetExecutablePath(path.data(), &size) == 0 ? std::filesystem::path(path.data()) : std::filesystem::path();
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
	std::error_code error;
	const std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
	return error ? std::filesystem::path() : path;
#endif
}

/// @brief The examples' folder (one folder per template, shared/ for the fonts): the one in the source tree they
/// were built from (AFFRESCOUI_EXAMPLES_ASSETS) while it is there, else "assets" next to the program, else, for a
/// program inside a macOS application bundle (Contents/MacOS/<program>), the bundle's Contents/Resources/assets.
/// @return The folder.
inline std::filesystem::path assetsDirectory() {
	std::filesystem::path built = AFFRESCOUI_EXAMPLES_ASSETS;
	std::error_code error;
	if(std::filesystem::exists(built / "shared", error)) return built;
	const std::filesystem::path folder = programFile().parent_path();
	const std::filesystem::path beside = folder / "assets";
	if(std::filesystem::exists(beside / "shared", error)) return beside;
	// A bundle keeps what isn't code in Resources: a signed bundle's MacOS folder may hold only programs
	const std::filesystem::path bundled = folder.parent_path() / "Resources" / "assets";
	if(folder.filename() == "MacOS" && std::filesystem::exists(bundled / "shared", error)) return bundled;
	return beside;
}
