/// @file
/// @brief affrescoui_host: shows a screen file and lets other programs change it while it runs.
//
//   affrescoui_host [file.screen.json] [--socket DIR] [--data DIR] [--mode default|acceptEdits|plan] [--auto-accept]
//               [--plugin changes.json ...]
//
// The window only renders: it has no idea where changes come from. Programs connect over two local sockets in DIR
// (default $AFFRESCOUI_SOCKET_DIR, else /tmp/affrescoui-<uid>, or %LOCALAPPDATA%\affrescoui\sockets on Windows):
//   studio.sock   the user's client (affrescoui_studio): author "studio", which the template lets review changes
//   agent.sock    an AI agent's MCP server (mcp/affrescoui_mcp.py): author "agent", whose changes wait for review
// What each author may do is up to the screen's "permissions"; --auto-accept is --mode acceptEdits, for this run only.
// The screen's history and "persist" state are kept in --data DIR (default $AFFRESCOUI_DATA_DIR, else ~/.affrescoui),
// and a plugin is applied once: the history remembers it. Default screen:
// the Kanban board template; the other templates are in assets/screens (smart-home, music-player, code-editor,
// habit-tracker).

#include "Assets.hpp"
#include "CodeEditor.hpp"
#include "SocketDir.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/format/Locale.hpp>

#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

/// @brief Lets a proposal's evidence play the screen's scenarios (`<name>.scenarios.json` beside `<name>.screen.json`)
/// on the proposal: those that draw no picture, headless, on a screen connected to nobody.
/// @param screen The screen.
/// @param file Its file.
void checkScenariosForEvidence(affresco::Screen& screen, const std::filesystem::path& file) {
	const std::string name = file.filename().string();
	if(!name.ends_with(".screen.json")) return;
	const std::filesystem::path scenarios =
	    file.parent_path() /
	    (name.substr(0, name.size() - std::string_view(".screen.json").size()) + ".scenarios.json");
	if(!std::filesystem::exists(scenarios)) return;
	screen.setScenarioCheck([scenarios](const std::string& document) {
		namespace automation = affresco::automation;
		affresco::json results = affresco::json::array();
		try {
			const automation::ScenarioFile file = automation::ScenarioFile::load(scenarios);
			automation::Options options;
			options.document = document;
			options.pictures = false;
			options.prepare = [](affresco::Screen& played) { examples::registerCodeEditor(played); };
			for(const automation::Scenario& scenario : file.scenarios)
				if(!scenario.needsPictures) options.only.push_back(scenario.name);
			if(options.only.empty()) return results.dump(); // all of them draw
			for(const automation::ScenarioResult& result : automation::run(file, options).scenarios) {
				std::string message;
				for(const automation::Failure& failure : result.failures)
					message +=
					    (message.empty() ? "" : "; ") + std::format("step {}: {}", failure.step, failure.problem);
				results.push_back({{"name", result.name}, {"passed", result.passed()}, {"message", message}});
			}
		} catch(const std::exception& error) { // the scenarios file itself is malformed
			results = {{{"name", scenarios.filename().string()}, {"passed", false}, {"message", error.what()}}};
		}
		return results.dump();
	});
}

/// @brief Applies the --plugin change sets, skipping those the screen's history already has from an earlier start.
/// @param screen The screen.
/// @param plugins The change set files, in order.
/// @throws std::runtime_error If one can't be read or applied.
void applyPlugins(affresco::Screen& screen, const std::vector<std::string>& plugins) {
	for(const std::string& plugin : plugins) {
		const std::ifstream in(plugin);
		std::stringstream text;
		text << in.rdbuf();
		std::string error;
		if(in && screen.hasImported(text.str())) {
			std::cerr << "plugin already applied: " << plugin << '\n';
			continue;
		}
		if(!in || !screen.importChanges(text.str(), &error))
			throw std::runtime_error(std::format("{}: {}", plugin, error));
		std::cerr << "plugin applied: " << plugin << " (revision " << screen.revision() << ")\n";
	}
}

/// @brief The screen file to show.
/// @param given The file on the command line, or "".
/// @return It, or the Kanban template.
std::filesystem::path screenFile(const std::string& given) {
	return given.empty() ? assetsDirectory() / "screens" / "kanban.screen.json" : std::filesystem::path(given);
}

