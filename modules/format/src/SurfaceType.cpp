/// @file
/// @brief The "surface" node type: a SurfaceView showing a GPU surface the host registered (Screen::registerSurface).
/// A document only names one and gives it parameters: it never supplies shader source.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/GpuSurface.hpp"

#include <memory>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Shows a surface node's surface with its parameters.
/// @param surfaces The surfaces the host registered.
/// @param panel The panel, a SurfaceView.
/// @param values Its surface, params and continuous, if given.
/// @param path The node's place, for errors.
/// @throws DocumentError If it names no surface. One that isn't registered shows empty, with a note: the screen opens
/// in a host without the program that draws it (e.g. Modello's screen in affrescoui_host).
void applySurface(const SurfaceRegistry& surfaces, Panel& panel, const json& values, const std::string& path) {
	auto* view = dynamic_cast<SurfaceView*>(&panel);
	if(!view) return;
	if(!values.contains("surface")) fail(path + ".surface", "a surface node names a surface the host registered");
	const std::string name = string(values["surface"], path + ".surface");
	const auto found = surfaces.find(name);
	if(found == surfaces.end()) {
		log(LogLevel::Info, "AffrescoUI screen: " + path + ".surface: \"" + name +
		                        "\" is drawn by the program that registers it (Screen::registerSurface); empty here");
		return;
	}
	view->setParams(values.contains("params") ? object(values["params"], path + ".params") : json::object());
	view->setContinuous(values.contains("continuous") && boolean(values["continuous"], path + ".continuous"));
	view->setSurface(name, found->second.factory, found->second.options);
}

} // namespace

void registerSurfaceType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"surface", ValueKind::Text}, {"params", ValueKind::Any}, {"continuous", ValueKind::Boolean}};
	// When the values change: new params draw it again; the registry outlives the set, which it owns
	set->apply = [surfaces = registry.surfaces()](Panel& panel, const json& values, const PropertyReader&,
	                                              const std::string& path) {
		applySurface(*surfaces, panel, values, path);
	};
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const SurfaceView*>(&panel) != nullptr; };

	registry.add({"surface", "Native GPU drawing the host registered, with the node's parameters",
	              [] { return std::make_unique<SurfaceView>(); }, set, false});
}

} // namespace affresco::format
