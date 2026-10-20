#pragma once

/// @file
/// @brief detail::GeometryCache: brings the retained geometry of a frame's panels up to date before it is drawn, and
/// says what that changed on screen (the damage) and which shaders the frame needs.

#include "Renderer.hpp"
#include "RetainedGeometry.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace affresco {

class Panel;

namespace detail {

/// @brief What refreshing a frame's geometry found.
struct Refreshed {
	std::optional<Rect> damage; ///< What changed on screen, in window units, besides what the window reported.
	bool shapes = false;        ///< A shown panel draws shapes: the frame needs the shader with shapes.
	bool color = false;         ///< A shown panel uses colour pages: the frame needs the shader with colour glyphs.
};

/// @brief Keeps each panel's recorded drawing up to date, frame after frame.
///
/// It visits only the panels that changed (their paint dirty bits) and those a layout pass laid out since the last
/// refresh (their pass stamp): a panel the pass skipped, and its subtree, can't have moved. A visited panel is recorded
/// again when it changed or when what its drawing depends on (its frame, bounds, clip, scroll) did; its old and new
/// areas (bounds and what it drew) are damage. A panel no longer shown is damage where it was. Which shaders the frame
/// needs is gathered from the whole tree, so a frame drawn only where it changed uses the shaders a full frame would.
///
/// One per renderer; main thread only. Panels own their geometry (Panel::geometry_), so a panel that is gone takes
/// its geometry with it (the window reports where it was).
class GeometryCache {
public:
	/// @brief Brings a frame's geometry up to date.
	/// @param frame The frame, laid out.
	/// @return The damage it found, and the shaders the frame needs.
	Refreshed refresh(const Frame& frame);

private:
	/// @brief Refreshes a panel, then the children that need it, and gathers the subtree's shaders.
	/// @param panel The panel.
	/// @param outerClip What clips it: its parent's bounds, or the window for roots and overlays.
	void refreshTree(Panel& panel, const Rect& outerClip);
	/// @brief Records a panel again if it changed, or what its drawing depends on did.
	/// @param panel The panel.
	/// @param outerClip What clips it.
	/// @param geometry Its geometry.
	void refreshPanel(Panel& panel, const Rect& outerClip, PanelGeometry& geometry);
	/// @brief Whether a panel needs a visit: it or a descendant changed, or a layout pass laid it out since the last
	/// refresh.
	/// @param panel The panel.
	/// @return true if so.
	[[nodiscard]] bool needsVisit(const Panel& panel) const noexcept;
	/// @brief Adds the old areas of the overlays and the tooltip when the set drawn above the tree changed (one closed,
	/// another opened, their order).
	/// @param frame The frame.
	void compareOverlays(const Frame& frame);

	std::uint64_t lastPass_ = 0;        ///< The layout pass at the last refresh: panels laid out after it are visited.
	std::uint64_t generation_ = 0;      ///< The paint generation at the last refresh.
	bool all_ = true;                   ///< This refresh visits and records everything (a new generation).
	std::optional<Rect> damage_;        ///< This refresh's damage.
	std::uint64_t frameGeneration_ = 0; ///< This refresh's generation, for the panels' keys.
	/// The overlays and tooltip drawn last time, in order, with the area they took (never dereferenced).
	std::vector<std::pair<const Panel*, Rect>> above_;
};

/// @brief What a panel can reach on screen: its bounds (its children are clipped to them) and what it drew (its shadow
/// may fall outside its bounds).
/// @param bounds Its bounds.
/// @param record What it drew.
/// @return The area.
[[nodiscard]] Rect reachOf(const Rect& bounds, const PaintRecord& record) noexcept;

} // namespace detail
} // namespace affresco
