/// @file
/// @brief Modello's budgets (examples/modello): a 200,000-triangle scene orbited, each frame drawn by the "scene"
/// surface without being presented (a hidden window: a display is needed, as for the "frame" workloads), and a click's
/// picking, a ray through a pixel tested against every mesh on the CPU.
///
/// Its own executable, affrescoui_perf_modello, which prints one JSON line per workload. With AFFRESCOUI_PERF_BUDGETS
/// set, a workload whose 99th percentile is over its budget fails the run: an orbited frame within 60 Hz (16.7 ms) and
/// a pick within 8 ms. The budgets are wall clock, so they aren't checked by default (busy runners).

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/scene3d/Camera.hpp>
#include <affresco/scene3d/Mesh.hpp>
#include <affresco/scene3d/Picking.hpp>
#include <affresco/scene3d/Renderer.hpp>
#include <affresco/scene3d/Scene.hpp>
#include <affresco/scene3d/SceneJson.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using affresco::json;
namespace scene3d = affresco::scene3d;

constexpr int samples = 200;                  ///< Operations one workload times.
constexpr int side = 7;                       ///< Spheres per row and column of the grid.
constexpr int width = 1280;                   ///< The window's width.
constexpr int height = 720;                   ///< The window's height.
constexpr double frameBudget = 1000.0 / 60.0; ///< An orbited frame, 99th percentile, in milliseconds.
constexpr double pickBudget = 8.0;            ///< A pick, 99th percentile, in milliseconds.

/// @brief A grid of side by side spheres of 4,096 triangles each (200,704 in all) sharing one mesh, under a sun.
/// @return The scene.
scene3d::Scene heavyScene() {
	scene3d::Scene scene;
	for(int row = 0; row < side; ++row)
		for(int column = 0; column < side; ++column) {
			scene3d::Node node;
			node.id = "sphere-" + std::to_string(row) + "-" + std::to_string(column);
			node.name = node.id;
			node.mesh = "bench:sphere";
			constexpr int middle = side / 2; // the grid's centre, a whole sphere
			node.transform.translation = {static_cast<float>(column - middle) * 2.5f, 1.0f,
			                              static_cast<float>(row - middle) * 2.5f};
			scene.nodes.push_back(std::move(node));
		}
	scene3d::Node sun;
	sun.id = "sun";
	sun.name = "Sun";
	sun.light = scene3d::Light{.intensity = 3.0f, .castsShadow = true};
	scene.nodes.push_back(std::move(sun));
	return scene;
}

/// @brief A window filled by the "scene" surface (the root's one child), with the camera in the state and an action
/// that moves it.
/// @param scene The scene.
/// @param camera Where the camera starts.
/// @return The screen document.
json benchDocument(const scene3d::Scene& scene, const scene3d::OrbitCamera& camera) {
	const json params = {{"scene", "{{state.scene}}"}, {"camera", "{{state.camera}}"}, {"grid", true}};
	return {{"format", "affrescoui-screen"},
	        {"version", 1},
	        {"window", {{"title", "Modello perf"}, {"width", width}, {"height", height}}},
	        {"state", {{"scene", scene3d::toJson(scene)}, {"camera", scene3d::toJson(camera)}}},
	        {"actions", {{"orbit", json::array({{{"set", "state.camera"}, {"to", "{{event.camera}}"}}})}}},
	        {"root",
	         {{"children", json::array({{{"type", "surface"},
	                                     {"id", "viewport"},
	                                     {"surface", "scene"},
	                                     {"width", "100%"},
	                                     {"height", "100%"},
	                                     {"params", params}}})}}}};
}

/// @brief The 99th percentile of some durations, in milliseconds.
/// @param durations The durations; reordered.
/// @return It.
double percentile99(std::vector<double>& durations) {
	const auto at = durations.begin() + static_cast<std::ptrdiff_t>(durations.size() * 99 / 100);
	std::ranges::nth_element(durations, at);
	return *at;
}

/// @brief Times operations one by one, prints their 99th percentile and checks it against a budget when budgets are on.
/// @param name The workload, for the line and the error.
/// @param budget The budget in milliseconds.
/// @param operation One operation.
/// @throws std::runtime_error If budgets are on and the 99th percentile is over the budget.
void timed(const std::string& name, double budget, const std::function<void()>& operation) {
	std::vector<double> durations(samples);
	for(double& duration : durations) {
		const auto start = std::chrono::steady_clock::now();
		operation();
		duration = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	}
	const double p99 = percentile99(durations);
	std::cout << json{{"name", name}, {"unit", "ms"}, {"p99", p99}, {"budget", budget}}.dump() << '\n';
	if(std::getenv("AFFRESCOUI_PERF_BUDGETS") != nullptr && p99 > budget)
		throw std::runtime_error(name + ": 99th percentile " + std::to_string(p99) + " ms, over its budget of " +
		                         std::to_string(budget) + " ms");
}

/// @brief Orbits the camera a step at a time, as a drag does, each step drawn.
/// @param app The windowing system.
/// @param assets The meshes.
/// @param scene The scene.
void orbiting(affresco::Application& app, const std::shared_ptr<scene3d::Assets>& assets, const scene3d::Scene& scene) {
	const auto probe = std::make_shared<scene3d::ViewportProbe>();
	scene3d::OrbitCamera camera;
	camera.distance = 24.0f;
	auto screen = affresco::Screen::fromString(benchDocument(scene, camera).dump());
	screen.registerSurface("scene", [assets, probe] { return scene3d::makeSceneRenderer(assets, probe); }, {true});
	screen.setSocketDirectory(std::filesystem::temp_directory_path() / "affrescoui-perf-modello-no-host");
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // never on screen
	affresco::Window& window = screen.createWindow(app);
	affresco::detail::renderWithoutPresenting(window); // the meshes go to the GPU once, before timing
	timed("modello/orbit-200k", frameBudget, [&] {
		camera.orbit(7.0f, 1.0f);
		screen.runAction("orbit", window.root(), json{{"camera", scene3d::toJson(camera)}}.dump());
		affresco::detail::renderWithoutPresenting(window);
	});
	if(probe->stats.triangles < 200'000)
		throw std::runtime_error("modello/orbit-200k: drew " + std::to_string(probe->stats.triangles) + " triangles");
}

/// @brief Picks through pixels spread over the view, as clicks do.
/// @param assets The meshes.
/// @param scene The scene.
void picking(const scene3d::Assets& assets, const scene3d::Scene& scene) {
	scene3d::OrbitCamera camera;
	camera.distance = 24.0f;
	int pixel = 0, hits = 0;
	timed("modello/pick-200k", pickBudget, [&] {
		pixel = (pixel + 7919) % (width * height); // a prime step: a new place every time
		const int column = pixel % width, line = pixel / width;
		const scene3d::Ray ray = camera.rayThrough(static_cast<float>(column), static_cast<float>(line),
		                                           static_cast<float>(width), static_cast<float>(height));
		if(scene3d::pickNode(scene, assets, ray)) ++hits;
	});
	if(hits == 0) throw std::runtime_error("modello/pick-200k: no ray hit a sphere");
}

} // namespace

/// @brief Runs both workloads.
/// @return 0, or 1 if one failed or was over its budget.
int main() {
	try {
		const auto assets = std::make_shared<scene3d::Assets>();
		assets->setMesh("bench:sphere", scene3d::makeSphere(64, 32));
		const scene3d::Scene scene = heavyScene();
		affresco::Application app;
		orbiting(app, assets, scene);
		picking(*assets, scene);
		return 0;
	} catch(const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
