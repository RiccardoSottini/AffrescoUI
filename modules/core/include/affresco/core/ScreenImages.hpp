#pragma once

/// @file
/// @brief The ScreenImages interface: pictures of the screen, for peers that want to see their work.

#include <optional>
#include <string>

namespace affresco {

/// @brief What a picture should show.
struct ImageRequest {
	std::optional<int> proposal; ///< This open proposal applied; main's head without one.
	int scale = 1;               ///< Pixels per window unit (2 looks like a Retina display).
};

/// @brief A picture of the screen.
struct Image {
	std::string png; ///< The PNG file's bytes.
	int width = 0;   ///< In pixels.
	int height = 0;  ///< In pixels.
};

/// @brief Draws the screen offscreen and encodes it, without changing what the person sees.
///
/// Implemented where the modules are put together (affresco::Screen), which has the window; used by the live
/// protocol (channel::Session, "render"), which can't depend on the rendering engine. Main thread only.
class ScreenImages {
public:
	/// The largest scale a peer may ask for: a picture's size is bounded by the window's.
	static constexpr int maxScale = 2;

	virtual ~ScreenImages() = default;

	/// @brief Draws the screen, as it is on main or with a proposal applied.
	/// @param request What to show.
	/// @param image Receives the picture.
	/// @return An error naming what went wrong (no window, no such proposal, ...), or std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> render(const ImageRequest& request, Image& image) = 0;

	/// @brief Whether a picture is being drawn right now. To draw a proposal the document may switch to it and back:
	/// review changes in that time are the picture's own, not news for peers.
	[[nodiscard]] virtual bool rendering() const noexcept = 0;
};

} // namespace affresco
