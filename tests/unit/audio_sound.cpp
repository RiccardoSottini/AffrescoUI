/// @file
/// @brief Sound descriptions (audio/Sound.hpp): read and bounded, rendered the same every time from any frame, and
/// played as an asset of a screen ("asset:<name>.sound.json") like a file.

#include "Headless.hpp"
#include "NetServer.hpp"

#include <affresco/affresco.hpp>
#include <affresco/audio/Engine.hpp>
#include <affresco/audio/Sound.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

using affresco::json;
namespace audio = affresco::audio;

namespace {

/// A bar of drums and a melody line: what an agent might write.
constexpr const char* groove = R"({ "tempo": 120, "tracks": [
	{ "instrument": "kick", "steps": "x...x...x...x..." },
	{ "instrument": "hat", "steps": "..x...x...x...x.", "volume": 0.5, "pan": 0.4 },
	{ "instrument": "snare", "steps": "....x.......x..." },
	{ "instrument": "triangle", "release": 0.2, "notes": [
		{ "at": 0, "note": "C4", "length": 1 }, { "at": 1, "note": "E4" }, { "at": 2, "note": 67, "velocity": 0.6 } ] } ] })";

/// @brief The error a description gives, or "".
/// @param text The description.
/// @return The error.
std::string soundError(const std::string& text) {
	std::string error;
	return audio::readSound(text, error) ? std::string() : error;
}

/// @brief The largest absolute sample in a range of frames.
/// @param frames Interleaved stereo frames.
/// @param from The first frame.
/// @param to The frame after the last.
/// @return It.
float peak(const std::vector<float>& frames, std::size_t from, std::size_t to) {
	float most = 0;
	for(std::size_t i = from * 2; i < to * 2 && i < frames.size(); ++i) most = std::max(most, std::abs(frames[i]));
	return most;
}

} // namespace

TEST_CASE("audio sound: a description reads, and what is wrong or too much is refused with its place") {
	std::string error;
	const auto score = audio::readSound(groove, error);
	REQUIRE_MESSAGE(score, error);
	CHECK(score->tracks.size() == 4);
	CHECK(score->events.size() == 4 + 4 + 2 + 3);
	CHECK(score->seconds == doctest::Approx(2.0)); // the last kick: beat 3 (1.5 s), and its half second
	const auto g =
	    std::ranges::find_if(score->events, [](const audio::SoundEvent& e) { return e.track == 3 && e.start == 1.0; });
	REQUIRE(g != score->events.end());
	CHECK(g->frequency == doctest::Approx(392.0).epsilon(0.001)); // MIDI 67 is G4

	CHECK(soundError("[]").contains("expected"));
	CHECK(soundError(R"({"tracks": []})").contains("1 to 16 tracks"));
	CHECK(soundError(R"({"tempo": 1000, "tracks": [{"instrument": "sine"}]})").contains("tempo"));
	CHECK(soundError(R"({"tracks": [{"instrument": "violin"}]})").contains("tracks[0].instrument"));
	CHECK(soundError(R"({"tracks": [{"instrument": "sine", "notes": [{"note": "H9"}]}]})")
	          .contains("tracks[0].notes[0].note"));
	CHECK(soundError(R"({"tracks": [{"instrument": "kick", "steps": "x?x"}]})").contains("steps"));
	CHECK(soundError(R"({"tracks": [{"instrument": "sine", "notes": [{"at": 3000, "length": 1}]}]})")
	          .contains("more than 600"));
	json chord = {{"tracks", {{{"instrument", "sine"}, {"notes", json::array()}}}}};
	for(int i = 0; i < 40; ++i) chord["tracks"][0]["notes"].push_back({{"at", 0}, {"note", 40 + i}});
	CHECK(soundError(chord.dump()).contains("more than 32 notes sound at once"));
	json many = {{"tracks", {{{"instrument", "hat"}, {"steps", std::string(5000, 'x')}, {"step", 0.01}}}}};
	CHECK(soundError(many.dump()).contains("more than 4096"));
}

TEST_CASE("audio sound: rendering is the same every time and from any frame, and the drums land on their steps") {
	std::string error;
	const auto score = audio::readSound(groove, error);
	REQUIRE(score);
	const audio::Format format{48000, 2};
	std::vector<float> whole(48000 * 3 * 2);
	const std::size_t frames = audio::renderSound(*score, whole, 0, format);
	CHECK(frames == static_cast<std::size_t>(score->seconds * 48000));
	std::vector<float> again(whole.size());
	CHECK(audio::renderSound(*score, again, 0, format) == frames);
	CHECK(again == whole);

	// From the middle, block by block: the same samples
	std::vector<float> block(1000 * 2);
	for(std::uint64_t first : {std::uint64_t{12000}, std::uint64_t{36500}}) {
		REQUIRE(audio::renderSound(*score, block, first, format) == 1000);
		for(std::size_t i = 0; i < block.size(); ++i)
			REQUIRE(block[i] == whole[static_cast<std::size_t>(first) * 2 + i]);
	}
	CHECK(peak(whole, 0, 2000) > 0.2F);                            // the kick on the first beat
	CHECK(peak(whole, 0, frames) <= 1.0F);                         // clamped
	CHECK(audio::renderSound(*score, block, frames, format) == 0); // its end
}

TEST_CASE("audio sound: an asset of a screen plays as a sound, on the null device and a Manual engine") {
	affresco::test::ThreadExecutor executor;
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(200, 100, clock);
	const json document = {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"state", json::object()},
	    {"audio", {{"beat", {{"tone", {{"frequency", 440}, {"seconds", 1}}}, {"into", "state.beat"}}}}},
	    {"actions", {{"play", json::array({{{"audio", "play"}, {"of", "beat"}}})}}},
	    {"root", {{"type", "label"}, {"id", "title"}, {"text", "Beat"}}}};
	affresco::Screen screen = affresco::Screen::fromString(document.dump());
	audio::EngineOptions options;
	options.output = audio::Output::Manual;
	options.executor = &executor;
	const auto engine = std::make_shared<audio::Engine>(options);
	screen.setAudio(engine);
	screen.setExecutor(&executor);
	screen.build(*window);
	REQUIRE(screen.applyRevision(json{
	    {"author", "app"},
	    {"assets", json::array({{{"name", "groove.sound.json"}, {"type", affresco::soundType}, {"text", groove}}})},
	    {"set", {{"/audio/beat", {{"file", "asset:groove.sound.json"}, {"into", "state.beat"}}}}}}
	                                 .dump()));
	CHECK(screen.assets().at(0).type == affresco::soundType);
	screen.runAction("play", window->root());
	const json state = json::parse(screen.stateJson());
	CHECK(state["beat"]["status"] == "playing");
	CHECK(state["beat"]["duration"] == doctest::Approx(2.0));
	std::vector<float> block(960 * 2);
	static_cast<void>(engine->render(block));
	CHECK(std::ranges::any_of(block, [](float sample) { return sample != 0.0F; }));

	// A wrong description is refused as an asset, before anyone reviews it
	std::string error;
	REQUIRE_FALSE(screen.applyRevision(
	    json{{"author", "app"},
		     {"assets", json::array({{{"name", "bad.sound.json"}, {"type", affresco::soundType}, {"text", "{}"}}})}}
	        .dump(),
	    &error));
	CHECK(error.contains("sound description"));
}
