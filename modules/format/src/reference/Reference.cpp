/// @file
/// @brief format::schema(): the JSON Schema of the format, generated from the registries and the reference.

#include "Reference.hpp"
#include "Engine.hpp"
#include "NodeKeys.hpp"

#include <algorithm>

namespace affresco::format {

namespace {

/// @brief Adds a name to a list unless it is there.
/// @param names The list.
/// @param name The name.
void addOnce(std::vector<std::string>& names, std::string_view name) {
	if(std::ranges::find(names, name) == names.end()) names.emplace_back(name);
}

/// @brief The schema an entry gives.
/// @param text A "$defs/v2" name, the JSON of a schema, or "" for {}.
/// @return The schema.
json schemaOf(std::string_view text) {
	if(text.empty()) return json::object();
	if(text.front() == '{') return json::parse(text);
	return {{"$ref", concatenate("#/$defs/v2/", text, "")}};
}

/// @brief The schemas of a list of keys: each from its entries in the reference, {} for one it lacks. Two node types
/// can give one key different meanings (an editor's rulers are columns, a canvas's a flag): the key then takes any of
/// them, since the flat schema doesn't know the node's type.
/// @param names The keys, in order.
/// @param reference The reference.
/// @return {key: schema}.
json schemasOf(const std::vector<std::string>& names, std::span<const KeyReference> reference) {
	json properties = json::object();
	for(const std::string& name : names) {
		json schemas = json::array();
		for(const KeyReference& entry : reference)
			if(entry.name == name)
				if(json schema = schemaOf(entry.schema); // an entry without one says nothing about the key's values
				   !schema.empty() && std::ranges::find(schemas, schema) == schemas.end())
					schemas.push_back(std::move(schema));
		if(schemas.empty()) properties[name] = json::object();
		else if(schemas.size() == 1) properties[name] = std::move(schemas[0]);
		else properties[name] = {{"anyOf", std::move(schemas)}};
	}
	return properties;
}

/// @brief Orders keys as the reference lists them, those it lacks after.
/// @param names The keys.
/// @param reference The reference.
/// @return The keys.
std::vector<std::string> inReferenceOrder(const std::vector<std::string>& names,
                                          std::span<const KeyReference> reference) {
	std::vector<std::string> ordered;
	for(const KeyReference& key : reference)
		if(std::ranges::find(names, key.name) != names.end()) addOnce(ordered, key.name);
	for(const std::string& name : names) addOnce(ordered, name);
	return ordered;
}

/// @brief The names of the node types.
/// @param registry The types.
/// @param application Whether to list the application's types too, or only the built-in ones.
/// @return The names, in the order first added.
json typeNames(const TypeRegistry& registry, bool application) {
	json names = json::array();
	for(const NodeType* type : registry.inOrder())
		if(application || !type->application) names.push_back(type->name);
	return names;
}

/// @brief The schema of "type": the names of the types, or another a later application registers.
/// @param registry The types.
/// @return The schema.
json typeSchema(const TypeRegistry& registry) {
	const json names = typeNames(registry, true);
	std::string about;
	for(const json& name : names) about += name.get<std::string>() + (about.empty() ? " (default), " : ", ");
	about += "or a type the application registers (which may add properties of its own)";
	return {{"type", "string"}, {"description", about}, {"anyOf", {{{"enum", names}}, {{"type", "string"}}}}};
}

/// @brief The schema of a node: every key the types have, checked; a type the schema doesn't know takes any.
/// @param engine The engine.
/// @return The schema.
json nodeSchema(const Engine& engine) {
	json properties = schemasOf(inReferenceOrder(nodeKeyNames(engine), nodeKeys()), nodeKeys());
	properties["type"] = typeSchema(engine.types);
	const json other = {{"required", {"type"}},
	                    {"properties", {{"type", {{"not", {{"enum", typeNames(engine.types, true)}}}}}}}};
	return {{"type", "object"},
	        {"properties", std::move(properties)},
	        {"if", other},
	        {"then",
	         {{"description", "A type the application registers: its own properties are checked by the application"},
	          {"additionalProperties", true}}},
	        {"unevaluatedProperties", false}};
}

/// @brief What a step's description says of each step: its name and the keys it reads, the review steps last.
/// @param registry The steps.
/// @return E.g. "set(+to), toggle, …; review: accept(+head), …".
std::string stepsAbout(const StepRegistry& registry) {
	std::string listed, review;
	for(const StepGroup& group : registry.groups()) {
		for(const StepType& step : group.steps) {
			std::string& text = step.listed ? listed : review;
			text += (text.empty() ? "" : ", ") + step.name;
			for(std::size_t i = 0; i < step.keys.size(); ++i) text += (i == 0 ? "(+" : ", ") + step.keys[i];
			if(!step.keys.empty()) text += ')';
		}
	}
	return review.empty() ? listed : listed + "; review: " + review;
}

/// @brief The schema of a step: one of the registered steps, and the keys it reads.
/// @param engine The engine.
/// @return The schema.
json stepSchema(const Engine& engine) {
	return {{"type", "object"},
	        {"minProperties", 1},
	        {"additionalProperties", false},
	        {"description", stepsAbout(*engine.steps)},
	        {"properties", schemasOf(stepKeyNames(engine), stepKeys())}};
}

/// @brief The schema of the document of version 2: its sections, those a document must have required.
/// @param engine The engine.
/// @return The schema.
json screenSchema(const Engine& engine) {
	json required = json::array();
	for(const SectionType& section : engine.sections->sections())
		if(section.missing) required.push_back(section.name);
	return {{"description", "Version 2 of the format"},
	        {"type", "object"},
	        {"required", std::move(required)},
	        {"additionalProperties", false},
	        {"properties", schemasOf(sectionNames(engine), sectionKeys())}};
}

} // namespace

std::vector<std::string> nodeKeyNames(const Engine& engine) {
	std::vector<std::string> names{"type"};
	for(const NodeType* type : engine.types.inOrder())
		for(const PropertySpec& property : type->own->properties) addOnce(names, property.name);
	for(const auto& set : engine.types.inherited())
		for(const PropertySpec& property : set->properties) addOnce(names, property.name);
	for(const std::string_view name : handlerNames) addOnce(names, name);
	addOnce(names, "slot");  // a component's placeholder for a use's children (Builder.cpp)
	addOnce(names, "point"); // a contribution point features add views to (Composition.cpp)
	return names;
}

std::vector<std::string> stepKeyNames(const Engine& engine) {
	std::vector<std::string> names, keys;
	for(const StepGroup& group : engine.steps->groups()) {
		for(const StepType& step : group.steps) {
			addOnce(names, step.name);
			for(const std::string& key : step.keys) addOnce(keys, key);
		}
	}
	for(const std::string& key : keys) addOnce(names, key);
	return names;
}

std::vector<std::string> sectionNames(const Engine& engine) {
	std::vector<std::string> names;
	for(const SectionType& section : engine.sections->sections()) names.push_back(section.name);
	return names;
}

json schema(const Engine& engine) {
	json defs = {{"screen", screenSchema(engine)},
	             {"node", nodeSchema(engine)},
	             {"step", stepSchema(engine)},
	             {"expression",
	              {{"type", "object"},
	               {"minProperties", 1},
	               {"maxProperties", 1},
	               {"propertyNames", {{"enum", operatorNames()}}}}}};
	for(const KeyReference& definition : definitions())
		defs[std::string(definition.name)] = schemaOf(definition.schema);
	return {{"$schema", "https://json-schema.org/draft/2020-12/schema"},
	        {"$id", "https://github.com/RiccardoSottini/AffrescoUI/schema/affrescoui-screen.schema.json"},
	        {"title", "AffrescoUI Screen Format"},
	        {"description", "A user interface for the AffrescoUI library. Specification: SCREEN_FORMAT.md. Each format "
	                        "version has its definitions under $defs/v<N>; \"version\" selects one."},
	        {"type", "object"},
	        {"required", {"version"}},
	        {"properties",
	         {{"version", {{"enum", {1, 2}}, {"description", "Format version. This library reads: 1 (by migrating it), 2"}}}}},
	        {"allOf",
	         {{{"if", {{"properties", {{"version", {{"const", 1}}}}}, {"required", {"version"}}}},
	           {"then", {{"$ref", "#/$defs/v1/screen"}}}},
	          {{"if", {{"properties", {{"version", {{"const", 2}}}}}, {"required", {"version"}}}},
	           {"then", {{"$ref", "#/$defs/v2/screen"}}}}}},
	        // Version 1 is only read by migrating it (Migrate.hpp): `affrescoui migrate` first, then the schema checks it
	        {"$defs",
	         {{"v1",
	           {{"screen",
	             {{"description", "Version 1 of the format: read by migrating it to version 2 (`affrescoui migrate`), "
	                              "which this schema then checks"},
	              {"type", "object"}}}}},
	          {"v2", std::move(defs)}}}};
}

} // namespace affresco::format
