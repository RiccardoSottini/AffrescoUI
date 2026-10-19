/// @file
/// @brief affrescoui_studio: a client for a running affrescoui_host.
//
// Ask an AI agent for changes in plain words, apply JSON Patches yourself, review the agent's proposals (preview them
// live in the host, accept, "always allow", reject), pick the mode, revoke grants, browse, revert and restore the
// history.
//
//   affrescoui_studio [--socket DIR]      (default: where affrescoui_host listens; see host.cpp)
//
// The Studio is itself an AffrescoUI screen (assets/screens/studio.screen.json): layout, behaviour and the connection
// to the host. Its "channels" section connects to studio.sock (and reconnects); every message from the host runs the
// screen's "host:<type>" action, and its "send" steps go to the host. This program only opens it.

#include "Assets.hpp"
#include "SocketDir.hpp"

#include <affresco/affresco.hpp>

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
	std::filesystem::path socketDir = affresco::Screen::defaultSocketDirectory();
	std::filesystem::path dataDir = affresco::Screen::defaultDataDirectory();
	for(int i = 1; i < argc; ++i) {
		if(std::string_view(argv[i]) == "--socket" && i + 1 < argc) socketDir = argv[++i];
		else if(std::string_view(argv[i]) == "--data" && i + 1 < argc) dataDir = argv[++i];
		else {
			std::cerr << "usage: affrescoui_studio [--socket DIR] [--data DIR]\n";
			return 2;
		}
	}

	try {
		checkSocketDir(socketDir);
		affresco::Application app;
		affresco::Screen studio =
		    affresco::Screen::fromFile(assetsDirectory() / "screens" / "studio.screen.json", dataDir);
		studio.setSocketDirectory(socketDir);
		studio.createWindow(app); // connects the file's channels
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_studio: " << error.what() << '\n';
		return 1;
	}
}
