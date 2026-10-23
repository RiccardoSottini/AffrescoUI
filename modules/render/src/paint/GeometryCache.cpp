/// @file
/// @brief detail::GeometryCache: the panels that changed or moved recorded again, the damage they make, and the
/// shaders the frame needs.

#include "paint/GeometryCache.hpp"
#include "paint/PaintRecorder.hpp"

#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace affresco::detail {

namespace {

/// @brief A new PanelGeometry id, never given before (main thread only).
/// @return The id.
std::uint64_t nextGeometryId() noexcept {
	static std::uint64_t last = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): the counter itself
	return ++last;
}

/// @brief Adds what a child's subtree draws to its parent's.
/// @param into The parent's geometry.
/// @param below The child's.
/// @param transformed Whether the child has a transform of its own.
void addSubtree(PanelGeometry& into, const PanelGeometry& below, bool transformed) noexcept {
	into.subtreeShapes = into.subtreeShapes || below.subtreeShapes;
	into.subtreeColor = into.subtreeColor || below.subtreeColor || below.cached; // a cached child is a colour quad
	into.subtreeTransformed = into.subtreeTransformed || below.subtreeTransformed || transformed;
}

} // namespace

Rect reachOf(const Rect& bounds, const PaintRecord& record) noexcept {
	return record.extent ? unite(bounds, *record.extent) : bounds;
}

Rect screenReachOf(const PanelGeometry& geometry) noexcept {
	const Rect reach = reachOf(geometry.key.bounds, geometry.record);
	return geometry.world.isIdentity() ? reach : geometry.world.boundingBox(reach);
}

Refreshed GeometryCache::refresh(const Frame& frame) {
	all_ = frame.paintGeneration != generation_;
	frameGeneration_ = frame.paintGeneration;
	damage_.reset();

	const Rect window{0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)};
	refreshRoot(frame.root, window);
	for(Panel* overlay : frame.overlays) refreshRoot(*overlay, window);
	if(frame.tooltip) refreshRoot(*frame.tooltip, window);
	compareOverlays(frame);

	// The shaders a full frame would use: what the tree, the overlays and the tooltip draw
	Refreshed result{damage_};
	addShaders(result, frame.root);
	for(const Panel* overlay : frame.overlays) addShaders(result, *overlay);
	if(frame.tooltip) addShaders(result, *frame.tooltip);

	lastPass_ = frame.layoutPass;
	generation_ = frame.paintGeneration;
	return result;
}

void GeometryCache::refreshRoot(Panel& root, const Rect& window) {
	if(needsVisit(root)) refreshTree(root, window, {}, false);
}

void GeometryCache::addShaders(Refreshed& result, const Panel& root) noexcept {
	if(const PanelGeometry* geometry = root.geometry_.get()) {
		result.shapes = result.shapes || geometry->subtreeShapes;
		result.color = result.color || geometry->subtreeColor || geometry->cached;
		result.transformed = result.transformed || geometry->subtreeTransformed || !geometry->world.isIdentity();
	}
}

bool GeometryCache::needsVisit(const Panel& panel) const noexcept {
	return all_ || panel.paintStale_ || panel.paintBelow_ || panel.layoutPass_ > lastPass_ || !panel.geometry_;
}

bool GeometryCache::needsRecord(const Panel& panel, const PanelGeometry& geometry, bool moved) const noexcept {
	return all_ || moved || panel.paintStale_ || panel.layoutPass_ > lastPass_ || !geometry.recorded;
}

PanelGeometry& GeometryCache::geometryOf(Panel& panel) {
	if(!panel.geometry_) {
		panel.geometry_ = std::make_unique<PanelGeometry>();
		panel.geometry_->id = nextGeometryId();
	}
	return *panel.geometry_;
}

