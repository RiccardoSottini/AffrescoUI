#pragma once

// Where the examples meet: the host listens there, the Studio and the MCP server (mcp/sglgui_mcp.py) connect there.
// Default: /tmp/sglgui-<uid>, readable only by this user.

#include <filesystem>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

inline std::filesystem::path defaultSocketDir() {
	return std::filesystem::path("/tmp") / ("sglgui-" + std::to_string(getuid()));
}

inline void createSocketDir(const std::filesystem::path& dir) {
	std::filesystem::create_directories(dir);
	chmod(dir.c_str(), S_IRWXU);
}
