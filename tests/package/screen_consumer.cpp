/// @file
/// @brief Uses the whole installed library (affrescoui::affrescoui) through the umbrella header: loads a screen,
/// changes it with a revision and reads it back, with no window. Exits with 0 when every check holds.
///
///   screen_consumer            the checks only (what check.py runs: it needs no display)
///   screen_consumer --window   then shows the screen until the window is closed

#include <affresco/affresco.hpp>

#include <exception>
#include <iostream>
#include <string_view>

namespace {

/// @brief A screen with one label.
constexpr std::string_view screenText = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"window": { "title": "Package test", "width": 320, "height": 120 },
	"root": { "type": "panel", "id": "main", "color": "#1e1e2e", "children": [
		{ "type": "label", "id": "greeting", "text": "Hello" }
	] }
})json";

} // namespace

/// @brief Runs the checks, then shows the screen if asked.
/// @param argc The number of arguments.
/// @param argv The arguments: "--window" to show the screen.
/// @return 0 if every check holds, 1 otherwise.
int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		affresco::Screen screen = affresco::Screen::fromString(screenText);
		std::string error;
		const bool applied = screen.applyRevision(
		    R"json({"author": "app", "message": "Greet", "set": {"#greeting/text": "Installed"}})json", &error);
		const affresco::json document = affresco::parseUntrusted(screen.documentJson());
		if(!applied || document.at("root").at("children").at(0).at("text") != "Installed" || screen.revision() != 1) {
			std::cerr << "screen_consumer: failed: " << error << '\n';
			return 1;
		}
		std::cout << "screen_consumer: loaded a screen and applied revision " << screen.revision() << '\n';

		if(argc > 1 && std::string_view(argv[1]) == "--window") {
			affresco::Application app;
			screen.createWindow(app);
			return app.run();
		}
		return 0;
	} catch(const std::exception& exception) {
		std::cerr << "screen_consumer: " << exception.what() << '\n';
		return 1;
	}
}
