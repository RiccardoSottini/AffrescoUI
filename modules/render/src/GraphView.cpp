/// @file
/// @brief GraphView: the link rules, the camera, node layout and culling, link meshes, the minimap and the editing.

#include "affresco/render/GraphView.hpp"
#include "affresco/render/RichText.hpp"
#include "affresco/render/VectorPath.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <numeric>

namespace affresco {

namespace {

constexpr float headerHeight = 24.0f;       ///< The height of a node's header, in graph units.
constexpr float portRow = 20.0f;            ///< The height of a row of ports, in graph units.
constexpr float bottomPadding = 6.0f;       ///< Room under a node's last row or body, in graph units.
constexpr float portRadius = 5.0f;          ///< The radius of a port's dot, in graph units.
constexpr float portSlop = 4.0f;            ///< Window units around a port's dot that still pick it.
constexpr float keyStep = 8.0f;             ///< Graph units an arrow key moves the selection.
constexpr float bigKeyStep = 32.0f;         ///< With Shift.
constexpr float keyZoom = 1.25f;            ///< What + multiplies the zoom by.
constexpr float gridStep = 32.0f;           ///< Graph units between grid lines.
constexpr float bodyZoom = 0.5f;            ///< Under this zoom, bodies and text are hidden.
constexpr float linkWidth = 2.0f;           ///< The width of a link, in window units.
constexpr Vec2 minimapSize{160.0f, 100.0f}; ///< The minimap's size, in window units.
constexpr float minimapMargin = 8.0f;       ///< Its distance from the view's edges.

/// @brief Adds two points.
/// @param a The first.
/// @param b The second.
/// @return a + b.
constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
/// @brief Subtracts two points.
/// @param a The first.
/// @param b The second.
/// @return a - b.
constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }

/// @brief Tells whether two rectangles overlap.
/// @param a The first.
/// @param b The second.
/// @return true if they share any point.
constexpr bool overlaps(const Rect& a, const Rect& b) noexcept {
	return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
}

/// @brief The rows of ports a node has.
/// @param node The node.
/// @return The larger of its inputs and outputs.
float rowsOf(const GraphNode& node) noexcept {
	return static_cast<float>(std::max(node.inputs.size(), node.outputs.size()));
}

/// @brief The index of a port in a list.
/// @param ports The list.
/// @param id The port's id.
/// @return Its index, or ports.size() if there is none.
std::size_t portIndex(const std::vector<GraphPort>& ports, std::string_view id) noexcept {
	for(std::size_t i = 0; i < ports.size(); ++i)
		if(ports[i].id == id) return i;
	return ports.size();
}

/// @brief The indices of a model's nodes, sorted by id (the first of equal ids first), for findSorted().
/// @param model The model.
/// @return The indices.
std::vector<std::size_t> sortedById(const GraphModel& model) {
	std::vector<std::size_t> order(model.nodes.size());
	std::iota(order.begin(), order.end(), std::size_t{0});
	std::stable_sort(order.begin(), order.end(),
	                 [&](std::size_t a, std::size_t b) { return model.nodes[a].id < model.nodes[b].id; });
	return order;
}

/// @brief Finds a node by id, as GraphModel::find() does, in sortedById()'s order.
/// @param model The model.
/// @param order Its sortedById().
/// @param id The node's id.
/// @return Its index, or model.nodes.size() if there is none.
std::size_t findSorted(const GraphModel& model, const std::vector<std::size_t>& order, std::string_view id) noexcept {
	const auto at = std::lower_bound(order.begin(), order.end(), id, [&](std::size_t index, std::string_view value) {
		return model.nodes[index].id < value;
	});
	return at != order.end() && model.nodes[*at].id == id ? *at : model.nodes.size();
}

/// @brief The nodes a link from or to a node would close a cycle with: those that reach it, or that it reaches.
/// @param model The model.
/// @param node The node.
/// @param reaching true for the nodes that reach it (a link from it to one of them closes a cycle), false for those
/// it reaches (a link to it from one of them does).
/// @param left A link left out (being replaced), or nullptr.
/// @return For each node, whether it is one; the node itself is.
std::vector<bool> cycleNodes(const GraphModel& model, std::size_t node, bool reaching, const GraphLink* left) {
	const std::vector<std::size_t> order = sortedById(model);
	std::vector<std::vector<std::size_t>> next(model.nodes.size());
	bool skipped = left == nullptr;
	for(const GraphLink& link : model.links) {
		if(!skipped && link == *left) {
			skipped = true;
			continue;
		}
		const std::size_t from = findSorted(model, order, link.from), to = findSorted(model, order, link.to);
		if(from == model.nodes.size() || to == model.nodes.size()) continue;
		if(reaching) next[to].push_back(from);
		else next[from].push_back(to);
	}
	std::vector<bool> found(model.nodes.size(), false);
	if(node >= found.size()) return found;
	std::deque<std::size_t> queue{node};
	found[node] = true;
	while(!queue.empty()) {
		const std::size_t at = queue.front();
		queue.pop_front();
		for(const std::size_t other : next[at])
			if(!found[other]) {
				found[other] = true;
				queue.push_back(other);
			}
	}
	return found;
}

/// @brief The curve a link is drawn along, from an output point to an input point.
/// @param start The output's point, in window units.
/// @param end The input's point.
/// @param zoom The camera's zoom, which scales the handles.
/// @return The path.
VectorPath linkPath(Vec2 start, Vec2 end, float zoom) {
	const float handle = std::max(std::abs(end.x - start.x) * 0.5f, 40.0f * zoom);
	VectorPath path;
	path.moveTo(start);
	path.cubicTo({start.x + handle, start.y}, {end.x - handle, end.y}, end);
	return path;
}

/// @brief The scale and offset that fit a graph box into the minimap.
struct MinimapFit {
	float scale = 1.0f; ///< Minimap units per graph unit.
	Vec2 offset;        ///< Where the graph's origin lands, relative to the view.
};

/// @brief Fits a graph box into a minimap box, centred, keeping its proportions.
/// @param graph The graph box.
/// @param map The minimap box.
/// @return The fit.
MinimapFit fitMinimap(const Rect& graph, const Rect& map) noexcept {
	const float scale =
	    std::min(map.width() / std::max(graph.width(), 1.0f), map.height() / std::max(graph.height(), 1.0f));
	const float left = map.left + (map.width() - graph.width() * scale) * 0.5f;
	const float top = map.top + (map.height() - graph.height() * scale) * 0.5f;
	return {scale, {left - graph.left * scale, top - graph.top * scale}};
}

} // namespace

