/// @file
/// @brief affrescoui_surfaces: the GPU surface demo. Shows examples/surfaces/surfaces.screen.json (or a screen file
/// given as the argument) with the plasma, cube and blur surfaces registered (Surfaces.hpp).
//
//   affrescoui_surfaces [file.screen.json]

#include "Surfaces.hpp"

#include <affresco/affresco.hpp>

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		const std::filesystem::path file = argc > 1 ? argv[1] : AFFRESCOUI_SURFACES_SCREEN;
		affresco::Application app;
		affresco::Screen screen = affresco::Screen::fromFile(file, affresco::Screen::defaultDataDirectory(),
		                                                     affresco::Screen::History::InMemory);
		examples::registerSurfaces(screen);
		screen.createWindow(app);
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_surfaces: " << error.what() << '\n';
		return 1;
	}
}
