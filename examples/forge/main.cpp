/// @file
/// @brief affrescoui_forge: Forge, a demo IDE whose whole workbench is a screen document (forge.screen.json).
//
//   affrescoui_forge [folder] [--socket DIR] [--data DIR]
//
// Opens the folder if one is given (else a welcome page, with Open Folder). Like affrescoui_host, it listens for the
// Studio and an agent in DIR (studio.sock, agent.sock), so they can change the workbench while it runs; the layout,
// theme and settings are kept in --data DIR ("persist").

#include "Assets.hpp"
#include "CodeEditor.hpp"
#include "Forge.hpp"
#include "SocketDir.hpp"

#include <affresco/affresco.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
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
	std::filesystem::path folder; ///< The folder to open; empty for the welcome page.
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
		else if(!arg.starts_with("--") && options.folder.empty()) options.folder = arg;
		else {
			std::cerr << "usage: affrescoui_forge [folder] [--socket DIR] [--data DIR]\n";
			return std::nullopt;
		}
	}
	return options;
}

/// @brief Shows Forge until the window closes.
/// @param options The command line.
/// @return The exit status.
/// @throws std::exception If the screen or a socket can't be opened.
int run(const Options& options) {
	affresco::Application app;
	const bool screenshotOnly = firstFrameOnly();
	using History = affresco::Screen::History;
	affresco::Screen screen = affresco::Screen::fromFile(assetsDirectory().parent_path() / "forge" / "forge.screen.json",
	                                                     options.dataDir,
	                                                     screenshotOnly ? History::InMemory : History::OnDisk);
	examples::registerCodeEditor(screen);
	auto workspace = std::make_shared<forge::Workspace>();
	if(!options.folder.empty()) workspace->root = std::filesystem::absolute(options.folder);
	forge::registerNatives(screen, workspace);
	screen.setSocketDirectory(options.socketDir); // the agent pane's channel reaches Forge's own studio.sock
	workspace->window = &screen.createWindow(app);

	if(!screenshotOnly) {
		createSocketDir(options.socketDir);
		screen.listen(app, options.socketDir / "studio.sock", "studio");
		screen.listen(app, options.socketDir / "agent.sock", "agent");
		std::cerr << "affrescoui_forge: listening in " << options.socketDir.string() << " (studio.sock, agent.sock)\n";
	}
	return app.run();
}

} // namespace

int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		const std::optional<Options> options = parseArguments(argc, argv);
		return options ? run(*options) : 2;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_forge: " << error.what() << '\n';
		return 1;
	}
}
