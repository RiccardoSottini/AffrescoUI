/// @file
/// @brief GraphView: the model and the camera it is given, node layout and culling, the hit tests, the link
/// rules as the pointer meets them, the minimap's geometry and the editing events.

#include "graph/GraphShared.hpp"

#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

using graph::bodyZoom;
using graph::boxOf;
using graph::cycleNodes;
using graph::findSorted;
using graph::fitMinimap;
using graph::headerHeight;
using graph::MinimapFit;
using graph::minimapMargin;
using graph::minimapSize;
using graph::overlaps;
using graph::portPointIn;
using graph::portRadius;
using graph::portRow;
using graph::portSlop;
using graph::rowsOf;
using graph::sortedById;
using graph::operator+;
using graph::operator-;

namespace {

/// @brief The event for a link dropped on empty space, so the owner can offer a node to link to.
/// @param node The node it hangs from.
/// @param port The port's index among the node's outputs or inputs.
/// @param output Whether it hangs from an output.
/// @param at Where it was dropped, in graph units.
/// @return The event.
GraphEvent dropEvent(const GraphNode& node, std::size_t port, bool output, Vec2 at) {
	GraphEvent event;
	event.kind = GraphEvent::Kind::LinkDrop;
	event.node = node.id;
	event.port = (output ? node.outputs : node.inputs)[port].id;
	event.output = output;
	event.at = at;
	return event;
}

} // namespace

GraphView::GraphView() {
	setColor(Color::hex(0x1e2026));
	setAccessibleRole("graph");
}

void GraphView::setModel(GraphModel model) {
	if(model.nodes.size() > maximumNodes) model.nodes.resize(maximumNodes);
	if(model.links.size() > maximumLinks) model.links.resize(maximumLinks);
	for(GraphNode& node : model.nodes) {
		if(node.inputs.size() > maximumPorts) node.inputs.resize(maximumPorts);
		if(node.outputs.size() > maximumPorts) node.outputs.resize(maximumPorts);
	}
	if(model == model_) return;
	model_ = std::move(model);
	// A link being drawn names ports by index, which a peer's change may have moved or removed
	linkStart_ = {};
	keyLink_ = false;
	replacing_ = model_.links.size();
	if(drag_ == Drag::Link) drag_ = Drag::None;
	byId_ = sortedById(model_);
	cycleKey_.reset();
	refreshSelected();
	linksDirty_ = true;
	invalidate();
}

void GraphView::setCyclesAllowed(bool allowed) { setLook(cyclesAllowed_, allowed); }

void GraphView::setSelection(std::vector<std::string> ids) {
	if(ids == selection_) return;
	selection_ = std::move(ids);
	refreshSelected();
	invalidatePaint();
}

void GraphView::refreshSelected() {
	selected_.assign(model_.nodes.size(), false);
	for(const std::string& id : selection_) {
		const std::size_t index = nodeIndex(id);
		if(index < selected_.size()) selected_[index] = true;
	}
}

void GraphView::setCamera(GraphCamera camera) {
	if(std::isnan(camera.zoom) || std::isnan(camera.pan.x) || std::isnan(camera.pan.y)) return;
	camera.zoom = std::clamp(camera.zoom, GraphCamera::minimumZoom, GraphCamera::maximumZoom);
	if(camera == camera_) return;
	camera_ = camera;
	linksDirty_ = true;
	invalidate();
}

void GraphView::setGivenCamera(GraphCamera camera) {
	if(givenCamera_ == camera) return;
	givenCamera_ = camera;
	setCamera(camera);
}

void GraphView::setGivenSelection(std::vector<std::string> ids) {
	if(givenSelection_ == ids) return;
	givenSelection_ = ids;
	setSelection(std::move(ids));
}

