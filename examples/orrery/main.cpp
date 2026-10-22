/// @file
/// @brief affrescoui_orrery: Orrery, a small planetarium whose whole window is a screen document
/// (orrery.screen.json). The host only adds what a file can't: the scene3d module's `scene` surface for the 3D view,
/// and a thumbnail image for each body, which the bodies strip (Sinopia's slide sorter, installed) draws.
//
//   affrescoui_orrery [screen.json] [--socket DIR] [--data DIR]
//
// Like affrescoui_host, it listens for the Studio and an agent in DIR (studio.sock, agent.sock), so they can change
// the screen while it runs.

#include "Assets.hpp"
#include "SocketDir.hpp"

#include <affresco/affresco.hpp>
#include <affresco/scene3d/Mesh.hpp>
#include <affresco/scene3d/Renderer.hpp>
#include <affresco/scene3d/Scene.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using affresco::json;

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
	std::filesystem::path screen = assetsDirectory() / "orrery" / "orrery.screen.json"; ///< The screen to show.
	std::filesystem::path socketDir = affresco::Screen::defaultSocketDirectory();       ///< Where peers connect.
	std::filesystem::path dataDir = affresco::Screen::defaultDataDirectory();           ///< The history, the state.
};

/// @brief Reads the command line.
/// @param argc The number of arguments, the program's name included.
/// @param argv The arguments.
/// @return The options, or std::nullopt after printing the usage if an argument isn't known.
std::optional<Options> parseArguments(int argc, const char* const* argv) {
	Options options;
	bool screenGiven = false;
	for(int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		const bool hasValue = i + 1 < argc;
		if(arg == "--socket" && hasValue) options.socketDir = argv[++i];
		else if(arg == "--data" && hasValue) options.dataDir = argv[++i];
		else if(!arg.starts_with("--") && !screenGiven) {
			options.screen = argv[i];
			screenGiven = true;
		} else {
			std::cerr << "usage: affrescoui_orrery [screen.json] [--socket DIR] [--data DIR]\n";
			return std::nullopt;
		}
	}
	return options;
}

/// @brief Reads a "#rrggbb" colour.
/// @param hex The colour.
/// @return Its red, green and blue, 0 to 1; grey if it isn't one.
std::array<float, 3> colourFromHex(const std::string& hex) {
	if(hex.size() != 7 || hex[0] != '#') return {0.6f, 0.6f, 0.6f};
	std::array<float, 3> colour{};
	for(std::size_t channel = 0; channel < 3; ++channel)
		colour[channel] = static_cast<float>(std::stoi(hex.substr(1 + (2 * channel), 2), nullptr, 16)) / 255.0f;
	return colour;
}

