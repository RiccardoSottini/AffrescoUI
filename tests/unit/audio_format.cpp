/// @file
/// @brief The screen format's audio: the "audio" section (a capability) and the "audio" step, on a Manual engine the
/// test pulls: the sounds' state, their actions, files only from allowed folders, URLs only through the host's
/// network, the host's own sources, and what revisions may and may not change.

#include "AudioWav.hpp"
#include "Headless.hpp"
#include "NetServer.hpp"

#include <affresco/affresco.hpp>
#include <affresco/audio/Engine.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::test::httpResponse;
using affresco::test::LocalServer;
using affresco::test::readRequest;
using affresco::test::ThreadExecutor;
namespace audio = affresco::audio;

namespace {

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).at(json::json_pointer(pointer));
}

/// @brief A screen with sounds and the actions that play them.
/// @param sounds The "audio" section.
/// @return The document.
json audioScreen(const json& sounds) {
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"audio", sounds},
	        {"state", {{"after", false}, {"ended", 0}, {"failed", nullptr}}},
	        {"actions",
	         {{"play", json::array({{{"audio", "play"}, {"of", "song"}}, {{"set", "state.after"}, {"to", true}}})},
	          {"playCaught", json::array({{{"audio", "play"},
	                                       {"of", "song"},
	                                       {"onError", json::array({{{"set", "state.failed"}, {"to", "{{error}}"}}})}}})},
	          {"pause", json::array({{{"audio", "pause"}, {"of", "song"}}})},
	          {"toggle", json::array({{{"audio", "toggle"}, {"of", "song"}}})},
	          {"stop", json::array({{{"audio", "stop"}, {"of", "song"}}})},
	          {"seek", json::array({{{"audio", "seek"}, {"of", "song"}, {"to", 0.05}}})},
	          {"quiet", json::array({{{"audio", "volume"}, {"of", "song"}, {"to", 0.25}},
	                                 {{"audio", "pan"}, {"of", "song"}, {"to", -0.5}},
	                                 {{"audio", "rate"}, {"of", "song"}, {"to", 2}},
	                                 {{"audio", "loop"}, {"of", "song"}, {"to", true}},
	                                 {{"audio", "master"}, {"to", 0.5}}})},
	          {"song:ended", json::array({{{"increment", "state.ended"}}})}}},
	        {"root", {{"type", "label"}, {"id", "title"}, {"text", "{{state.song.status}}"}}}};
}

/// @brief A Manual engine whose events go to an executor.
/// @param executor The executor.
/// @return The engine.
std::shared_ptr<audio::Engine> manualEngine(affresco::Executor& executor) {
	audio::EngineOptions options;
	options.output = audio::Output::Manual;
	options.channels = 2;
	options.executor = &executor;
	return std::make_shared<audio::Engine>(options);
}

/// @brief Pulls the engine and runs its events until a condition holds.
/// @param engine The engine.
/// @param executor Its executor.
/// @param done The condition.
/// @return Whether it held within 2,000 blocks.
bool playUntil(audio::Engine& engine, ThreadExecutor& executor, const std::function<bool()>& done) {
	std::vector<float> block(960 * 2);
	for(int i = 0; i < 2000; ++i) {
		if(done()) return true;
		static_cast<void>(engine.render(block));
		executor.runFor(std::chrono::milliseconds{1});
	}
	return done();
}

/// @brief A temporary folder with a WAV file of a tenth of a second.
struct SoundFolder {
	std::filesystem::path folder; ///< The folder.
	/// @brief Makes it.
	/// @param name The folder's name.
	explicit SoundFolder(const std::string& name)
	    : folder(std::filesystem::temp_directory_path() / ("affrescoui-" + name)) {
		std::filesystem::remove_all(folder);
		std::filesystem::create_directories(folder / "sounds");
		write(folder / "sounds" / "beep.wav");
	}
	SoundFolder(const SoundFolder&) = delete;
	SoundFolder& operator=(const SoundFolder&) = delete;
	SoundFolder(SoundFolder&&) = delete;
	SoundFolder& operator=(SoundFolder&&) = delete;
	/// @brief Removes it.
	~SoundFolder() { std::filesystem::remove_all(folder); }
	/// @brief Writes a WAV file.
	/// @param file Where.
	static void write(const std::filesystem::path& file) {
		const std::vector<std::uint8_t> bytes = affresco::test::wavFile(affresco::test::toneSamples(4800, 48000, 440),
		                                                                48000, 1);
		std::ofstream(file, std::ios::binary)
		    .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
};

} // namespace

