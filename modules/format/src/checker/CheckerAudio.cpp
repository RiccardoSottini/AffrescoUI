/// @file
/// @brief The checker's look at the sounds a screen plays: the state paths the "audio" section publishes to, and the
/// "audio" step's operation and the sound it names.

#include "AudioPlayers.hpp"
#include "CheckerImpl.hpp"

#include <algorithm>
#include <format>
#include <string>

namespace affresco::format::checker {

namespace {

/// @brief A value written as it is, not computed.
/// @param value The value.
/// @return Whether it is text without "{{".
[[nodiscard]] bool literalText(const json& value) { return value.is_string() && !isDynamic(value); }

} // namespace

// Each sound's state is published to its "into" path: the screen reads it there
void Checker::walkAudio() {
	const json* sounds = section("audio");
	if(sounds == nullptr) return;
	for(const auto& [name, sound] : sounds->items())
		if(sound.is_object() && sound.contains("into") && literalText(sound["into"]))
			write(sound["into"], anything, Change::set, Scope(), std::format("audio.{}.into", name));
}

void Checker::walkAudioStep(const json& step, const std::string& place) {
	const json& name = step["audio"];
	if(literalText(name) && !std::ranges::contains(audioOperations(), name.get<std::string>())) {
		report(place + ".audio", std::format("unknown audio operation '{}'", name.get<std::string>()),
		       hint([&] { return hintFor(name.get<std::string>(), audioOperations(), "", "expected:"); }));
		return;
	}
	if(name == "master") return; // the whole mix: no sound
	const json* sounds = section("audio");
	if(sounds == nullptr || !sounds->is_object() || sounds->empty()) {
		report(place, "an \"audio\" step needs an \"audio\" section");
		return;
	}
	if(!step.contains("of") && sounds->size() != 1)
		report(place, "\"of\" names the sound when a screen declares more than one");
	if(step.contains("of") && literalText(step["of"]))
		refer("sound", "audio", step["of"].get<std::string>(), "", place + ".of");
}

} // namespace affresco::format::checker
