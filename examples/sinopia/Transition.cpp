/// @file
/// @brief Slide transitions for the audience view, blended on the CPU.

#include "Transition.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace sinopia {

namespace {

/// @brief Where a pixel starts in a frame's bytes.
/// @param frame The frame.
/// @param x Its column.
/// @param y Its row.
/// @return The offset of its red byte.
[[nodiscard]] std::size_t offset(const Frame& frame, int x, int y) noexcept {
	return (static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)) * 4;
}

/// @brief Copies one pixel.
/// @param into The frame written.
/// @param x Its column there.
/// @param y Its row there.
/// @param from The frame read.
/// @param fromX The column read.
/// @param fromY The row read.
void copyPixel(Frame& into, int x, int y, const Frame& from, int fromX, int fromY) noexcept {
	const std::size_t target = offset(into, x, y);
	const std::size_t source = offset(from, fromX, fromY);
	for(std::size_t c = 0; c < 4; ++c) into.pixels[target + c] = from.pixels[source + c];
}

/// @brief Blends the two frames, each byte weighted by the time.
/// @param from The slide going.
/// @param to The slide coming.
/// @param time From 0 to 1.
/// @return The frame.
[[nodiscard]] Frame fade(const Frame& from, const Frame& to, float time) {
	Frame frame = to;
	for(std::size_t i = 0; i < frame.pixels.size(); ++i) {
		const float value =
		    static_cast<float>(from.pixels[i]) * (1.0f - time) + static_cast<float>(to.pixels[i]) * time;
		frame.pixels[i] = static_cast<std::uint8_t>(std::lround(value));
	}
	return frame;
}

/// @brief The new slide comes in from the right, pushing the old one out to the left (or, for a wipe, uncovers the
/// new one without moving either).
/// @param from The slide going.
/// @param to The slide coming.
/// @param time From 0 to 1.
/// @param moves Whether the slides move (push) or stay (wipe).
/// @return The frame.
[[nodiscard]] Frame slide(const Frame& from, const Frame& to, float time, bool moves) {
	Frame frame = to;
	const int edge = static_cast<int>(std::lround(static_cast<float>(to.width) * time));
	for(int y = 0; y < to.height; ++y)
		for(int x = 0; x < to.width; ++x) {
			const bool showsNew = moves ? x >= to.width - edge : x < edge;
			if(showsNew) copyPixel(frame, x, y, to, moves ? x - (to.width - edge) : x, y);
			else copyPixel(frame, x, y, from, moves ? x + edge : x, y);
		}
	return frame;
}

/// @brief The new slide grows from the centre over the old one.
/// @param from The slide going.
/// @param to The slide coming.
/// @param time From 0 to 1.
/// @return The frame.
[[nodiscard]] Frame zoom(const Frame& from, const Frame& to, float time) {
	Frame frame = from;
	const float scale = std::max(time, 0.01f);
	const float halfWidth = static_cast<float>(to.width) / 2.0f;
	const float halfHeight = static_cast<float>(to.height) / 2.0f;
	for(int y = 0; y < to.height; ++y)
		for(int x = 0; x < to.width; ++x) {
			const float sourceX = halfWidth + (static_cast<float>(x) + 0.5f - halfWidth) / scale;
			const float sourceY = halfHeight + (static_cast<float>(y) + 0.5f - halfHeight) / scale;
			if(sourceX < 0.0f || sourceY < 0.0f || sourceX >= static_cast<float>(to.width) ||
			   sourceY >= static_cast<float>(to.height))
				continue;
			copyPixel(frame, x, y, to, static_cast<int>(sourceX), static_cast<int>(sourceY));
		}
	return frame;
}

} // namespace

Frame blendTransition(const Frame& from, const Frame& to, std::string_view kind, float time) {
	const float t = std::clamp(time, 0.0f, 1.0f);
	const std::size_t bytes =
	    static_cast<std::size_t>(std::max(to.width, 0)) * static_cast<std::size_t>(std::max(to.height, 0)) * 4;
	if(t >= 1.0f || from.width != to.width || from.height != to.height || to.pixels.size() != bytes ||
	   from.pixels.size() != bytes)
		return to;
	if(kind == "fade") return fade(from, to, t);
	if(kind == "push") return slide(from, to, t, true);
	if(kind == "wipe") return slide(from, to, t, false);
	if(kind == "zoom") return zoom(from, to, t);
	return to;
}

} // namespace sinopia
