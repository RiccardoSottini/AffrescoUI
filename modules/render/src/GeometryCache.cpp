/// @file
/// @brief detail::GeometryCache: the panels that changed or moved recorded again, the damage they make, and the
/// shaders the frame needs.

#include "GeometryCache.hpp"
#include "PaintRecorder.hpp"

#include "affresco/render/Panel.hpp"

#include <algorithm>
#include <memory>

namespace affresco::detail {

Rect reachOf(const Rect& bounds, const PaintRecord& record) noexcept {
	return record.extent ? unite(bounds, *record.extent) : bounds;
}

Refreshed GeometryCache::refresh(const Frame& frame) {
	all_ = frame.paintGeneration != generation_;
	frameGeneration_ = frame.paintGeneration;
	damage_.reset();

	const Rect window{0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)};
	if(needsVisit(frame.root)) refreshTree(frame.root, window);
	for(Panel* overlay : frame.overlays)
		if(needsVisit(*overlay)) refreshTree(*overlay, window);
	if(frame.tooltip && needsVisit(*frame.tooltip)) refreshTree(*frame.tooltip, window);
	compareOverlays(frame);

	// The shaders a full frame would use: what the tree, the overlays and the tooltip draw
	Refreshed result{damage_};
	const auto gather = [&result](const Panel& panel) {
		if(const PanelGeometry* geometry = panel.geometry_.get()) {
			result.shapes = result.shapes || geometry->subtreeShapes;
			result.color = result.color || geometry->subtreeColor;
		}
	};
	gather(frame.root);
	for(const Panel* overlay : frame.overlays) gather(*overlay);
	if(frame.tooltip) gather(*frame.tooltip);

	lastPass_ = frame.layoutPass;
	generation_ = frame.paintGeneration;
	return result;
}

bool GeometryCache::needsVisit(const Panel& panel) const noexcept {
	return all_ || panel.paintStale_ || panel.paintBelow_ || panel.layoutPass_ > lastPass_ || !panel.geometry_;
}

void GeometryCache::refreshTree(Panel& panel, const Rect& outerClip) {
	if(!panel.geometry_) panel.geometry_ = std::make_unique<PanelGeometry>();
	PanelGeometry& geometry = *panel.geometry_;
	if(all_ || panel.paintStale_ || panel.layoutPass_ > lastPass_ || !geometry.recorded)
		refreshPanel(panel, outerClip, geometry);

	// A hidden panel draws no child: it was damage where it reached, which holds its children
	bool shapes = geometry.recorded && geometry.record.shapes, color = geometry.recorded && geometry.record.color;
	if(panel.shown_) {
		for(const auto& child : panel.children_) {
			if(child->overlay_) continue; // refreshed from the frame's overlays, unclipped
			if(needsVisit(*child)) refreshTree(*child, panel.bounds_);
			if(const PanelGeometry* below = child->geometry_.get()) {
				shapes = shapes || below->subtreeShapes;
				color = color || below->subtreeColor;
			}
		}
	}
	geometry.subtreeShapes = panel.shown_ && shapes;
	geometry.subtreeColor = panel.shown_ && color;
	panel.paintStale_ = panel.paintBelow_ = false;
}

void GeometryCache::refreshPanel(Panel& panel, const Rect& outerClip, PanelGeometry& geometry) {
	const PaintKey key{panel.frame_,      panel.bounds_,     outerClip,       panel.scroll_,
	                   panel.contentMin_, panel.contentMax_, frameGeneration_};
	if(panel.shown_ && geometry.recorded && !panel.paintStale_ && geometry.key == key) return; // nothing changed

	if(geometry.recorded) addTo(damage_, reachOf(geometry.key.bounds, geometry.record));
	geometry.record.clear();
	geometry.recorded = false;
	if(!panel.shown_) return;

	PaintRecorder recorder(geometry.record, panel.bounds_, outerClip);
	panel.paint(recorder);
	geometry.record.overStart = geometry.record.ops.size();
	panel.paintOver(recorder);
	geometry.key = key;
	geometry.recorded = true;
	addTo(damage_, reachOf(panel.bounds_, geometry.record));
}

// Compared by address only: a panel that is gone reported where it was itself, and a new one is recorded and damaged
void GeometryCache::compareOverlays(const Frame& frame) {
	std::vector<const Panel*> now(frame.overlays.begin(), frame.overlays.end());
	if(frame.tooltip) now.push_back(frame.tooltip);
	const bool same = std::ranges::equal(now, above_, {}, {}, &std::pair<const Panel*, Rect>::first);
	if(!same)
		for(const auto& [panel, area] : above_) addTo(damage_, area);

	above_.clear();
	for(const Panel* panel : now) {
		const PanelGeometry* geometry = panel->geometry_.get();
		const Rect area = geometry && geometry->recorded ? reachOf(panel->bounds_, geometry->record) : Rect{};
		above_.emplace_back(panel, area);
		if(!same) addTo(damage_, area);
	}
}

} // namespace affresco::detail
