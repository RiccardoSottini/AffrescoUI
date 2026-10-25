/// @file
/// @brief affrescoui_launcher: shows a screen file and lets other programs change it while it runs.
//
//   affrescoui_launcher [file.screen.json] [--socket DIR] [--data DIR] [--mode default|acceptEdits|plan] [--auto-accept]
//               [--plugin changes.json ...] [--usage] [--folder DIR] [--allow NAME[=PROGRAM ARG...] ...] [--ask TEXT]
//               [--apps DIR]
//
// The window only renders: it has no idea where changes come from. Programs connect over two local sockets in DIR
// (default $AFFRESCOUI_SOCKET_DIR, else /tmp/affrescoui-<uid>, or %LOCALAPPDATA%\affrescoui\sockets on Windows):
//   studio.sock   a reviewing client: author "studio", which the templates let review changes
//   agent.sock    an AI agent's MCP server (mcp/affrescoui_mcp.py): author "agent", whose changes wait for review
// The person reviews an agent's changes in the app's own assistant (examples/shared/addons/assistant). When the
// AffrescoUI Launcher's Settings set up a provider of intelligence (<data DIR>/intelligence.json, the key in the
// system's keychain), the host also runs an agent of its own, connected as author "agent" like an MCP one, that
// answers the assistant with it (examples/shared/intelligence); the Launcher's host alone gets the settings' natives,
// and no agent.
// For tests only, AFFRESCOUI_KEYS=memory keeps the keys in the host's memory instead of the system's keychain, and
// AFFRESCOUI_PROVIDERS=all lets the agent use the providers the Launcher's Settings hide.
// --ask TEXT opens the assistant and sends TEXT as the person's first request once the window is up.
// What each author may do is up to the screen's "permissions"; --auto-accept is --mode acceptEdits, for this run only.
// The screen's history and "persist" state are kept in --data DIR (default $AFFRESCOUI_DATA_DIR, else ~/.affrescoui),
// and a plugin is applied once: the history remembers it.
// --usage counts how the screen is used (VERSION_CONTROL.md, "Usage"), on this device and only once the person agreed,
// asked once on the terminal. A screen's services may use the folder "workspace": --folder DIR, or the one the person
// chooses in its folder dialog (the chooseFolder step); --allow grants them a program: a name on the PATH, "shell" (the
// login shell) or "git" (the source control commands only), or NAME=PROGRAM ARG...
// Without a file, the host opens the Launcher (examples/launcher), which lists the apps and opens them, and grants it
// what it needs (grantLauncher()): the examples folder to read, the person's apps folder ("apps": --apps DIR, else
// AffrescoUI in the home folder) and the sibling programs, each app opened with --data DIR and a socket folder of its
// own in DIR. Every template has a folder in examples (examples/README.md): the Kanban board is examples/flow.

#include "Assets.hpp"
#include "intelligence/Intelligence.hpp"
#include "SocketDir.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/format/Locale.hpp>
#include <affresco/services/Grants.hpp>