/// @brief Whether this is a plain screenshot run (AFFRESCOUI_SCREENSHOT without AFFRESCOUI_SCREENSHOT_AFTER), which
/// draws one frame and exits.
/// @return true if it is.
bool firstFrameOnly() {
	return environmentHas("AFFRESCOUI_SCREENSHOT") && !environmentHas("AFFRESCOUI_SCREENSHOT_AFTER");
}

/// @brief What the command line asks for.
struct Options {
	std::string file; ///< The screen file; "" for the Kanban template.
	std::filesystem::path socketDir = affresco::Screen::defaultSocketDirectory(); ///< Where peers connect.
	std::filesystem::path dataDir = affresco::Screen::defaultDataDirectory();     ///< Where the history is kept.
	/// For this run only: without --mode, the one a reviewer last chose, which the screen's history keeps.
	std::optional<affresco::Mode> mode;
	std::vector<std::string> plugins; ///< The change sets to apply, in order.
};

/// @brief Reads one argument of the command line.
/// @param options Where it goes.
/// @param arg The argument.
/// @param value The one after it, or nullptr if it is the last.
/// @return How many arguments it took: 1, or 2 for an option with a value; 0 if it isn't one.
int readArgument(Options& options, std::string_view arg, const char* value) {
	if(arg == "--auto-accept") {
		options.mode = affresco::Mode::AcceptEdits;
		return 1;
	}
	if(!arg.starts_with("--")) {
		options.file = arg;
		return 1;
	}
	if(!value) return 0; // every other option takes a value
	if(arg == "--socket") options.socketDir = value;
	else if(arg == "--data") options.dataDir = value;
	else if(arg == "--plugin") options.plugins.emplace_back(value);
	else if(arg == "--mode" && affresco::modeFromString(value)) options.mode = affresco::modeFromString(value);
	else return 0;
	return 2;
}

/// @brief Reads the command line.
/// @param argc The number of arguments, the program's name included.
/// @param argv The arguments.
/// @return The options, or std::nullopt after printing the usage if an argument isn't known.
std::optional<Options> parseArguments(int argc, const char* const* argv) {
	Options options;
	for(int i = 1; i < argc;) {
		const int taken = readArgument(options, argv[i], i + 1 < argc ? argv[i + 1] : nullptr);
		if(taken == 0) {
			std::cerr << "usage: affrescoui_host [file.screen.json] [--socket DIR] [--data DIR] "
			             "[--mode default|acceptEdits|plan] [--auto-accept] [--plugin changes.json]\n";
			return std::nullopt;
		}
		i += taken;
	}
	return options;
}

/// @brief Shows the screen and lets peers change it, until the window closes.
/// @param options The command line.
/// @return The exit status.
/// @throws std::exception If the screen, a plugin or a socket can't be opened.
int run(const Options& options) {
	affresco::Application app;
	// A plain screenshot run shows the file as it is, so it neither loads nor writes the screen's history
	const bool screenshotOnly = firstFrameOnly();
	using History = affresco::Screen::History;
	affresco::Screen screen = affresco::Screen::fromFile(screenFile(options.file), options.dataDir,
	                                                     screenshotOnly ? History::InMemory : History::OnDisk);
	screen.setPlaceholderTypes(true);     // custom types of other apps show as plain panels
	examples::registerCodeEditor(screen); // the Forge template's "code" type (application layer)
	if(options.mode) screen.setModeForThisRun(*options.mode);
	if(!screenshotOnly) screen.setLocale(affresco::format::systemLocale()); // screenshots stay in the default locale
	screen.createWindow(app);
	applyPlugins(screen, options.plugins);
	checkScenariosForEvidence(screen, screenFile(options.file));

	if(!screenshotOnly) {
		createSocketDir(options.socketDir);
		screen.listen(app, options.socketDir / "studio.sock", "studio");
		screen.listen(app, options.socketDir / "agent.sock", "agent");
		std::cerr << "affrescoui_host: listening in " << options.socketDir.string() << " (studio.sock, agent.sock)"
		          << ", mode " << affresco::toString(screen.versions().mode()) << '\n';
	}
	return app.run();
}

} // namespace

int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		const std::optional<Options> options = parseArguments(argc, argv);
		return options ? run(*options) : 2;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_host: " << error.what() << '\n';
		return 1;
	}
}
