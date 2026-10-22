#pragma once

/// @file
/// @brief gl::StencilClips: clips that are not axis-aligned (a parent's bounds under a turned or skewed child), drawn
/// into the stencil buffer between a frame's batches.

#include <glad/glad.h>

#include <cstddef>
#include <cstdint>

namespace affresco::gl {

/// @brief What a batch does to the stencil clips instead of drawing colour.
enum class ClipStep : std::uint8_t {
	none, ///< It draws colour, inside the clips pushed so far.
	push, ///< Its quad narrows the clip: only what is inside it and the clips so far is drawn from now on.
	pop,  ///< Its quad, the one pushed last, widens the clip back.
};

/// @brief How many stencil clips nest; a child turned deeper than that is clipped to its bounding box instead.
constexpr std::size_t maxClipDepth = 8;

/// @brief The stencil clips of one frame as its batches are drawn: the level is how many are pushed, and colour is
/// drawn only where the stencil holds it.
///
/// It leaves the stencil test off whenever no clip is pushed, so frames without clips draw as they always did.
class StencilClips {
public:
	/// @brief Starts a frame's clips: none pushed, and the stencil buffer cleared to 0 (within the scissor).
	void start() noexcept;
	/// @brief Gets ready to draw a clip's quad: colour off, the stencil counting up (push) or down (pop) where it
	/// holds the current level.
	/// @param step push or pop.
	void beginStep(ClipStep step) noexcept;
	/// @brief Draws colour again, within the clips now pushed.
	/// @param step The step just drawn.
	void endStep(ClipStep step) noexcept;
	/// @brief Turns the stencil test off and forgets the clips (after a frame).
	void finish() noexcept;

private:
	GLint level_ = 0; ///< The clips pushed.
};

} // namespace affresco::gl
