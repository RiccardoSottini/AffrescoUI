#pragma once

/// @file
/// @brief detail::writeStateFile: writes a screen's "persist" state file, readable by this user only when it is new.

#include <filesystem>
#include <cstddef>
#include <fstream>
#include <string>
#include <system_error>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace affresco::detail {

/// @brief Writes a screen's "persist" state file, replacing what it holds.
///
/// The state is the screen's, as its history is, so a new file is its owner's only (`0600`) whatever the process's
/// umask, as the history's files are. A file that is already there keeps its permissions. It is written through
/// the descriptor opened here, which never follows a link and must be a plain file of this user: the data folder may
/// sit in a shared place, and a link planted in the state file's place would have the state written over its target.
/// @param file The file; its folder must exist.
/// @param text What it holds.
/// @return Whether it was written; false for a link or anything but a plain file of this user.
[[nodiscard]] inline bool writeStateFile(const std::filesystem::path& file, const std::string& text) {
#ifndef _WIN32
	const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
	if(descriptor < 0) return false;
	struct stat status {};
	bool written = ::fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode) && status.st_uid == ::geteuid() &&
	               ::ftruncate(descriptor, 0) == 0;
	for(std::size_t offset = 0; written && offset < text.size();) {
		const ::ssize_t count = ::write(descriptor, text.data() + offset, text.size() - offset);
		if(count < 0 && errno == EINTR) continue;
		written = count > 0;
		offset += written ? static_cast<std::size_t>(count) : 0;
	}
	return ::close(descriptor) == 0 && written;
#else
	std::error_code error;
	if(std::filesystem::is_symlink(file, error)) return false;
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	return static_cast<bool>(out << text);
#endif
}

} // namespace affresco::detail