/// @brief Draws a body's thumbnail: a lit disc on a starless night, a glowing one for a star (a body without a
/// parent).
/// @param colour The body's colour.
/// @param star Whether it glows instead of being lit from the left.
/// @param width The image's width in pixels.
/// @param height Its height.
/// @return The pixels, RGBA, row after row from the top.
std::vector<std::uint8_t> drawBody(const std::array<float, 3>& colour, bool star, int width, int height) {
	std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
	const float centreX = static_cast<float>(width) / 2.0f;
	const float centreY = static_cast<float>(height) / 2.0f;
	const float radius = static_cast<float>(std::min(width, height)) * 0.36f;
	const std::array<float, 3> night{0.04f, 0.05f, 0.08f};
	for(int y = 0; y < height; ++y) {
		for(int x = 0; x < width; ++x) {
			const float dx = (static_cast<float>(x) + 0.5f - centreX) / radius;
			const float dy = (static_cast<float>(y) + 0.5f - centreY) / radius;
			const float distance = std::sqrt((dx * dx) + (dy * dy));
			const float cover = std::clamp((1.0f - distance) * radius, 0.0f, 1.0f); // anti-aliased edge
			float light = 0.0f;
			if(star) light = 1.0f - (0.25f * distance * distance);
			else {
				const float dz = std::sqrt(std::max(0.0f, 1.0f - (dx * dx) - (dy * dy)));
				light = std::max(0.0f, (-0.6f * dx) + (-0.3f * dy) + (0.74f * dz)) + 0.08f;
			}
			const float glow = star ? std::max(0.0f, 0.5f - (0.3f * distance)) : 0.0f;
			const std::size_t at =
			    (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
			for(std::size_t channel = 0; channel < 3; ++channel) {
				const float lit = std::min(1.0f, colour[channel] * light);
				const float sky = night[channel] + (glow * colour[channel]);
				const float value = (lit * cover) + (sky * (1.0f - cover));
				pixels[at + channel] = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
			}
			pixels[at + 3] = 255;
		}
	}
	return pixels;
}

/// @brief Gives each body in the screen's initial state a thumbnail, named "body:<id>" (the sky.bodies export's
/// thumbnail). Clones keep their original's look, so the bodies the file starts with are all it needs.
/// @param screen The screen.
/// @param file The screen's file, read for its state.bodies.
/// @throws std::exception If the file can't be read.
void drawThumbnails(affresco::Screen& screen, const std::filesystem::path& file) {
	std::ifstream in(file);
	const json document = json::parse(in);
	const json bodies = document.value("state", json::object()).value("bodies", json::array());
	constexpr int width = 96;
	constexpr int height = 72;
	for(const json& body : bodies) {
		const std::vector<std::uint8_t> pixels = drawBody(colourFromHex(body.value("hex", std::string())),
		                                                  body.value("parent", std::string()).empty(), width, height);
		screen.setImage("body:" + body.value("id", std::string()), width, height, pixels);
	}
}

/// @brief Registers the 3D view: the `scene` surface, with a sphere for the bodies and a thin ring for the orbits,
/// and the `orrery.scene` call the screen makes to know the surface is there (the affrescoui tool has none, so the
/// call fails and the screen shows a note instead).
/// @param screen The screen.
void registerScene(affresco::Screen& screen) {
	using namespace affresco::scene3d;
	auto assets = std::make_shared<Assets>();
	assets->setMesh("sphere", makeSphere(48, 24));
	assets->setMesh("orbit", makeTorus(1.0f, 0.006f, 128, 6));
	auto probe = std::make_shared<ViewportProbe>();
	screen.registerSurface("scene", [assets, probe] { return makeSceneRenderer(assets, probe); }, {true});
	screen.onCall("orrery.scene", [](const std::string&) { return std::string("true"); });
}

/// @brief Shows Orrery until the window closes.
/// @param options The command line.
/// @return The exit status.
/// @throws std::exception If the screen or a socket can't be opened.
int run(const Options& options) {
	affresco::Application app;
	const bool screenshotOnly = firstFrameOnly();
	using History = affresco::Screen::History;
	affresco::Screen screen = affresco::Screen::fromFile(options.screen, options.dataDir,
	                                                     screenshotOnly ? History::InMemory : History::OnDisk);
	registerScene(screen);
	drawThumbnails(screen, options.screen);
	screen.setSocketDirectory(options.socketDir);
	screen.createWindow(app);

	if(!screenshotOnly) {
		createSocketDir(options.socketDir);
		screen.listen(app, options.socketDir / "studio.sock", "studio");
		screen.listen(app, options.socketDir / "agent.sock", "agent");
		std::cerr << "affrescoui_orrery: listening in " << options.socketDir.string() << " (studio.sock, agent.sock)\n";
	}
	return app.run();
}

} // namespace

/// @brief Runs Orrery.
/// @param argc The number of arguments.
/// @param argv The arguments.
/// @return 0, 1 for an error (written to stderr), or 2 for wrong arguments.
int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		const std::optional<Options> options = parseArguments(argc, argv);
		return options ? run(*options) : 2;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_orrery: " << error.what() << '\n';
		return 1;
	}
}
