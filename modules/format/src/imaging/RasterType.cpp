/// @file
/// @brief The "raster" node type: a RasterView showing an image the "images" section declares, the pointer playing
/// the current tool on it.

#include "NodeKeys.hpp"
#include "RasterView.hpp"
#include "TypeRegistry.hpp"

#include <memory>
#include <string>
#include <typeinfo>

namespace affresco::format {

namespace {

/// @brief Shows a raster node's image with its look.
/// @param panel The panel, a RasterView.
/// @param values Its image, checker, overlayColor and shadeColor, if given.
/// @param reader Reads its colours.
/// @param path The node's place, for errors.
/// @throws DocumentError If it names no image, or one the "images" section doesn't declare.
void applyRaster(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* view = dynamic_cast<RasterView*>(&panel);
	if(view == nullptr) return;
	if(!values.contains("image")) fail(path + ".image", "a raster node names an image of the \"images\" section");
	const std::string name = string(values["image"], path + ".image");
	RasterLook look;
	if(values.contains("checker")) look.checker = boolean(values["checker"], path + ".checker");
	if(values.contains("overlayColor")) look.overlay = reader.color(values["overlayColor"], path + ".overlayColor");
	if(values.contains("shadeColor")) look.shade = reader.color(values["shadeColor"], path + ".shadeColor");
	if(!view->show(name, look)) fail(path + ".image", "no image \"" + name + "\" in the \"images\" section");
	if(panel.accessibleRole().empty()) panel.setAccessibleRole("image");
	if(panel.accessibleLabel().empty()) panel.setAccessibleLabel(name); // unless the node names it
}

} // namespace

void registerRasterType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"image", ValueKind::Text},
	                   {"checker", ValueKind::Boolean},
	                   {"overlayColor", ValueKind::Color},
	                   {"shadeColor", ValueKind::Color}};
	set->apply = applyRaster;
	set->appliesTo = [](const Panel& panel) { return typeid(panel) == typeid(RasterView); };
	// The views hold the images weakly: the registry owns them, and a view may outlive it
	registry.add({"raster", "An image the screen edits, shown at its view; the pointer plays its current tool",
	              [images = std::weak_ptr(registry.imageDocuments())] { return std::make_unique<RasterView>(images); },
	              set, false});
}

} // namespace affresco::format
