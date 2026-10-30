/// @file
/// @brief Tempo (examples/tempo) on a Manual audio engine: a song that names a sound of its own plays it instead of
/// the beat, and the next song without one goes back to the beat. The song's sound is a tone here, standing in for
/// the `asset:` file a reviewed proposal links (the asset store is the library's, tested with it). Songs are records
/// of the data layer (the `playlists` model), so a song's sound is a field the proposal's run-once `update` writes.

#include "AudioWav.hpp"
#include "Headless.hpp"
#include "NetServer.hpp"

#include <affresco/affresco.hpp>
#include <affresco/audio/Engine.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::test::ThreadExecutor;
namespace audio = affresco::audio;

namespace {

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value, or null.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).value(json::json_pointer(pointer), json());
}

} // namespace

TEST_CASE("tempo: a song with a sound of its own plays it, a song without one plays the beat") {
	const std::filesystem::path folder = std::filesystem::path(AFFRESCOUI_EXAMPLES_ASSETS) / "tempo" / "json";
	json document = json::parse(std::ifstream(folder / "index.screen.json"));
	document["audio"]["own"] = {{"tone", {{"frequency", 330}, {"seconds", 30}}}, {"into", "state.sounds.own"}};
	document["models"]["playlists"]["seed"][0]["tracks"][2]["sound"] = "own"; // the song Tempo opens on

	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(1280, 800, clock);
	affresco::Screen screen = affresco::Screen::fromString(document.dump(), folder);
	audio::EngineOptions options;
	options.output = audio::Output::Manual;
	options.channels = 2;
	options.executor = &executor;
	screen.setAudio(std::make_shared<audio::Engine>(options));
	screen.setExecutor(&executor);
	screen.build(*window);
	window->render();
	const auto run = [&](const char* action) { // an action, then the update that runs its watches
		screen.runAction(action, window->root());
		window->render();
	};

	run("playPause"); // Tempo opens playing, silently: this starts the song's sound
	CHECK(stateAt(screen, "/soundOn") == "own");
	CHECK(stateAt(screen, "/sounds/own/status") == "playing");
	CHECK(stateAt(screen, "/beat/status") == "stopped");
	CHECK(stateAt(screen, "/isPlaying") == true);

	run("next"); // the queue's first song has no sound: the beat
	CHECK(stateAt(screen, "/playingTrack") == 3);
	CHECK(stateAt(screen, "/soundOn") == "beat");
	CHECK(stateAt(screen, "/beat/status") == "playing");
	CHECK(stateAt(screen, "/sounds/own/status") == "stopped");

	run("playPause");
	CHECK(stateAt(screen, "/beat/status") == "paused");
	run("previous"); // back to the song with its sound, which plays it
	CHECK(stateAt(screen, "/playingTrack") == 2);
	CHECK(stateAt(screen, "/isPlaying") == true);
	CHECK(stateAt(screen, "/soundOn") == "own");
	CHECK(stateAt(screen, "/sounds/own/status") == "playing");
	CHECK(stateAt(screen, "/beat/status") == "stopped");
}

TEST_CASE("tempo: a file the person attaches becomes a song's sound in one proposal the person accepts") {
	const std::filesystem::path folder = std::filesystem::path(AFFRESCOUI_EXAMPLES_ASSETS) / "tempo" / "json";
	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(1280, 800, clock);
	std::ifstream file(folder / "index.screen.json");
	affresco::Screen screen =
	    affresco::Screen::fromString(std::string(std::istreambuf_iterator<char>(file), {}), folder);
	audio::EngineOptions options;
	options.output = audio::Output::Manual;
	options.channels = 2;
	options.executor = &executor;
	screen.setAudio(std::make_shared<audio::Engine>(options));
	screen.setExecutor(&executor);
	screen.build(*window);
	window->render();

	const std::vector<std::uint8_t> wav =
	    affresco::test::wavFile(affresco::test::toneSamples(22050, 22050, 330), 22050, 1); // a second of a tone
	const affresco::Attached attached =
	    screen.versions().attach("My Song.wav", "audio/wav", std::string(wav.begin(), wav.end()), json::object());
	REQUIRE(attached.error.empty());
	const json revision = {{"author", "agent"},
	                       {"message", "Play the attached file for this song"},
	                       {"assets", {{{"name", "my-song.wav"}, {"attachment", attached.attachment.id}}}},
	                       {"set", {{"/audio/own", {{"file", "asset:my-song.wav"}, {"into", "state.sounds.own"}}}}},
	                       // The song is a record: the view's revision can't write it, a run-once step does on accept
	                       {"run",
	                        {{{"update", "playlists"},
	                          {"id", "p1"},
	                          {"fields",
	                           {{"tracks", "{{map(data.playlists[0].tracks, index == 2 ? merge(it, {sound: 'own'}) : it)}}"}}}}}}};
	const affresco::RevisionResult result = screen.submitRevision(revision.dump());
	REQUIRE_MESSAGE(result.status == affresco::RevisionStatus::Proposed, result.error);
	window->render();
	screen.runAction("playPause", window->root()); // nothing changes until the person accepts: the beat plays
	window->render();
	CHECK(stateAt(screen, "/soundOn") == "beat");
	screen.runAction("playPause", window->root());
	window->render();
	std::string error;
	REQUIRE_MESSAGE(screen.acceptProposal(result.proposal, result.commit, &error), error);
	window->render();

	screen.runAction("playPause", window->root());
	window->render();
	CHECK(stateAt(screen, "/soundOn") == "own");
	CHECK(stateAt(screen, "/sounds/own/status") == "playing");
	CHECK(stateAt(screen, "/sounds/own/duration") == doctest::Approx(1.0));
}
