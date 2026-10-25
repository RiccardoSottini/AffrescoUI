/// @file
/// @brief Forge's terminal (Terminal.hpp): the shell its `terminal` nodes may run.

#include "features/terminal/Terminal.hpp"

#include <affresco/core/Services.hpp>

#include <cstdlib>
#include <string>

namespace forge {

namespace fs = std::filesystem;

namespace {

/// @brief The user's shell, as an absolute path.
/// @return $SHELL if it is an absolute path to a file, else /bin/sh; on Windows %COMSPEC%, else cmd.exe.
fs::path shellProgram() {
#ifdef _WIN32
	std::size_t length = 0;
	if(getenv_s(&length, nullptr, 0, "COMSPEC") == 0 && length > 0) {
		std::string value(length, '\0');
		if(getenv_s(&length, value.data(), value.size(), "COMSPEC") == 0) {
			value.resize(length > 0 ? length - 1 : 0);
			if(fs::path(value).is_absolute()) return value;
		}
	}
	return "C:\\Windows\\System32\\cmd.exe";
#else
	const char* shell = std::getenv("SHELL"); // NOLINT(concurrency-mt-unsafe): read once, at launch
	std::error_code error;
	if(shell && fs::path(shell).is_absolute() && fs::is_regular_file(shell, error)) return shell;
	return "/bin/sh";
#endif
}

} // namespace

void grantTerminal(affresco::Screen& screen, const fs::path& folder) {
#ifdef _WIN32
	screen.services().allowProcess("shell", shellProgram());
#else
	screen.services().allowProcess("shell", shellProgram(), {"-l"});
#endif
	screen.services().addRoot("terminal", folder.empty() ? fs::current_path() : folder);
}

} // namespace forge
