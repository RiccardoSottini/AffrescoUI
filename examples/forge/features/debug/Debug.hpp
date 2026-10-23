#pragma once

/// @file
/// @brief Forge's debugger (features/debug/debug.json): the debug adapters' grants and the two natives the JSON can't
/// do itself. The adapters run through the `dap` provider; what they send is bounded there (DapProtocol.hpp).

#include "Forge.hpp"

#include <affresco/core/Json.hpp>

#include <filesystem>
#include <memory>
#include <string_view>

namespace forge {

/// @brief Reads the launch configurations of a folder's `.forge/launch.json`, in the subset Forge knows: `name`,
/// `type` (`lldb`, `lldb-dap`, `python` or `debugpy`), `request` (`launch` or `attach`), `program`, `args`, `cwd`,
/// `env`, `stopOnEntry`, `console` and, to attach, `pid` (lldb) or `port` (debugpy, on 127.0.0.1). Comments are
/// allowed; other keys are left out.
/// @param root The open folder, or empty.
/// @return `{ "configurations": [{ "name", "type": "lldb"|"python", "request", "arguments" }], "skipped" }`, where
/// `arguments` are the adapter's `launch` or `attach` arguments and `skipped` counts those Forge can't run; no
/// configurations when there is no folder or no file.
/// @throws std::runtime_error If the file isn't JSON or is too large.
[[nodiscard]] affresco::json launchConfigurations(const std::filesystem::path& root);

/// @brief Reads the tasks of a folder's `.forge/tasks.json`, in the subset Forge runs: `label`, `type` (`shell` or
/// `process`), `command`, `args` and `group` (`build` or `test`, or `{ "kind" }`). `${workspaceFolder}` becomes `.`;
/// a task with another variable is skipped. Comments are allowed.
/// @param root The open folder, or empty.
/// @return `{ "tasks": [{ "label", "line", "group" }], "skipped" }`, where `line` is what the task's terminal runs
/// (a process task's program and every argument single-quoted); no tasks when there is no folder or no file.
/// @throws std::runtime_error If the file isn't JSON or is too large.
[[nodiscard]] affresco::json taskList(const std::filesystem::path& root);

/// @brief The line a POSIX shell runs for an adapter's `runInTerminal` request: `cd '<cwd>' && exec env 'K=V'...
/// '<argument>'...`, every word single-quoted.
/// @param root The folder the debugger runs in; `cwd` must be inside it (empty: the folder itself).
/// @param request The request's arguments: `cwd`, `args` (text, the program first) and `env` (text values; null ones
/// are left out).
/// @return The line, without its newline.
/// @throws std::runtime_error If `cwd` leaves the folder, there is no program, or a word is not text, holds a control
/// character or the line would be too long.
[[nodiscard]] std::string terminalCommandLine(const std::filesystem::path& root, const affresco::json& request);

/// @brief Grants the debug adapters found on PATH and registers the debugger's natives:
///   - the program `lldb-dap`, and `debugpy` (`python3 -m debugpy.adapter`);
///   - the root `debug`: the open folder, or the current folder if none is open (a program an adapter asks to run in
///     a terminal runs in the terminal's shell, under its grant);
///   - `forge.debug.launchConfigurations`: launchConfigurations() of the open folder;
///   - `forge.tasks.list`: taskList() of the open folder;
///   - `forge.debug.commandLine` `{ "cwd", "args", "env" }`: `{ "line" }`, terminalCommandLine() in the root `debug`.
/// @param screen The screen.
/// @param workspace The open folder.
/// @param grantAdapters Whether to grant the adapters found on PATH; the scenarios grant none, so none starts.
void registerDebug(affresco::Screen& screen, const std::shared_ptr<Workspace>& workspace, bool grantAdapters = true);

} // namespace forge
