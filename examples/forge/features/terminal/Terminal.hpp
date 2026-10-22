#pragma once

/// @file
/// @brief Forge's terminal: the `terminal` type, a VT terminal (TerminalModel.hpp) on a pty of the `terminal` service
/// (the `pty` provider), and the natives its tabs use. The tabs and the panel are in terminal.json.

#include <affresco/affresco.hpp>
#include <affresco/core/Executor.hpp>

#include <cstddef>
#include <filesystem>

namespace forge {

/// @brief The most terminals open at once (the `pty` provider's limit).
constexpr std::size_t mostTerminals = 8;

/// @brief Lets the `terminal` service run the user's shell in a folder: the program `shell` is $SHELL when it is an
/// absolute path to a file (else /bin/sh), started as a login shell; on Windows, %COMSPEC% (else cmd.exe). The root
/// `terminal` is the folder.
/// @param screen The screen.
/// @param folder Where terminals start; empty for the current folder.
/// @throws std::exception If the folder doesn't exist.
void grantTerminal(affresco::Screen& screen, const std::filesystem::path& folder);

/// @brief Registers the `terminal` type and the terminal's native functions, for `call` steps.
///
/// A `terminal` node shows the session its `session` property names, made at its first use and kept while the node
/// is away (another tab shows): at most mostTerminals. Its properties: `session`, `font` (a name in "fonts"),
/// `textColor`, `cursorColor`, `scrollback` (lines, at most 10000) and `allowTitle` (whether the program may set the
/// session's title; clipboard writes and the bell are always dropped). Resizing the node resizes the pty. Keys go to
/// the program, except those with Super or with Control and Shift together, which stay shortcuts; Shift+Page Up and
/// Down, and the wheel, scroll back.
///   - `forge.terminal.close` `{ "id" }`: ends the session's program and forgets the session;
///   - `forge.terminal.clear` `{ "id" }`: clears the scrollback, and asks the program to redraw (Ctrl+L);
///   - `forge.terminal.run` `{ "id", "text" }`: types the line and a newline into the session, or, if no view has shown
///     it yet, when one first does (the shell runs it once it starts);
///   - `forge.terminal.info` `{ "id" }`: `{ "exists", "running", "title", "status", "code" }`.
/// @param screen The screen.
/// @param executor The main thread's executor (outlives the screen), or nullptr to start no program (the status says
/// so): for headless checks.
void registerTerminal(affresco::Screen& screen, affresco::Executor* executor);

} // namespace forge
