#pragma once

/// @file
/// @brief What the audio tests share: 16-bit PCM WAV files made in the test (tones, ramps), so that no recorded audio
/// is needed.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

namespace affresco::test {

/// @brief Writes a little-endian number into bytes.
/// @param out The bytes.
/// @param value The number.
/// @param size Its bytes.
inline void appendLittleEndian(std::vector<std::uint8_t>& out, std::uint32_t value, int size) {
	for(int i = 0; i < size; ++i) out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xffU));
}

/// @brief A 16-bit PCM WAV file of samples.
/// @param samples Interleaved samples.
/// @param sampleRate Frames per second.
/// @param channels Samples per frame.
/// @return The file's bytes.
inline std::vector<std::uint8_t> wavFile(const std::vector<std::int16_t>& samples, std::uint32_t sampleRate,
                                         std::uint32_t channels) {
	const auto data = static_cast<std::uint32_t>(samples.size() * 2);
	std::vector<std::uint8_t> out;
	const auto text = [&out](const char* word) { out.insert(out.end(), word, word + 4); };
	text("RIFF");
	appendLittleEndian(out, 36 + data, 4);
	text("WAVE");
	text("fmt ");
	appendLittleEndian(out, 16, 4);
	appendLittleEndian(out, 1, 2); // PCM
	appendLittleEndian(out, channels, 2);
	appendLittleEndian(out, sampleRate, 4);
	appendLittleEndian(out, sampleRate * channels * 2, 4);
	appendLittleEndian(out, channels * 2, 2);
	appendLittleEndian(out, 16, 2);
	text("data");
	appendLittleEndian(out, data, 4);
	for(const std::int16_t sample : samples) appendLittleEndian(out, static_cast<std::uint16_t>(sample), 2);
	return out;
}

/// @brief Mono samples that rise by one step each frame (frame i is i * step), so a test can tell frames apart.
/// @param frames How many.
/// @param step The rise per frame.
/// @return The samples.
inline std::vector<std::int16_t> rampSamples(std::size_t frames, int step = 8) {
	std::vector<std::int16_t> samples(frames);
	for(std::size_t i = 0; i < frames; ++i) samples[i] = static_cast<std::int16_t>((static_cast<int>(i) * step) % 32000);
	return samples;
}

/// @brief Mono samples of a sine tone.
/// @param frames How many.
/// @param sampleRate Frames per second.
/// @param frequency Hertz.
/// @return The samples, at half the full scale.
inline std::vector<std::int16_t> toneSamples(std::size_t frames, std::uint32_t sampleRate, double frequency) {
	std::vector<std::int16_t> samples(frames);
	for(std::size_t i = 0; i < frames; ++i)
		samples[i] = static_cast<std::int16_t>(
		    std::lround(16000 * std::sin(2 * std::numbers::pi * frequency * static_cast<double>(i) / sampleRate)));
	return samples;
}

} // namespace affresco::test
