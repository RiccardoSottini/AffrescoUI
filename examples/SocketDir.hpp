#pragma once

/// @file
/// @brief Where the examples meet: the host listens there, the Studio and the MCP server (mcp/affrescoui_mcp.py)
/// connect there.

#include <filesystem>
#include <stdexcept>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

/// @brief The default socket directory.
/// @return /tmp/affrescoui-<uid>.
inline std::filesystem::path defaultSocketDir() {
	return std::filesystem::path("/tmp") / ("affrescoui-" + std::to_string(getuid()));
}

/// @brief Refuses a socket directory another user could control (its path in /tmp is easy to guess and take first).
/// @param dir The directory; nothing is checked if it doesn't exist yet.
/// @throws std::runtime_error If it is a symbolic link, not a directory, or not owned by this user.
inline void checkSocketDir(const std::filesystem::path& dir) {
	struct stat info {};
	if(lstat(dir.c_str(), &info) != 0) return;
	if(!S_ISDIR(info.st_mode) || info.st_uid != getuid())
		throw std::runtime_error(dir.string() + " is not a directory owned by this user: remove it or pass --socket DIR");
}

/// @brief Creates the socket directory, accessible only to this user.
/// @param dir The directory.
/// @throws std::runtime_error If it exists but another user could control it.
inline void createSocketDir(const std::filesystem::path& dir) {
	std::filesystem::create_directories(dir);
	checkSocketDir(dir);
	chmod(dir.c_str(), S_IRWXU);
}
