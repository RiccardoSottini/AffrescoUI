/// @file
/// @brief The window steps: openWindow, closeWindow and fullscreen, on the windows of the "windows" section
/// (SecondWindows) and the screen's own; and openUrl, which opens a web address in the browser on the person's yes.

#include "Engine.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace affresco::format {

namespace {

/// @brief Reads a step's "fullscreen": true, false, or (with @p toggle) "toggle".
/// @param s The step.
/// @param value The value under its key.
/// @param at Its place.
/// @param toggle Whether "toggle" is allowed, read as nothing.
/// @return The value, or nothing for "toggle".
/// @throws DocumentError If it is none of them.
std::optional<bool> readFullscreen(const Running& s, const json& value, const std::string& at, bool toggle) {
	const json read = s.value(value, at);
	if(read.is_boolean()) return read.get<bool>();
	if(toggle && read == "toggle") return std::nullopt;
	fail(at, toggle ? R"(expected true, false or "toggle")" : "expected true or false");
}

/// @brief Whether a text is a web address the openUrl step may offer: http:// or https:// (any case) and a host,
/// at most 2,048 bytes, with no spaces, control characters or user information ('@' before the host).
/// @param url The text.
/// @return true if so.
bool webAddress(std::string_view url) {
	if(url.size() > 2048) return false;
	const auto lower = [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); };
	std::string scheme;
	for(const char c : url.substr(0, url.find(':'))) scheme += lower(c);
	if(scheme != "http" && scheme != "https") return false;
	const std::string_view rest = url.substr(scheme.size());
	if(!rest.starts_with("://")) return false;
	const std::string_view authority = rest.substr(3, rest.find_first_of("/?#", 3) - 3);
	if(authority.empty() || authority.contains('@') || authority.starts_with(':')) return false;
	return std::ranges::none_of(url, [](char c) { return static_cast<unsigned char>(c) <= ' ' || c == 0x7f; });
}

/// @brief Runs an openUrl step: asks the person, in a question the window draws itself, whether to open the address
/// in their browser; opens it only on their yes. Whoever ran the step (a click, a timer, a revision's code), the
/// answer is the person's: the window takes it only from their own input.
/// @param s The step.
void openUrlStep(const Running& s) {
	Engine& engine = s.steps.engine();
	const std::string url = text(s.value("openUrl"));
	if(!webAddress(url)) fail(s.where + ".openUrl", "expected an http:// or https:// web address");
	if(!engine.openWebAddress || !engine.window) {
		engine.report("AffrescoUI screen: " + s.where + ": this host opens no web addresses");
		return;
	}
	if(engine.versions.previewing()) {
		engine.reportRefused("AffrescoUI screen: " + s.where + ": no web address opens while a proposal is previewed");
		return;
	}
	if(engine.webQuestion != 0) engine.window->dismissConfirmation(engine.webQuestion); // the newest one is asked
	engine.webQuestion = engine.window->confirm(
	    {.title = "Open this web address?",
	     .message = "It opens in your browser. Open it only if you expect it and know where it goes.",
	     .text = url,
	     .accept = "Open",
	     .decline = "Cancel"},
	    [&engine, url](bool yes) {
		    engine.webQuestion = 0;
		    if(yes && engine.openWebAddress) engine.openWebAddress(url);
	    });
	if(engine.webQuestion == 0) engine.report("AffrescoUI screen: " + s.where + ": a question is shown already");
}

/// @brief Runs an openWindow, closeWindow or fullscreen step.
/// @param s The step.
/// @return std::nullopt if it is none of them, else true.
std::optional<bool> runWindowStep(const Running& s) {
	SecondWindows& windows = s.steps.engine().otherWindows;
	if(s.step.contains("openUrl")) {
		openUrlStep(s);
		return true;
	}
	if(s.step.contains("openWindow")) {
		const std::string name = text(s.value("openWindow"));
		std::optional<bool> fullscreen;
		if(s.step.contains("fullscreen"))
			fullscreen = readFullscreen(s, s.step["fullscreen"], s.where + ".fullscreen", false);
		windows.open(name, fullscreen, s.where);
		return true;
	}
	if(s.step.contains("closeWindow")) {
		const std::string name = text(s.value("closeWindow"));
		if(name == "main") windows.closeMain(s.where); // the screen's own: the app ends
		else windows.close(name);
		return true;
	}
	if(!s.step.contains("fullscreen")) return std::nullopt;
	const std::string name = s.step.contains("of") ? text(s.value("of")) : std::string("main");
	windows.setFullscreen(name, readFullscreen(s, s.step["fullscreen"], s.where + ".fullscreen", true), s.where);
	return true;
}

} // namespace

void registerWindowSteps(StepRegistry& registry) {
	registry.add({"windows",
	              {{"openWindow", {"fullscreen"}},
	               {"closeWindow", {}},
	               {"fullscreen", {"of"}, true},
	               {"openUrl", {}}},
	              runWindowStep});
}

} // namespace affresco::format
