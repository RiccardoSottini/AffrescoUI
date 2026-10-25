/// @file
/// @brief affrescoui_sinopia: Sinopia, slides and vector documents, whose whole workspace is a screen document
/// (index.screen.json).
//
//   affrescoui_sinopia [--socket DIR] [--data DIR]
//   affrescoui_sinopia --check index.scenarios.json [--out DIR]
//
// Opens the sample deck in decks/. Like affrescoui_launcher, it listens for the person's client and an agent in DIR
// (studio.sock, agent.sock). --check plays the scenarios headless, with the natives registered, and prints the report; with --out,
// their pictures are drawn and captures are saved in DIR (tools/media/capture.py).

#include "Sinopia.hpp"
#include "SocketDir.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/intelligence/Intelligence.hpp>

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
	std::filesystem::path out;   ///< With check: where captures are saved, which draws pictures; empty: none drawn.
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
		else if(value != nullptr && arg == "--out") options.out = value;
		else if(value != nullptr && arg == "--socket") options.socketDir = value;
		else if(value != nullptr && arg == "--data") options.dataDir = value;
		else {
			std::cerr
			    << "usage: affrescoui_sinopia [--socket DIR] [--data DIR] | --check FILE.scenarios.json [--out DIR]\n";
			return std::nullopt;
		}
	}
	return options;
}

/// @brief Registers Sinopia's type and natives on a screen.
/// @param screen The screen.
/// @param session What the natives share.
void prepare(affresco::Screen& screen, const std::shared_ptr<sinopia::Session>& session) {
	sinopia::registerNatives(screen, session);
	sinopia::registerPresenting(screen, session);
	screen.allowImageDirectory(session->folder / "decks");
}

/// @brief Plays a scenarios file headless, each scenario on a fresh session whose exports go to a temporary folder.
/// @param file The scenarios file.
/// @param out Where captures are saved; empty: no pictures are drawn.
/// @return The exit status: 0 if every scenario passed.
int check(const std::filesystem::path& file, const std::filesystem::path& out) {
	namespace automation = affresco::automation;
	const automation::ScenarioFile scenarios = automation::ScenarioFile::load(file);
	automation::Options options;
	options.pictures = !out.empty();
	options.output = out;
	options.prepare = [](affresco::Screen& screen) {
		const auto session = std::make_shared<sinopia::Session>();
		session->folder = AFFRESCOUI_SINOPIA_DIR; // the sample decks, wherever the scenarios file is (capture.py's)
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
	// A peer: it outlives the screen
	affresco::intelligence::HostAssistant assistant(app, options.dataDir,
	                                              std::filesystem::path(AFFRESCOUI_SINOPIA_DIR).parent_path());
	affresco::Screen screen = affresco::Screen::fromFile(folder / "index.screen.json", options.dataDir);
	const auto session = std::make_shared<sinopia::Session>();
	session->folder = folder;
	session->exportFolder = options.dataDir / "sinopia-exports";
	session->app = &app;
	prepare(screen, session);
	session->window = &screen.createWindow(app);
	assistant.attach(screen); // the provider chosen in the Launcher's Settings answers the assistant
	createSocketDir(options.socketDir);
	screen.listen(app, options.socketDir / "studio.sock", "studio");
	screen.listen(app, options.socketDir / "agent.sock", "agent");
	std::cerr << "affrescoui_sinopia: listening in " << options.socketDir.string() << " (studio.sock, agent.sock)\n";
	return app.run();
}

} // namespace

/// @brief Runs Sinopia, or plays its scenarios with --check.
/// @param argc The number of arguments.
/// @param argv The arguments.
/// @return 0 when the window closed or every scenario passed, 1 on an error or a failed scenario, 2 for a wrong command
/// line.
int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		const std::optional<Options> options = parseArguments(argc, argv);
		if(!options) return 2;
		return options->check.empty() ? run(*options) : check(options->check, options->out);
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_sinopia: " << error.what() << '\n';
		return 1;
	}
}
