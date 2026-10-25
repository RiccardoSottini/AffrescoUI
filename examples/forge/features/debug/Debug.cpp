/// @file
/// @brief Forge's debug adapter grants (Debug.hpp).

#include "features/debug/Debug.hpp"
#include "features/lsp/Lsp.hpp" // findProgram

#include <affresco/core/Services.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <filesystem>

namespace forge {

void registerDebug(affresco::Screen& screen, bool grantAdapters) {
	if(!grantAdapters) return;
	if(const std::filesystem::path lldb = findProgram("lldb-dap"); !lldb.empty())
		screen.services().allowProcess("lldb-dap", lldb);
	if(const std::filesystem::path python = findProgram("python3"); !python.empty())
		screen.services().allowProcess("debugpy", python, {"-m", "debugpy.adapter"});
}

} // namespace forge
