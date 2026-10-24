/// @file
/// @brief The audio module (modules/audio) on engines the test pulls (Output::Manual) and on the null device: decoding
/// WAV made here, the streams and their bounds, the voices' transport and gain, the events, and the limits.

#include "AudioWav.hpp"
#include "Helpers.hpp"

#include <affresco/audio/Engine.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

using namespace affresco;
using namespace affresco::audio;

namespace {

/// @brief An engine the test pulls: 48 kHz, the channels given.
/// @param executor Where events go, or none.
/// @param channels Samples per frame.
/// @param limits Its limits.
/// @return The options.
EngineOptions manual(Executor* executor = nullptr, std::uint32_t channels = 1, Limits limits = {}) {
	EngineOptions options;
	options.output = Output::Manual;
	options.sampleRate = 48000;
	options.channels = channels;
	options.executor = executor;
	options.limits = limits;
	return options;
}

/// @brief Pulls frames from a manual engine.
/// @param engine The engine.
/// @param frames How many.
/// @return The samples, interleaved.
std::vector<float> pull(Engine& engine, std::size_t frames) {
	std::vector<float> out(frames * engine.format().channels);
	CHECK(engine.render(out) == frames);
	return out;
}

/// @brief A generator whose frame i is i / 1000 on every channel, for checking which frames play.
/// @param seconds How long, if it ends.
/// @return The source.
std::shared_ptr<Source> indexSource(std::optional<double> seconds = {}) {
	return Source::generator(
	    [](std::span<float> out, std::uint64_t first, const Format& format) {
		    const std::size_t frames = out.size() / format.channels;
		    for(std::size_t i = 0; i < frames; ++i)
			    for(std::uint32_t c = 0; c < format.channels; ++c)
				    out[(i * format.channels) + c] = static_cast<float>(first + i) / 1000.0F;
		    return frames;
	    },
	    seconds, true);
}

/// @brief A generator of a constant on every channel.
/// @param value The sample.
/// @return The source.
std::shared_ptr<Source> constantSource(float value) {
	return Source::generator([value](std::span<float> out, std::uint64_t, const Format&) {
		std::ranges::fill(out, value);
		return out.size();
	});
}

/// @brief The events a voice got, in order.
struct Recorder {
	std::vector<Event> events; ///< What came.
	/// @brief A handler that keeps them.
	/// @return It.
	EventHandler handler() {
		return [this](const Event& event) { events.push_back(event); };
	}
	/// @brief How many of a type came.
	/// @param type The type.
	/// @return The count.
	[[nodiscard]] std::size_t count(EventType type) const {
		return static_cast<std::size_t>(std::ranges::count(events, type, &Event::type));
	}
};

} // namespace

TEST_CASE("audio: a WAV made in memory decodes to its samples, mono to every channel of the mix") {
	const std::vector<std::int16_t> samples = test::rampSamples(3000);
	Engine engine(manual(nullptr, 2));
	std::string error;
	const auto voice = engine.play(Source::memory(test::wavFile(samples, 48000, 1), Encoding::Wav), {}, error);
	REQUIRE_MESSAGE(voice, error);
	CHECK(voice->duration().value() == doctest::Approx(3000.0 / 48000));
	CHECK(voice->seekable());
	const std::vector<float> out = pull(engine, 3000);
	for(std::size_t i = 0; i < 3000; i += 97) {
		INFO("frame ", i);
		CHECK(out[i * 2] == doctest::Approx(samples[i] / 32768.0).epsilon(1e-4));
		CHECK(out[(i * 2) + 1] == doctest::Approx(samples[i] / 32768.0).epsilon(1e-4));
	}
	static_cast<void>(pull(engine, 16));
	CHECK(voice->state() == State::Stopped);
	CHECK(voice->position() == doctest::Approx(voice->duration().value()));
}

TEST_CASE("audio: a WAV file at another rate is resampled, and only a file that exists and decodes opens") {
	const std::filesystem::path folder = std::filesystem::temp_directory_path() / "affrescoui-audio-test";
	std::filesystem::create_directories(folder);
	const std::filesystem::path file = folder / "tone.wav";
	{
		const std::vector<std::uint8_t> bytes = test::wavFile(test::toneSamples(24000, 24000, 440), 24000, 1);
		std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),
		                                             static_cast<std::streamsize>(bytes.size()));
	}
	Engine engine(manual());
	std::string error;
	const auto voice = engine.play(Source::file(file), {}, error);
	REQUIRE_MESSAGE(voice, error);
	CHECK(voice->duration().value() == doctest::Approx(1.0).epsilon(0.001));
	const std::vector<float> out = pull(engine, 48000);
	float peak = 0;
	for(const float sample : out) peak = std::max(peak, std::fabs(sample));
	CHECK(peak == doctest::Approx(16000.0 / 32768).epsilon(0.02));

	CHECK_FALSE(engine.play(Source::file(folder / "missing.wav"), {}, error));
	CHECK(error.contains("missing.wav"));
	CHECK_FALSE(error.contains(folder.string())); // the error names the file, not where the machine keeps it
	const std::filesystem::path text = folder / "notes.wav";
	std::ofstream(text) << "not a sound";
	CHECK_FALSE(engine.play(Source::file(text), {}, error));
	CHECK(error.contains("couldn't be decoded"));
	std::filesystem::remove_all(folder);
}