#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

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
	screen.setScenarioCheck([scenarios = scenarios](const std::string& document) {
		namespace automation = affresco::automation;
		affresco::json results = affresco::json::array();
		try {
			const automation::ScenarioFile loaded = automation::ScenarioFile::load(scenarios);
			automation::Options options;
			options.document = document;
			options.pictures = false;
			for(const automation::Scenario& scenario : loaded.scenarios)
				if(!scenario.needsPictures) options.only.push_back(scenario.name);
			if(options.only.empty()) return results.dump(); // all of them draw
			for(const automation::ScenarioResult& result : automation::run(loaded, options).scenarios) {
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
/// @return It, or the Launcher.
std::filesystem::path screenFile(const std::string& given) {
	return given.empty() ? assetsDirectory() / "launcher" / "index.screen.json" : std::filesystem::path(given);
}

/// @brief Whether this is a plain screenshot run (AFFRESCOUI_SCREENSHOT without AFFRESCOUI_SCREENSHOT_AFTER), which
/// draws one frame and exits.
/// @return true if it is.
bool firstFrameOnly() {
	return environmentHas("AFFRESCOUI_SCREENSHOT") && !environmentHas("AFFRESCOUI_SCREENSHOT_AFTER");
}

/// @brief What the command line asks for.
struct Options {
	std::string file; ///< The screen file; "" for the Launcher.
	std::filesystem::path socketDir = affresco::Screen::defaultSocketDirectory(); ///< Where peers connect.
	std::filesystem::path dataDir = affresco::Screen::defaultDataDirectory();     ///< Where the history is kept.
	/// For this run only: without --mode, the one a reviewer last chose, which the screen's history keeps.
	std::optional<affresco::Mode> mode;
	std::vector<std::string> plugins; ///< The change sets to apply, in order.
	bool usage = false;               ///< Count usage summaries, with the person's consent (off by default).
	std::filesystem::path folder;     ///< The folder "workspace", or empty until the person chooses one.
	std::vector<std::string> allowed; ///< The programs granted (Grants.hpp's specs).
	std::string ask;                  ///< The person's first request to the assistant, or "".
	std::filesystem::path apps;       ///< The Launcher's apps folder; empty for AffrescoUI in the home folder.
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
	if(arg == "--usage") {
		options.usage = true;
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
	else if(arg == "--folder") options.folder = value;
	else if(arg == "--allow") options.allowed.emplace_back(value);
	else if(arg == "--ask") options.ask = value;
	else if(arg == "--apps") options.apps = value;
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
			std::cerr << "usage: affrescoui_launcher [file.screen.json] [--socket DIR] [--data DIR] "
			             "[--mode default|acceptEdits|plan] [--auto-accept] [--plugin changes.json] [--usage] "
			             "[--folder DIR] [--allow NAME[=PROGRAM ARG...]] [--ask TEXT] [--apps DIR]\n";
			return std::nullopt;
		}
		i += taken;
	}
	return options;
}

/// @brief Switches usage summaries on with --usage, asking the person once, on the terminal, the first time: without a
/// terminal to ask on, or without a "yes", nothing is counted.
/// @param screen The screen.
void enableUsage(affresco::Screen& screen) {
#ifdef _WIN32
	const bool terminal = _isatty(_fileno(stdin)) != 0;
#else
	const bool terminal = isatty(STDIN_FILENO) != 0;
#endif
	if(!screen.usageConsented() && terminal) {
		std::cerr << "affrescoui_launcher: count how this screen is used (which controls run and fail, which screens are "
		             "visited; ids, counts and times only, kept on this device for 30 days)? [y/N] ";
		std::string answer;
		std::getline(std::cin, answer);
		if(answer == "y" || answer == "yes") screen.giveUsageConsent();
	}
	screen.setUsageEnabled(true);
	if(!screen.usageConsented()) std::cerr << "affrescoui_launcher: usage summaries stay off: no consent\n";
}

/// @brief Gives the screen the assistant's agent, when a provider of intelligence is set up, and with --ask the first
/// request once the window is up; the Launcher (the screen opened without a file) gets the settings' natives instead,
/// never the agent: the screen that holds the provider, its key and the grants to the person's folders has no agent.
/// @param assistant The host's agent.
/// @param app The application.
/// @param screen The screen.
/// @param window Its window.
/// @param options The command line.
void startAssistant(examples::intelligence::HostAssistant& assistant, affresco::Application& app,
                    affresco::Screen& screen, affresco::Window& window, const Options& options) {
	if(options.file.empty()) {
		assistant.enableSettings(screen); // only the Launcher may change the provider
		return;
	}
	assistant.attach(screen);
	if(!options.ask.empty()) examples::intelligence::askOnStart(app, screen, window, options.ask);
}

/// @brief The Launcher's apps folder: --apps DIR, else "AffrescoUI" in the home folder, where a person sees it.
/// @param options The command line.
/// @return It, absolute; empty if there is no home folder.
std::filesystem::path appsFolder(const Options& options) {
	if(!options.apps.empty()) return std::filesystem::absolute(options.apps);
#ifdef _WIN32
	const char* variable = "USERPROFILE";
#else
	const char* variable = "HOME";
#endif
	if(!environmentHas(variable)) return {};
#ifdef _WIN32
	std::size_t length = 0;
	std::vector<char> home(32768);
	if(getenv_s(&length, home.data(), home.size(), variable) != 0 || length == 0) return {};
	return std::filesystem::path(home.data()) / "AffrescoUI";
#else
	return std::filesystem::path(std::getenv(variable)) / "AffrescoUI"; // NOLINT(concurrency-mt-unsafe): at launch
#endif
}

/// @brief Grants the Launcher (the screen the host opens without a file) what it uses: the examples folder to read
/// ("examples"), the person's apps folder ("apps", made if missing), and the sibling programs that open the apps
/// ("affresco", detached: each with --data DIR and a socket folder of its own in DIR; Grants.hpp, grantAppPrograms).
/// @param screen The Launcher's screen.
/// @param options The command line.
/// @throws std::runtime_error If the apps folder can't be made.
void grantLauncher(affresco::Screen& screen, const Options& options) {
	namespace services = affresco::services;
	const std::filesystem::path examples = assetsDirectory();
	screen.services().addRoot("examples", examples, true);
	const std::filesystem::path apps = appsFolder(options);
	if(apps.empty()) throw std::runtime_error("no home folder for the apps: pass --apps DIR");
	std::filesystem::create_directories(apps);
	if(services::rootsOverlap(apps, examples)) // the apps folder is written: never the examples, nor above them
		throw std::runtime_error("the apps folder (" + apps.string() + ") and the examples (" + examples.string() +
		                         ") overlap: pass --apps DIR, a folder outside the examples that doesn't hold them");
	screen.services().addRoot("apps", apps);
	services::AppPrograms programs;
	programs.folder = programFile().parent_path();
	programs.programs = {{"affrescoui_launcher", true, true},
	                     {"affrescoui_forge", false, false}};
	programs.roots = {{"examples", examples}, {"apps", apps}};
	programs.dataDirectory = std::filesystem::absolute(options.dataDir);
	programs.socketDirectory = std::filesystem::absolute(options.socketDir);
	programs.reveal = true;
	if(const auto refused = services::grantAppPrograms(screen.services(), "affresco", std::move(programs)))
		throw std::runtime_error("the Launcher's programs: " + *refused);
}

/// @brief Lets a screen that declares sounds (its "audio" section, which revisions can't add) play them on the system's
/// output; other screens open no audio device. A screenshot plays nothing ($AFFRESCOUI_AUDIO_OUTPUT=null plays to no
/// hardware, for tests).
/// @param screen The screen.
/// @param screenshotOnly Whether this run only takes a picture.
void enableAudio(affresco::Screen& screen, bool screenshotOnly) {
	if(screenshotOnly || !screen.versions().document().contains("audio")) return;
	static_cast<void>(screen.audio()); // nullptr in a build without audio: its steps then say so
}

/// @brief Shows the screen and lets peers change it, until the window closes.
/// @param options The command line.
/// @return The exit status.
/// @throws std::exception If the screen, a plugin or a socket can't be opened.
int run(const Options& options) {
	affresco::Application app;
	examples::intelligence::HostAssistant assistant(app, options.dataDir); // a peer: it outlives the screen
	// A plain screenshot run shows the file as it is, so it neither loads nor writes the screen's history
	const bool screenshotOnly = firstFrameOnly();
	using History = affresco::Screen::History;
	affresco::Screen screen = affresco::Screen::fromFile(screenFile(options.file), options.dataDir,
	                                                     screenshotOnly ? History::InMemory : History::OnDisk);
	screen.setPlaceholderTypes(true); // custom types of other apps show as plain panels
	screen.services().allowChosenRoot("workspace");
	if(!options.folder.empty()) screen.services().addRoot("workspace", options.folder);
	for(const std::string& spec : options.allowed)
		if(const auto refused = affresco::services::grantProgram(screen.services(), spec))
			throw std::runtime_error("--allow " + spec + ": " + *refused);
#ifndef __EMSCRIPTEN__
	if(options.file.empty() && !screenshotOnly) grantLauncher(screen, options); // a screenshot shows its stand-ins
#endif
	if(options.mode) screen.setModeForThisRun(*options.mode);
	if(options.usage && !screenshotOnly) enableUsage(screen);
	if(!screenshotOnly) screen.setLocale(affresco::format::systemLocale()); // screenshots stay in the default locale
	enableAudio(screen, screenshotOnly);
	affresco::Window& window = screen.createWindow(app);
	if(!screenshotOnly) startAssistant(assistant, app, screen, window, options);
	applyPlugins(screen, options.plugins);
	checkScenariosForEvidence(screen, screenFile(options.file));

#ifndef __EMSCRIPTEN__ // a browser tab has no local sockets to listen on (web/README.md)
	if(!screenshotOnly) {
		createSocketDir(options.socketDir);
		screen.listen(app, options.socketDir / "studio.sock", "studio");
		screen.listen(app, options.socketDir / "agent.sock", "agent");
		std::cerr << "affrescoui_launcher: listening in " << options.socketDir.string() << " (studio.sock, agent.sock)"
		          << ", mode " << affresco::toString(screen.versions().mode()) << '\n';
	}
#endif
	return app.run();
}

} // namespace

int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		const std::optional<Options> options = parseArguments(argc, argv);
		return options ? run(*options) : 2;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_launcher: " << error.what() << '\n';
		return 1;
	}
}
