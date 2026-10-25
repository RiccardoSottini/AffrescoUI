/// @file
/// @brief affrescoui_launcher: runs an app, a folder of files with an app.json, and lets other programs change it
/// while it runs.
//
//   affrescoui_launcher [APP] [--socket DIR] [--data DIR] [--mode default|acceptEdits|plan] [--auto-accept]
//               [--plugin changes.json ...] [--usage] [--folder DIR] [--allow NAME[=PROGRAM ARG...] ...] [--ask TEXT]
//               [--apps DIR] [--grant KEY ...]
//   affrescoui_launcher --permissions APP
//
// APP is an app's folder, its app.json, or a screen file (SCREEN_FORMAT.md, "Apps: app.json"): a screen that is the
// entry of the app in its folder opens that app; any other is opened bare, and its manifest declares what it uses.
// The app's history, state and data are kept by its id in --data DIR (default $AFFRESCOUI_DATA_DIR, else
// ~/.affrescoui): <DIR>/apps/<id>. It may use nothing its app.json declares (connections' origins, folders,
// programs, sound) until the person allows it: the host asks in a window of its own the first time, for this app only,
// and remembers the answer (<DIR>/apps/<id>/grants.json); --permissions APP forgets it. --grant KEY allows one for
// this run ("network:https://api.example.com", "folder:workspace:write", "program:git", "audio"); a screenshot asks
// nothing, and allows only those. What the screen uses and the manifest doesn't declare is never allowed.
//
// The window only renders: it has no idea where changes come from. Programs connect over two local sockets in DIR
// (default $AFFRESCOUI_SOCKET_DIR, else /tmp/affrescoui-<uid>, or %LOCALAPPDATA%\affrescoui\sockets on Windows):
//   studio.sock   a reviewing client: author "studio", which the templates let review changes
//   agent.sock    an AI agent's MCP server (mcp/affrescoui_mcp.py): author "agent", whose changes wait for review
// The person reviews an agent's changes in the app's own assistant (examples/shared/addons/assistant). When the
// AffrescoUI Launcher's Settings set up a provider of intelligence (<data DIR>/intelligence.json, the key in the
// system's keychain) and the person allowed the app's "assistant" capability, the host also runs an agent of its own,
// connected as author "agent" like an MCP one, that answers the assistant with it (modules/intelligence); only the
// last agent connected answers a request. The Launcher's host alone gets the settings' natives, and no agent.
// For tests only, AFFRESCOUI_KEYS=memory keeps the keys in the host's memory instead of the system's keychain, and
// AFFRESCOUI_PROVIDERS=all lets the agent use the providers the Launcher's Settings hide.
// --ask TEXT opens the assistant and sends TEXT as the person's first request once the window is up.
// What each author may do is up to the screen's "permissions"; --auto-accept is --mode acceptEdits, for this run only.
// A plugin is applied once: the history remembers it.
// --usage counts how the screen is used (VERSION_CONTROL.md, "Usage"), on this device and only once the person agreed,
// asked once on the terminal. --folder DIR gives the services the folder "workspace" without asking; --allow grants
// them a program: a name on the PATH, "shell" (the login shell) or "git" (the source control commands only), or
// NAME=PROGRAM ARG...
// Without an app, the host opens the Launcher (examples/launcher), which lists the apps and opens them, and grants it
// what it needs (grantLauncher()): the examples folder to read, the person's apps folder ("apps": --apps DIR, else
// AffrescoUI in the home folder) and the sibling programs, each app opened with --data DIR and a socket folder of its
// own in DIR. Every template has a folder in examples (examples/README.md): the Kanban board is examples/flow.

#include "Assets.hpp"
#include "CodeEditor.hpp"
#include "Permissions.hpp"
#include "SocketDir.hpp"
#include "tempo/Visualiser.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/format/App.hpp>
#include <affresco/format/Locale.hpp>
#include <affresco/intelligence/Intelligence.hpp>
#include <affresco/services/Grants.hpp>

#include <algorithm>
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
			options.prepare = [](affresco::Screen& played) {
				examples::registerCodeEditor(played);
				examples::tempo::registerVisualiser(played);
			};
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

/// @brief What the host opens.
struct Opened {
	std::filesystem::path location;         ///< The app's folder, or a bare screen file.
	bool app = false;                       ///< An app, with its app.json: opened with Screen::fromApp().
	affresco::format::AppManifest manifest; ///< Its manifest; a bare screen's is manifestOf() its file.
	std::filesystem::path entry;            ///< Its screen file.
	std::vector<std::string> undeclared;    ///< The capabilities its screen uses that its manifest doesn't declare.
};

