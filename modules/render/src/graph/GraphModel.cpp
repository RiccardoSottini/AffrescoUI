/// @file
/// @brief The graph's rules, free of any view: finding nodes, the link rules and cycles, and the camera.

#include "graph/GraphShared.hpp"

#include <cmath>
#include <deque>
#include <numeric>

namespace affresco {

using graph::cycleNodes;
using graph::findSorted;
using graph::portIndex;
using graph::sortedById;

namespace {

/// @brief A link's two ports, found in a model.
struct LinkPorts {
	std::size_t from = 0;              ///< The index of the node it starts at.
	std::size_t to = 0;                ///< The index of the node it ends at.
	const GraphPort* output = nullptr; ///< The port it starts at, or nullptr.
	const GraphPort* input = nullptr;  ///< The port it ends at, or nullptr.
};

/// @brief Finds a link's ports.
/// @param model The model.
/// @param link The link.
/// @return Both, or nullptrs if a node or a port isn't there.
LinkPorts linkPorts(const GraphModel& model, const GraphLink& link) noexcept {
	const std::size_t from = model.find(link.from), to = model.find(link.to);
	if(from == model.nodes.size() || to == model.nodes.size()) return {};
	const std::vector<GraphPort>& outputs = model.nodes[from].outputs;
	const std::vector<GraphPort>& inputs = model.nodes[to].inputs;
	const std::size_t output = portIndex(outputs, link.output), input = portIndex(inputs, link.input);
	if(output == outputs.size() || input == inputs.size()) return {};
	return {from, to, &outputs[output], &inputs[input]};
}

/// @brief How many links a new link's ports have already, without the one it replaces.
struct PortLoad {
	std::size_t from = 0;   ///< At its output.
	std::size_t to = 0;     ///< At its input.
	bool duplicate = false; ///< Whether an equal link stays.
};

/// @brief Counts the links that stay at a new link's ports: all but the one being replaced (the first equal to it).
/// @param links The model's links.
/// @param link The new link.
/// @param replacing The link it replaces, or nullptr.
/// @return The counts; they stop at a duplicate.
PortLoad portLoad(const std::vector<GraphLink>& links, const GraphLink& link, const GraphLink* replacing) noexcept {
	bool skipped = replacing == nullptr;
	PortLoad load;
	for(const GraphLink& other : links) {
		if(!skipped && other == *replacing) {
			skipped = true;
			continue;
		}
		if(other == link) {
			load.duplicate = true;
			return load;
		}
		if(other.from == link.from && other.output == link.output) ++load.from;
		if(other.to == link.to && other.input == link.input) ++load.to;
	}
	return load;
}

/// @brief Each node's neighbours along the links, by index.
/// @param model The model.
/// @param reaching true to follow the links backwards (to the nodes that reach each one).
/// @param left A link left out, or nullptr.
/// @return For each node, the nodes one link away.
std::vector<std::vector<std::size_t>> neighbours(const GraphModel& model, bool reaching, const GraphLink* left) {
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
	return next;
}

} // namespace

std::vector<std::size_t> graph::sortedById(const GraphModel& model) {
	std::vector<std::size_t> order(model.nodes.size());
	// NOLINTNEXTLINE(modernize-use-ranges): std::ranges::iota is missing from some supported standard libraries
	std::iota(order.begin(), order.end(), std::size_t{0});
	std::ranges::stable_sort(order,
	                         [&](std::size_t a, std::size_t b) { return model.nodes[a].id < model.nodes[b].id; });
	return order;
}

std::vector<bool> graph::cycleNodes(const GraphModel& model, std::size_t node, bool reaching, const GraphLink* left) {
	const std::vector<std::vector<std::size_t>> next = neighbours(model, reaching, left);
	std::vector<bool> found(model.nodes.size(), false);
	if(node >= found.size()) return found;
	// Breadth first, so a long chain costs a queue rather than the stack
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

std::size_t GraphModel::find(std::string_view id) const noexcept {
	for(std::size_t i = 0; i < nodes.size(); ++i)
		if(nodes[i].id == id) return i;
	return nodes.size();
}

bool kindsMatch(std::string_view output, std::string_view input) noexcept {
	return output == input || output == "any" || input == "any";
}

LinkVerdict checkLink(const GraphModel& model, const GraphLink& link, bool cyclesAllowed, const GraphLink* replacing) {
	const LinkPorts ports = linkPorts(model, link);
	const GraphPort *out = ports.output, *in = ports.input;
	if(!out || !in) return LinkVerdict::UnknownPort;
	if(!kindsMatch(out->kind, in->kind)) return LinkVerdict::KindMismatch;
	const PortLoad load = portLoad(model.links, link, replacing);
	if(load.duplicate) return LinkVerdict::Duplicate;
	if((out->links != 0 && load.from >= out->links) || (in->links != 0 && load.to >= in->links))
		return LinkVerdict::Full;
	// A cycle if the link's start can be reached from its end (or is its end)
	if(!cyclesAllowed && cycleNodes(model, ports.to, false, replacing)[ports.from]) return LinkVerdict::Cycle;
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

} // namespace affresco
