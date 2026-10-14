// Opens any sGLGUI screen file: write a UI in JSON and see it, without writing C++.
//   screen_viewer path/to/file.screen.json
// Custom types show as plain panels, and actions are printed when triggered.

#include <sgl/sgl.hpp>

#include <iostream>

int main(int argc, char** argv) {
	if(argc != 2) {
		std::cerr << "usage: screen_viewer <file.screen.json>\n";
		return 2;
	}

	try {
		sgl::Application app;

		sgl::Screen screen = sgl::Screen::fromFile(argv[1]);
		screen.setPlaceholderTypes(true);
		screen.onUnhandledAction([](const std::string& action, sgl::Panel& sender) {
			std::cout << "action '" << action << "' from '" << sender.id() << "'\n";
		});

		screen.createWindow(app);
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