void GraphView::setKindColor(std::string kind, Color color) {
	for(auto& [name, value] : kindColors_)
		if(name == kind) {
			if(value == color) return;
			value = color;
			linksDirty_ = true;
			invalidatePaint();
			return;
		}
	if(kindColors_.size() >= maximumKinds) return;
	kindColors_.emplace_back(std::move(kind), color);
	linksDirty_ = true;
	invalidatePaint();
}

void GraphView::setColors(Color node, Color header, Color text, Color accent) {
	setLook(nodeColor_, node);
	setLook(headerColor_, header);
	setLook(textColor_, text);
	setLook(accent_, accent);
}

void GraphView::setFont(std::shared_ptr<const Font> font) { setProperty(font_, std::move(font)); }

void GraphView::setMinimap(bool shown) { setLook(minimap_, shown); }

const Font* GraphView::font() const {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

Vec2 GraphView::shownPosition(std::size_t index) const noexcept {
	const Vec2 position = model_.nodes[index].position;
	return drag_ == Drag::Nodes && isSelected(index) ? position + dragDelta_ : position;
}

bool GraphView::isSelected(std::size_t index) const { return index < selected_.size() && selected_[index]; }

Color GraphView::kindColor(std::string_view kind) const {
	for(const auto& [name, color] : kindColors_)
		if(name == kind) return color;
	return Color::hex(0x9ca3af);
}

Rect GraphView::nodeBox(std::size_t index) const noexcept {
	if(index >= model_.nodes.size()) return {};
	return boxOf(model_.nodes[index], shownPosition(index));
}

Vec2 GraphView::portPoint(std::size_t index, std::size_t port, bool output) const noexcept {
	return portPointIn(nodeBox(index), port, output);
}

void GraphView::arrange() {
	const Vec2 size = resolvedSize();
	const Vec2 first = camera_.toGraph({0.0f, 0.0f}), last = camera_.toGraph(size);
	const Rect seen{first.x, first.y, last.x, last.y};
	visible_.assign(model_.nodes.size(), false);
	visibleCount_ = 0;
	for(std::size_t i = 0; i < model_.nodes.size(); ++i)
		if(overlaps(nodeBox(i), seen)) {
			visible_[i] = true;
			++visibleCount_;
		}
	if(selected_.size() != model_.nodes.size()) refreshSelected();

	// Each child is the body of the node at its index, under the node's ports
	const auto& children = this->children();
	for(std::size_t i = 0; i < children.size(); ++i) {
		Panel& body = *children[i];
		if(body.isOverlay()) continue;
		const bool shown =
		    i < model_.nodes.size() && visible_[i] && model_.nodes[i].bodyHeight > 0.0f && camera_.zoom >= bodyZoom;
		body.setVisible(shown);
		if(!shown) continue;
		const GraphNode& node = model_.nodes[i];
		const Rect box = nodeBox(i);
		const Vec2 at = camera_.toView({box.left, box.top + headerHeight + rowsOf(node) * portRow});
		body.clearAnchor(Edge::Right);
		body.clearAnchor(Edge::Bottom);
		body.setAnchor(Edge::Left, px(at.x));
		body.setAnchor(Edge::Top, px(at.y));
		arrangeSize(body, node.width * camera_.zoom, node.bodyHeight * camera_.zoom);
		if(body.accessibleRole().empty()) body.setAccessibleRole("group");
		if(body.accessibleLabel().empty()) body.setAccessibleLabel(node.title.empty() ? node.id : node.title);
	}
	linksDirty_ = true;
}

GraphView::PortRef GraphView::portAt(Vec2 point) const {
	const float reach = portRadius * camera_.zoom + portSlop;
	for(std::size_t i = model_.nodes.size(); i-- > 0;) {
		if(i >= visible_.size() || !visible_[i]) continue;
		for(const bool output : {false, true}) {
			const auto& ports = output ? model_.nodes[i].outputs : model_.nodes[i].inputs;
			for(std::size_t p = 0; p < ports.size(); ++p) {
				const Vec2 at = camera_.toView(portPoint(i, p, output));
				const float dx = at.x - point.x, dy = at.y - point.y;
				if(dx * dx + dy * dy <= reach * reach) return {i, p, output, true};
			}
		}
	}
	return {};
}

std::size_t GraphView::nodeAt(Vec2 point) const {
	const Vec2 at = camera_.toGraph(point);
	for(std::size_t i = model_.nodes.size(); i-- > 0;)
		if(i < visible_.size() && visible_[i] && nodeBox(i).contains(at)) return i;
	return model_.nodes.size();
}

std::size_t GraphView::linkAt(const PortRef& port) const {
	if(!port.valid || port.output) return model_.links.size();
	const GraphNode& node = model_.nodes[port.node];
	for(std::size_t i = model_.links.size(); i-- > 0;)
		if(model_.links[i].to == node.id && model_.links[i].input == node.inputs[port.port].id) return i;
	return model_.links.size();
}

std::size_t GraphView::nodeIndex(std::string_view id) const noexcept { return findSorted(model_, byId_, id); }

GraphLink GraphView::candidate(const PortRef& target) const {
	const PortRef& out = linkStart_.output ? linkStart_ : target;
	const PortRef& in = linkStart_.output ? target : linkStart_;
	return {model_.nodes[out.node].id, model_.nodes[out.node].outputs[out.port].id, model_.nodes[in.node].id,
	        model_.nodes[in.node].inputs[in.port].id};
}

bool GraphView::accepts(const PortRef& target) const {
	if(!target.valid || !linkStart_.valid || target.output == linkStart_.output) return false;
	if(target.node >= model_.nodes.size() || linkStart_.node >= model_.nodes.size()) return false;
	const GraphLink* replaced = replacing_ < model_.links.size() ? &model_.links[replacing_] : nullptr;
	// The rules but cycles, then the cycles from the nodes found once for the link being drawn (checkLink() would
	// search the graph again for every port it is asked about)
	if(checkLink(model_, candidate(target), true, replaced) != LinkVerdict::Allowed) return false;
	if(cyclesAllowed_) return true;
	const CycleKey key{linkStart_.node, linkStart_.output, replacing_};
	if(cycleKey_ != key) {
		// From an output: linking to a node that reaches it closes a cycle; from an input, to one it reaches
		cycleNodes_ = cycleNodes(model_, linkStart_.node, linkStart_.output, replaced);
		cycleKey_ = key;
	}
	return target.node < cycleNodes_.size() && !cycleNodes_[target.node];
}

void GraphView::finishLink(const PortRef& target, Vec2 at) {
	const PortRef start = linkStart_;
	const std::size_t replacing = replacing_;
	linkStart_ = {};
	replacing_ = model_.links.size();
	invalidatePaint();
	if(!start.valid || start.node >= model_.nodes.size()) return;
	linkStart_ = start; // candidate() and accepts() read it
	replacing_ = replacing;
	const bool allowed = accepts(target);
	const GraphLink made = allowed ? candidate(target) : GraphLink{};
	linkStart_ = {};
	replacing_ = model_.links.size();

	GraphEvent event;
	if(replacing < model_.links.size()) {
		// A picked-up end: moved to another port, or detached on empty space
		const GraphLink old = model_.links[replacing];
		if(target.valid && (!allowed || made == old)) return;
		event.kind = GraphEvent::Kind::Disconnect;
		event.link = old;
		event.linkIndex = replacing;
		onEdit(event);
		if(!allowed) return;
	} else if(!target.valid) {
		if(nodeAt(camera_.toView(at)) < model_.nodes.size()) return;
		onEdit(dropEvent(model_.nodes[start.node], start.port, start.output, at));
		return;
	} else if(!allowed) {
		return; // refused: nothing changes
	}
	GraphEvent connect;
	connect.kind = GraphEvent::Kind::Connect;
	connect.link = made;
	onEdit(connect);
}

void GraphView::select(std::vector<std::string> ids) {
	if(ids == selection_) return;
	setSelection(std::move(ids));
	GraphEvent event;
	event.kind = GraphEvent::Kind::Select;
	for(std::size_t i = 0; i < model_.nodes.size(); ++i)
		if(isSelected(i)) event.nodes.push_back(i);
	onEdit(event);
}

void GraphView::moveSelection(Vec2 delta) {
	if(delta == Vec2{}) return; // a drag that ends where it started moves nothing
	GraphEvent event;
	event.kind = GraphEvent::Kind::Move;
	event.delta = delta;
	for(std::size_t i = 0; i < model_.nodes.size(); ++i)
		if(isSelected(i)) {
			// Kept in the model too, so the move shows at once; the owner's next model confirms or undoes it
			model_.nodes[i].position = model_.nodes[i].position + delta;
			event.nodes.push_back(i);
			event.positions.push_back(model_.nodes[i].position);
		}
	if(event.nodes.empty()) return;
	linksDirty_ = true;
	invalidate();
	onEdit(event);
}

Rect GraphView::minimapBox() const noexcept {
	const Vec2 size = resolvedSize();
	if(!minimap_ || model_.nodes.empty() || size.x < minimapSize.x * 2.0f || size.y < minimapSize.y * 2.0f) return {};
	return {size.x - minimapMargin - minimapSize.x, size.y - minimapMargin - minimapSize.y, size.x - minimapMargin,
	        size.y - minimapMargin};
}

Rect GraphView::graphBounds() const noexcept {
	Rect bounds{};
	for(std::size_t i = 0; i < model_.nodes.size(); ++i) {
		const Rect box = nodeBox(i);
		if(i == 0) bounds = box;
		bounds = {std::min(bounds.left, box.left), std::min(bounds.top, box.top), std::max(bounds.right, box.right),
		          std::max(bounds.bottom, box.bottom)};
	}
	// The part of the graph on screen counts too, so the view's frame always fits in the minimap
	const Vec2 first = camera_.toGraph({0.0f, 0.0f}), last = camera_.toGraph(resolvedSize());
	return {std::min(bounds.left, first.x), std::min(bounds.top, first.y), std::max(bounds.right, last.x),
	        std::max(bounds.bottom, last.y)};
}

void GraphView::panToMinimap(Vec2 point) {
	const Rect map = minimapBox();
	if(map.width() <= 0.0f) return;
	const MinimapFit fit = fitMinimap(graphBounds(), map);
	const Vec2 target{(point.x - fit.offset.x) / fit.scale, (point.y - fit.offset.y) / fit.scale};
	const Vec2 size = resolvedSize();
	GraphCamera camera = camera_;
	camera.pan = {size.x * 0.5f - target.x * camera.zoom, size.y * 0.5f - target.y * camera.zoom};
	setCamera(camera);
}

bool GraphView::scrollContent(Vec2 delta) {
	GraphCamera camera = camera_;
	camera.pan = camera.pan - delta;
	setCamera(camera);
	return true;
}

void GraphView::describe() {
	if(keyNode_ >= model_.nodes.size()) {
		setAccessibleDescription({});
		return;
	}
	const GraphNode& node = model_.nodes[keyNode_];
	std::string text = "node " + (node.title.empty() ? node.id : node.title);
	if(keyPort_ < node.inputs.size() + node.outputs.size()) {
		const bool output = keyPort_ >= node.inputs.size();
		const GraphPort& port = output ? node.outputs[keyPort_ - node.inputs.size()] : node.inputs[keyPort_];
		text += std::string(output ? ", output " : ", input ") + (port.label.empty() ? port.id : port.label) + " (" +
		        port.kind + ")";
	}
	if(keyLink_) text += ", linking";
	setAccessibleDescription(std::move(text));
}

void GraphView::focusEvent(bool focused) {
	invalidatePaint();
	Panel::focusEvent(focused);
}

} // namespace affresco