TEST_CASE("audio format: without an engine a sound says so, and the action goes on") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(
	    audioScreen({{"song", {{"tone", {{"frequency", 440}, {"seconds", 1}}}, {"into", "state.song"}}}}).dump());
	screen.build(*window);
	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/status") == "error");
	CHECK(stateAt(screen, "/song/error") == "this host plays no audio");
	CHECK(stateAt(screen, "/after") == true);
	screen.runAction("playCaught", window->root());
	CHECK(stateAt(screen, "/failed") == "this host plays no audio");
}

TEST_CASE("audio format: a tone plays, its state follows, and its end runs the sound's action") {
	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(
	    audioScreen({{"song", {{"tone", {{"frequency", 440}, {"seconds", 0.1}}}, {"into", "state.song"}}}}).dump());
	const auto engine = manualEngine(executor);
	screen.setAudio(engine);
	screen.setExecutor(&executor);
	screen.build(*window);
	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/status") == "playing");
	CHECK(stateAt(screen, "/song/duration") == 0.1);
	CHECK(stateAt(screen, "/song/progress") == 0.0);
	REQUIRE(playUntil(*engine, executor, [&] { return stateAt(screen, "/ended") == 1; }));
	CHECK(stateAt(screen, "/song/status") == "stopped");
	CHECK(stateAt(screen, "/song/position") == 0.1);
	CHECK(stateAt(screen, "/song/progress") == 1.0);

	screen.runAction("toggle", window->root()); // plays again from the start
	CHECK(stateAt(screen, "/song/status") == "playing");
	screen.runAction("toggle", window->root());
	CHECK(stateAt(screen, "/song/status") == "paused");
	screen.runAction("seek", window->root());
	CHECK(stateAt(screen, "/song/position") == 0.05);
	screen.runAction("quiet", window->root());
	CHECK(stateAt(screen, "/song/volume") == 0.25);
	CHECK(stateAt(screen, "/song/pan") == -0.5);
	CHECK(stateAt(screen, "/song/rate") == 2.0);
	CHECK(stateAt(screen, "/song/loop") == true);
	CHECK(engine->masterVolume() == doctest::Approx(0.5));
	screen.runAction("stop", window->root());
	CHECK(stateAt(screen, "/song/status") == "stopped");
	CHECK(stateAt(screen, "/song/position") == 0.0);
}

TEST_CASE("audio format: a sound with bands has a spectrum in its state while it plays, and zeros after") {
	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen =
	    affresco::Screen::fromString(audioScreen({{"song",
		                                           {{"tone", {{"frequency", 1000}, {"seconds", 0.5}}},
		                                            {"into", "state.song"},
		                                            {"bands", 8},
		                                            {"updates", 50}}}})
		                                 .dump());
	const auto engine = manualEngine(executor);
	screen.setAudio(engine);
	screen.setExecutor(&executor);
	screen.build(*window);
	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/spectrum") == json::array({0, 0, 0, 0, 0, 0, 0, 0}));
	const auto loudest = [&] {
		const json bands = stateAt(screen, "/song/spectrum");
		return bands.is_array() && bands.size() == 8 && bands[4].get<double>() > 0.5; // 1 kHz is in the fifth of 8
	};
	REQUIRE(playUntil(*engine, executor, loudest));
	CHECK(stateAt(screen, "/song/spectrum")[0].get<double>() < 0.3);
	REQUIRE(playUntil(*engine, executor, [&] { return stateAt(screen, "/ended") == 1; }));
	CHECK(stateAt(screen, "/song/spectrum") == json::array({0, 0, 0, 0, 0, 0, 0, 0}));

	const affresco::format::Rules rules;
	const auto check = [&rules](const json& sounds) { rules.validate(audioScreen(sounds)); };
	CHECK_THROWS(check({{"song", {{"file", "a.wav"}, {"bands", 65}}}}));
	CHECK_THROWS(check({{"song", {{"file", "a.wav"}, {"bands", 0}}}}));
	CHECK_THROWS_WITH(check({{"song", {{"file", "a.wav"}, {"bands", 2.5}}}}), doctest::Contains("whole number"));
}

