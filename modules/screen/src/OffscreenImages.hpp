#pragma once

/// @file
/// @brief detail::OffscreenImages: pictures of a screen, drawn offscreen by its window.

#include "affresco/core/ScreenImages.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/format/Runtime.hpp"

namespace affresco::detail {

/// @brief Draws a screen offscreen, the way the rendering check does (Window::saveScreenshot at a fixed scale), and
/// encodes it as a PNG.
///
/// To draw a proposal it previews it, draws, and puts back the preview that was shown before (or none), all within
/// one call on the main thread: no frame is presented in between, so the window never shows it.
/// Main thread only.
class OffscreenImages final : public ScreenImages {
public:
	/// @brief Draws a screen.
	/// @param versions Its document; must outlive this object.
	/// @param runtime Its panels and window; must outlive this object.
	OffscreenImages(VersionControl& versions, const format::Runtime& runtime)
	    : versions_(versions), runtime_(runtime) {}

	[[nodiscard]] std::optional<std::string> render(const ImageRequest& request, Image& image) override;
	[[nodiscard]] bool rendering() const noexcept override { return rendering_; }

private:
	/// @brief Previews a proposal, or ends the preview.
	/// @param proposal The proposal, or std::nullopt for main.
	/// @return An error, or std::nullopt.
	std::optional<std::string> show(std::optional<int> proposal);
	/// @brief Draws what the window shows now.
	/// @param scale Pixels per window unit.
	/// @param image Receives the picture.
	/// @return An error, or std::nullopt.
	std::optional<std::string> draw(int scale, Image& image) const;

	VersionControl& versions_;       ///< The document.
	const format::Runtime& runtime_; ///< Has the window.
	bool rendering_ = false;         ///< See rendering().
};

} // namespace affresco::detail
