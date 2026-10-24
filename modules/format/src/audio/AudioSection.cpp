/// @file
/// @brief The "audio" section: the sounds a screen plays, each from one source it names (a file, a URL, a source the
/// host gives, or a tone), with how it starts. A capability: revisions can never change it.

#include "AudioPlayers.hpp"
#include "SectionRegistry.hpp"
#include "NodeKeys.hpp"

#include <array>
#include <format>
#include <string_view>

namespace affresco::format {

namespace {

/// The keys that name a sound's source: exactly one of them.
constexpr std::array<std::string_view, 4> sourceKeys = {"file", "url", "source", "tone"};

/// @brief Fails unless a value is a number in a range.
/// @param value The value.
/// @param at Its place.
/// @param low The least.
/// @param high The most.
void checkRange(const json& value, const std::string& at, double low, double high) {
	if(!value.is_number() || !(value.get<double>() >= low && value.get<double>() <= high))
		expect::fail(at, std::format("expected a number from {} to {}", low, high));
}

/// @brief A tone: its frequency, length and amplitude.
/// @param tone The value.
/// @param at Its place.
void checkTone(const json& tone, const std::string& at) {
	for(const auto& [key, item] : expect::object(tone, at).items()) {
		const std::string where = concatenate(at, ".", key);
		if(key == "frequency") checkRange(item, where, 1, 20000);
		else if(key == "seconds") checkRange(item, where, 0.001, 86400);
		else if(key == "amplitude") checkRange(item, where, 0, 1);
		else expect::fail(where, "unknown property (expected frequency, seconds or amplitude)");
	}
	if(!tone.contains("frequency") || !tone.contains("seconds"))
		expect::fail(at, "a tone needs a \"frequency\" and \"seconds\"");
}

/// @brief One key of a sound.
/// @param key The key.
/// @param item Its value.
/// @param at Its place.
void checkKey(const std::string& key, const json& item, const std::string& at) {
	if(key == "file" || key == "source" || key == "url") {
		const std::string text = expect::string(item, at);
		if(text.empty() || text.contains("{{")) expect::fail(at, "expected a literal name: no {{ }}");
		if(key == "url" && !text.starts_with("https://") && !text.starts_with("http://"))
			expect::fail(at, "expected an https:// URL (http:// only to the loopback)");
	} else if(key == "tone") checkTone(item, at);
	else if(key == "into") {
		if(!expect::string(item, at).starts_with("state.") || item.get<std::string>().contains("{{"))
			expect::fail(at, "expected a literal state path, e.g. \"state.song\"");
	} else if(key == "volume") checkRange(item, at, 0, 4);
	else if(key == "pan") checkRange(item, at, -1, 1);
	else if(key == "rate") checkRange(item, at, 0.25, 4);
	else if(key == "loop") static_cast<void>(expect::boolean(item, at));
	else if(key == "updates") checkRange(item, at, 50, 5000);
	else if(key == "maxBytes")
		checkRange(item, at, 1024, static_cast<double>(AudioPlayers::maxStreamBytes));
	else if(key != "note")
		expect::fail(at, "unknown property (expected file, url, source, tone, into, volume, pan, rate, loop, updates, "
		                 "maxBytes or note)");
}

/// @brief "audio": at most 32 sounds, each with one source.
/// @param value The value.
/// @param name "audio".
void checkAudio(const json& value, const std::string& name) {
	if(expect::object(value, name).size() > AudioPlayers::maxSounds)
		expect::fail(name, std::format("at most {} sounds", AudioPlayers::maxSounds));
	for(const auto& [sound, definition] : value.items()) {
		const std::string at = std::format("{}.{}", name, sound);
		if(sound.empty() || sound.size() > 64 || sound.contains('.') || sound.contains(':'))
			expect::fail(at, R"(a sound's name has 1 to 64 characters and no "." or ":")");
		std::size_t sources = 0;
		for(const auto& [key, item] : expect::object(definition, at).items()) {
			checkKey(key, item, std::format("{}.{}", at, key));
			sources += std::ranges::find(sourceKeys, key) != sourceKeys.end() ? 1 : 0;
		}
		if(sources != 1) expect::fail(at, "a sound needs one of \"file\", \"url\", \"source\" or \"tone\"");
		if(definition.contains("maxBytes") && !definition.contains("url"))
			expect::fail(at + ".maxBytes", "only a \"url\" has a buffer");
	}
}

} // namespace

SectionType audioSection() { return {.name = "audio", .validate = checkAudio}; } // Rules::checkChange: checkAudioChange()

} // namespace affresco::format
