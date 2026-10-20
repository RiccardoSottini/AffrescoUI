/// @file
/// @brief Plays one rendering scenario from a file in scenarios/ and saves a frame at each "capture" step.
///
/// Usage: affrescoui_render_runner <scenarios file> <scenario name> <output dir>
///
/// The file is one area's (e.g. scenarios/kanban.json), and its "screens" folder is relative to it.
///
/// The frames don't depend on the machine or on what ran before: the window is hidden and only receives the events the
/// steps send, captures are drawn offscreen at the scenario's "scale" (default 1) whatever the display, and screens
/// are loaded without a state file. check.py runs every scenario and compares the frames with the golden images.
///
/// A capture is drawn the way the window draws its frames: from each panel's retained geometry, only where something
/// changed since the last capture. Each one is also drawn from scratch, and the two must be the same: a pixel the
/// renderer failed to draw again (a stale pixel) fails the scenario, whatever the golden image says.

#include "CodeEditor.hpp"

#include "Headless.hpp"
#include "detail/Ppm.hpp"

#include <affresco/affresco.hpp>

#define GLFW_INCLUDE_NONE // only window hints and events here: no OpenGL header (Linux may have none installed)
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace {

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its contents.
/// @throws std::runtime_error If it can't be read.
std::string readFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	if(!file) throw std::runtime_error("could not open " + path.string());
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

/// @brief Finds the n-th shown panel with an id, in tree order.
/// @param panel Where to search.
/// @param id The id.
/// @param n How many matches to skip; counts down.
/// @return The panel, or nullptr.
affresco::Panel* findShown(affresco::Panel& panel, const std::string& id, int& n) {
	if(panel.isShown() && panel.id() == id && n-- == 0) return &panel;
	for(const auto& child : panel.children())
		if(affresco::Panel* found = findShown(*child, id, n)) return found;
	return nullptr;
}

/// @brief Where a step points: [x, y] in window units, or "#id" (the centre of that panel; "nth" picks among repeats).
/// @param window The window.
/// @param step The step.
/// @param target Its target.
/// @return The point.
/// @throws std::runtime_error If no shown panel has that id.
affresco::Vec2 pointOf(affresco::Window& window, const json& step, const json& target) {
	if(target.is_array()) return {target[0].get<float>(), target[1].get<float>()};

	static_cast<void>(window.panelAt({})); // lays out, so the bounds are current
	const std::string id = target.get<std::string>().substr(1);
	int n = step.value("nth", 0);
	const affresco::Panel* panel = findShown(window.root(), id, n);
	if(!panel)
		throw std::runtime_error("no shown panel '#" + id + "' (nth " + std::to_string(step.value("nth", 0)) + ")");
	const affresco::Rect& r = panel->bounds();
	return {(r.left + r.right) / 2.0f, (r.top + r.bottom) / 2.0f};
}

/// @brief Parses a key such as "enter", "a" or "cmd+z".
/// @param text The key.
/// @return The key and its modifiers.
/// @throws std::runtime_error If the key is unknown.
std::pair<affresco::Key, affresco::Mods> keyOf(const std::string& text) {
	static const std::map<std::string, affresco::Key> named = {
	    {"enter", affresco::Key::Enter},         {"escape", affresco::Key::Escape}, {"tab", affresco::Key::Tab},
	    {"backspace", affresco::Key::Backspace}, {"delete", affresco::Key::Delete}, {"space", affresco::Key::Space},
	    {"left", affresco::Key::Left},           {"right", affresco::Key::Right},   {"up", affresco::Key::Up},
	    {"down", affresco::Key::Down},           {"home", affresco::Key::Home},     {"end", affresco::Key::End},
	    {"slash", affresco::Key::Slash}};
	static const std::map<std::string, int> modifiers = {{"shift", 0x1}, {"ctrl", 0x2}, {"alt", 0x4}, {"cmd", 0x8}};

	affresco::Mods mods;
	std::string rest = text;
	for(std::size_t plus; (plus = rest.find('+')) != std::string::npos; rest.erase(0, plus + 1)) {
		const auto modifier = modifiers.find(rest.substr(0, plus));
		if(modifier == modifiers.end()) throw std::runtime_error("unknown modifier in '" + text + "'");
		mods.bits |= modifier->second;
	}
	if(const auto key = named.find(rest); key != named.end()) return {key->second, mods};
	if(rest.size() == 1 && rest[0] >= 'a' && rest[0] <= 'z')
		return {static_cast<affresco::Key>(static_cast<int>(affresco::Key::A) + (rest[0] - 'a')), mods};
	throw std::runtime_error("unknown key '" + text + "'");
}

/// @brief Reads the pixels of a binary PPM the window wrote.
/// @param path The file.
/// @return Its RGB bytes.
/// @throws std::runtime_error If it can't be read.
std::vector<std::uint8_t> ppmPixels(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	std::string magic;
	int width = 0, height = 0, maximum = 0;
	if(!(file >> magic >> width >> height >> maximum) || magic != "P6")
		throw std::runtime_error("not a PPM: " + path.string());
	file.get(); // the one whitespace after the header
	std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
	file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
	if(!file) throw std::runtime_error("truncated PPM: " + path.string());
	return pixels;
}

/// @brief Checks that a capture, drawn from retained geometry where something changed, is the frame drawn from scratch.
/// @param window The window.
/// @param path The capture.
/// @param scale Its pixels per window unit.
/// @throws std::runtime_error If they differ: the frame drawn from scratch is saved next to it.
void checkAgainstScratch(affresco::Window& window, const std::filesystem::path& path, int scale) {
	const std::vector<std::uint8_t> scratch = affresco::detail::drawFromScratch(window, scale);
	if(ppmPixels(path) == scratch) return;
	std::filesystem::path reference = path;
	reference.replace_extension(".scratch.ppm");
	affresco::detail::writePpm(reference, window.width() * scale, window.height() * scale, scratch);
	throw std::runtime_error("stale pixels: " + path.string() + " differs from the same frame drawn from scratch (" +
	                         reference.string() + ")");
}

/// @brief A scenario being played.
struct Player {
	affresco::Screen& screen;  ///< The screen.
	affresco::Window& window;  ///< Its window.
	std::filesystem::path out; ///< Where captures go.
	std::string name;          ///< The scenario's name, the prefix of its captures.
	int scale;                 ///< Pixels per window unit in captures.
};

/// @brief Plays one step.
/// @param player The scenario.
/// @param step The step (see tests/render/README.md).
/// @throws std::runtime_error If the step is invalid or fails.
void play(Player& player, const json& step) {
	using affresco::Action;
	using affresco::MouseButton;
	auto& [screen, window, out, name, scale] = player;

	auto button = [&](const json& target, Action action) {
		const affresco::Vec2 at = pointOf(window, step, target);
		window.routeMouseMove(at);
		window.routeMouseButton({MouseButton::Left, action, {}, at});
	};

	if(step.contains("capture")) {
		const auto path = out / (name + "-" + step["capture"].get<std::string>() + ".ppm");
		if(!window.saveScreenshot(path, scale)) throw std::runtime_error("could not save " + path.string());
		checkAgainstScratch(window, path, scale);
	} else if(step.contains("move")) {
		window.routeMouseMove(pointOf(window, step, step["move"]));
	} else if(step.contains("click")) {
		button(step["click"], Action::Press);
		button(step["click"], Action::Release);
	} else if(step.contains("press")) {
		button(step["press"], Action::Press);
	} else if(step.contains("release")) {
		button(step["release"], Action::Release);
	} else if(step.contains("scroll")) {
		const affresco::Vec2 at = pointOf(window, step, step["scroll"]);
		window.routeMouseMove(at);
		window.routeScroll({{step.value("dx", 0.0f), step.value("dy", 0.0f)}, at});
	} else if(step.contains("type")) {
		for(const char c : step["type"].get<std::string>()) // ASCII
			window.routeChar({static_cast<char32_t>(static_cast<unsigned char>(c))});
	} else if(step.contains("key")) {
		const auto [key, mods] = keyOf(step["key"].get<std::string>());
		window.routeKey({key, 0, Action::Press, mods});
		window.routeKey({key, 0, Action::Release, mods});
	} else if(step.contains("revision")) {
		std::string error;
		if(!screen.applyRevision(step["revision"].dump(), &error)) throw std::runtime_error("revision: " + error);
	} else if(step.contains("action")) {
		screen.runAction(step["action"].get<std::string>(), window.root(), step.value("event", json::object()).dump());
	} else if(step.contains("resize")) {
		window.setSize(step["resize"][0].get<int>(), step["resize"][1].get<int>());
	} else if(step.contains("wait")) {
		// Runs timers for real: capture only once what they animate has finished, or frames differ run to run
		const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(step["wait"].get<int>());
		while(std::chrono::steady_clock::now() < until) {
			window.render();
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	} else {
		throw std::runtime_error("unknown step " + step.dump());
	}
}

} // namespace

int main(int argc, char** argv) {
	if(argc != 4) {
		std::cerr << "usage: affrescoui_render_runner <scenarios file> <scenario> <output dir>\n";
		return 2;
	}
	const std::filesystem::path scenarios = argv[1], out = argv[3];
	const std::string name = argv[2];

	try {
		const json all = json::parse(readFile(scenarios));
		const auto scenario = std::ranges::find_if(all["scenarios"], [&](const json& s) { return s["name"] == name; });
		if(scenario == all["scenarios"].end()) throw std::runtime_error("no scenario '" + name + "'");

		const std::filesystem::path file =
		    scenarios.parent_path() / all["screens"].get<std::string>() / (*scenario)["screen"].get<std::string>();
		affresco::Application app;
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // never on screen
		affresco::Screen screen = affresco::Screen::fromString(readFile(file), file.parent_path());
		screen.setPlaceholderTypes(true);           // as affrescoui_host does
		examples::registerCodeEditor(screen);       // and its application-layer "code" type
		screen.setSocketDirectory(out / "no-host"); // channels find nobody: the "disconnected" look, every time

		Player player{screen, screen.createWindow(app), out, name, scenario->value("scale", 1)};
		for(const json& step : (*scenario)["steps"]) play(player, step);
		return EXIT_SUCCESS;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_render_runner: " << name << ": " << error.what() << '\n';
		return EXIT_FAILURE;
	}
}
