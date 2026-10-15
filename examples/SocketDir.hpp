#pragma once

/// @file
/// @brief Where the examples meet: the host listens there, the Studio and the MCP server (mcp/sglgui_mcp.py)
/// connect there.

#include <filesystem>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

/// @brief The default socket directory.
/// @return /tmp/sglgui-<uid>.
inline std::filesystem::path defaultSocketDir() {
	return std::filesystem::path("/tmp") / ("sglgui-" + std::to_string(getuid()));
}

/// @brief Creates the socket directory, accessible only to this user.
/// @param dir The directory.
inline void createSocketDir(const std::filesystem::path& dir) {
	std::filesystem::create_directories(dir);
	chmod(dir.c_str(), S_IRWXU);
}
