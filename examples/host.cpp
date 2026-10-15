/// @file
/// @brief sglgui_host: shows a screen file and lets other programs change it while it runs.
//
//   sglgui_host [file.screen.json] [--socket DIR] [--auto-accept] [--plugin changes.json ...]
//
// The window only renders: it has no idea where changes come from. Programs connect over two local sockets in DIR
// (default /tmp/sglgui-<uid>):
//   studio.sock   the user's client (sglgui_studio): author "studio", which the template lets review changes
//   agent.sock    an AI agent's MCP server (mcp/sglgui_mcp.py): author "agent", whose changes wait for review
// What each author may do is up to the screen's "permissions". Default screen: the Kanban board template; the other
// templates are in assets/screens (smart-home, music-player, code-editor, habit-tracker).

#include "SocketDir.hpp"

#include <sgl/sgl.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
	std::string file = SGLGUI_EXAMPLES_ASSETS "/screens/kanban.screen.json";
	std::filesystem::path socketDir = defaultSocketDir();
	bool autoAccept = false;
	std::vector<std::string> plugins;

	for(int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if(arg == "--socket" && i + 1 < argc) socketDir = argv[++i];
		else if(arg == "--plugin" && i + 1 < argc) plugins.emplace_back(argv[++i]);
		else if(arg == "--auto-accept") autoAccept = true;
		else if(!arg.starts_with("--")) file = arg;
		else {
			std::cerr
			    << "usage: sglgui_host [file.screen.json] [--socket DIR] [--auto-accept] [--plugin changes.json]\n";
			return 2;
		}
	}

	try {
		sgl::Application app;
		sgl::Screen screen = sgl::Screen::fromFile(file);
		screen.setPlaceholderTypes(true); // custom types of other apps show as plain panels
		screen.setAutoAccept(autoAccept);
		screen.createWindow(app);

		for(const std::string& plugin : plugins) {
			std::ifstream in(plugin);
			std::stringstream text;
			text << in.rdbuf();
			std::string error;
			if(!in || !screen.importChanges(text.str(), &error)) throw std::runtime_error(plugin + ": " + error);
			std::cerr << "plugin applied: " << plugin << " (revision " << screen.revision() << ")\n";
		}

		// A plain screenshot run (SGLGUI_SCREENSHOT without SGLGUI_SCREENSHOT_AFTER) draws one frame and exits
		if(!std::getenv("SGLGUI_SCREENSHOT") || std::getenv("SGLGUI_SCREENSHOT_AFTER")) {
			createSocketDir(socketDir);
			screen.listen(app, socketDir / "studio.sock", "studio");
			screen.listen(app, socketDir / "agent.sock", "agent");
			std::cerr << "sglgui_host: listening in " << socketDir.string() << " (studio.sock, agent.sock)"
			          << (autoAccept ? ", auto-accept on" : "") << '\n';
		}
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << "sglgui_host: " << error.what() << '\n';
		return 1;
	}
}
