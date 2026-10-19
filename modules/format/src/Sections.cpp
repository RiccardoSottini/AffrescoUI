/// @file
/// @brief format::SectionRegistry, and the built-in sections that aren't capabilities.

#include "SectionRegistry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace affresco::format {

namespace {

/// Format versions this library reads. A future version gets its own entry, and its differences branch on the
/// document's "version" where they apply.
constexpr int supportedVersions[] = {1};

/// @brief "format": "affrescoui-screen".
/// @param value The value.
/// @param name "format".
void checkFormat(const json& value, const std::string& name) {
	if(expect::string(value, name) != "affrescoui-screen") expect::fail(name, "expected \"affrescoui-screen\"");
}

/// @brief "version": one this library reads.
/// @param value The value.
/// @param name "version".
void checkVersion(const json& value, const std::string& name) {
	const int v = expect::integer(value, name);
	if(std::ranges::find(supportedVersions, v) == std::end(supportedVersions))
		expect::fail(name, "unsupported version " + std::to_string(v) + " (this library reads version 1)");
}

/// @brief A section that is an object of named entries ("actions", "styles", ...).
/// @param value The value.
/// @param name Its name.
void checkObject(const json& value, const std::string& name) { expect::object(value, name); }

/// @brief "components": named components, each with a "node" and optional "params".
/// @param value The value.
/// @param name "components".
void checkComponents(const json& value, const std::string& name) {
	for(const auto& [component, definition] : expect::object(value, name).items()) {
		const std::string at = name + "." + component;
		expect::object(definition, at);
		if(!definition.contains("node")) expect::fail(at, "a component needs a \"node\"");
		for(const auto& [key, item] : definition.items())
			if(key != "node" && key != "params" && key != "note")
				expect::fail(at + "." + key, "unknown property (expected node, params or note)");
	}
}

} // namespace

void SectionRegistry::add(SectionType section) {
	if(find(section.name))
		throw std::logic_error("AffrescoUI screen: the section '" + section.name + "' is registered twice");
	sections_.push_back(std::move(section));
}

const SectionType* SectionRegistry::find(std::string_view name) const {
	const auto it = std::ranges::find(sections_, name, &SectionType::name);
	return it == sections_.end() ? nullptr : &*it;
}

// Unknown sections first, then the missing ones, then each section's own check, in registration order
void SectionRegistry::validate(const json& document) const {
	expect::object(document, "");
	for(const auto& [key, value] : document.items()) {
		const SectionType* section = find(key);
		if(!section || !section->inDocument) expect::fail(key, "unknown section");
	}
	for(const SectionType& section : sections_)
		if(section.missing && !document.contains(section.name)) expect::fail("", section.missing);
	for(const SectionType& section : sections_)
		if(section.validate && document.contains(section.name)) section.validate(document[section.name], section.name);
}

std::shared_ptr<const SectionRegistry> builtInSections() {
	static const std::shared_ptr<const SectionRegistry> sections = [] {
		auto registry = std::make_shared<SectionRegistry>();
		registerBuiltInSections(*registry);
		return registry;
	}();
	return sections;
}

void registerBuiltInSections(SectionRegistry& registry) {
	registry.add({"$schema", nullptr, nullptr, false, false});
	registry.add({"format", checkFormat, nullptr, false, false});
	registry.add({"version", checkVersion, "missing \"version\" (this library reads version 1)", false, false});
	registry.add({"note", nullptr, nullptr, false, false});
	registry.add({"window"}); // checked by the runtime when it opens the window
	registry.add({"fonts"});  // checked by the runtime, which loads them
	registry.add({"colors"});
	registry.add({"theme"});
	registry.add({"styles", checkObject});
	registry.add({"components", checkComponents});
	registry.add({"templates", checkObject});
	registry.add({"screens", checkObject});
	registry.add({"state"});
	registry.add({"computed", checkObject});
	registry.add({"watch", checkObject});
	registry.add({"actions", checkObject});
	registry.add({"keys", checkObject});
	registry.add({"onLoad"});
	registry.add(channelsSection());
	registry.add(persistSection());
	registry.add({.name = "include", .inDocument = false}); // merged when the file is read (Screen)
	registry.add(permissionsSection());
	registry.add({"root", nullptr, "missing \"root\""});
}

} // namespace affresco::format
