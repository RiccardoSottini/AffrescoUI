/// @file
/// @brief Forge's terminal (Terminal.hpp): the shell it may run, and the `terminal` type and natives it registers. The
/// sessions are in TerminalSessions.cpp, the widget in TerminalView.cpp.

#include "features/terminal/Terminal.hpp"

#include "features/terminal/TerminalSessions.hpp"
#include "features/terminal/TerminalView.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/core/Services.hpp>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

namespace forge {

namespace fs = std::filesystem;
using affresco::json;
using terminal::TerminalSessions;
using terminal::TerminalView;

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

/// @brief A call's `id` argument.
/// @param arguments The arguments, as JSON text.
/// @return The id, or "".
std::string idOf(const std::string& arguments) {
	const json with = affresco::parseUntrusted(arguments);
	return with.is_object() && with.contains("id") && with["id"].is_string() ? with["id"].get<std::string>() : "";
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

void registerTerminal(affresco::Screen& screen, affresco::Executor* executor) {
	const auto sessions = std::make_shared<TerminalSessions>(screen, executor);
	screen.registerType(
	    "terminal", [sessions] { return std::make_unique<TerminalView>(sessions); },
	    {"session", "font", "textColor", "cursorColor", "scrollback", "allowTitle"},
	    [&screen](affresco::Panel& panel, const std::string& propertiesJson) {
		    dynamic_cast<TerminalView&>(panel).apply(screen, json::parse(propertiesJson)); // the factory made it
	    });
	screen.onCall("forge.terminal.close", [sessions](const std::string& arguments) {
		sessions->close(idOf(arguments));
		return json::object().dump();
	});
	screen.onCall("forge.terminal.clear", [sessions](const std::string& arguments) {
		sessions->clear(idOf(arguments));
		return json::object().dump();
	});
	screen.onCall("forge.terminal.run", [sessions](const std::string& arguments) {
		const json with = affresco::parseUntrusted(arguments);
		if(!with.is_object() || !with.contains("text") || !with["text"].is_string())
			throw std::runtime_error("forge.terminal.run takes { id, text }");
		sessions->run(idOf(arguments), with["text"].get<std::string>());
		return json::object().dump();
	});
	screen.onCall("forge.terminal.info",
	              [sessions](const std::string& arguments) { return sessions->info(idOf(arguments)).dump(); });
}

} // namespace forge
