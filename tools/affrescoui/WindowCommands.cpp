/// @file
/// @brief `affrescoui run` and `affrescoui render`: the commands that open a window.

#include "Cli.hpp"

#include "SocketDir.hpp" // from examples/: the socket folder's checks, shared with affrescoui_host

#include <affresco/affresco.hpp>

#define GLFW_INCLUDE_NONE // window hints only: no OpenGL header
#include <GLFW/glfw3.h>

#include "Png.hpp" // the library's PNG encoder (modules/screen/src)

#include <array>
#include <charconv>
#include <iostream>

namespace affresco::cli {

namespace {

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

/// @brief Applies change sets the screen's history doesn't have yet, like affrescoui_host --plugin.
/// @param screen The screen.
/// @param plugins The change set files.
/// @throws std::runtime_error If one can't be read or applied.
void applyPlugins(Screen& screen, const std::vector<std::string>& plugins) {
	for(const std::string& plugin : plugins) {
		const std::string text = readFile(plugin);
		if(screen.hasImported(text)) continue;
		if(std::string error; !screen.importChanges(text, &error)) throw std::runtime_error(plugin + ": " + error);
		std::cerr << "plugin applied: " << plugin << " (revision " << screen.revision() << ")\n";
	}
}

} // namespace

Exit runScreen(std::span<const std::string_view> args) {
	// Options repeat (--plugin), so they are read here, not by Arguments
	std::filesystem::path file, socketDirectory = Screen::defaultSocketDirectory(), data = Screen::defaultDataDirectory();
	std::optional<Mode> mode;
	std::vector<std::string> plugins;
	for(std::size_t i = 0; i < args.size(); ++i) {
		const std::string_view arg = args[i];
		const bool valued = arg == "--socket" || arg == "--data" || arg == "--mode" || arg == "--plugin";
		if(valued && i + 1 >= args.size()) throw UsageError(std::string(arg) + " needs a value");
		if(arg == "--socket") socketDirectory = args[++i];
		else if(arg == "--data") data = args[++i];
		else if(arg == "--mode") mode = modeOf(std::string(args[++i]));
		else if(arg == "--plugin") plugins.emplace_back(args[++i]);
		else if(arg.starts_with('-') || !file.empty()) throw UsageError("unexpected " + std::string(arg));
		else file = arg;
	}
	if(file.empty()) throw UsageError("expected one screen file");

	Application app;
	Screen screen = Screen::fromFile(file, data);
	registerApplicationTypes(screen);
	if(mode) screen.setModeForThisRun(*mode);
	screen.createWindow(app);
	applyPlugins(screen, plugins);
	createSocketDir(socketDirectory);
	screen.listen(app, socketDirectory / "studio.sock", "studio");
	screen.listen(app, socketDirectory / "agent.sock", "agent");
	std::cerr << "affrescoui run: listening in " << socketDirectory.string() << " (studio.sock, agent.sock), mode "
	          << toString(screen.versions().mode()) << '\n';
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

	Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // never on screen
	Screen screen = Screen::fromString(readFile(file), file.parent_path());
	registerApplicationTypes(screen);
	const std::filesystem::path frame = std::filesystem::path(*out).concat(".ppm");
	screen.setSocketDirectory(frame.parent_path() / "affrescoui-render-no-host"); // never created: nobody answers
	Window& window = screen.createWindow(app);
	if(const std::optional<std::string> size = arguments.value("--size")) {
		const std::size_t x = size->find('x');
		if(x == std::string::npos) throw UsageError("--size: expected WIDTHxHEIGHT, e.g. 1280x800");
		window.setSize(wholeNumber(std::string_view(*size).substr(0, x), "--size"),
		               wholeNumber(std::string_view(*size).substr(x + 1), "--size"));
	}

	if(!window.saveScreenshot(frame, scale)) throw std::runtime_error("couldn't draw the frame");
	const std::optional<detail::RgbImage> image = detail::readPpm(frame);
	std::filesystem::remove(frame);
	if(!image) throw std::runtime_error("couldn't read the frame back");
	writeFile(*out, detail::encodePng(image->width, image->height, image->rgb));
	std::cout << *out << ": " << image->width << 'x' << image->height << " pixels\n";
	return Exit::Success;
}

} // namespace affresco::cli
