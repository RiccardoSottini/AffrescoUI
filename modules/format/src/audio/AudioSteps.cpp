/// @file
/// @brief The "audio" step: one operation on a sound the "audio" section declares (AudioPlayers.hpp). What can't play
/// (no engine, a file that doesn't open) shows in the sound's state, runs "onError" with `error` if the step has it,
/// and is logged otherwise: the action goes on, as a channel's does without its host.

#include "AudioPlayers.hpp"
#include "Engine.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <array>

namespace affresco::format {

namespace {

/// The keys an operation's values come from.
constexpr std::array<const char*, 3> valueKeys = {"to", "from", "ms"};

/// @brief The name of the sound a step names: its "of", or the only one the screen declares.
/// @param s The step.
/// @param sounds The "audio" section.
/// @return The name.
/// @throws DocumentError If it names none, or one not declared.
[[nodiscard]] std::string soundNameOf(const Running& s, const json& sounds) {
	if(!s.step.contains("of")) {
		if(sounds.size() != 1) fail(s.where, "\"of\" names the sound when a screen declares more than one");
		return sounds.begin().key();
	}
	std::string name = string(s.value("of"), s.where + ".of");
	if(!sounds.contains(name)) fail(s.where + ".of", "no sound \"" + name + "\" in the \"audio\" section");
	return name;
}

/// @brief Runs an "audio" step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runAudioStep(const Running& s) {
	const json& step = s.step;
	if(!step.contains("audio")) return std::nullopt;
	const std::string& where = s.where;
	Engine& engine = s.steps.engine();
	const std::string operation = string(step["audio"], where + ".audio");
	if(!std::ranges::contains(audioOperations(), operation))
		fail(where + ".audio", "unknown audio operation \"" + operation +
		                           "\" (expected play, pause, toggle, stop, seek, volume, pan, rate, loop or master)");
	std::string name;
	if(operation != "master") {
		const json& document = engine.versions.document();
		const auto sounds = document.find("audio");
		if(sounds == document.end() || !sounds->is_object() || sounds->empty())
			fail(where, "an \"audio\" step needs an \"audio\" section");
		name = soundNameOf(s, *sounds);
	}
	json values = json::object();
	for(const char* key : valueKeys)
		if(step.contains(key)) values[key] = s.value(key);
	const std::optional<std::string> problem = engine.audio.run(name, operation, values, where);
	if(!problem) return true;
	if(step.contains("onError")) {
		const Scope scope{&s.locals, nullptr, {{"error", *problem}}, {}, {}};
		return s.run(step["onError"], scope, where + ".onError");
	}
	log(LogLevel::Warning, "AffrescoUI screen: " + where + ": " + *problem);
	return true;
}

} // namespace

const std::vector<std::string>& audioOperations() {
	static const std::vector<std::string> operations = {"play", "pause", "toggle", "stop", "seek",
	                                                    "volume", "pan", "rate", "loop", "master"};
	return operations;
}

void registerAudioSteps(StepRegistry& registry) {
	registry.add({"audio", {{"audio", {"of", "to", "from", "ms", "onError"}}}, runAudioStep});
}

} // namespace affresco::format
