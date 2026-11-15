/// @file
/// @brief `affrescoui run` and `affrescoui render`: the commands that open a window.

#include "Cli.hpp"
#include "JsonText.hpp"

#include "SocketDir.hpp" // from apps/shared: the socket folder's checks, shared with affrescoui_launcher
#ifndef __EMSCRIPTEN__
#include "EditorPort.hpp" // from apps/shared: the .nrepl-port file, shared with affrescoui_launcher
#endif

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp> // unconnectedScreen()
#include <affresco/core/Portable.hpp>        // environmentVariable()
#include <affresco/notation/Notation.hpp>

#include "Png.hpp" // the library's PNG encoder (modules/screen/src)

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <optional>

namespace affresco::cli {

namespace {

/// The widest and tallest frame `render` draws, in pixels: the renderbuffer size every desktop OpenGL driver offers
/// (Apple's and Mesa's llvmpipe have 16,384). A larger one came back black, after allocating gigabytes for it.
constexpr int maxFrameSide = 16384;

/// @brief Reads a whole number from the command line.
/// @param text The digits.
/// @param option The option, for the error.
/// @return The number.
/// @throws UsageError If it isn't one from 1 to 100000.
int wholeNumber(std::string_view text, std::string_view option) {
	int value = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if(error != std::errc() || end != text.data() + text.size() || value < 1 || value > 100000)
		throw UsageError(std::string(option) + ": expected a whole number, got '" + std::string(text) + "'");
	return value;
}

/// @brief Reads a mode.
/// @param text "default", "acceptEdits" or "plan".
/// @return The mode.
/// @throws UsageError If it is none of them.
Mode modeOf(const std::string& text) {
	const std::optional<Mode> mode = modeFromString(text);
	if(!mode) throw UsageError("--mode: expected default, acceptEdits or plan");
	return *mode;
}

/// @brief Applies change sets the screen's history doesn't have yet, like affrescoui_launcher --plugin.
/// @param screen The screen.
/// @param plugins The change set files.
/// @throws std::runtime_error If one can't be read or applied.
void applyPlugins(Screen& screen, const std::vector<std::string>& plugins) {
	for(const std::string& plugin : plugins) {
		const std::string text = readFile(plugin);
		if(screen.hasImported(text)) continue;
		if(std::string error; !screen.importChanges(text, &error))
			throw std::runtime_error(std::format("{}: {}", plugin, error));
		std::cerr << "plugin applied: " << plugin << " (revision " << screen.revision() << ")\n";
	}
}

/// @brief What `run` is told.
struct RunOptions {
	std::filesystem::path file;                                               ///< The screen.
	std::filesystem::path socketDirectory = Screen::defaultSocketDirectory(); ///< Where peers connect.
	std::filesystem::path data = Screen::defaultDataDirectory();              ///< The history and "persist" state.
	std::optional<Mode> mode;                                                 ///< For this run only, if given.
	std::vector<std::string> plugins;                                         ///< Change sets to apply once.
	bool print = false;           ///< Print a Lisp file's built tree instead of showing it.
	bool editor = false;          ///< Let nREPL clients connect (--nrepl), each once the person says yes.
	std::uint16_t editorPort = 0; ///< Their port on 127.0.0.1 (--nrepl PORT); 0 for any free one.
};

/// @brief Reads the port that may follow --nrepl.
/// @param text The argument after it.
/// @return The port, or nothing if the argument isn't one (then it is read as itself).
std::optional<std::uint16_t> portOf(std::string_view text) {
	unsigned port = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
	if(text.empty() || error != std::errc() || end != text.data() + text.size() || port > 65535U) return std::nullopt;
	return static_cast<std::uint16_t>(port);
}

/// @brief Reads `run`'s command line. Options repeat (--plugin), so Arguments doesn't read it.
/// @param args The arguments.
/// @return The options.
/// @throws UsageError If they are wrong.
RunOptions runOptions(std::span<const std::string_view> args) {
	RunOptions options;
	for(std::size_t i = 0; i < args.size(); ++i) {
		const std::string_view arg = args[i];
		if(!arg.starts_with('-')) {
			if(!options.file.empty()) throw UsageError("unexpected " + std::string(arg));
			options.file = arg;
			continue;
		}
		if(arg == "--print") {
			options.print = true;
			continue;
		}
		if(arg == "--nrepl") {
			options.editor = true;
			if(const std::optional<std::uint16_t> port = i + 1 < args.size() ? portOf(args[i + 1]) : std::nullopt) {
				options.editorPort = *port;
				++i;
			}
			continue;
		}
		if(i + 1 >= args.size()) throw UsageError(std::string(arg) + " needs a value, or isn't an option of run");
		const std::string_view value = args[++i];
		if(arg == "--socket") options.socketDirectory = value;
		else if(arg == "--data") options.data = value;
		else if(arg == "--mode") options.mode = modeOf(std::string(value));
		else if(arg == "--plugin") options.plugins.emplace_back(value);
		else throw UsageError("unknown option " + std::string(arg));
	}
	if(options.file.empty()) throw UsageError("expected one screen file");
	return options;
}

/// @brief The app a file opens as: an app's folder, its app.json or app.lisp, or the entry screen its manifest names,
/// in either notation (a program's entry names its other files with an `include` form only an app's loader reads).
/// @param file The file, or a folder.
/// @return The app's folder, or nothing for a screen of its own.
/// @throws ScreenError If the manifest beside it can't be read.
std::optional<std::filesystem::path> appLocation(const std::filesystem::path& file) {
	std::error_code error;
	if(std::filesystem::is_directory(file, error)) return file;
	if(notation::isNamedDocument(file, "app")) return file.parent_path();
	if(!Screen::appOf(file).empty()) return file.parent_path();
	return std::nullopt;
}

#ifndef __EMSCRIPTEN__
/// @brief With --nrepl, lets nREPL clients connect to the app's program (Screen::listenEditor()), and keeps the port
/// in .nrepl-port in the app's folder while it runs; a plain screenshot (AFFRESCOUI_SCREENSHOT alone) takes none.
/// @param screen The screen, with its window.
/// @param app The application, which delivers the server's messages.
/// @param options The command line.
/// @param folder The app's folder, or the screen file's.
/// @return The port file, or nullptr when there is no server.
/// @throws std::runtime_error If the screen isn't a program, or the port is taken.
std::unique_ptr<EditorPort> listenForEditors(Screen& screen, Application& app, const RunOptions& options,
                                             const std::filesystem::path& folder) {
	if(!options.editor ||
	   (environmentVariable("AFFRESCOUI_SCREENSHOT") && !environmentVariable("AFFRESCOUI_SCREENSHOT_AFTER")))
		return nullptr;
	net::EditorServerOptions server;
	server.port = options.editorPort;
	const std::uint16_t port = screen.listenEditor(app, std::move(server));
	return std::make_unique<EditorPort>(std::filesystem::absolute(folder), port);
}
#endif

} // namespace

Exit runScreen(std::span<const std::string_view> args) {
	const RunOptions options = runOptions(args);
	const std::optional<std::filesystem::path> location = appLocation(options.file);
	if(location && options.print) { // the app's document, as the host loads it: its program built, its files merged
		const Screen loaded = Screen::fromApp(*location, {}, Screen::History::InMemory);
		std::cout << printText(parseText(loaded.documentJson())) << std::flush;
		return Exit::Success;
	}
	if(!location)
		if(const std::optional<Exit> built = runLisp(options.file, options.print)) return *built;
	Application app;
	Screen screen = location ? Screen::fromApp(*location, options.data) : Screen::fromFile(options.file, options.data);
	registerApplicationTypes(screen);
	if(options.mode) screen.setModeForThisRun(*options.mode);
	screen.setSocketDirectory(options.socketDirectory); // its channels reach this program's sockets only
	screen.confineChannels(true);
	screen.createWindow(app);
	applyPlugins(screen, options.plugins);
	createSocketDir(options.socketDirectory);
	screen.listen(app, options.socketDirectory / "studio.sock", "studio");
	screen.listen(app, options.socketDirectory / "agent.sock", "agent");
	std::cerr << "affrescoui run: listening in " << options.socketDirectory.string()
	          << " (studio.sock, agent.sock), mode " << toString(screen.versions().mode()) << '\n';
#ifndef __EMSCRIPTEN__
	// Goes before the screen: the file first, then the server with it
	const std::unique_ptr<EditorPort> editorPort =
	    listenForEditors(screen, app, options, location ? *location : options.file.parent_path());
#endif
	return app.run() == 0 ? Exit::Success : Exit::Failure;
}

// Drawn as the rendering check draws a first frame: a hidden window, offscreen at the scale asked for, the file as it
// is (no history, no saved state), channels finding nobody
Exit renderScreen(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 3> valued = {"-o", "--scale", "--size"};
	const Arguments arguments(args, valued, {});
	const std::filesystem::path file = arguments.file();
	const std::optional<std::string> out = arguments.value("-o");
	if(!out) throw UsageError("expected -o out.png");
	const int scale = wholeNumber(arguments.value("--scale").value_or("1"), "--scale");
	if(scale > ScreenImages::maxScale) throw UsageError("--scale: at most " + std::to_string(ScreenImages::maxScale));

	Application app(ApplicationOptions{nullptr, true}); // never on screen
	const std::filesystem::path frame = std::filesystem::path(*out).concat(".ppm");
	Screen screen = automation::unconnectedScreen(automation::screenDocument(file), file.parent_path(),
	                                              frame.parent_path() / "affrescoui-render-no-host");
	registerApplicationTypes(screen);
	Window& window = screen.createWindow(app);
	if(const std::optional<std::string> size = arguments.value("--size")) {
		const std::size_t x = size->find('x');
		if(x == std::string::npos) throw UsageError("--size: expected WIDTHxHEIGHT, e.g. 1280x800");
		window.setSize(wholeNumber(std::string_view(*size).substr(0, x), "--size"),
		               wholeNumber(std::string_view(*size).substr(x + 1), "--size"));
	}
	// The window's size comes from --size or the file. Divided, not multiplied, so a huge one can't overflow
	if(std::max(window.width(), window.height()) > maxFrameSide / scale)
		throw std::runtime_error(std::format("a frame of {}x{} at scale {} is larger than {} pixels a side",
		                                     window.width(), window.height(), scale, maxFrameSide));

	screen.waitForImages(); // image files decode on a worker thread
	if(!window.saveScreenshot(frame, scale)) throw std::runtime_error("couldn't draw the frame");
	const std::optional<detail::RgbImage> image = detail::readPpm(frame);
	std::filesystem::remove(frame);
	if(!image) throw std::runtime_error("couldn't read the frame back");
	writeFile(*out, detail::encodePng(image->width, image->height, image->rgb));
	std::cout << *out << ": " << image->width << 'x' << image->height << " pixels\n";
	return Exit::Success;
}

} // namespace affresco::cli
