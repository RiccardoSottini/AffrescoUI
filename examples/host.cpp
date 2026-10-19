/// @file
/// @brief affrescoui_host: shows a screen file and lets other programs change it while it runs.
//
//   affrescoui_host [file.screen.json] [--socket DIR] [--mode default|acceptEdits|plan] [--auto-accept]
//               [--plugin changes.json ...]
//
// The window only renders: it has no idea where changes come from. Programs connect over two local sockets in DIR
// (default /tmp/affrescoui-<uid>):
//   studio.sock   the user's client (affrescoui_studio): author "studio", which the template lets review changes
//   agent.sock    an AI agent's MCP server (mcp/affrescoui_mcp.py): author "agent", whose changes wait for review
// What each author may do is up to the screen's "permissions"; --auto-accept is --mode acceptEdits. Default screen:
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

int main(int argc, char** argv) {
	std::string file = AFFRESCOUI_EXAMPLES_ASSETS "/screens/kanban.screen.json";
	std::filesystem::path socketDir = defaultSocketDir();
	// For this run only: without --mode, the one a reviewer last chose, which the screen's history keeps
	std::optional<affresco::Mode> mode;
	std::vector<std::string> plugins;

	for(int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if(arg == "--socket" && i + 1 < argc) socketDir = argv[++i];
		else if(arg == "--plugin" && i + 1 < argc) plugins.emplace_back(argv[++i]);
		else if(arg == "--auto-accept") mode = affresco::Mode::AcceptEdits;
		else if(arg == "--mode" && i + 1 < argc && affresco::modeFromString(argv[i + 1]))
			mode = *affresco::modeFromString(argv[++i]);
		else if(!arg.starts_with("--")) file = arg;
		else {
			std::cerr << "usage: affrescoui_host [file.screen.json] [--socket DIR] [--mode default|acceptEdits|plan] "
			             "[--auto-accept] [--plugin changes.json]\n";
			return 2;
		}
	}

	try {
		affresco::Application app;
		affresco::Screen screen = affresco::Screen::fromFile(file);
		screen.setPlaceholderTypes(true); // custom types of other apps show as plain panels
		examples::registerCodeEditor(screen); // the Forge template's "code" type (application layer)
		if(mode) screen.setModeForThisRun(*mode);
		screen.createWindow(app);

		for(const std::string& plugin : plugins) {
			std::ifstream in(plugin);
			std::stringstream text;
			text << in.rdbuf();
			std::string error;
			if(!in || !screen.importChanges(text.str(), &error)) throw std::runtime_error(plugin + ": " + error);
			std::cerr << "plugin applied: " << plugin << " (revision " << screen.revision() << ")\n";
		}

		// A plain screenshot run (AFFRESCOUI_SCREENSHOT without AFFRESCOUI_SCREENSHOT_AFTER) draws one frame and exits
		if(!std::getenv("AFFRESCOUI_SCREENSHOT") || std::getenv("AFFRESCOUI_SCREENSHOT_AFTER")) {
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