std::size_t GraphModel::find(std::string_view id) const noexcept {
	for(std::size_t i = 0; i < nodes.size(); ++i)
		if(nodes[i].id == id) return i;
	return nodes.size();
}

bool kindsMatch(std::string_view output, std::string_view input) noexcept {
	return output == input || output == "any" || input == "any";
}

LinkVerdict checkLink(const GraphModel& model, const GraphLink& link, bool cyclesAllowed, const GraphLink* replacing) {
	const std::size_t from = model.find(link.from), to = model.find(link.to);
	if(from == model.nodes.size() || to == model.nodes.size()) return LinkVerdict::UnknownPort;
	const std::size_t output = portIndex(model.nodes[from].outputs, link.output);
	const std::size_t input = portIndex(model.nodes[to].inputs, link.input);
	if(output == model.nodes[from].outputs.size() || input == model.nodes[to].inputs.size())
		return LinkVerdict::UnknownPort;
	const GraphPort& out = model.nodes[from].outputs[output];
	const GraphPort& in = model.nodes[to].inputs[input];
	if(!kindsMatch(out.kind, in.kind)) return LinkVerdict::KindMismatch;

	// The links that stay: all but the one being replaced (the first equal to it)
	bool skipped = replacing == nullptr;
	std::size_t fromLinks = 0, toLinks = 0;
	for(const GraphLink& other : model.links) {
		if(!skipped && other == *replacing) {
			skipped = true;
			continue;
		}
		if(other == link) return LinkVerdict::Duplicate;
		if(other.from == link.from && other.output == link.output) ++fromLinks;
		if(other.to == link.to && other.input == link.input) ++toLinks;
	}
	if((out.links != 0 && fromLinks >= out.links) || (in.links != 0 && toLinks >= in.links)) return LinkVerdict::Full;

	// A cycle if the link's start can be reached from its end (or is its end)
	if(!cyclesAllowed && cycleNodes(model, to, false, replacing)[from]) return LinkVerdict::Cycle;
	return LinkVerdict::Allowed;
}

