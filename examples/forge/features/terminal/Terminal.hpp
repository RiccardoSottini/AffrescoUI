#pragma once

/// @file
/// @brief Forge's terminal: the shell the library's `terminal` nodes run on the `terminal` service (the `pty`
/// provider). The tabs and the panel are in terminal.json.

#include <affresco/affresco.hpp>

#include <filesystem>

namespace forge {

/// @brief Lets the `terminal` service run the user's shell in a folder: the program `shell` is $SHELL when it is an
/// absolute path to a file (else /bin/sh), started as a login shell; on Windows, %COMSPEC% (else cmd.exe). The root
/// `terminal` is the folder.
/// @param screen The screen.
/// @param folder Where terminals start; empty for the current folder.
/// @throws std::exception If the folder doesn't exist.
void grantTerminal(affresco::Screen& screen, const std::filesystem::path& folder);

} // namespace forge