TEST_CASE("audio: a tone has its frequency, its length and ramps at its ends") {
	Engine engine(manual());
	std::string error;
	const auto voice = engine.play(Source::tone(1000, 0.1, 0.5F), {}, error);
	REQUIRE(voice);
	CHECK(voice->duration().value() == doctest::Approx(0.1));
	const std::vector<float> out = pull(engine, 4800);
	CHECK(std::fabs(out[0]) < 0.01F); // the ramp in
	float peak = 0;
	for(const float sample : out) peak = std::max(peak, std::fabs(sample));
	CHECK(peak == doctest::Approx(0.5).epsilon(0.01));
	int crossings = 0; // 1 kHz over 0.1 s crosses zero upwards about 100 times
	for(std::size_t i = 1; i < out.size(); ++i) crossings += out[i - 1] < 0 && out[i] >= 0 ? 1 : 0;
	CHECK(crossings >= 98);
	CHECK(crossings <= 101);
	static_cast<void>(pull(engine, 10));
	CHECK(voice->state() == State::Stopped);
	CHECK_FALSE(Source::tone(0, 1));
	CHECK_FALSE(Source::tone(440, -1));
	CHECK_FALSE(Source::tone(440, 1, 2));
}

TEST_CASE("audio: seek, loop, pause, stop and play again follow the source's frames") {
	Engine engine(manual());
	std::string error;
	VoiceOptions options;
	options.loop = true;
	const auto voice = engine.play(indexSource(0.002), options, error); // 96 frames
	REQUIRE(voice);
	std::vector<float> out = pull(engine, 200);
	CHECK(out[0] == doctest::Approx(0.0));
	CHECK(out[95] == doctest::Approx(0.095));
	CHECK(out[96] == doctest::Approx(0.0)); // looped back
	CHECK(out[199] == doctest::Approx((199 - 192) / 1000.0));
	CHECK(voice->state() == State::Playing);

	CHECK(voice->seek(0.001)); // frame 48
	out = pull(engine, 2);
	CHECK(out[0] == doctest::Approx(0.048));
	voice->pause();
	out = pull(engine, 10);
	CHECK(out[0] == 0);
	CHECK(voice->position() == doctest::Approx(50.0 / 48000));
	CHECK(voice->play());
	CHECK(pull(engine, 1)[0] == doctest::Approx(0.050));
	voice->stop();
	CHECK(voice->state() == State::Stopped);
	CHECK(voice->position() == 0);
	CHECK(voice->play());
	CHECK(pull(engine, 1)[0] == doctest::Approx(0.0));
	CHECK(voice->seek(1e9)); // clamped to the end
	CHECK(voice->position() == doctest::Approx(96.0 / 48000));
}

TEST_CASE("audio: volume, balance, the master volume and a fade are the expected gains") {
	Engine engine(manual(nullptr, 2));
	std::string error;
	VoiceOptions options;
	options.volume = 0.5F;
	options.pan = 1;
	const auto voice = engine.play(constantSource(1), options, error);
	REQUIRE(voice);
	std::vector<float> out = pull(engine, 4);
	CHECK(out[0] == doctest::Approx(0.0)); // all the way right
	CHECK(out[1] == doctest::Approx(0.5));
	voice->setPan(-0.5F);
	out = pull(engine, 1);
	CHECK(out[0] == doctest::Approx(0.5));
	CHECK(out[1] == doctest::Approx(0.25));
	voice->setPan(0);
	engine.setMasterVolume(0.5F);
	CHECK(pull(engine, 1)[0] == doctest::Approx(0.25));
	engine.setMasterVolume(1);
	static_cast<void>(pull(engine, 1)); // the meter is of the last block
	CHECK(engine.levels().peak[0] == doctest::Approx(0.5));

	voice->fade(1, 100.0 / 48000); // 0.5 to 1 in 100 frames
	out = pull(engine, 100);
	CHECK(out[0] == doctest::Approx(0.5));
	CHECK(out[100] == doctest::Approx(0.75).epsilon(0.02));
	CHECK(pull(engine, 1)[0] == doctest::Approx(1.0));
	voice->fade(0, 10.0 / 48000, true); // a fade out stops it, and leaves the volume for the next play
	static_cast<void>(pull(engine, 20));
	CHECK(voice->state() == State::Stopped);
	CHECK(voice->volume() == doctest::Approx(1.0));
	voice->setVolume(9); // clamped
	CHECK(voice->volume() == doctest::Approx(4.0));
}

