#pragma once

/// @file
/// @brief Slide transitions for the audience view: two images of slides blended into one frame, on the CPU.

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace sinopia {

/// @brief An image of a slide: RGBA pixels, top row first.
struct Frame {
	int width = 0;                    ///< In pixels.
	int height = 0;                   ///< In pixels.
	std::vector<std::uint8_t> pixels; ///< width * height * 4 bytes.
};

/// @brief One frame of a transition from one slide to the next.
///
/// "fade" blends them; "push" slides the new one in from the right, pushing the old one out; "wipe" uncovers the new
/// one from left to right; "zoom" grows the new one from the centre over the old one. Any other kind (and a time of 1
/// or more) shows the new one.
/// @param from The slide going (the same size as @p to; another size shows @p to).
/// @param to The slide coming.
/// @param kind "fade", "push", "wipe" or "zoom".
/// @param time How far along it is, from 0 to 1 (clamped).
/// @return The frame, the size of @p to.
[[nodiscard]] Frame blendTransition(const Frame& from, const Frame& to, std::string_view kind, float time);

} // namespace sinopia
