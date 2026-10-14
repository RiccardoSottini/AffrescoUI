// Opens any sGLGUI screen file: write a UI in JSON and see it, without writing C++.
//   screen_viewer path/to/file.screen.json [--brain "command" | --stdio] [--plugin changes.json ...]
// Custom types show as plain panels, and actions nobody handles are reported on stderr.

#include "AppOptions.hpp"

#include <sgl/sgl.hpp>

#include <iostream>
#include <memory>

int main(int argc, char** argv) {
	const AppOptions options = AppOptions::parse(argc, argv);
	if(options.positional.size() != 1) {
		std::cerr
		    << "usage: screen_viewer <file.screen.json> [--brain \"command\" | --stdio] [--plugin changes.json]\n";
		return 2;
	}

	try {
		sgl::Application app;
		std::unique_ptr<sgl::Channel> channel; // declared before the screen, so it outlives it

		sgl::Screen screen = sgl::Screen::fromFile(options.positional.front());
		screen.setPlaceholderTypes(true);
		screen.onUnhandledAction([](const std::string& action, sgl::Panel& sender) {
			std::cerr << "action '" << action << "' from '" << sender.id() << "'\n"; // stderr: stdout may be the stream
		});

		screen.createWindow(app);
		options.connect(app, screen, channel);
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