/// @brief Whether a screen file is the entry of the app in its folder (an app.json there that names it), so opening
/// the file opens the app. A damaged app.json counts, so that opening it says what is wrong.
/// @param file The file.
/// @return Whether it is.
bool isAppEntry(const std::filesystem::path& file) {
	const std::filesystem::path manifest = file.parent_path() / "app.json";
	std::error_code error;
	if(!std::filesystem::is_regular_file(manifest, error)) return false;
	try {
		if(std::filesystem::file_size(manifest) > affresco::format::maxAppManifestBytes) return true;
		const affresco::format::AppManifest app =
		    affresco::format::parseApp(affresco::json::parse(std::ifstream(manifest)));
		return std::filesystem::equivalent(file.parent_path() / app.entry, file, error);
	} catch(const std::exception&) {
		return true;
	}
}

/// @brief The name a bare screen file is known by: its name without ".screen.json", or its folder's for an index.
/// @param file The file.
/// @return The name.
std::string screenName(const std::filesystem::path& file) {
	std::string name = file.filename().string();
	if(name.ends_with(".screen.json")) name.resize(name.size() - std::string_view(".screen.json").size());
	else if(name.ends_with(".json")) name.resize(name.size() - std::string_view(".json").size());
	return name == "index" ? std::filesystem::absolute(file).parent_path().filename().string() : name;
}

/// @brief Reads a bare screen file as fromString() does, keeping nothing on disk.
/// @param file The file.
/// @return The screen.
/// @throws affresco::ScreenError If it can't be read, or isn't a screen.
affresco::Screen bareScreen(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if(!in)
		throw affresco::ScreenError("AffrescoUI screen: could not open " + std::filesystem::absolute(file).string());
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return affresco::Screen::fromString(text, file.parent_path());
}

/// @brief Finds what to open and reads what it asks for, keeping nothing on disk: an app's folder or its app.json, a
/// screen that is its folder's app's entry (both opened as the app), or a bare screen, whose manifest declares what it
/// uses. With nothing given, the Launcher.
/// @param given The command line's file, or "".
/// @return What to open.
/// @throws affresco::ScreenError If it can't be read.
Opened locate(const std::string& given) {
	namespace fs = std::filesystem;
	fs::path path = given.empty() ? assetsDirectory() / "launcher" : fs::path(given);
	if(path.filename() == "app.json") path = path.parent_path();
	Opened opened;
	std::error_code error;
	opened.app = fs::is_directory(path, error) || isAppEntry(path);
	opened.location = opened.app && !fs::is_directory(path, error) ? path.parent_path() : path;
	affresco::setLogSink([](affresco::LogLevel, std::string_view) {}); // the screen's findings show when it opens
	try {
		const affresco::Screen check =
		    opened.app ? affresco::Screen::fromApp(opened.location, {}) : bareScreen(path); // nothing kept on disk
		affresco::setLogSink({});
		const affresco::json document = affresco::json::parse(check.documentJson());
		const std::string name = screenName(path);
		opened.manifest = opened.app ? *check.app()
		                             : affresco::format::manifestOf(affresco::format::appId(name), name,
		                                                            path.filename().string(), document);
		opened.undeclared = affresco::format::undeclaredCapabilities(opened.manifest, document);
	} catch(...) {
		affresco::setLogSink({});
		throw;
	}
	opened.entry = opened.app ? opened.location / opened.manifest.entry : path;
	return opened;
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
	std::vector<std::string> grants;  ///< Capabilities allowed for this run, by key (--grant).
	std::string forget;               ///< The app whose decisions to forget (--permissions), or "".
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
	else if(arg == "--grant") options.grants.emplace_back(value);
	else if(arg == "--permissions") options.forget = value;
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
			             "[--folder DIR] [--allow NAME[=PROGRAM ARG...]] [--ask TEXT] [--apps DIR] [--grant KEY]\n"
			             "       affrescoui_launcher --permissions APP\n";
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

/// @brief Gives the screen the assistant's agent, when the person allowed the app the `assistant` capability (it
/// answers with the provider set up then, or says none is), and with --ask the first request once the window is up;
/// the Launcher (the screen opened without a file) gets the settings' natives instead, never the agent: the screen
/// that holds the provider, its key and the grants to the person's folders has no agent.
/// @param assistant The host's agent.
/// @param app The application.
/// @param screen The screen.
/// @param window Its window.
/// @param options The command line.
/// @param allowed What the person allowed the app.
void startAssistant(affresco::intelligence::HostAssistant& assistant, affresco::Application& app,
                    affresco::Screen& screen, affresco::Window& window, const Options& options,
                    const host::Allowed& allowed) {
	if(options.file.empty()) {
		assistant.enableSettings(screen); // only the Launcher may change the provider
		return;
	}
	if(allowed.contains("assistant")) assistant.attach(screen);
	if(!options.ask.empty()) affresco::intelligence::askOnStart(app, screen, window, options.ask);
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
	                     {"affrescoui_forge", false, false},
	                     {"affrescoui_tempera", false, false},
	                     {"affrescoui_sinopia", false, false}};
	programs.roots = {{"examples", examples}, {"apps", apps}};
	programs.dataDirectory = std::filesystem::absolute(options.dataDir);
	programs.socketDirectory = std::filesystem::absolute(options.socketDir);
	programs.reveal = true;
	if(const auto refused = services::grantAppPrograms(screen.services(), "affresco", std::move(programs)))
		throw std::runtime_error("the Launcher's programs: " + *refused);
}

