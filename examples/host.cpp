/// @file
/// @brief affrescoui_host: shows a screen file and lets other programs change it while it runs.
//
//   affrescoui_host [file.screen.json] [--socket DIR] [--data DIR] [--mode default|acceptEdits|plan] [--auto-accept]
//               [--plugin changes.json ...]
//
// The window only renders: it has no idea where changes come from. Programs connect over two local sockets in DIR
// (default $AFFRESCOUI_SOCKET_DIR, else /tmp/affrescoui-<uid>, or %LOCALAPPDATA%\affrescoui\sockets on Windows):
//   studio.sock   the user's client (affrescoui_studio): author "studio", which the template lets review changes
//   agent.sock    an AI agent's MCP server (mcp/affrescoui_mcp.py): author "agent", whose changes wait for review
// What each author may do is up to the screen's "permissions"; --auto-accept is --mode acceptEdits, for this run only.
// The screen's history and "persist" state are kept in --data DIR (default $AFFRESCOUI_DATA_DIR, else ~/.affrescoui),
// and a plugin is applied once: the history remembers it. Default screen:
// the Kanban board template; the other templates are in assets/screens (smart-home, music-player, code-editor,
// habit-tracker).

#include "CodeEditor.hpp"
#include "SocketDir.hpp"

#include <affresco/affresco.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

/// @brief Whether an environment variable is set (getenv_s on Windows, where getenv is deprecated).
/// @param name The variable.
/// @return true if it is.
bool environmentHas(const char* name) {
#ifdef _WIN32
	std::size_t length = 0; // the size its value needs, 0 if it is not set
	return getenv_s(&length, nullptr, 0, name) == 0 && length > 0;
#else
	return std::getenv(name) != nullptr;
#endif
}

/// @brief Applies the --plugin change sets, skipping those the screen's history already has from an earlier start.
/// @param screen The screen.
/// @param plugins The change set files, in order.
/// @throws std::runtime_error If one can't be read or applied.
void applyPlugins(affresco::Screen& screen, const std::vector<std::string>& plugins) {
	for(const std::string& plugin : plugins) {
		std::ifstream in(plugin);
		std::stringstream text;
		text << in.rdbuf();
		std::string error;
		if(in && screen.hasImported(text.str())) {
			std::cerr << "plugin already applied: " << plugin << '\n';
			continue;
		}
		if(!in || !screen.importChanges(text.str(), &error)) throw std::runtime_error(plugin + ": " + error);
		std::cerr << "plugin applied: " << plugin << " (revision " << screen.revision() << ")\n";
	}
}

} // namespace

int main(int argc, char** argv) {
	std::string file = AFFRESCOUI_EXAMPLES_ASSETS "/screens/kanban.screen.json";
	std::filesystem::path socketDir = affresco::Screen::defaultSocketDirectory();
	std::filesystem::path dataDir = affresco::Screen::defaultDataDirectory();
	// For this run only: without --mode, the one a reviewer last chose, which the screen's history keeps
	std::optional<affresco::Mode> mode;
	std::vector<std::string> plugins;

	for(int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if(arg == "--socket" && i + 1 < argc) socketDir = argv[++i];
		else if(arg == "--data" && i + 1 < argc) dataDir = argv[++i];
		else if(arg == "--plugin" && i + 1 < argc) plugins.emplace_back(argv[++i]);
		else if(arg == "--auto-accept") mode = affresco::Mode::AcceptEdits;
		else if(arg == "--mode" && i + 1 < argc && affresco::modeFromString(argv[i + 1]))
			mode = *affresco::modeFromString(argv[++i]);
		else if(!arg.starts_with("--")) file = arg;
		else {
			std::cerr << "usage: affrescoui_host [file.screen.json] [--socket DIR] [--data DIR] "
			             "[--mode default|acceptEdits|plan] [--auto-accept] [--plugin changes.json]\n";
			return 2;
		}
	}

	try {
		affresco::Application app;
		// A plain screenshot run (AFFRESCOUI_SCREENSHOT without AFFRESCOUI_SCREENSHOT_AFTER) draws one frame and exits.
		// It shows the file as it is, so it neither loads nor writes the screen's history.
		const bool firstFrameOnly =
		    environmentHas("AFFRESCOUI_SCREENSHOT") && !environmentHas("AFFRESCOUI_SCREENSHOT_AFTER");
		using History = affresco::Screen::History;
		affresco::Screen screen =
		    affresco::Screen::fromFile(file, dataDir, firstFrameOnly ? History::InMemory : History::OnDisk);
		screen.setPlaceholderTypes(true);     // custom types of other apps show as plain panels
		examples::registerCodeEditor(screen); // the Forge template's "code" type (application layer)
		if(mode) screen.setModeForThisRun(*mode);
		screen.createWindow(app);
		applyPlugins(screen, plugins);

		if(!firstFrameOnly) {
			createSocketDir(socketDir);
			screen.listen(app, socketDir / "studio.sock", "studio");
			screen.listen(app, socketDir / "agent.sock", "agent");
			std::cerr << "affrescoui_host: listening in " << socketDir.string() << " (studio.sock, agent.sock)"
			          << ", mode " << affresco::toString(screen.versions().mode()) << '\n';
		}
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_host: " << error.what() << '\n';
		return 1;
	}
}
