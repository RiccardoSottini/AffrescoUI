/// @file
/// @brief The "canvas" workloads: a canvas of 100,000 items panned to a new place and recorded, on a headless window,
/// and the same with 1,000 items, so that a lock shows the cost follows what is in view, not what the canvas holds.

#include "Bench.hpp"
#include "paint/GeometryCache.hpp"
#include "Headless.hpp"

#include <affresco/render/CanvasView.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

constexpr float spacing = 20.0f; ///< World units from one item to the next, across and down.

/// @brief A headless window holding a canvas that fills it, kept as long as the workloads that use them.
struct HeadlessCanvas {
	detail::ManualClock clock;      ///< The window's clock.
	std::unique_ptr<Window> window; ///< 800 by 600.
	detail::GeometryCache cache;    ///< Records the panels' painting, as a renderer would, with no OpenGL.
	CanvasView* canvas = nullptr;   ///< The canvas, owned by the window.
	float extent = 0.0f;            ///< How far the items reach, across and down, in world units.

	/// @brief Opens the window and gives the canvas a square grid of items, each 10 units wide, with some text.
	/// @param count How many items.
	explicit HeadlessCanvas(std::size_t count) : window(detail::createHeadlessWindow(800, 600, clock)) {
		const auto columns = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
		std::vector<CanvasItem> items(count);
		for(std::size_t i = 0; i < count; ++i) {
			const std::size_t column = i % columns, row = i / columns;
			const float x = static_cast<float>(column) * spacing, y = static_cast<float>(row) * spacing;
			items[i].id = "item " + std::to_string(i);
			items[i].bounds = {x, y, x + 10.0f, y + 10.0f};
		}
		extent = static_cast<float>(columns) * spacing;
		canvas = &window->root().add<CanvasView>();
		canvas->setSize(px(800), px(600));
		canvas->setModel(std::make_shared<CanvasItemList>(std::move(items)));
		window->render();
		static_cast<void>(cache.refresh(detail::takeFrame(*window)));
	}
};

/// @brief A workload that pans a canvas to a new place each run, within its items where they reach past the view,
/// then lays out and records a frame (all but OpenGL).
/// @param name The workload's name.
/// @param count How many items the canvas holds.
/// @return The workload.
Workload panning(std::string name, std::size_t count) {
	const auto view = std::make_shared<HeadlessCanvas>(count);
	const auto next = std::make_shared<std::size_t>(0);
	return {.name = std::move(name), .runs = 21, .operations = 1, .step = [view, next] {
		        *next = (*next + 7919) % 100'003; // a prime stride: a new place every time
		        const float room = std::max(view->extent - 800.0f, 1.0f);
		        const float x = std::fmod(static_cast<float>(*next) * 3.7f, room);
		        const float y = std::fmod(static_cast<float>(*next) * 1.3f, std::max(view->extent - 600.0f, 1.0f));
		        view->canvas->setCamera({x, y, 1.0f});
		        view->window->render();
		        static_cast<void>(view->cache.refresh(detail::takeFrame(*view->window)));
	        }};
}

} // namespace

std::vector<Workload> canvasWorkloads(Context& /*context*/) {
	std::vector<Workload> workloads;
	workloads.push_back(panning("canvas/pan-100k", 100'000));
	workloads.push_back(panning("canvas/pan-1k", 1'000));
	return workloads;
}

} // namespace affresco::bench
