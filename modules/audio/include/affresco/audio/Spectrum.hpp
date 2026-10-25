#pragma once

/// @file
/// @brief audio::spectrum: how loud each band of frequencies was in the last samples played, for visualisers.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace affresco::audio {

/// The samples a spectrum reads: the last ones played, about 23 ms at 44.1 kHz.
inline constexpr std::size_t spectrumSamples = 1024;
/// The most bands a spectrum has.
inline constexpr std::size_t maxSpectrumBands = 64;

/// @brief How loud each band of frequencies is in a signal: bands spaced evenly in pitch from 40 Hz to the lower of
/// 16 kHz and half the sample rate, through a Hann window and a Fourier transform of the last spectrumSamples.
/// @param samples Mono samples, oldest first: the last spectrumSamples are read (fewer are padded with silence).
/// @param sampleRate Their rate, in Hz (at least 1).
/// @param bands How many bands, 1 to maxSpectrumBands (clamped).
/// @return Each band's loudness: 0 at -60 dB or quieter, 1 at 0 dB (a full-scale sine), in between linear in dB.
[[nodiscard]] std::vector<float> spectrum(std::span<const float> samples, std::uint32_t sampleRate, std::size_t bands);

} // namespace affresco::audio
