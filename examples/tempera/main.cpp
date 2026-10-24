/// @file
/// @brief affrescoui_tempera: Tempera, an image editor whose workspace is a screen document (index.screen.json).
//
//   affrescoui_tempera [--socket DIR] [--data DIR]
//
// Opens a generated sample image. Like affrescoui_host, it listens for the person's client and an agent in DIR
// (studio.sock, agent.sock), so they can change the workspace while it runs; the theme and the panels' sizes are kept in --data DIR.

#include "SocketDir.hpp"
#include "tempera/Tempera.hpp"

#include <affresco/affresco.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>

namespace {

/// @brief Whether an environment variable is set (getenv_s on Windows, where getenv is deprecated).
/// @param name The variable.
/// @return true if it is.
bool environmentHas(const char* name) {
#ifdef _WIN32
	std::size_t length = 0; // the size its value needs, 0 if it is not set
	return getenv_s(&length, nullptr, 0, name) == 0 && length > 0;
#else
	return std::getenv(name) != nullptr;
#endif
}

/// @brief Whether this is a plain screenshot run (AFFRESCOUI_SCREENSHOT without AFFRESCOUI_SCREENSHOT_AFTER), which
/// draws one frame and exits.
/// @return true if it is.
bool firstFrameOnly() {
	return environmentHas("AFFRESCOUI_SCREENSHOT") && !environmentHas("AFFRESCOUI_SCREENSHOT_AFTER");
}

/// @brief What the command line asks for.
struct Options {
	std::filesystem::path socketDir = affresco::Screen::defaultSocketDirectory(); ///< Where peers connect.
	std::filesystem::path dataDir = affresco::Screen::defaultDataDirectory();     ///< Where the state is kept.
};

/// @brief Reads the command line.
/// @param argc The number of arguments, the program's name included.
/// @param argv The arguments.
/// @return The options, or std::nullopt after printing the usage if an argument isn't known.
std::optional<Options> parseArguments(int argc, const char* const* argv) {
	Options options;
	for(int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		const bool hasValue = i + 1 < argc;
		if(arg == "--socket" && hasValue) options.socketDir = argv[++i];
		else if(arg == "--data" && hasValue) options.dataDir = argv[++i];
		else {
			std::cerr << "usage: affrescoui_tempera [--socket DIR] [--data DIR]\n";
			return std::nullopt;
		}
	}
	return options;
}

/// @brief Shows Tempera until the window closes.
/// @param options The command line.
/// @return The exit status.
/// @throws std::exception If the screen or a socket can't be opened.
int run(const Options& options) {
	affresco::Application app;
	const bool screenshotOnly = firstFrameOnly();
	using History = affresco::Screen::History;
	affresco::Screen screen =
	    affresco::Screen::fromFile(std::filesystem::path(AFFRESCOUI_TEMPERA_DIRECTORY) / "index.screen.json",
		                           options.dataDir, screenshotOnly ? History::InMemory : History::OnDisk);
	const auto session = std::make_shared<tempera::Session>(affresco::imaging::Limits{});
	tempera::registerTempera(screen, session, &app);
	screen.setSocketDirectory(options.socketDir);
	session->window = &screen.createWindow(app);

	if(!screenshotOnly) {
		createSocketDir(options.socketDir);
		screen.listen(app, options.socketDir / "studio.sock", "studio");
		screen.listen(app, options.socketDir / "agent.sock", "agent");
		std::cerr << "affrescoui_tempera: listening in " << options.socketDir.string()
		          << " (studio.sock, agent.sock)\n";
	}
	const int status = app.run();
	session->window = nullptr;
	return status;
}

} // namespace

/// @brief Runs Tempera.
/// @param argc The number of arguments.
/// @param argv The arguments.
/// @return 0 when the window closed, 1 on an error, 2 for a wrong command line.
int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		const auto options = parseArguments(argc, argv);
		return options ? run(*options) : 2;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_tempera: " << error.what() << '\n';
		return 1;
	}
}
