#pragma once

/// @file
/// @brief detail::takePicture: draws what a window shows, and compares it with a golden image or saves it.

#include "affresco/automation/Scenarios.hpp"

#include <filesystem>
#include <string>

namespace affresco {
class Window;
}

namespace affresco::automation::detail {

/// @brief What a picture step asks for.
enum class PictureKind {
	Expect, ///< `expect image`: compare with the golden image.
	Capture ///< `capture`: save it, compare nothing.
};

/// @brief Draws what a window shows offscreen, as the rendering check always has (Window::saveScreenshot at a fixed
/// scale), then compares it with its golden image or saves it, as the options say.
/// @param window The window; it must be able to draw (not headless).
/// @param scale Pixels per window unit.
/// @param name The picture's name: `<scenario>-<name in the step>`.
/// @param kind Expect or Capture.
/// @param options Where golden images are and where pictures go.
/// @param scratch A folder of this run's own, for the drawn frame.
/// @return What became of it; its problem says why when it Differs.
[[nodiscard]] Picture takePicture(Window& window, int scale, const std::string& name, PictureKind kind,
                                  const Options& options, const std::filesystem::path& scratch);

} // namespace affresco::automation::detail
