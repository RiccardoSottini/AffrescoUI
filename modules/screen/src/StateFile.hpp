#pragma once

/// @file
/// @brief detail::writeStateFile: writes a screen's "persist" state file, readable by this user only when it is new.

#include <filesystem>
#include <fstream>
#include <string>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace affresco::detail {

/// @brief Writes a screen's "persist" state file, replacing what it holds.
///
/// The state is the screen's, as its history is, so a new file is its owner's only (`0600`) whatever the process's
/// umask, as the history's files are. A file that is already there keeps its permissions, and is still written in
/// place, through a link if it is one: the user may have chosen both.
/// @param file The file; its folder must exist.
/// @param text What it holds.
/// @return Whether it was written.
[[nodiscard]] inline bool writeStateFile(const std::filesystem::path& file, const std::string& text) {
#ifndef _WIN32
	const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600); // created before ofstream can
	if(descriptor < 0) return false;
	::close(descriptor);
#endif
	std::ofstream out(file);
	return static_cast<bool>(out << text);
}

} // namespace affresco::detail