Vec2 GraphCamera::toView(Vec2 point) const noexcept { return {pan.x + point.x * zoom, pan.y + point.y * zoom}; }

Vec2 GraphCamera::toGraph(Vec2 point) const noexcept { return {(point.x - pan.x) / zoom, (point.y - pan.y) / zoom}; }

void GraphCamera::zoomAt(Vec2 point, float zoomTo) noexcept {
	if(std::isnan(zoomTo)) return;
	const Vec2 anchor = toGraph(point);
	zoom = std::clamp(zoomTo, minimumZoom, maximumZoom);
	pan = {point.x - anchor.x * zoom, point.y - anchor.y * zoom};
}

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
	const GraphNode& node = model_.nodes[index];
	const Vec2 at = shownPosition(index);
	const float height = headerHeight + rowsOf(node) * portRow + std::max(node.bodyHeight, 0.0f) + bottomPadding;
	return {at.x, at.y, at.x + node.width, at.y + height};
}

Vec2 GraphView::portPoint(std::size_t index, std::size_t port, bool output) const noexcept {
	const Rect box = nodeBox(index);
	return {output ? box.right : box.left, box.top + headerHeight + (static_cast<float>(port) + 0.5f) * portRow};
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
		event.kind = GraphEvent::Kind::LinkDrop;
		event.node = model_.nodes[start.node].id;
		const auto& ports = start.output ? model_.nodes[start.node].outputs : model_.nodes[start.node].inputs;
		event.port = ports[start.port].id;
		event.output = start.output;
		event.at = at;
		onEdit(event);
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

void GraphView::mouseButtonEvent(const MouseButtonEvent& event) {
	const Vec2 point{event.position.x - frame().left, event.position.y - frame().top};
	if(event.action == Action::Press && event.button == MouseButton::Left) {
		pressed_ = pointer_ = point;
		dragDelta_ = {};
		if(minimapBox().contains(point) && minimapBox().width() > 0.0f) {
			drag_ = Drag::Minimap;
			panToMinimap(point);
		} else if(const PortRef port = portAt(point); port.valid) {
			const std::size_t link = linkAt(port);
			drag_ = Drag::Link;
			replacing_ = link;
			linkStart_ = port;
			if(link < model_.links.size()) {
				// Picking up a linked input: the link now hangs from its output
				const std::size_t from = nodeIndex(model_.links[link].from);
				linkStart_ = {from, portIndex(model_.nodes[from].outputs, model_.links[link].output), true, true};
			}
			keyNode_ = port.node;
		} else if(const std::size_t node = nodeAt(point); node < model_.nodes.size()) {
			std::vector<std::string> ids = selection_;
			if(event.mods.shift()) {
				const auto found = std::find(ids.begin(), ids.end(), model_.nodes[node].id);
				if(found == ids.end()) ids.push_back(model_.nodes[node].id);
				else ids.erase(found);
			} else if(!isSelected(node)) {
				ids = {model_.nodes[node].id};
			}
			select(std::move(ids));
			keyNode_ = node;
			keyPort_ = 0;
			drag_ = isSelected(node) ? Drag::Nodes : Drag::None;
		} else {
			drag_ = Drag::Marquee;
			pressSelection_ = event.mods.shift() ? selection_ : std::vector<std::string>{};
		}
		describe();
		invalidatePaint();
	} else if(event.action == Action::Press) {
		drag_ = Drag::Pan;
		pressed_ = point;
		pressCamera_ = camera_;
	} else if(event.action == Action::Release && drag_ != Drag::None) {
		const Drag drag = drag_;
		drag_ = Drag::None;
		if(drag == Drag::Nodes) {
			const Vec2 delta = dragDelta_;
			dragDelta_ = {};
			if(delta.x != 0.0f || delta.y != 0.0f) moveSelection(delta);
		} else if(drag == Drag::Link) {
			finishLink(portAt(point), camera_.toGraph(point));
		} else if(drag == Drag::Marquee) {
			const Vec2 a = camera_.toGraph(pressed_), b = camera_.toGraph(point);
			const Rect area{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
			std::vector<std::string> ids = pressSelection_;
			for(std::size_t i = 0; i < model_.nodes.size(); ++i)
				if(overlaps(nodeBox(i), area) && std::find(ids.begin(), ids.end(), model_.nodes[i].id) == ids.end())
					ids.push_back(model_.nodes[i].id);
			select(std::move(ids));
		}
		linksDirty_ = true;
		invalidate();
	}
	Panel::mouseButtonEvent(event);
}

void GraphView::mouseMoveEvent(const MouseMoveEvent& event) {
	pointer_ = {event.position.x - frame().left, event.position.y - frame().top};
	switch(drag_) {
	case Drag::Nodes:
		dragDelta_ = {(pointer_.x - pressed_.x) / camera_.zoom, (pointer_.y - pressed_.y) / camera_.zoom};
		linksDirty_ = true;
		invalidate();
		break;
	case Drag::Pan: {
		GraphCamera camera = pressCamera_;
		camera.pan = pressCamera_.pan + (pointer_ - pressed_);
		setCamera(camera);
		break;
	}
	case Drag::Minimap:
		panToMinimap(pointer_);
		break;
	case Drag::Link:
	case Drag::Marquee:
		invalidatePaint();
		break;
	case Drag::None:
		break;
	}
	Panel::mouseMoveEvent(event);
}

bool GraphView::scrollContent(Vec2 delta) {
	GraphCamera camera = camera_;
	camera.pan = camera.pan - delta;
	setCamera(camera);
	return true;
}

void GraphView::keyEvent(const KeyEvent& event) {
	const std::size_t count = model_.nodes.size();
	if(event.action != Action::Release && count > 0) {
		keyNode_ = std::min(keyNode_, count - 1);
		const GraphNode& node = model_.nodes[keyNode_];
		const std::size_t ports = node.inputs.size() + node.outputs.size();
		const PortRef port =
		    keyPort_ < ports
		        ? PortRef{keyNode_, keyPort_ < node.inputs.size() ? keyPort_ : keyPort_ - node.inputs.size(),
		                  keyPort_ >= node.inputs.size(), true}
		        : PortRef{};
		const float step = event.mods.shift() ? bigKeyStep : keyStep;
		if(event.key == Key::PageUp || event.key == Key::PageDown) {
			keyNode_ = event.key == Key::PageDown ? (keyNode_ + 1) % count : (keyNode_ + count - 1) % count;
			keyPort_ = 0;
			select({model_.nodes[keyNode_].id});
			// Bring it on screen
			const Rect box = nodeBox(keyNode_);
			const Vec2 size = resolvedSize(), at = camera_.toView({box.left, box.top});
			if(at.x < 0.0f || at.y < 0.0f || at.x > size.x || at.y > size.y) {
				GraphCamera camera = camera_;
				camera.pan = {size.x * 0.5f - (box.left + box.width() * 0.5f) * camera.zoom,
				              size.y * 0.5f - (box.top + box.height() * 0.5f) * camera.zoom};
				setCamera(camera);
			}
		} else if(event.key == Key::Left) moveSelection({-step, 0.0f});
		else if(event.key == Key::Right) moveSelection({step, 0.0f});
		else if(event.key == Key::Up) moveSelection({0.0f, -step});
		else if(event.key == Key::Down) moveSelection({0.0f, step});
		else if(event.key == Key::LeftBracket && ports > 0) keyPort_ = (keyPort_ + ports - 1) % ports;
		else if(event.key == Key::RightBracket && ports > 0) keyPort_ = (keyPort_ + 1) % ports;
		else if(event.key == Key::Enter && port.valid) {
			if(!keyLink_) {
				linkStart_ = port;
				replacing_ = model_.links.size();
				keyLink_ = true;
			} else {
				keyLink_ = false;
				finishLink(port, portPoint(port.node, port.port, port.output));
			}
		} else if(event.key == Key::Escape) {
			if(keyLink_) {
				keyLink_ = false;
				linkStart_ = {};
			} else {
				select({});
			}
		} else if((event.key == Key::Delete || event.key == Key::Backspace) && event.mods.shift() && port.valid) {
			// The links at the keyboard's port, the last first so the indices stay right; gathered before any is
			// fired, since the owner may change the model in between
			const std::string& id = port.output ? node.outputs[port.port].id : node.inputs[port.port].id;
			std::vector<GraphEvent> disconnects;
			for(std::size_t i = model_.links.size(); i-- > 0;) {
				const GraphLink& link = model_.links[i];
				const bool here =
				    port.output ? link.from == node.id && link.output == id : link.to == node.id && link.input == id;
				if(!here) continue;
				GraphEvent& disconnect = disconnects.emplace_back();
				disconnect.kind = GraphEvent::Kind::Disconnect;
				disconnect.link = link;
				disconnect.linkIndex = i;
			}
			for(const GraphEvent& disconnect : disconnects) onEdit(disconnect);
		} else if(event.key == Key::Delete || event.key == Key::Backspace) {
			GraphEvent erase;
			erase.kind = GraphEvent::Kind::Delete;
			for(std::size_t i = 0; i < count; ++i)
				if(isSelected(i)) erase.nodes.push_back(i);
			if(!erase.nodes.empty()) onEdit(erase);
		} else if(event.key == Key::Equal || event.key == Key::Minus) {
			GraphCamera camera = camera_;
			const Vec2 size = resolvedSize();
			camera.zoomAt({size.x * 0.5f, size.y * 0.5f},
			              event.key == Key::Equal ? camera.zoom * keyZoom : camera.zoom / keyZoom);
			setCamera(camera);
		} else if(event.key == Key::Home) {
			setCamera({});
		}
		describe();
		invalidatePaint();
	}
	Panel::keyEvent(event);
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

void GraphView::buildLinks() const {
	linksDirty_ = false;
	for(LinkMesh& mesh : linkMeshes_) mesh.quads.clear();
	const Vec2 size = resolvedSize();
	const Rect seen{0.0f, 0.0f, size.x, size.y};
	const StrokeStyle style{.width = linkWidth, .cap = LineCap::round};
	for(std::size_t i = 0; i < model_.links.size(); ++i) {
		if(i == replacing_ && drag_ == Drag::Link) continue; // drawn as the preview instead
		const GraphLink& link = model_.links[i];
		const std::size_t from = nodeIndex(link.from), to = nodeIndex(link.to);
		if(from == model_.nodes.size() || to == model_.nodes.size()) continue;
		const std::size_t output = portIndex(model_.nodes[from].outputs, link.output);
		const std::size_t input = portIndex(model_.nodes[to].inputs, link.input);
		if(output == model_.nodes[from].outputs.size() || input == model_.nodes[to].inputs.size()) continue;
		const Vec2 start = camera_.toView(portPoint(from, output, true));
		const Vec2 end = camera_.toView(portPoint(to, input, false));
		const float handle = std::max(std::abs(end.x - start.x) * 0.5f, 40.0f * camera_.zoom);
		const Rect box{std::min(start.x, end.x - handle), std::min(start.y, end.y), std::max(start.x + handle, end.x),
		               std::max(start.y, end.y)};
		if(!overlaps(box, seen)) continue; // culled

		const GraphPort& port = model_.nodes[from].outputs[output];
		const std::string& kind = port.kind == "any" ? model_.nodes[to].inputs[input].kind : port.kind;
		const Color color = kindColor(kind);
		auto mesh =
		    std::find_if(linkMeshes_.begin(), linkMeshes_.end(), [&](const LinkMesh& m) { return m.color == color; });
		if(mesh == linkMeshes_.end()) mesh = linkMeshes_.insert(linkMeshes_.end(), LinkMesh{color, {}});
		if(mesh->quads.size() / 4 >= meshQuadLimit) continue;
		appendStroke(linkPath(start, end, camera_.zoom).flatten({}, 0.25f), style, mesh->quads);
	}
}

void GraphView::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& f = frame();
	const Vec2 size = resolvedSize();

	// The grid, when its lines are far enough apart
	const float spacing = gridStep * camera_.zoom;
	if(spacing >= 8.0f) {
		const Color line = Color::mix(color(), textColor_, 0.06f);
		for(float x = std::fmod(camera_.pan.x, spacing) + (camera_.pan.x < 0.0f ? spacing : 0.0f); x < size.x;
		    x += spacing)
			painter.fillRect({f.left + x, f.top, f.left + x + 1.0f, f.bottom}, line);
		for(float y = std::fmod(camera_.pan.y, spacing) + (camera_.pan.y < 0.0f ? spacing : 0.0f); y < size.y;
		    y += spacing)
			painter.fillRect({f.left, f.top + y, f.right, f.top + y + 1.0f}, line);
	}

	if(linksDirty_) buildLinks();
	for(const LinkMesh& mesh : linkMeshes_)
		if(!mesh.quads.empty()) painter.fillMesh(mesh.quads, {f.left, f.top}, mesh.color);

	const Font* text = font();
	const bool linking = drag_ == Drag::Link || keyLink_;
	const float zoom = camera_.zoom;
	for(std::size_t i = 0; i < model_.nodes.size(); ++i) {
		if(i >= visible_.size() || !visible_[i]) continue;
		const GraphNode& node = model_.nodes[i];
		const Rect box = nodeBox(i);
		const Vec2 topLeft = camera_.toView({box.left, box.top});
		const Rect area{f.left + topLeft.x, f.top + topLeft.y, f.left + topLeft.x + box.width() * zoom,
		                f.top + topLeft.y + box.height() * zoom};
		const float radius = 6.0f * zoom;
		painter.fillRoundedRect(area, CornerRadii::all(radius), nodeColor_);
		painter.fillRoundedRect({area.left, area.top, area.right, area.top + headerHeight * zoom},
		                        {radius, radius, 0.0f, 0.0f}, headerColor_);
		if(isSelected(i)) painter.strokeRoundedRect(area, CornerRadii::all(radius), 2.0f, accent_);
		if(text && zoom >= bodyZoom) {
			const float baseline = area.top + (headerHeight * zoom - text->lineHeight()) * 0.5f + text->ascent();
			painter.drawText(*text, codepointsOf(node.title.empty() ? node.id : node.title),
			                 {area.left + 8.0f * zoom, baseline}, textColor_);
		}

		for(const bool output : {false, true}) {
			const auto& ports = output ? node.outputs : node.inputs;
			for(std::size_t p = 0; p < ports.size(); ++p) {
				const Vec2 at = camera_.toView(portPoint(i, p, output));
				const Vec2 point{f.left + at.x, f.top + at.y};
				const float r = portRadius * zoom;
				const bool keyed = hasFocus() && i == keyNode_ && keyPort_ == (output ? node.inputs.size() + p : p);
				if((linking && accepts({i, p, output, true})) || keyed) {
					const float ring = r + 3.0f;
					painter.fillRoundedRect({point.x - ring, point.y - ring, point.x + ring, point.y + ring},
					                        CornerRadii::all(ring), accent_);
				}
				painter.fillRoundedRect({point.x - r, point.y - r, point.x + r, point.y + r}, CornerRadii::all(r),
				                        kindColor(ports[p].kind));
				if(text && zoom >= bodyZoom) {
					const std::u32string label = codepointsOf(ports[p].label.empty() ? ports[p].id : ports[p].label);
					const float width = text->measure(label);
					const float x = output ? point.x - r - 6.0f - width : point.x + r + 6.0f;
					painter.drawText(*text, label, {x, point.y - text->lineHeight() * 0.5f + text->ascent()},
					                 textColor_);
				}
			}
		}
	}
}

void GraphView::paintOver(Painter& painter) const {
	const Rect& f = frame();
	const Vec2 origin{f.left, f.top};

	// The link being drawn, from its port to the pointer (or the keyboard's port)
	if((drag_ == Drag::Link || keyLink_) && linkStart_.valid && linkStart_.node < model_.nodes.size()) {
		const Vec2 start = camera_.toView(portPoint(linkStart_.node, linkStart_.port, linkStart_.output));
		Vec2 end = pointer_;
		if(keyLink_ && keyNode_ < model_.nodes.size()) {
			const GraphNode& node = model_.nodes[keyNode_];
			if(keyPort_ < node.inputs.size() + node.outputs.size()) {
				const bool output = keyPort_ >= node.inputs.size();
				end = camera_.toView(portPoint(keyNode_, output ? keyPort_ - node.inputs.size() : keyPort_, output));
			}
		}
		std::vector<MeshVertex> quads;
		const VectorPath path =
		    linkStart_.output ? linkPath(start, end, camera_.zoom) : linkPath(end, start, camera_.zoom);
		appendStroke(path.flatten({}, 0.25f), StrokeStyle{.width = linkWidth, .cap = LineCap::round}, quads);
		const auto& ports =
		    linkStart_.output ? model_.nodes[linkStart_.node].outputs : model_.nodes[linkStart_.node].inputs;
		painter.fillMesh(quads, origin, kindColor(ports[linkStart_.port].kind).withAlpha(0.8f));
	}

	if(drag_ == Drag::Marquee) {
		const Rect area{f.left + std::min(pressed_.x, pointer_.x), f.top + std::min(pressed_.y, pointer_.y),
		                f.left + std::max(pressed_.x, pointer_.x), f.top + std::max(pressed_.y, pointer_.y)};
		painter.fillRect(area, accent_.withAlpha(0.15f));
		painter.strokeRoundedRect(area, {}, 1.0f, accent_);
	}

	// The minimap: every node as a box, and the part on screen
	const Rect map = minimapBox();
	if(map.width() > 0.0f) {
		const Rect area{f.left + map.left, f.top + map.top, f.left + map.right, f.top + map.bottom};
		painter.fillRoundedRect(area, CornerRadii::all(4.0f), nodeColor_.withAlpha(0.9f));
		const MinimapFit fit = fitMinimap(graphBounds(), map);
		const auto place = [&](const Rect& box) {
			return Rect{f.left + fit.offset.x + box.left * fit.scale, f.top + fit.offset.y + box.top * fit.scale,
			            f.left + fit.offset.x + box.right * fit.scale, f.top + fit.offset.y + box.bottom * fit.scale};
		};
		for(std::size_t i = 0; i < model_.nodes.size(); ++i)
			painter.fillRect(place(nodeBox(i)), isSelected(i) ? accent_ : headerColor_);
		const Vec2 first = camera_.toGraph({0.0f, 0.0f}), last = camera_.toGraph(resolvedSize());
		painter.strokeRoundedRect(place({first.x, first.y, last.x, last.y}), {}, 1.0f, accent_);
	}
	Panel::paintOver(painter);
}

} // namespace affresco
