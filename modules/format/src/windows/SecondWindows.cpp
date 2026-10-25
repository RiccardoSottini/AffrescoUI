/// @file
/// @brief format::SecondWindows: the windows of the "windows" section, each showing a named image, and the section's
/// check.

#include "SecondWindows.hpp"
#include "Engine.hpp"

#include "affresco/render/Application.hpp"
#include "affresco/render/ImageView.hpp"
#include "affresco/render/Window.hpp"

#include <array>
#include <memory>
#include <string_view>

namespace affresco::format {

namespace {

/// The keys of a window's entry.
constexpr std::array<std::string_view, 9> entryKeys{"title",      "width", "height",  "image",  "background",
                                                    "fullscreen", "keys",  "onClick", "onClose"};

constexpr int smallestSide = 100;        ///< The smallest width or height, in window units.
constexpr int largestSide = 8192;        ///< The largest.
constexpr std::size_t longestText = 200; ///< The longest title or image name, in bytes.
constexpr double watchMs = 500.0;        ///< How often a closed window is noticed.

/// @brief Whether a window's name is one: a letter, then letters, digits, "-" or "_", 64 at most, never "main".
/// @param name The name.
/// @return Whether it is.
[[nodiscard]] bool validName(std::string_view name) noexcept {
	const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
	if(name.empty() || name.size() > 64 || !letter(name.front()) || name == "main") return false;
	for(const char c : name)
		if(!letter(c) && !(c >= '0' && c <= '9') && c != '-' && c != '_') return false;
	return true;
}

/// @brief Checks one window's entry.
/// @param entry The entry.
/// @param at Its place.
/// @throws DocumentError If it is wrong.
void checkEntry(const json& entry, const std::string& at) {
	for(const auto& [key, value] : expect::object(entry, at).items()) {
		const std::string place = at + "." + key;
		if(!std::ranges::contains(entryKeys, std::string_view(key))) expect::fail(place, "unknown property");
		if(key == "width" || key == "height") {
			const int side = expect::integer(value, place);
			if(side < smallestSide || side > largestSide) expect::fail(place, "expected 100 to 8192");
		} else if(key == "title" || key == "image" || key == "background") {
			if(expect::string(value, place).size() > longestText) expect::fail(place, "is longer than 200 bytes");
		} else if(key == "fullscreen" || key == "keys") {
			static_cast<void>(expect::boolean(value, place));
		} else if(!value.is_string() && !value.is_array() && !value.is_object()) {
			expect::fail(place, "expected an action's name or steps");
		}
	}
	if(!entry.contains("image")) expect::fail(at + ".image", "required: the named image the window shows");
}

} // namespace

void checkWindowsSection(const json& section, const std::string& at) {
	if(expect::object(section, at).size() > maxSecondWindows) expect::fail(at, "declares more than 4 windows");
	for(const auto& [name, entry] : section.items()) {
		if(!validName(name))
			expect::fail(at + "." + name, "a name is a letter, then letters, digits, - or _ (64 at most), not main");
		checkEntry(entry, at + "." + name);
	}
}

SecondWindows::~SecondWindows() {
	if(watchTimer_ != 0 && engine_.window) engine_.window->cancelTimer(watchTimer_);
	for(auto& [name, shown] : shown_) {
		if(!shown.window) continue;
		shown.window->onKey.disconnect(shown.keyHook);
		shown.window->onMouseButton.disconnect(shown.buttonHook);
		shown.window->cancelTimer(shown.watchTimer);
		shown.window->setShouldClose(true); // hidden until the Application goes: windows can't be destroyed
	}
}

const json* SecondWindows::entry(const std::string& name) const {
	const json& document = engine_.versions.document();
	const auto section = document.find("windows");
	if(section == document.end() || !section->is_object()) return nullptr;
	const auto found = section->find(name);
	return found == section->end() ? nullptr : &*found;
}

void SecondWindows::open(const std::string& name, std::optional<bool> fullscreen, const std::string& at) {
	const json* declared = entry(name);
	if(!declared) fail(at, "no window \"" + name + "\" in the \"windows\" section");
	Shown& shown = shown_[name];
	const bool fill = fullscreen.value_or(declared->value("fullscreen", false));
	if(application_ && engine_.window) {
		if(!shown.window) make(name, shown, *declared);
		apply(shown, *declared);
		shown.window->setShouldClose(false); // a window closed before is shown again
		shown.window->setVisible(true);
		shown.window->setFullscreen(fill);
		watch();
	}
	shown.open = true;
	shown.fullscreen = fill;
	changed();
}

void SecondWindows::close(const std::string& name) {
	const auto found = shown_.find(name);
	if(found == shown_.end() || !found->second.open) return;
	Shown& shown = found->second;
	if(shown.window) {
		shown.window->setFullscreen(false); // a display left filled by a hidden window would stay black
		shown.window->setShouldClose(true); // Application hides it, and no longer waits for it
	}
	shown.open = false;
	changed();
}

void SecondWindows::setFullscreen(const std::string& name, std::optional<bool> fullscreen, const std::string& at) {
	if(name == "main") {
		if(!engine_.window) return;
		engine_.window->setFullscreen(fullscreen.value_or(!engine_.window->isFullscreen()));
		changed();
		return;
	}
	if(!entry(name)) fail(at, "no window \"" + name + "\" in the \"windows\" section");
	const auto found = shown_.find(name);
	if(found == shown_.end() || !found->second.open) fail(at, "the window \"" + name + "\" isn't open");
	Shown& shown = found->second;
	shown.fullscreen = fullscreen.value_or(!shown.fullscreen);
	if(shown.window) shown.window->setFullscreen(shown.fullscreen);
	changed();
}

json SecondWindows::status() const {
	json result = json::object();
	result["main"] = {{"fullscreen", engine_.window && engine_.window->isFullscreen()}};
	const json& document = engine_.versions.document();
	const auto section = document.find("windows");
	if(section == document.end() || !section->is_object()) return result;
	for(const auto& [name, declared] : section->items()) {
		const auto found = shown_.find(name);
		const bool open = found != shown_.end() && found->second.open;
		result[name] = {{"open", open}, {"fullscreen", open && found->second.fullscreen}};
	}
	return result;
}

void SecondWindows::documentChanged() {
	for(auto& [name, shown] : shown_) {
		const json* declared = entry(name);
		if(!declared) close(name);
		else if(shown.window && shown.open) apply(shown, *declared);
	}
}

void SecondWindows::make(const std::string& name, Shown& shown, const json& declared) {
	Window& window =
	    application_->createWindow(declared.value("title", std::string("AffrescoUI")), declared.value("width", 800),
		                           declared.value("height", 600), *engine_.window);
	auto view = std::make_unique<ImageView>();
	for(const Edge edge : {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right}) view->setAnchor(edge, Length{});
	shown.view = &static_cast<ImageView&>(window.root().addChild(std::move(view)));
	shown.window = &window;
	// Its keys run the screen's, as if nothing had the focus; a press of the mouse runs its "onClick"
	shown.keyHook = window.onKey.connect([this, name](const KeyEvent& event) {
		const json* declared = entry(name);
		if(declared && declared->value("keys", true)) engine_.events.keyFromAnotherWindow(event);
	});
	shown.buttonHook = window.onMouseButton.connect([this, name](const MouseButtonEvent& event) {
		if(event.action == affresco::Action::Press) runHandler(name, "onClick");
	});
	watchScreen(shown);
}

void SecondWindows::apply(Shown& shown, const json& declared) const {
	shown.window->setTitle(declared.value("title", std::string("AffrescoUI")));
	const json background = declared.value("background", json("#000000"));
	shown.window->root().setColor(engine_.document.color(background, "windows.background"));
	shown.view->showNamed(engine_.document.images(), declared.value("image", std::string()));
}

void SecondWindows::runHandler(const std::string& name, const char* key) {
	const json* declared = entry(name);
	if(!declared || !declared->contains(key) || !engine_.window) return;
	engine_.runner.handle((*declared)[key], engine_.window->root(), {{"type", key}, {"window", name}}, *noVariables(),
	                      "windows." + name + "." + key);
}

void SecondWindows::watch() {
	if(watchTimer_ != 0 || !engine_.window) return;
	watchTimer_ = engine_.window->setTimer(watchMs, [this] {
		watchTimer_ = 0;
		bool open = false;
		for(auto& [name, shown] : shown_) {
			if(shown.open && shown.window && shown.window->shouldClose()) { // its own button closed it
				shown.window->setFullscreen(false);
				shown.open = false;
				changed();
				runHandler(name, "onClose");
			}
			open = open || shown.open;
		}
		if(open) watch();
	});
}

void SecondWindows::watchScreen(Shown& shown) {
	shown.watchTimer = shown.window->setTimer(watchMs, [this, &shown] {
		if(engine_.window && engine_.window->shouldClose()) shown.window->setShouldClose(true);
		watchScreen(shown);
	});
}

void SecondWindows::changed() {
	engine_.document.globalsChanged();
	engine_.reconciler.requestReconcile();
}

} // namespace affresco::format