TEST_CASE("audio format: a file plays only from the screen's folder or one the host allows") {
	const SoundFolder screenFolder("audio-screen");
	const SoundFolder other("audio-other");
	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	const std::string outside = (other.folder / "sounds" / "beep.wav").string();
	json document = audioScreen({{"song", {{"file", "sounds/beep.wav"}, {"into", "state.song"}}},
	                             {"away", {{"file", outside}, {"into", "state.away"}}},
	                             {"up", {{"file", "../affrescoui-audio-other/sounds/beep.wav"}, {"into", "state.up"}}}});
	document["actions"]["away"] = json::array({{{"audio", "play"}, {"of", "away"}}});
	document["actions"]["up"] = json::array({{{"audio", "play"}, {"of", "up"}}});
	affresco::Screen screen = affresco::Screen::fromString(document.dump(), screenFolder.folder);
	const auto engine = manualEngine(executor);
	screen.setAudio(engine);
	screen.setExecutor(&executor);
	screen.build(*window);

	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/status") == "playing");
	CHECK(stateAt(screen, "/song/duration") == 0.1);
	REQUIRE(playUntil(*engine, executor, [&] { return stateAt(screen, "/ended") == 1; }));

	screen.runAction("away", window->root());
	CHECK(stateAt(screen, "/away/status") == "error");
	CHECK(stateAt(screen, "/away/error").get<std::string>().contains("outside the folders"));
	CHECK_FALSE(stateAt(screen, "/away/error").get<std::string>().contains(other.folder.string()));
	screen.runAction("up", window->root());
	CHECK(stateAt(screen, "/up/status") == "error"); // ".." out of the folder is outside too

	screen.allowAudioDirectory(other.folder);
	screen.runAction("away", window->root());
	CHECK(stateAt(screen, "/away/status") == "playing");
}

TEST_CASE("audio format: a source the host names plays a stream it feeds") {
	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen =
	    affresco::Screen::fromString(audioScreen({{"song", {{"source", "speech"}, {"into", "state.song"}}}}).dump());
	const auto engine = manualEngine(executor);
	screen.setAudio(engine);
	screen.setExecutor(&executor);
	screen.build(*window);
	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/error") == "the host gives no source named 'speech'");

	const auto stream = audio::PcmStream::create({16000, 1}, 16000);
	screen.setAudioSource("speech", stream);
	CHECK(stream->push(std::vector<std::int16_t>(1600, 8000)) == 1600);
	stream->finish();
	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/status") == "playing");
	CHECK(stateAt(screen, "/song/duration").is_null()); // a live stream has none
	REQUIRE(playUntil(*engine, executor, [&] { return stateAt(screen, "/ended") == 1; }));
}