/// @brief Decides what the app may use: what the person decided before, and, when @p ask, what the person answers
/// now about the rest (asked in a window, remembered), and --grant for this run. A capability its manifest doesn't
/// declare is never allowed, and said so.
/// @param opened The app.
/// @param options The command line.
/// @param ask Whether the person can be asked (not in a screenshot).
/// @return The keys allowed.
/// @throws std::runtime_error If --grant names a capability the app doesn't declare, or the decisions can't be kept.
host::Allowed decide(const Opened& opened, const Options& options, bool ask) {
	const affresco::format::AppManifest& app = opened.manifest;
	for(const std::string& key : opened.undeclared)
		std::cerr << "affrescoui_launcher: " << app.id << " uses " << key << ", which its app.json doesn't declare: it "
		          << "isn't allowed\n";
	const auto declared = [&](std::string_view key) {
		return std::ranges::any_of(app.capabilities, [&](const auto& capability) { return capability.key() == key; });
	};
	for(const std::string& key : options.grants)
		if(!declared(key)) throw std::runtime_error("--grant " + key + ": not a capability " + app.id + " declares");
	const std::filesystem::path file = host::grantsFile(options.dataDir, app.id);
	host::Decisions decisions = host::loadDecisions(file);
	std::vector<affresco::format::AppCapability> asked;
	for(const affresco::format::AppCapability& capability : app.capabilities)
		if(!decisions.contains(capability.key()) &&
		   std::ranges::find(options.grants, capability.key()) == options.grants.end())
			asked.push_back(capability);
	if(ask && !asked.empty()) {
		host::Decisions answered =
		    host::askPerson(app, asked, assetsDirectory() / "shared" / "host" / "permissions.screen.json");
		if(!answered.empty()) {
			decisions.merge(answered);
			host::saveDecisions(file, decisions);
		}
	}
	host::Allowed allowed(options.grants.begin(), options.grants.end());
	for(const auto& [key, yes] : decisions)
		if(yes && declared(key)) allowed.insert(key);
	return allowed;
}

/// @brief Forgets what the person decided about an app (--permissions): it asks again the next time it opens.
/// @param options The command line.
/// @return The exit status.
/// @throws std::exception If the app can't be read.
int forgetPermissions(const Options& options) {
	const Opened opened = locate(options.forget);
	std::filesystem::remove(host::grantsFile(options.dataDir, opened.manifest.id));
	std::cout << "affrescoui_launcher: " << opened.manifest.name << " asks again for what it uses when it opens\n";
	return EXIT_SUCCESS;
}

/// @brief Opens the app's screen, with its history, state and data in the data folder.
/// @param opened The app.
/// @param options The command line.
/// @param history Where its history is kept.
/// @return The screen.
/// @throws affresco::ScreenError If it can't be opened.
affresco::Screen openScreen(const Opened& opened, const Options& options, affresco::Screen::History history) {
	affresco::Screen screen = opened.app ? affresco::Screen::fromApp(opened.location, options.dataDir, history)
	                                     : affresco::Screen::fromFile(opened.location, options.dataDir, history);
	screen.setPlaceholderTypes(true);            // custom types of other apps show as plain panels
	examples::registerCodeEditor(screen);        // the Forge template's "code" type (application layer)
	examples::tempo::registerVisualiser(screen); // the Tempo template's visualiser surface
	return screen;
}

/// @brief Shows the screen and lets peers change it, until the window closes.
/// @param options The command line.
/// @return The exit status.
/// @throws std::exception If the screen, a plugin or a socket can't be opened.
int run(const Options& options) {
	// A plain screenshot run shows the file as it is, so it neither loads nor writes the screen's history
	const bool screenshotOnly = firstFrameOnly();
	const Opened opened = locate(options.file);
	// The Launcher is the host's own: it gets what it uses (grantLauncher()); an app what the person allows, asked
	// before its window opens, and never in a screenshot
	const host::Allowed allowed =
	    options.file.empty() ? host::Allowed() : decide(opened, options, !environmentHas("AFFRESCOUI_SCREENSHOT"));
	affresco::Application app;
	// A peer: it outlives the screen
	affresco::intelligence::HostAssistant assistant(app, options.dataDir, assetsDirectory());
	using History = affresco::Screen::History;
	affresco::Screen screen = openScreen(opened, options, screenshotOnly ? History::InMemory : History::OnDisk);
	host::applyGrants(screen, opened.manifest, allowed, screenshotOnly);
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
	affresco::Window& window = screen.createWindow(app);
	if(!screenshotOnly) startAssistant(assistant, app, screen, window, options, allowed);
	applyPlugins(screen, options.plugins);
	checkScenariosForEvidence(screen, opened.entry);

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
		if(!options) return 2;
		return options->forget.empty() ? run(*options) : forgetPermissions(*options);
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_launcher: " << error.what() << '\n';
		return 1;
	}
}
