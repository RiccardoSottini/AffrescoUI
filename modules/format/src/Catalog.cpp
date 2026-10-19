/// @file
/// @brief format::catalog(): the catalog built from the registries.

#include "Catalog.hpp"
#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief The names of a map's entries, in its order.
/// @param map The map.
/// @return The names.
json names(const auto& map) {
	json list = json::array();
	for(const auto& [name, value] : map) list.push_back(name);
	return list;
}

/// @brief The property names of a type, in catalog order.
/// @param type The type.
/// @return The names.
json propertyNames(const NodeType& type) {
	json list = json::array();
	for(const PropertySpec& property : type.own->properties) list.push_back(property.name);
	return list;
}

/// @brief The node types, with their properties and what they are.
/// @param registry The types.
/// @return {name: {"properties", "about"}}.
json typesOf(const TypeRegistry& registry) {
	json types = json::object();
	for(const NodeType* type : registry.inOrder())
		types[type->name] = {{"properties", propertyNames(*type)}, {"about", type->about}};
	return types;
}

/// @brief The steps the catalog lists under "steps", and the control steps, in registration order.
/// @param registry The steps.
/// @return {"steps", "controlSteps"}.
json stepsOf(const StepRegistry& registry) {
	json steps = json::array(), control = json::array();
	for(const StepGroup& group : registry.groups()) {
		for(const StepType& step : group.steps) {
			if(step.listed) steps.push_back(step.name);
			if(step.control) control.push_back(step.name);
		}
	}
	return {{"steps", steps}, {"controlSteps", control}};
}

} // namespace

json catalog(const Engine& engine) {
	json events = json::array();
	for(std::string_view name : handlerNames) events.push_back(std::string(name));

	json sections = json::array();
	for(const SectionType& section : engine.sections->sections())
		if(section.listed) sections.push_back(section.name);

	json customTypes = json::array(); // by name
	for(const auto& [name, type] : engine.types.types())
		if(type.application) customTypes.push_back(name);

	const NodeType* panel = engine.types.find("panel");
	json steps = stepsOf(*engine.steps);
	return {{"types", typesOf(engine.types)},
	        {"common", panel ? propertyNames(*panel) : json::array()},
	        {"events", events},
	        {"steps", std::move(steps["steps"])},
	        {"controlSteps", std::move(steps["controlSteps"])},
	        {"operators", operatorNames()},
	        {"variables",
	         {"state", "event", "window", "revision", "proposals", "history", "preview", "mode", "computed", "theme",
	          "screen", "nodes"}},
	        {"sections", sections},
	        {"customTypes", customTypes},
	        {"nativeActions", names(engine.nativeActions)},
	        {"nativeCalls", names(engine.nativeCalls)}};
}

} // namespace affresco::format
