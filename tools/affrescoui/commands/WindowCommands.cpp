/// @file
/// @brief `affrescoui run` and `affrescoui render`: the commands that open a window.

#include "Cli.hpp"

#include "SocketDir.hpp" // from apps/shared: the socket folder's checks, shared with affrescoui_launcher

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp> // unconnectedScreen()

#include "Png.hpp" // the library's PNG encoder (modules/screen/src)

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <iostream>

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
};

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

} // namespace

Exit runScreen(std::span<const std::string_view> args) {
	const RunOptions options = runOptions(args);
	Application app;
	Screen screen = Screen::fromFile(options.file, options.data);
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
