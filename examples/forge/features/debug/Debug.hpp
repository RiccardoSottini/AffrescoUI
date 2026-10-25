#pragma once

/// @file
/// @brief Forge's debugger (features/debug/debug.json): the debug adapters' grants. The adapters run through the
/// `dap` provider; what they send is bounded there (DapProtocol.hpp).

#include "Forge.hpp"

namespace forge {

/// @brief Grants the debug adapters found on PATH: the program `lldb-dap`, and `debugpy` (`python3 -m
/// debugpy.adapter`).
/// @param screen The screen.
/// @param grantAdapters Whether to grant the adapters found on PATH; the scenarios grant none, so none starts.
void registerDebug(affresco::Screen& screen, bool grantAdapters = true);

} // namespace forge
