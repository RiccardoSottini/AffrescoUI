#pragma once

/// @file
/// @brief What the GraphView's sources share and keep out of its public header: the sizes, the point arithmetic,
/// and the questions asked of a model by GraphModel.cpp (the link rules), GraphView.cpp (the layout and the hit
/// tests), GraphInput.cpp (the pointer and the keys) and GraphPaint.cpp (the drawing).

#include <affresco/render/GraphView.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <vector>

namespace affresco::graph {

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
[[nodiscard]] inline float rowsOf(const GraphNode& node) noexcept {
	return static_cast<float>(std::max(node.inputs.size(), node.outputs.size()));
}

/// @brief The index of a port in a list.
/// @param ports The list.
/// @param id The port's id.
/// @return Its index, or ports.size() if there is none.
[[nodiscard]] inline std::size_t portIndex(const std::vector<GraphPort>& ports, std::string_view id) noexcept {
	for(std::size_t i = 0; i < ports.size(); ++i)
		if(ports[i].id == id) return i;
	return ports.size();
}

/// @brief A node's box at a position: its header, its rows of ports, its body and the room under them.
/// @param node The node.
/// @param at Where its top-left corner is, in graph units.
/// @return The box, in graph units.
[[nodiscard]] inline Rect boxOf(const GraphNode& node, Vec2 at) noexcept {
	const float height = headerHeight + rowsOf(node) * portRow + std::max(node.bodyHeight, 0.0f) + bottomPadding;
	return {at.x, at.y, at.x + node.width, at.y + height};
}

/// @brief Where a port's dot is on a node's box: inputs on its left edge, outputs on its right, one per row.
/// @param box The node's box, in graph units.
/// @param port The port's index among the inputs or the outputs.
/// @param output Whether it is an output.
/// @return The point, in graph units.
[[nodiscard]] inline Vec2 portPointIn(const Rect& box, std::size_t port, bool output) noexcept {
	return {output ? box.right : box.left, box.top + headerHeight + (static_cast<float>(port) + 0.5f) * portRow};
}

/// @brief A port by the indices GraphView's own port references hold, for the sources' free functions.
struct KeyPort {
	std::size_t node = 0; ///< The node's index.
	std::size_t port = 0; ///< The port's index among the inputs or the outputs.
	bool output = false;  ///< Whether it is an output.
	bool valid = false;   ///< Whether it names a port.
};

/// @brief The port the keyboard is on: its place counts the inputs first, then the outputs.
/// @param node The node the keyboard is on.
/// @param index That node's index.
/// @param place The port's place, from 0 to the node's inputs and outputs.
/// @return The port; not valid past the last one.
[[nodiscard]] inline KeyPort keyPortOf(const GraphNode& node, std::size_t index, std::size_t place) noexcept {
	if(place >= node.inputs.size() + node.outputs.size()) return {};
	const bool output = place >= node.inputs.size();
	return {index, output ? place - node.inputs.size() : place, output, true};
}

/// @brief A port of a node, by a KeyPort.
/// @param node The node.
/// @param port The port; valid, and on that node.
/// @return The port.
[[nodiscard]] inline const GraphPort& portOf(const GraphNode& node, const KeyPort& port) noexcept {
	return port.output ? node.outputs[port.port] : node.inputs[port.port];
}

/// @brief Reads one of GraphView's per-node flags, which may be shorter than the model.
/// @param flags The flags.
/// @param index The node's index.
/// @return Its flag, or false past the end.
[[nodiscard]] inline bool flagged(const std::vector<bool>& flags, std::size_t index) noexcept {
	return index < flags.size() && flags[index];
}

/// @brief How far a link's curve reaches out of its ports: half the distance across, at least 40 graph units.
/// @param start The output's point, in window units.
/// @param end The input's point.
/// @param zoom The camera's zoom.
/// @return The handles' length, in window units.
[[nodiscard]] inline float linkHandle(Vec2 start, Vec2 end, float zoom) noexcept {
	return std::max(std::abs(end.x - start.x) * 0.5f, 40.0f * zoom);
}

/// @brief Whether a point is on a shown box: one that is empty (the minimap hidden) holds none.
/// @param box The box.
/// @param point The point.
/// @return true if it is on it.
[[nodiscard]] inline bool onShown(const Rect& box, Vec2 point) noexcept {
	return box.contains(point) && box.width() > 0.0f;
}

/// @brief The indices of a model's nodes, sorted by id (the first of equal ids first), for findSorted().
/// @param model The model.
/// @return The indices.
[[nodiscard]] std::vector<std::size_t> sortedById(const GraphModel& model);

/// @brief Finds a node by id, as GraphModel::find() does, in sortedById()'s order.
/// @param model The model.
/// @param order Its sortedById().
/// @param id The node's id.
/// @return Its index, or model.nodes.size() if there is none.
[[nodiscard]] inline std::size_t findSorted(const GraphModel& model, const std::vector<std::size_t>& order,
                                            std::string_view id) noexcept {
	const auto at = std::ranges::lower_bound(
	    order, id, {}, [&](std::size_t index) { return std::string_view{model.nodes[index].id}; });
	return at != order.end() && model.nodes[*at].id == id ? *at : model.nodes.size();
}

/// @brief The nodes a link from or to a node would close a cycle with: those that reach it, or that it reaches.
/// @param model The model.
/// @param node The node.
/// @param reaching true for the nodes that reach it (a link from it to one of them closes a cycle), false for those
/// it reaches (a link to it from one of them does).
/// @param left A link left out (being replaced), or nullptr.
/// @return For each node, whether it is one; the node itself is.
[[nodiscard]] std::vector<bool> cycleNodes(const GraphModel& model, std::size_t node, bool reaching,
                                           const GraphLink* left);

/// @brief The scale and offset that fit a graph box into the minimap.
struct MinimapFit {
	float scale = 1.0f; ///< Minimap units per graph unit.
	Vec2 offset;        ///< Where the graph's origin lands, relative to the view.
};

/// @brief Fits a graph box into a minimap box, centred, keeping its proportions.
/// @param graph The graph box.
/// @param map The minimap box.
/// @return The fit.
[[nodiscard]] inline MinimapFit fitMinimap(const Rect& graph, const Rect& map) noexcept {
	const float scale =
	    std::min(map.width() / std::max(graph.width(), 1.0f), map.height() / std::max(graph.height(), 1.0f));
	const float left = map.left + (map.width() - graph.width() * scale) * 0.5f;
	const float top = map.top + (map.height() - graph.height() * scale) * 0.5f;
	return {scale, {left - graph.left * scale, top - graph.top * scale}};
}

} // namespace affresco::graph
