/// @file
/// @brief The filters' kernels: three box blurs for a Gaussian (running sums), the motion blur's line average, and
/// hashed noise.

#include "Kernels.hpp"

#include "affresco/imaging/Pixels.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace affresco::imaging::kernels {

namespace {

/// @brief One box blur along rows (step 4, stride side × 4) or columns (step side × 4, stride 4).
/// @param data The region.
/// @param side Its side.
/// @param radius The box's radius.
/// @param step From one pixel to the next along a line.
/// @param stride From one line to the next.
/// @param line Scratch for one line.
void boxPass(std::vector<float>& data, int side, int radius, std::size_t step, std::size_t stride,
             std::vector<float>& line) {
	const float scale = 1.0f / static_cast<float>(2 * radius + 1);
	line.resize(static_cast<std::size_t>(side) * 4);
	for(std::size_t lineIndex = 0; lineIndex < static_cast<std::size_t>(side); ++lineIndex) {
		float* start = data.data() + lineIndex * stride;
		for(int at = 0; at < side; ++at)
			std::copy_n(start + static_cast<std::size_t>(at) * step, 4, &line[static_cast<std::size_t>(at) * 4]);
		for(int channel = 0; channel < 4; ++channel) {
			const auto sample = [&](int at) {
				return line[static_cast<std::size_t>(std::clamp(at, 0, side - 1)) * 4 + static_cast<std::size_t>(channel)];
			};
			float sum = 0.0f;
			for(int at = -radius; at <= radius; ++at) sum += sample(at);
			for(int at = 0; at < side; ++at) {
				start[static_cast<std::size_t>(at) * step + static_cast<std::size_t>(channel)] = sum * scale;
				sum += sample(at + radius + 1) - sample(at - radius);
			}
		}
	}
}

/// @brief A region's pixel, bilinear, its edges extended.
/// @param region The region.
/// @param side Its side.
/// @param x Across.
/// @param y Down.
/// @param out The four channels, added to.
void addBilinear(const std::vector<float>& region, int side, float x, float y, std::array<float, 4>& out) noexcept {
	x = std::clamp(x, 0.0f, static_cast<float>(side - 1)), y = std::clamp(y, 0.0f, static_cast<float>(side - 1));
	const int left = std::min(static_cast<int>(x), side - 2), top = std::min(static_cast<int>(y), side - 2);
	const float across = x - static_cast<float>(left), down = y - static_cast<float>(top);
	const auto at = [&](int px, int py) { return static_cast<std::size_t>(py * side + px) * 4; };
	for(std::size_t channel = 0; channel < 4; ++channel) {
		const float above = region[at(left, top) + channel] * (1.0f - across) + region[at(left + 1, top) + channel] * across;
		const float below =
		    region[at(left, top + 1) + channel] * (1.0f - across) + region[at(left + 1, top + 1) + channel] * across;
		out[channel] += above * (1.0f - down) + below * down;
	}
}

} // namespace

std::array<int, 3> gaussianBoxes(float sigma) noexcept {
	constexpr int count = 3;
	const float ideal = std::sqrt(12.0f * sigma * sigma / count + 1.0f);
	int lower = static_cast<int>(std::floor(ideal));
	if(lower % 2 == 0) --lower;
	const int upper = lower + 2;
	const float lowerFloat = static_cast<float>(lower);
	const int lowerCount = static_cast<int>(std::round((12.0f * sigma * sigma - count * lowerFloat * lowerFloat -
	                                                    4.0f * count * lowerFloat - 3.0f * count) /
	                                                   (-4.0f * lowerFloat - 4.0f)));
	std::array<int, 3> radii{};
	for(int index = 0; index < count; ++index)
		radii[static_cast<std::size_t>(index)] = std::max(0, ((index < lowerCount ? lower : upper) - 1) / 2);
	return radii;
}

void gaussianBlur(std::vector<float>& region, int side, float sigma, std::vector<float>& scratch) {
	const auto across = std::size_t{4}, down = static_cast<std::size_t>(side) * 4;
	for(const int radius : gaussianBoxes(sigma))
		if(radius > 0) boxPass(region, side, radius, across, down, scratch);
	for(const int radius : gaussianBoxes(sigma))
		if(radius > 0) boxPass(region, side, radius, down, across, scratch);
}

void motionBlur(const std::vector<float>& region, int side, int halo, float angle, float distance,
                std::vector<float>& out) {
	out.assign(tilePixels * 4, 0.0f);
	const float radians = angle * std::numbers::pi_v<float> / 180.0f;
	const float stepX = std::cos(radians), stepY = -std::sin(radians);
	const int samples = std::max(1, static_cast<int>(std::ceil(distance))) + 1;
	const float scale = 1.0f / static_cast<float>(samples);
	for(int y = 0; y < tileSize; ++y)
		for(int x = 0; x < tileSize; ++x) {
			std::array<float, 4> sum{};
			for(int sample = 0; sample < samples; ++sample) {
				const float along = distance * (static_cast<float>(sample) / static_cast<float>(samples - 1) - 0.5f);
				addBilinear(region, side, static_cast<float>(x + halo) + along * stepX,
				            static_cast<float>(y + halo) + along * stepY, sum);
			}
			for(std::size_t channel = 0; channel < 4; ++channel)
				out[static_cast<std::size_t>(y * tileSize + x) * 4 + channel] = sum[channel] * scale;
		}
}

float noise(std::uint32_t seed, int x, int y, int channel) noexcept {
	std::uint32_t hash = seed * 0x9e3779b9u ^ static_cast<std::uint32_t>(x) * 0x85ebca6bu ^
	                     static_cast<std::uint32_t>(y) * 0xc2b2ae35u ^ static_cast<std::uint32_t>(channel) * 0x27d4eb2fu;
	hash ^= hash >> 16, hash *= 0x7feb352du, hash ^= hash >> 15, hash *= 0x846ca68bu, hash ^= hash >> 16;
	return static_cast<float>(hash >> 8) / static_cast<float>(1u << 23) - 1.0f;
}

} // namespace affresco::imaging::kernels
