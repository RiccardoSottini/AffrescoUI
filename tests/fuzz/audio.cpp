/// @file
/// @brief Fuzzes the audio module's decoders on untrusted bytes (miniaudio's WAV, FLAC and MP3): a screen may play any
/// file in an allowed folder or any stream the host's network allows, and its bytes can hold anything.
///
/// The first byte picks how the rest arrives: its lowest bit, as bytes in memory or as an encoded stream pushed in
/// chunks (its next three bits choose their size); the two bits after, the encoding hinted (none, WAV, FLAC, MP3). The
/// engine is pulled by the target (Output::Manual) within small limits so that inputs reach them: 512 KiB, two
/// seconds of sound.

#include "Fuzz.hpp"

#include <affresco/audio/Engine.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

/// The engine's limits: small, so that inputs reach them.
constexpr double maxSeconds = 2;

/// @brief Pulls the engine until the voice stops, checking every sample and the spectrum after each block.
/// @param engine The engine.
/// @param voice The voice.
/// @param streamed Whether its frames come from a decoding thread (which may be late).
void playOut(affresco::audio::Engine& engine, affresco::audio::Voice& voice, bool streamed) {
	const std::uint32_t channels = engine.format().channels;
	std::vector<float> block(512 * channels);
	// Two seconds of frames at most, and a little more for a stream's thread to catch up; never forever
	const auto blocks = static_cast<int>(maxSeconds * engine.format().sampleRate / 512) + 8;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
	for(int i = 0; i < blocks * (streamed ? 4 : 1) && voice.state() == affresco::audio::State::Playing; ++i) {
		if(engine.render(block) != 512) affresco::fuzz::broken("a Manual engine renders every frame asked for");
		for(const float sample : block)
			if(!std::isfinite(sample)) affresco::fuzz::broken("every sample is finite");
		for(const float band : voice.spectrum(16))
			if(!(band >= 0 && band <= 1)) affresco::fuzz::broken("every band of a spectrum is from 0 to 1");
		if(streamed) std::this_thread::sleep_for(std::chrono::microseconds{200});
		if(std::chrono::steady_clock::now() > deadline) break;
	}
	if(voice.position() > maxSeconds + 0.01) affresco::fuzz::broken("a sound plays no longer than the limit");
	if(const auto duration = voice.duration(); duration && *duration > maxSeconds + 0.01)
		affresco::fuzz::broken("a sound longer than the limit is refused");
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace audio = affresco::audio;
	if(size == 0) return 0;
	const std::uint8_t mode = data[0];
	const std::span<const std::uint8_t> bytes(data + 1, size - 1);
	const auto hint = static_cast<audio::Encoding>((mode >> 4) & 3);

	audio::EngineOptions options;
	options.output = audio::Output::Manual;
	options.sampleRate = 44100;
	options.channels = 2;
	options.limits.memoryBytes = std::size_t{512} << 10;
	options.limits.seconds = maxSeconds;
	audio::Engine engine(options);
	std::string error;
	const bool streamed = (mode & 1) != 0;
	std::shared_ptr<audio::Voice> voice;
	if(!streamed) {
		voice = engine.play(audio::Source::memory({bytes.begin(), bytes.end()}, hint), {}, error);
		if(!voice) {
			if(error.empty()) affresco::fuzz::broken("a refused sound says why");
			return 0;
		}
	} else {
		const auto stream = audio::EncodedStream::create(std::size_t{512} << 10, hint);
		voice = engine.play(stream, {}, error);
		if(!voice) affresco::fuzz::broken("an encoded stream always opens");
		const std::size_t chunk = std::size_t{1} << (((mode >> 1) & 7) + 4); // 16 to 2,048 bytes
		const std::size_t pushed = std::min(bytes.size(), std::size_t{256} << 10); // within the capacity
		for(std::size_t at = 0; at < pushed; at += chunk)
			if(!stream->push(bytes.subspan(at, std::min(chunk, pushed - at))))
				affresco::fuzz::broken("a stream within its capacity takes every chunk");
		stream->finish();
	}
	playOut(engine, *voice, streamed);
	return 0;
}
