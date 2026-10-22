#pragma once

/// @file
/// @brief The ScreenImages interface: pictures of the screen, for peers that want to see their work.

#include "affresco/core/Json.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

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

/// @brief What a proposal does, gathered by the host itself (VERSION_CONTROL.md, "Evidence"): an author can't supply
/// any of it.
struct Evidence {
	Image before;                       ///< Main's head.
	Image after;                        ///< With the proposal applied.
	std::array<int, 4> region = {};     ///< The pixels that differ, `{x, y, width, height}`; all 0 when none does.
	std::vector<std::string> behaviour; ///< What it changes in behaviour (ProposalInfo::behaviour).
	json scenarios = json::array();     ///< The screen's scenarios played on it: `[{"name", "passed", "message"}]`.
	json texts = json::array();         ///< Each text it changes: `[{"node", "original", "modified", "language"}]`.
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

	/// @brief Gathers the evidence of a proposal: draws main and the proposal, finds what differs, and plays the
	/// screen's scenarios on the proposal.
	/// @param proposal The open proposal.
	/// @param scale Pixels per window unit in the pictures.
	/// @param evidence Receives it.
	/// @return An error, or std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> evidence(int proposal, int scale, Evidence& evidence) {
		(void)proposal, (void)scale, (void)evidence;
		return "AffrescoUI screen: this host gathers no evidence";
	}
};

} // namespace affresco
