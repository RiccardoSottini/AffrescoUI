/// @file
/// @brief GraphView's drawing: the grid, the link meshes, the nodes and their ports, and over them the link being
/// drawn, the marquee and the minimap.

#include "graph/GraphShared.hpp"

#include "affresco/render/RichText.hpp"
#include "affresco/render/VectorPath.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>

namespace affresco {

using graph::bodyZoom;
using graph::boxOf;
using graph::findSorted;
using graph::fitMinimap;
using graph::flagged;
using graph::gridStep;
using graph::headerHeight;
using graph::KeyPort;
using graph::keyPortOf;
using graph::linkHandle;
using graph::linkWidth;
using graph::MinimapFit;
using graph::overlaps;
using graph::portIndex;
using graph::portOf;
using graph::portRadius;
using graph::operator+;

namespace {

/// @brief The curve a link is drawn along, from an output point to an input point.
/// @param start The output's point, in window units.
/// @param end The input's point.
/// @param zoom The camera's zoom, which scales the handles.
/// @return The path.
VectorPath linkPath(Vec2 start, Vec2 end, float zoom) {
	const float handle = linkHandle(start, end, zoom);
	VectorPath path;
	path.moveTo(start);
	path.cubicTo({start.x + handle, start.y}, {end.x - handle, end.y}, end);
	return path;
}

/// @brief The box a link's curve stays in, for culling.
/// @param start The output's point, in window units.
/// @param end The input's point.
/// @param zoom The camera's zoom.
/// @return The box, in window units.
Rect linkBounds(Vec2 start, Vec2 end, float zoom) noexcept {
	const float handle = linkHandle(start, end, zoom);
	return {std::min(start.x, end.x - handle), std::min(start.y, end.y), std::max(start.x + handle, end.x),
	        std::max(start.y, end.y)};
}

/// @brief A link's two ports, by index.
struct LinkEnds {
	std::size_t from = 0;   ///< The node it starts at.
	std::size_t output = 0; ///< Its output there.
	std::size_t to = 0;     ///< The node it ends at.
	std::size_t input = 0;  ///< Its input there.
};

/// @brief Finds a link's two ports.
/// @param model The model.
/// @param byId Its nodes' indices sorted by id.
/// @param link The link.
/// @return Its ends, or none if a node or a port isn't in the graph (the link isn't drawn).
std::optional<LinkEnds> linkEnds(const GraphModel& model, const std::vector<std::size_t>& byId,
                                 const GraphLink& link) noexcept {
	const std::size_t from = findSorted(model, byId, link.from), to = findSorted(model, byId, link.to);
	if(from == model.nodes.size() || to == model.nodes.size()) return std::nullopt;
	const std::size_t output = portIndex(model.nodes[from].outputs, link.output);
	const std::size_t input = portIndex(model.nodes[to].inputs, link.input);
	if(output == model.nodes[from].outputs.size() || input == model.nodes[to].inputs.size()) return std::nullopt;
	return LinkEnds{from, output, to, input};
}

/// @brief The kind a link is coloured by: its output's, or its input's when the output takes any.
/// @param model The model.
/// @param ends The link's ends.
/// @return The kind.
const std::string& linkKind(const GraphModel& model, const LinkEnds& ends) noexcept {
	const GraphPort& port = model.nodes[ends.from].outputs[ends.output];
	return port.kind == "any" ? model.nodes[ends.to].inputs[ends.input].kind : port.kind;
}

/// @brief Draws the grid, when its lines are far enough apart.
/// @param painter The painter.
/// @param frame The view's frame.
/// @param size The view's size.
/// @param camera The camera.
/// @param line The lines' colour.
void paintGrid(Painter& painter, const Rect& frame, Vec2 size, const GraphCamera& camera, Color line) {
	const float spacing = gridStep * camera.zoom;
	if(spacing < 8.0f) return;
	for(float x = std::fmod(camera.pan.x, spacing) + (camera.pan.x < 0.0f ? spacing : 0.0f); x < size.x; x += spacing)
		painter.fillRect({frame.left + x, frame.top, frame.left + x + 1.0f, frame.bottom}, line);
	for(float y = std::fmod(camera.pan.y, spacing) + (camera.pan.y < 0.0f ? spacing : 0.0f); y < size.y; y += spacing)
		painter.fillRect({frame.left, frame.top + y, frame.right, frame.top + y + 1.0f}, line);
}

/// @brief How nodes are drawn this frame.
struct NodeLook {
	const Font* text = nullptr; ///< The font, or nullptr for no text.
	float zoom = 1.0f;          ///< The camera's zoom.
	Color node;                 ///< A node's colour.
	Color header;               ///< Its header's.
	Color label;                ///< Its text's.
	Color accent;               ///< The selection's and the rings'.
};

/// @brief One port, ready to draw.
struct PortLook {
	Vec2 at;                         ///< Its dot's centre, relative to the view.
	Color color;                     ///< Its kind's colour.
	bool ring = false;               ///< Whether it is ringed (a link may end there, or the keyboard is on it).
	bool output = false;             ///< Whether it is an output.
	const GraphPort* port = nullptr; ///< The port.
};

/// @brief Where a node's box is on screen.
/// @param camera The camera.
/// @param origin The view's top-left corner.
/// @param box The box, in graph units.
/// @return The box, in window units.
Rect screenBox(const GraphCamera& camera, Vec2 origin, const Rect& box) noexcept {
	const Vec2 topLeft = camera.toView({box.left, box.top});
	return {origin.x + topLeft.x, origin.y + topLeft.y, origin.x + topLeft.x + box.width() * camera.zoom,
	        origin.y + topLeft.y + box.height() * camera.zoom};
}

/// @brief Draws a node's body, its header and its title.
/// @param painter The painter.
/// @param area Its box, in window units.
/// @param node The node.
/// @param selected Whether it is selected.
/// @param look How nodes are drawn.
void paintNodeFrame(Painter& painter, const Rect& area, const GraphNode& node, bool selected, const NodeLook& look) {
	const float radius = 6.0f * look.zoom;
	painter.fillRoundedRect(area, CornerRadii::all(radius), look.node);
	painter.fillRoundedRect({area.left, area.top, area.right, area.top + headerHeight * look.zoom},
	                        {radius, radius, 0.0f, 0.0f}, look.header);
	if(selected) painter.strokeRoundedRect(area, CornerRadii::all(radius), 2.0f, look.accent);
	if(!look.text || look.zoom < bodyZoom) return;
	const float baseline = area.top + (headerHeight * look.zoom - look.text->lineHeight()) * 0.5f + look.text->ascent();
	painter.drawText(*look.text, codepointsOf(node.title.empty() ? node.id : node.title),
	                 {area.left + 8.0f * look.zoom, baseline}, look.label);
}

/// @brief Draws a node's ports: each one's ring, its dot and its label beside it, inside the node.
/// @param painter The painter.
/// @param origin The view's top-left corner.
/// @param ports The ports, inputs first.
/// @param look How nodes are drawn.
void paintPorts(Painter& painter, Vec2 origin, std::span<const PortLook> ports, const NodeLook& look) {
	const float r = portRadius * look.zoom;
	for(const PortLook& port : ports) {
		const Vec2 point{origin.x + port.at.x, origin.y + port.at.y};
		if(port.ring) {
			const float ring = r + 3.0f;
			painter.fillRoundedRect({point.x - ring, point.y - ring, point.x + ring, point.y + ring},
			                        CornerRadii::all(ring), look.accent);
		}
		painter.fillRoundedRect({point.x - r, point.y - r, point.x + r, point.y + r}, CornerRadii::all(r), port.color);
		if(!look.text || look.zoom < bodyZoom) continue;
		const std::u32string label = codepointsOf(port.port->label.empty() ? port.port->id : port.port->label);
		const float width = look.text->measure(label);
		const float x = port.output ? point.x - r - 6.0f - width : point.x + r + 6.0f;
		painter.drawText(*look.text, label, {x, point.y - look.text->lineHeight() * 0.5f + look.text->ascent()},
		                 look.label);
	}
}

/// @brief Draws the marquee being dragged.
/// @param painter The painter.
/// @param frame The view's frame.
/// @param a Where the drag started, relative to the view.
/// @param b Where the pointer is.
/// @param accent Its colour.
void paintMarquee(Painter& painter, const Rect& frame, Vec2 a, Vec2 b, Color accent) {
	const Rect area{frame.left + std::min(a.x, b.x), frame.top + std::min(a.y, b.y), frame.left + std::max(a.x, b.x),
	                frame.top + std::max(a.y, b.y)};
	painter.fillRect(area, accent.withAlpha(0.15f));
	painter.strokeRoundedRect(area, {}, 1.0f, accent);
}

/// @brief What the minimap shows.
struct MinimapScene {
	const GraphModel& model;           ///< The model.
	const std::vector<bool>& selected; ///< Which nodes are selected.
	bool moving = false;               ///< Whether the selected nodes are being dragged.
	Vec2 delta;                        ///< How far, in graph units.
	Rect bounds;                       ///< The graph's bounds, in graph units.
	Rect map;                          ///< The minimap's box, relative to the view.
	Rect seen;                         ///< The part of the graph on screen, in graph units.
	Color back;                        ///< The minimap's colour.
	Color node;                        ///< A node's.
	Color accent;                      ///< A selected node's and the part on screen's.
};

/// @brief Where a graph box lands on the minimap.
/// @param fit The minimap's fit.
/// @param origin The view's top-left corner.
/// @param box The box, in graph units.
/// @return The box, in window units.
Rect placed(const MinimapFit& fit, Vec2 origin, const Rect& box) noexcept {
	return {origin.x + fit.offset.x + box.left * fit.scale, origin.y + fit.offset.y + box.top * fit.scale,
	        origin.x + fit.offset.x + box.right * fit.scale, origin.y + fit.offset.y + box.bottom * fit.scale};
}

/// @brief Draws the minimap: every node as a box, where it is shown, and the part on screen.
/// @param painter The painter.
/// @param origin The view's top-left corner.
/// @param scene What it shows.
void paintMinimap(Painter& painter, Vec2 origin, const MinimapScene& scene) {
	const Rect& map = scene.map;
	painter.fillRoundedRect({origin.x + map.left, origin.y + map.top, origin.x + map.right, origin.y + map.bottom},
	                        CornerRadii::all(4.0f), scene.back);
	const MinimapFit fit = fitMinimap(scene.bounds, map);
	for(std::size_t i = 0; i < scene.model.nodes.size(); ++i) {
		const GraphNode& node = scene.model.nodes[i];
		const bool selected = flagged(scene.selected, i);
		const Vec2 at = scene.moving && selected ? node.position + scene.delta : node.position;
		painter.fillRect(placed(fit, origin, boxOf(node, at)), selected ? scene.accent : scene.node);
	}
	painter.strokeRoundedRect(placed(fit, origin, scene.seen), {}, 1.0f, scene.accent);
}

} // namespace

void GraphView::buildLinks() const {
	linksDirty_ = false;
	for(LinkMesh& mesh : linkMeshes_) mesh.quads.clear();
	const Vec2 size = resolvedSize();
	const Rect seen{0.0f, 0.0f, size.x, size.y};
	const StrokeStyle style{.width = linkWidth, .cap = LineCap::round};
	for(std::size_t i = 0; i < model_.links.size(); ++i) {
		if(i == replacing_ && drag_ == Drag::Link) continue; // drawn as the preview instead
		const std::optional<LinkEnds> ends = linkEnds(model_, byId_, model_.links[i]);
		if(!ends) continue;
		const Vec2 start = camera_.toView(portPoint(ends->from, ends->output, true));
		const Vec2 end = camera_.toView(portPoint(ends->to, ends->input, false));
		if(!overlaps(linkBounds(start, end, camera_.zoom), seen)) continue; // culled

		const Color color = kindColor(linkKind(model_, *ends));
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
	const Vec2 origin{f.left, f.top};
	paintGrid(painter, f, resolvedSize(), camera_, Color::mix(color(), textColor_, 0.06f));

	if(linksDirty_) buildLinks();
	for(const LinkMesh& mesh : linkMeshes_)
		if(!mesh.quads.empty()) painter.fillMesh(mesh.quads, origin, mesh.color);

	const NodeLook look{font(), camera_.zoom, nodeColor_, headerColor_, textColor_, accent_};
	const bool linking = drag_ == Drag::Link || keyLink_;
	const std::size_t keyed = hasFocus() ? keyNode_ : model_.nodes.size(); // the node whose port is ringed
	std::vector<PortLook> ports;
	for(std::size_t i = 0; i < model_.nodes.size(); ++i) {
		if(!flagged(visible_, i)) continue;
		const GraphNode& node = model_.nodes[i];
		ports.clear();
		for(std::size_t place = 0; place < node.inputs.size() + node.outputs.size(); ++place) {
			const KeyPort port = keyPortOf(node, i, place);
			// accepts() is asked only while linking, and in the order the ports are drawn
			const bool ring =
			    (linking && accepts({i, port.port, port.output, true})) || (i == keyed && place == keyPort_);
			ports.push_back({camera_.toView(portPoint(i, port.port, port.output)), kindColor(portOf(node, port).kind),
			                 ring, port.output, &portOf(node, port)});
		}
		paintNodeFrame(painter, screenBox(camera_, origin, nodeBox(i)), node, isSelected(i), look);
		paintPorts(painter, origin, ports, look);
	}
}

void GraphView::paintOver(Painter& painter) const {
	const Rect& f = frame();
	const Vec2 origin{f.left, f.top};

	// The link being drawn, from its port to the pointer (or the keyboard's port)
	if((drag_ == Drag::Link || keyLink_) && linkStart_.valid && linkStart_.node < model_.nodes.size()) {
		const Vec2 start = camera_.toView(portPoint(linkStart_.node, linkStart_.port, linkStart_.output));
		const KeyPort keyed = keyLink_ && keyNode_ < model_.nodes.size()
		                          ? keyPortOf(model_.nodes[keyNode_], keyNode_, keyPort_)
		                          : KeyPort{};
		const Vec2 end = keyed.valid ? camera_.toView(portPoint(keyed.node, keyed.port, keyed.output)) : pointer_;
		std::vector<MeshVertex> quads;
		const VectorPath path =
		    linkStart_.output ? linkPath(start, end, camera_.zoom) : linkPath(end, start, camera_.zoom);
		appendStroke(path.flatten({}, 0.25f), StrokeStyle{.width = linkWidth, .cap = LineCap::round}, quads);
		const GraphPort& port = portOf(model_.nodes[linkStart_.node],
		                               {linkStart_.node, linkStart_.port, linkStart_.output, linkStart_.valid});
		painter.fillMesh(quads, origin, kindColor(port.kind).withAlpha(0.8f));
	}

	if(drag_ == Drag::Marquee) paintMarquee(painter, f, pressed_, pointer_, accent_);

	if(const Rect map = minimapBox(); map.width() > 0.0f) {
		const Vec2 first = camera_.toGraph({0.0f, 0.0f}), last = camera_.toGraph(resolvedSize());
		paintMinimap(painter, origin,
		             {model_,
		              selected_,
		              drag_ == Drag::Nodes,
		              dragDelta_,
		              graphBounds(),
		              map,
		              {first.x, first.y, last.x, last.y},
		              nodeColor_.withAlpha(0.9f),
		              headerColor_,
		              accent_});
	}
	Panel::paintOver(painter);
}

} // namespace affresco
