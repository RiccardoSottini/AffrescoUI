/// @file
/// @brief The window steps: openWindow, closeWindow and fullscreen, on the windows of the "windows" section
/// (SecondWindows) and the screen's own.

#include "Engine.hpp"

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

/// @brief Runs an openWindow, closeWindow or fullscreen step.
/// @param s The step.
/// @return std::nullopt if it is none of them, else true.
std::optional<bool> runWindowStep(const Running& s) {
	SecondWindows& windows = s.steps.engine().otherWindows;
	if(s.step.contains("openWindow")) {
		const std::string name = text(s.value("openWindow"));
		std::optional<bool> fullscreen;
		if(s.step.contains("fullscreen"))
			fullscreen = readFullscreen(s, s.step["fullscreen"], s.where + ".fullscreen", false);
		windows.open(name, fullscreen, s.where);
		return true;
	}
	if(s.step.contains("closeWindow")) {
		windows.close(text(s.value("closeWindow")));
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
	              {{"openWindow", {"fullscreen"}}, {"closeWindow", {}}, {"fullscreen", {"of"}, true}},
	              runWindowStep});
}

} // namespace affresco::format
