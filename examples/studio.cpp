/// @file
/// @brief affrescoui_studio: a client for a running affrescoui_host.
//
// Ask an AI agent for changes in plain words, apply JSON Patches yourself, review the agent's proposals (preview them
// live in the host, accept, "always allow", reject), pick the mode, revoke grants, browse, revert and restore the
// history.
//
//   affrescoui_studio [--socket DIR]      (default /tmp/affrescoui-<uid>, where affrescoui_host listens)
//
// The Studio is itself an AffrescoUI screen (assets/screens/studio.screen.json): layout, behaviour and the connection
// to the host. Its "channels" section connects to studio.sock (and reconnects); every message from the host runs the
// screen's "host:<type>" action, and its "send" steps go to the host. This program only opens it.

#include "SocketDir.hpp"

#include <affresco/affresco.hpp>

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
	std::filesystem::path socketDir = defaultSocketDir();
	for(int i = 1; i < argc; ++i) {
		if(std::string_view(argv[i]) == "--socket" && i + 1 < argc) socketDir = argv[++i];
		else {
			std::cerr << "usage: affrescoui_studio [--socket DIR]\n";
			return 2;
		}
	}

	try {
		affresco::Application app;
		affresco::Screen studio = affresco::Screen::fromFile(AFFRESCOUI_EXAMPLES_ASSETS "/screens/studio.screen.json");
		studio.setSocketDirectory(socketDir);
		studio.createWindow(app); // connects the file's channels
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_studio: " << error.what() << '\n';
		return 1;
	}
}
