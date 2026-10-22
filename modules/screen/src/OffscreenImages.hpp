#pragma once

/// @file
/// @brief detail::OffscreenImages: pictures of a screen, drawn offscreen by its window.

#include "affresco/core/ScreenImages.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/format/Runtime.hpp"

#include "Png.hpp"

#include <functional>

namespace affresco::detail {

/// @brief Draws a screen offscreen, the way the rendering check does (Window::saveScreenshot at a fixed scale), and
/// encodes it as a PNG.
///
/// To draw a proposal it previews it, draws, and puts back the preview that was shown before (or none), all within
/// one call on the main thread: no frame is presented in between, so the window never shows it. The tasks main had
/// deferred run first, on main; then, until the screen is put back, the runtime's handlers are suspended
/// (Runtime::setHandlersSuspended), so the proposal's onShow, onHide and watch steps, and main's when it comes back,
/// never run: a picture changes nothing the person sees or what runs next.
/// Main thread only.
class OffscreenImages final : public ScreenImages {
public:
	/// @brief Draws a screen.
	/// @param versions Its document; must outlive this object.
	/// @param runtime Its panels and window; must outlive this object.
	OffscreenImages(VersionControl& versions, format::Runtime& runtime) : versions_(versions), runtime_(runtime) {}

	[[nodiscard]] std::optional<std::string> render(const ImageRequest& request, Image& image) override;
	[[nodiscard]] bool rendering() const noexcept override { return rendering_; }
	[[nodiscard]] std::optional<std::string> evidence(int proposal, int scale, Evidence& evidence) override;

	/// Plays the screen's scenarios on a document (JSON text): `[{"name", "passed", "message"}]`.
	using ScenarioCheck = std::function<json(const std::string& document)>;
	/// @brief Sets how evidence plays the screen's scenarios; without one, evidence has none.
	/// @param check The check, or an empty function.
	void setScenarioCheck(ScenarioCheck check) { scenarioCheck_ = std::move(check); }

private:
	/// @brief Puts back what the person sees, whatever happens while a picture is drawn.
	class PutBack;

	/// @brief Previews a proposal, or ends the preview.
	/// @param proposal The proposal, or std::nullopt for main.
	/// @return An error, or std::nullopt.
	std::optional<std::string> show(std::optional<int> proposal);
	/// @brief Draws what the window shows now.
	/// @param scale Pixels per window unit.
	/// @param image Receives the picture.
	/// @return An error, or std::nullopt.
	std::optional<std::string> draw(int scale, Image& image) const;
	/// @brief Draws what the window shows now, as pixels.
	/// @param scale Pixels per window unit.
	/// @param pixels Receives the picture.
	/// @return An error, or std::nullopt.
	std::optional<std::string> drawPixels(int scale, RgbImage& pixels) const;
	/// @brief Draws main and a proposal, one after the other, and puts back what the person sees.
	/// @param proposal The proposal.
	/// @param scale Pixels per window unit.
	/// @param before Receives main.
	/// @param after Receives the proposal.
	/// @param main Receives main's document.
	/// @param document Receives the proposal's document, as JSON text.
	/// @return An error, or std::nullopt.
	std::optional<std::string> drawBoth(int proposal, int scale, RgbImage& before, RgbImage& after, json& main,
	                                    std::string& document);

	VersionControl& versions_;       ///< The document.
	format::Runtime& runtime_;       ///< Has the window.
	bool rendering_ = false;         ///< See rendering().
	ScenarioCheck scenarioCheck_;    ///< See setScenarioCheck().
};

} // namespace affresco::detail