TEST_CASE("audio: the playback rate steps through the source, interpolating between its frames") {
	Engine engine(manual());
	std::string error;
	VoiceOptions options;
	options.rate = 2;
	const auto voice = engine.play(indexSource(), options, error);
	REQUIRE(voice);
	std::vector<float> out = pull(engine, 4);
	CHECK(out[1] == doctest::Approx(0.002));
	CHECK(out[3] == doctest::Approx(0.006));
	voice->setRate(0.5F);
	out = pull(engine, 4);
	CHECK(out[1] - out[0] == doctest::Approx(0.0005)); // half a frame each
	voice->setRate(100); // clamped to 4
	CHECK(voice->rate() == doctest::Approx(4.0));
}

TEST_CASE("audio: a PCM stream plays what is pushed, converts it, runs dry with an underrun, and is bounded") {
	test::QueueExecutor executor;
	Engine engine(manual(&executor, 2));
	const auto stream = PcmStream::create({24000, 1}, 1000);
	REQUIRE(stream);
	CHECK_FALSE(PcmStream::create({24000, 0}, 10));
	CHECK_FALSE(PcmStream::create({24000, 1}, 0));
	std::vector<float> pushed(1500, 0.25F);
	CHECK(stream->push(pushed) == 1000); // the ring holds 1000
	CHECK(stream->buffered() == 1000);
	Recorder recorder;
	VoiceOptions options;
	options.onEvent = recorder.handler();
	std::string error;
	const auto voice = engine.play(stream, options, error);
	REQUIRE_MESSAGE(voice, error);
	CHECK_FALSE(engine.play(stream, {}, error)); // one voice only
	CHECK(error.contains("one voice"));
	std::vector<float> out = pull(engine, 1000); // 24 kHz to 48 kHz: 500 frames of the stream
	CHECK(out[401] == doctest::Approx(0.25).epsilon(0.01)); // both channels
	out = pull(engine, 2000); // dry
	CHECK(voice->underruns() == 1);
	CHECK(voice->state() == State::Playing);
	const std::vector<std::int16_t> more(200, 16384);
	CHECK(stream->push(more) == 200);
	stream->finish();
	CHECK(stream->push(more) == 0);
	static_cast<void>(pull(engine, 2000));
	executor.runPosted();
	CHECK(voice->state() == State::Stopped);
	CHECK(recorder.count(EventType::Started) == 1);
	CHECK(recorder.count(EventType::Underrun) == 1);
	CHECK(recorder.count(EventType::Ended) == 1);
	CHECK_FALSE(voice->play()); // a stream that ended can't go back
	CHECK_FALSE(voice->duration());
}

