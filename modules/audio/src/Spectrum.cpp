/// @file
/// @brief audio::spectrum: a Hann window, a radix-2 Fourier transform and bands spaced evenly in pitch.

#include "affresco/audio/Spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace affresco::audio {

namespace {

/// The lowest frequency of the first band, in Hz.
constexpr double lowestHz = 40;
/// The highest frequency of the last band, in Hz (lower when half the sample rate is).
constexpr double highestHz = 16000;
/// How quiet a band is at 0, in dB.
constexpr double floorDecibels = -60;

/// @brief Transforms values in place, from samples to frequencies (radix 2, iterative).
/// @param values A power of two of them.
void transform(std::vector<std::complex<double>>& values) {
	const std::size_t count = values.size();
	for(std::size_t i = 1, j = 0; i < count; ++i) { // in bit-reversed order
		std::size_t bit = count >> 1;
		for(; (j & bit) != 0; bit >>= 1) j ^= bit;
		j ^= bit;
		if(i < j) std::swap(values[i], values[j]);
	}
	for(std::size_t length = 2; length <= count; length <<= 1) {
		const std::complex<double> turn = std::polar(1.0, -2 * std::numbers::pi / static_cast<double>(length));
		const std::size_t half = length / 2;
		for(std::size_t start = 0; start < count; start += length) {
			std::complex<double> twiddle = 1;
			for(std::size_t k = 0; k < half; ++k) {
				const std::complex<double> even = values[start + k];
				const std::complex<double> odd = values[start + k + half] * twiddle;
				values[start + k] = even + odd;
				values[start + k + half] = even - odd;
				twiddle *= turn;
			}
		}
	}
}

/// @brief The magnitude of each frequency bin of the last samples, through a Hann window.
/// @param samples The samples, oldest first.
/// @return spectrumSamples / 2 + 1 magnitudes, from 0 Hz to half the sample rate.
std::vector<double> magnitudes(std::span<const float> samples) {
	std::vector<std::complex<double>> values(spectrumSamples);
	const std::size_t count = std::min(samples.size(), spectrumSamples);
	const std::size_t padding = spectrumSamples - count; // silence before fewer samples
	const std::span<const float> last = samples.last(count);
	for(std::size_t i = 0; i < count; ++i) {
		const double sample = std::isfinite(last[i]) ? last[i] : 0.0;
		const double phase = 2 * std::numbers::pi * static_cast<double>(padding + i) / spectrumSamples;
		values[padding + i] = sample * (0.5 - (0.5 * std::cos(phase)));
	}
	transform(values);
	std::vector<double> result(spectrumSamples / 2 + 1);
	for(std::size_t i = 0; i < result.size(); ++i) result[i] = std::abs(values[i]);
	return result;
}

/// @brief A bin's magnitude as a band's loudness.
/// @param magnitude The magnitude.
/// @return 0 at floorDecibels or quieter, 1 at a full-scale sine, linear in dB between.
float loudness(double magnitude) {
	const double fullScale = spectrumSamples / 4.0; // a full-scale sine's bin through the Hann window
	if(!(magnitude > 0)) return 0;
	const double decibels = 20 * std::log10(magnitude / fullScale);
	return static_cast<float>(std::clamp((decibels - floorDecibels) / -floorDecibels, 0.0, 1.0));
}

} // namespace

std::vector<float> spectrum(std::span<const float> samples, std::uint32_t sampleRate, std::size_t bands) {
	bands = std::clamp<std::size_t>(bands, 1, maxSpectrumBands);
	std::vector<float> result(bands, 0.0F);
	const double rate = std::max<std::uint32_t>(sampleRate, 1);
	const double high = std::min(highestHz, rate / 2);
	if(high <= lowestHz) return result;
	const std::vector<double> bins = magnitudes(samples);
	const double binHz = rate / spectrumSamples;
	const auto binOf = [&](double hertz) {
		return std::min(static_cast<std::size_t>(std::lround(hertz / binHz)), bins.size() - 1);
	};
	const auto count = static_cast<double>(bands);
	for(std::size_t band = 0; band < bands; ++band) {
		const double from = lowestHz * std::pow(high / lowestHz, static_cast<double>(band) / count);
		const double to = lowestHz * std::pow(high / lowestHz, static_cast<double>(band + 1) / count);
		const std::size_t first = binOf(from);
		const std::size_t last = std::max(first, binOf(to));
		const std::span<const double> within = std::span(bins).subspan(first, last - first + 1);
		result[band] = loudness(*std::ranges::max_element(within));
	}
	return result;
}

} // namespace affresco::audio
