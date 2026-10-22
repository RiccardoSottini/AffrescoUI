/// @file
/// @brief affrescoui_sinopia: Sinopia, slides and vector documents, whose whole workspace is a screen document
/// (sinopia.screen.json).
//
//   affrescoui_sinopia [--socket DIR] [--data DIR]
//   affrescoui_sinopia --check sinopia.scenarios.json
//
// Opens the sample deck in decks/. Like affrescoui_host, it listens for the Studio and an agent in DIR (studio.sock,
// agent.sock). --check plays the scenarios headless, with the natives registered, and prints the report.

#include "SocketDir.hpp"
#include "ShapePanel.hpp"
#include "Sinopia.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace {

/// @brief What the command line asks for.
struct Options {
	std::filesystem::path check; ///< A scenarios file to play instead of opening the window; empty: none.
	std::filesystem::path socketDir = affresco::Screen::defaultSocketDirectory(); ///< Where peers connect.
	std::filesystem::path dataDir = affresco::Screen::defaultDataDirectory();     ///< Where the state is kept.
};

/// @brief Reads the command line.
/// @param argc The number of arguments, the program's name included.
/// @param argv The arguments.
/// @return The options, or std::nullopt after printing the usage if an argument isn't known.
std::optional<Options> parseArguments(int argc, const char* const* argv) {
	Options options;
	for(int i = 1; i < argc; i += 2) {
		const std::string_view arg = argv[i];
		const char* value = i + 1 < argc ? argv[i + 1] : nullptr;
		if(value != nullptr && arg == "--check") options.check = value;
		else if(value != nullptr && arg == "--socket") options.socketDir = value;
		else if(value != nullptr && arg == "--data") options.dataDir = value;
		else {
			std::cerr << "usage: affrescoui_sinopia [--socket DIR] [--data DIR] | --check FILE.scenarios.json\n";
			return std::nullopt;
		}
	}
	return options;
}

/// @brief Registers Sinopia's type and natives on a screen.
/// @param screen The screen.
/// @param session What the natives share.
void prepare(affresco::Screen& screen, const std::shared_ptr<sinopia::Session>& session) {
	sinopia::registerShape(screen);
	sinopia::registerNatives(screen, session);
	sinopia::registerPresenting(screen, session);
	screen.allowImageDirectory(session->folder / "decks");
}

/// @brief Plays a scenarios file headless, each scenario on a fresh session whose exports go to a temporary folder.
/// @param file The scenarios file.
/// @return The exit status: 0 if every scenario passed.
int check(const std::filesystem::path& file) {
	namespace automation = affresco::automation;
	const automation::ScenarioFile scenarios = automation::ScenarioFile::load(file);
	automation::Options options;
	options.pictures = false;
	options.prepare = [folder = file.parent_path()](affresco::Screen& screen) {
		auto session = std::make_shared<sinopia::Session>();
		session->folder = folder;
		session->exportFolder = std::filesystem::temp_directory_path() / "affrescoui-sinopia-check";
		session->saveFolder = session->exportFolder; // never into the source tree
		prepare(screen, session);
	};
	const automation::Report report = automation::run(scenarios, options);
	std::cout << report.text();
	return report.failed() == 0 ? 0 : 1;
}

/// @brief Shows the workspace and lets peers change it, until its window closes.
/// @param options The command line.
/// @return The exit status.
/// @throws std::exception If the screen or a socket can't be opened.
int run(const Options& options) {
	const std::filesystem::path folder = AFFRESCOUI_SINOPIA_DIR;
	affresco::Application app;
	affresco::Screen screen = affresco::Screen::fromFile(folder / "sinopia.screen.json", options.dataDir);
	auto session = std::make_shared<sinopia::Session>();
	session->folder = folder;
	session->exportFolder = options.dataDir / "sinopia-exports";
	session->app = &app;
	prepare(screen, session);
	session->window = &screen.createWindow(app);
	createSocketDir(options.socketDir);
	screen.listen(app, options.socketDir / "studio.sock", "studio");
	screen.listen(app, options.socketDir / "agent.sock", "agent");
	std::cerr << "affrescoui_sinopia: listening in " << options.socketDir.string() << " (studio.sock, agent.sock)\n";
	return app.run();
}

} // namespace

int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		const std::optional<Options> options = parseArguments(argc, argv);
		if(!options) return 2;
		return options->check.empty() ? run(*options) : check(options->check);
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_sinopia: " << error.what() << '\n';
		return 1;
	}
}