TEST_CASE("audio: an encoded stream pushed in chunks decodes on its own thread, and garbage fails") {
	test::QueueExecutor executor;
	Engine engine(manual(&executor));
	const std::vector<std::int16_t> samples = test::rampSamples(4800);
	const std::vector<std::uint8_t> bytes = test::wavFile(samples, 48000, 1);
	const auto stream = EncodedStream::create(1 << 16, Encoding::Wav);
	REQUIRE(stream);
	CHECK_FALSE(EncodedStream::create(10));
	std::string error;
	Recorder recorder;
	VoiceOptions options;
	options.onEvent = recorder.handler();
	const auto voice = engine.play(stream, options, error);
	REQUIRE(voice);
	CHECK_FALSE(voice->seekable());
	std::vector<float> heard;
	for(std::size_t at = 0; at < bytes.size(); at += 1000) {
		const std::size_t size = std::min<std::size_t>(1000, bytes.size() - at);
		CHECK(stream->push(std::span<const std::uint8_t>(bytes.data() + at, size)));
	}
	stream->finish();
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{5};
	while(voice->state() == State::Playing && std::chrono::steady_clock::now() < until) {
		const std::vector<float> out = pull(engine, 480);
		heard.insert(heard.end(), out.begin(), out.end());
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	executor.runPosted();
	CHECK(recorder.count(EventType::Ended) == 1);
	const auto first = std::ranges::find_if(heard, [](float sample) { return sample != 0; });
	REQUIRE(first != heard.end()); // silence while it starts, then the samples in order
	const auto offset = static_cast<std::size_t>(first - heard.begin()) - 1; // frame 0 is silence itself
	for(std::size_t i = 1; i < 4800; i += 211) {
		INFO("frame ", i);
		CHECK(heard[offset + i] == doctest::Approx(samples[i] / 32768.0).epsilon(1e-4));
	}

	const auto garbage = EncodedStream::create(4096);
	CHECK(garbage->push(std::string(2000, 'x')));
	CHECK_FALSE(garbage->push(std::string(3000, 'y'))); // past its capacity
	garbage->finish();
	Recorder failed;
	options.onEvent = failed.handler();
	const auto bad = engine.play(garbage, options, error);
	REQUIRE(bad);
	for(int i = 0; i < 2000 && !bad->error(); ++i) {
		static_cast<void>(pull(engine, 64));
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	executor.runPosted();
	CHECK(bad->error().value_or("").contains("couldn't be decoded"));
	CHECK(failed.count(EventType::Error) == 1);
}

TEST_CASE("audio: the limits refuse too many voices, too many bytes, too long a sound") {
	Limits limits;
	limits.voices = 2;
	limits.memoryBytes = 1000;
	limits.seconds = 0.01;
	Engine engine(manual(nullptr, 1, limits));
	std::string error;
	CHECK_FALSE(engine.play(Source::memory(test::wavFile(test::rampSamples(1000), 48000, 1)), {}, error));
	CHECK(error.contains("limit"));
	limits.memoryBytes = 1 << 20;
	Engine longer(manual(nullptr, 1, limits));
	CHECK_FALSE(longer.play(Source::memory(test::wavFile(test::rampSamples(4800), 48000, 1)), {}, error));
	CHECK(error.contains("longer than the limit"));
	const auto endless = longer.play(constantSource(0.1F), {}, error); // a generator ends at the limit
	REQUIRE(endless);
	static_cast<void>(pull(longer, 1000));
	CHECK(endless->state() == State::Stopped);

	auto first = engine.createVoice(constantSource(0), {}, error);
	auto second = engine.createVoice(constantSource(0), {}, error);
	REQUIRE(second);
	CHECK_FALSE(engine.createVoice(constantSource(0), {}, error));
	CHECK(error.contains("too many voices"));
	first.reset(); // a stopped voice without a handle is forgotten at the next block
	static_cast<void>(pull(engine, 1));
	CHECK(engine.voiceCount() == 1);
	CHECK(engine.createVoice(constantSource(0), {}, error));
}

TEST_CASE("audio: a voice plays on when its handle goes, and events stop once the engine is gone") {
	test::QueueExecutor executor;
	auto engine = std::make_unique<Engine>(manual(&executor));
	Recorder recorder;
	VoiceOptions options;
	options.onEvent = recorder.handler();
	options.positionInterval = std::chrono::milliseconds{20};
	std::string error;
	static_cast<void>(engine->play(Source::tone(440, 0.05), options, error)); // fire and forget
	static_cast<void>(pull(*engine, 100));
	CHECK(engine->voiceCount() == 1);
	std::this_thread::sleep_for(std::chrono::milliseconds{25});
	static_cast<void>(pull(*engine, 100)); // a Position, at most every 20 ms
	static_cast<void>(pull(*engine, 100));
	executor.runPosted();
	CHECK(recorder.count(EventType::Position) == 1);
	static_cast<void>(pull(*engine, 4800));
	CHECK(engine->voiceCount() == 0);
	const auto kept = engine->play(Source::tone(440, 1), options, error);
	static_cast<void>(pull(*engine, 10));
	const std::size_t before = recorder.events.size();
	engine.reset(); // its events posted already do nothing; the handle is inert
	executor.runPosted();
	CHECK(recorder.events.size() == before);
	CHECK_FALSE(kept->play());
	CHECK(kept->state() == State::Stopped);
}

TEST_CASE("audio: the null device plays in real time, without hardware") {
	test::QueueExecutor executor;
	EngineOptions options;
	options.output = Output::Null;
	options.executor = &executor;
	Engine engine(options);
	CHECK(engine.output() == Output::Null);
	CHECK(engine.deviceName() == "null");
	std::vector<float> buffer(64);
	CHECK(engine.render(buffer) == 0); // only a Manual engine is pulled
	Recorder recorder;
	VoiceOptions voiceOptions;
	voiceOptions.onEvent = recorder.handler();
	std::string error;
	const auto voice = engine.play(Source::tone(440, 0.1), voiceOptions, error);
	REQUIRE(voice);
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{5};
	while(recorder.count(EventType::Ended) == 0 && std::chrono::steady_clock::now() < until) {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		executor.runPosted();
	}
	CHECK(recorder.count(EventType::Started) == 1);
	CHECK(recorder.count(EventType::Ended) == 1);
	CHECK(voice->position() == doctest::Approx(0.1));
}

TEST_CASE("audio: encodings are guessed from names") {
	CHECK(encodingOf("song.MP3") == Encoding::Mp3);
	CHECK(encodingOf("https://example.com/live.flac?token=1") == Encoding::Flac);
	CHECK(encodingOf("a.wav") == Encoding::Wav);
	CHECK(encodingOf("a.ogg") == Encoding::Unknown);
	CHECK(encodingOf("noextension") == Encoding::Unknown);
}