TEST_CASE("audio format: a URL streams only through the host's network and its allowlist") {
	const std::vector<std::uint8_t> wav =
	    affresco::test::wavFile(affresco::test::toneSamples(4800, 48000, 440), 48000, 1);
	LocalServer server([&wav](affresco::net::tcp::Connection& connection) {
		static_cast<void>(readRequest(connection));
		const std::string body(wav.begin(), wav.end());
		CHECK(connection.send(httpResponse("200 OK", body, "Content-Type: audio/wav\r\n"), std::chrono::seconds{5}));
	});
	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(
	    audioScreen({{"song", {{"url", server.origin() + "/beep.wav"}, {"into", "state.song"}}}}).dump());
	const auto engine = manualEngine(executor);
	screen.setAudio(engine);
	screen.setExecutor(&executor);
	screen.build(*window);
	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/error") == "the host gives this screen no network");

	static_cast<void>(screen.network()); // a network that allows no origin yet
	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/error").get<std::string>().contains("isn't a host this app may connect to"));

	CHECK_FALSE(screen.network().allow(server.origin()));
	screen.runAction("play", window->root());
	CHECK(stateAt(screen, "/song/status") == "playing");
	REQUIRE(playUntil(*engine, executor, [&] { return stateAt(screen, "/ended") == 1; }));
	CHECK(stateAt(screen, "/song/error").is_null());
}

TEST_CASE("audio format: the section is checked, and revisions change it only for the person, and may play what it declares") {
	const affresco::format::Rules rules;
	const auto check = [&rules](const json& sounds) { rules.validate(audioScreen(sounds)); };
	CHECK_NOTHROW(check({{"song", {{"file", "a.mp3"}, {"loop", true}, {"volume", 0.5}}}}));
	CHECK_THROWS_WITH(check({{"song", {{"file", "a.mp3"}, {"tone", {{"frequency", 440}, {"seconds", 1}}}}}}),
	                  doctest::Contains("one of"));
	CHECK_THROWS_WITH(check({{"song", {{"into", "state.song"}}}}), doctest::Contains("one of"));
	CHECK_THROWS_WITH(check({{"song", {{"url", "file:///etc/passwd"}}}}), doctest::Contains("https://"));
	CHECK_THROWS_WITH(check({{"song", {{"file", "{{state.path}}"}}}}), doctest::Contains("literal"));
	CHECK_THROWS_WITH(check({{"song", {{"file", "a.wav"}, {"into", "{{state.x}}"}}}}), doctest::Contains("literal"));
	CHECK_THROWS_WITH(check({{"song", {{"file", "a.wav"}, {"volume", 9}}}}), doctest::Contains("from 0 to 4"));
	CHECK_THROWS_WITH(check({{"song", {{"file", "a.wav"}, {"maxBytes", 4096}}}}), doctest::Contains("only a"));
	CHECK_THROWS_WITH(check({{"a.b", {{"file", "a.wav"}}}}), doctest::Contains("no \".\""));
	json many = json::object();
	for(int i = 0; i < 33; ++i) many["s" + std::to_string(i)] = {{"file", "a.wav"}};
	CHECK_THROWS_WITH(check(many), doctest::Contains("at most 32"));

	affresco::vcs::Repository repository(
	    rules, audioScreen({{"song", {{"file", "sounds/beep.wav"}, {"into", "state.song"}}}}));
	const auto submit = [&repository](const json& revision) {
		json full = revision;
		full["author"] = "agent";
		return repository.submit(full).status;
	};
	CHECK(submit({{"set", {{"/audio/song/file", "/Users/someone/private.wav"}}}}) == RevisionStatus::Rejected);
	CHECK(submit({{"add", {{"/audio/spy", {{"url", "https://example.com/a.mp3"}}}}}}) == RevisionStatus::Rejected);
	CHECK(submit({{"remove", json::array({"/audio/song"})}}) == RevisionStatus::Rejected); // its actions play it
	CHECK(submit({{"set", {{"/audio/song/file", "../../private.wav"}}}}) == RevisionStatus::Rejected);
	// Giving a sound a file of the screen's folder waits for the person (VERSION_CONTROL.md, "Assets")
	CHECK(submit({{"set", {{"/audio/song/file", "sounds/other.wav"}}}}) == RevisionStatus::Proposed);
	const affresco::RevisionResult played = repository.submit(
	    {{"author", "agent"}, {"add", {{"/actions/again", json::array({{{"audio", "play"}, {"of", "song"}}})}}}});
	CHECK_MESSAGE(played.status == RevisionStatus::Applied, played.error);
}
