/// @file
/// @brief The "graph" workloads: a 1,000-node graph (a grid of nodes, each linked to its neighbour) set and drawn, and
/// one of its nodes dragged by its header, a frame per pointer move, on a headless window (no display needed). A frame
/// is laid out and painted into a record, as the renderer would draw it, without OpenGL.

#include "Bench.hpp"
#include "Headless.hpp"
#include "PaintRecorder.hpp"

#include <affresco/render/GraphView.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

constexpr std::size_t nodeCount = 1'000; ///< Nodes in the graph.
constexpr std::size_t perRow = 40;       ///< Nodes in a row of the grid.
constexpr int dragMoves = 10;            ///< Pointer moves (frames) in one drag.

/// @brief A headless window and its clock, kept as long as the workloads that use them.
struct HeadlessGraph {
	detail::ManualClock clock;      ///< The window's clock.
	std::unique_ptr<Window> window; ///< 1280 by 800.

	/// @brief Opens the window.
	HeadlessGraph() : window(detail::createHeadlessWindow(1280, 800, clock)) {}
};

/// @brief A GraphView whose painting can be recorded, as a frame records it.
class RecordedGraph : public GraphView {
public:
	/// @brief Paints the view (under and over its bodies) into a record, emptied first.
	/// @param record The record.
	void record(detail::PaintRecord& record) const {
		record.clear();
		detail::PaintRecorder painter(record, bounds(), bounds());
		paint(painter);
		paintOver(painter);
	}
};

/// @brief The graph: a grid of nodes with an input and an output, each node's output linked to the next one's input.
/// @return The model.
GraphModel manyNodes() {
	GraphModel model;
	model.nodes.reserve(nodeCount);
	for(std::size_t i = 0; i < nodeCount; ++i) {
		GraphNode& node = model.nodes.emplace_back();
		node.id = "node " + std::to_string(i);
		node.position = {static_cast<float>(i % perRow) * 200.0f, static_cast<float>(i / perRow) * 100.0f};
		node.inputs.push_back({"in", "", "float", 0});
		node.outputs.push_back({"out", "", "float", 0});
		if(i > 0) model.links.push_back({model.nodes[i - 1].id, "out", node.id, "in"});
	}
	return model;
}

} // namespace

std::vector<Workload> graphWorkloads(const Context& context) {
	const auto headless = std::make_shared<HeadlessGraph>();
	auto& view = headless->window->root().add<RecordedGraph>();
	view.setSize(px(1280), px(800));
	view.setFont(Font::load(context.examples() / "shared" / "fonts" / "Roboto-Regular.ttf", 13.0f));
	const auto record = std::make_shared<detail::PaintRecord>();
	const auto model = std::make_shared<const GraphModel>(manyNodes());
	view.setModel(*model);
	headless->window->render();
	// The first node's header, on screen with the default camera
	const Vec2 header = view.camera().toView({30.0f, 10.0f});

	std::vector<Workload> workloads;
	workloads.push_back({.name = "graph/1000", .runs = 11, .step = [headless, &view, model, record] {
		                     view.setModel(*model);
		                     headless->window->render();
		                     view.record(*record);
	                     }});
	workloads.push_back(
	    {.name = "graph/1000/drag", .runs = 11, .operations = dragMoves, .step = [headless, &view, header, record] {
		     Window& window = *headless->window;
		     window.routeMouseButton({MouseButton::Left, Action::Press, {}, header});
		     for(int move = 1; move <= dragMoves; ++move) {
			     window.routeMouseMove({header.x + static_cast<float>(move) * 4.0f, header.y});
			     window.render();
			     view.record(*record);
		     }
		     window.routeMouseButton({MouseButton::Left, Action::Release, {}, header}); // back where it started
		     window.render();
	     }});
	return workloads;
}

} // namespace affresco::bench