void GeometryCache::refreshTree(Panel& panel, const Rect& outerClip, const Affine2& parentWorld, bool moved) {
	PanelGeometry& geometry = geometryOf(panel);
	const std::uint64_t changesBefore = changes_;
	const bool marked = panel.paintStale_ || panel.paintBelow_;
	const Affine2 world = panel.isTransformed() ? parentWorld * panel.localTransform() : parentWorld;
	moved = moved || geometry.world != world;
	if(needsRecord(panel, geometry, moved)) refreshPanel(panel, outerClip, geometry, world);

	// A hidden panel draws no child: it was damage where it reached, which holds its children
	geometry.subtreeShapes = geometry.subtreeColor = geometry.subtreeTransformed = false;
	if(panel.shown_) refreshChildren(panel, geometry, moved);
	// A cached subtree is drawn again into its texture when anything in it was recorded again or marked
	if(panel.cached_ && (all_ || marked || changes_ != changesBefore)) ++geometry.subtreeVersion;
	geometry.cached = panel.shown_ && panel.cached_;
	panel.paintStale_ = panel.paintBelow_ = false;
}

void GeometryCache::refreshChildren(Panel& panel, PanelGeometry& geometry, bool moved) {
	geometry.subtreeShapes = geometry.recorded && geometry.record.shapes;
	geometry.subtreeColor = geometry.recorded && geometry.record.color;
	geometry.subtreeTransformed = false;
	for(const auto& child : panel.children_) {
		if(child->isOverlay()) continue; // refreshed from the frame's overlays, unclipped; snapshots never
		if(moved || needsVisit(*child)) refreshChild(panel, *child, geometry.world, moved);
		if(const PanelGeometry* below = child->geometry_.get())
			addSubtree(geometry, *below, child->isTransformed());
	}
}

void GeometryCache::refreshChild(const Panel& parent, Panel& child, const Affine2& parentWorld, bool moved) {
	// A transformed child's shadow is clipped in its own space: the box of its parent's bounds seen from there
	const std::optional<Affine2> inverse =
	    child.isTransformed() ? child.localTransform().inverse() : std::nullopt;
	refreshTree(child, inverse ? inverse->boundingBox(parent.childClip()) : parent.childClip(), parentWorld, moved);
}

void GeometryCache::refreshPanel(Panel& panel, const Rect& outerClip, PanelGeometry& geometry, const Affine2& world) {
	const PaintKey key{panel.frame_,      panel.bounds_,     outerClip,        panel.scroll_,
	                   panel.contentMin_, panel.contentMax_, frameGeneration_, !world.isAxisAligned()};
	if(panel.shown_ && geometry.recorded && !panel.paintStale_ && geometry.key == key) {
		if(geometry.world == world) return; // nothing changed
		// Only its place on screen changed: the same drawing, moved
		++changes_;
		addTo(damage_, screenReachOf(geometry));
		geometry.world = world;
		addTo(damage_, screenReachOf(geometry));
		return;
	}

	++changes_;
	if(geometry.recorded) addTo(damage_, screenReachOf(geometry));
	geometry.record.clear();
	geometry.recorded = false;
	geometry.world = world;
	if(!panel.shown_) return;

	PaintRecorder recorder(geometry.record, panel.bounds_, outerClip);
	if(key.smooth) recorder.smoothEdges(panel.frame_);
	panel.paint(recorder);
	geometry.record.overStart = geometry.record.ops.size();
	panel.paintOver(recorder);
	geometry.key = key;
	geometry.recorded = true;
	addTo(damage_, screenReachOf(geometry));
}

// Compared by address only: a panel that is gone reported where it was itself, and a new one is recorded and damaged.
// Walked in place, the overlays then the tooltip: a list of them made here would be an allocation every frame
void GeometryCache::compareOverlays(const Frame& frame) {
	const std::size_t count = frame.overlays.size() + (frame.tooltip ? 1 : 0);
	const auto drawnAbove = [&frame](std::size_t index) -> const Panel* {
		return index < frame.overlays.size() ? frame.overlays[index] : frame.tooltip;
	};
	bool same = above_.size() == count;
	for(std::size_t index = 0; same && index < count; ++index) same = above_[index].first == drawnAbove(index);
	if(!same)
		for(const auto& [panel, area] : above_) addTo(damage_, area);

	above_.resize(count);
	for(std::size_t index = 0; index < count; ++index) {
		const Panel* panel = drawnAbove(index);
		const PanelGeometry* geometry = panel->geometry_.get();
		const Rect area = geometry && geometry->recorded ? screenReachOf(*geometry) : Rect{};
		above_[index] = {panel, area};
		if(!same) addTo(damage_, area);
	}
}

} // namespace affresco::detail
