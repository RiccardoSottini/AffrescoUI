/// @file
/// @brief format::SectionRegistry, and the built-in sections that aren't capabilities.

#include "SectionRegistry.hpp"
#include "ContributionPoints.hpp"
#include "Contributions.hpp"
#include "FeatureManifest.hpp"
#include "Language.hpp"
#include "Settings.hpp"
#include "StateTypes.hpp"
#include "Strings.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <utility>

namespace affresco::format {

namespace {

/// Format versions this library reads: version 1 is migrated to version 2 when it is read (Migrate.hpp), and what it
/// wrote two ways is checked in its version 2 form.
constexpr std::array<int, 2> supportedVersions = {1, 2};

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
	if(std::ranges::find(supportedVersions, v) == supportedVersions.end())
		expect::fail(name, "unsupported version " + std::to_string(v) + " (this library reads versions 1 and 2)");
}

/// @brief A section that is an object of named entries ("actions", "styles", ...).
/// @param value The value.
/// @param name Its name.
void checkObject(const json& value, const std::string& name) { expect::object(value, name); }

/// @brief Checks "languages" by compiling them (LanguageCatalog::read()); the last valid one is remembered, as
/// each revision checks the whole document and languages seldom change.
/// @param value The section.
/// @param name Its name.
void checkLanguages(const json& value, const std::string& name) {
	static_cast<void>(name);
	thread_local json lastValid;
	if(value == lastValid) return;
	static_cast<void>(LanguageCatalog::read(value));
	lastValid = value;
}

/// @brief "components": named components, each with a "node" and optional "params".
/// @param value The value.
/// @param name "components".
void checkComponents(const json& value, const std::string& name) {
	for(const auto& [component, definition] : expect::object(value, name).items()) {
		const std::string at = std::format("{}.{}", name, component);
		expect::object(definition, at);
		if(!definition.contains("node")) expect::fail(at, "a component needs a \"node\"");
		for(const auto& [key, item] : definition.items())
			if(key != "node" && key != "params" && key != "note")
				expect::fail(std::format("{}.{}", at, key), "unknown property (expected node, params or note)");
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
	registry.add({"version", checkVersion, "missing \"version\" (this library reads versions 1 and 2)", false, false});
	registry.add({"note", nullptr, nullptr, false, false});
	registry.add({"window"}); // checked by the runtime when it opens the window
	registry.add({"fonts"});  // checked by the runtime, which loads them
	registry.add({"colors"});
	registry.add({"tokens"}); // checked by the runtime, which resolves them
	registry.add({"theme"});
	registry.add({"styles", checkObject});
	registry.add({"components", checkComponents});
	registry.add({"templates", checkObject});
	registry.add({"screens", checkObject});
	registry.add({"overlays", checkObject});     // laid over the window, above the root (Reconciler::childrenOf())
	registry.add({"languages", checkLanguages}); // code languages for editors (Language.hpp)
	registry.add({"state"});
	registry.add({"stateTypes", checkStateTypes}); // a type per state key (StateTypes.hpp)
	registry.add({"computed", checkObject});
	registry.add({"watch", checkObject});
	registry.add({"actions", checkObject});
	registry.add({"keys", checkObject});
	registry.add({"menus", checkMenus});       // Contributions.hpp
	registry.add({"settings", checkSettings}); // Settings.hpp
	registry.add({"strings", checkStrings});   // translations per locale (Strings.hpp)
	registry.add({"onLoad"});
	registry.add(channelsSection());
	registry.add(servicesSection());
	registry.add(connectionsSection());
	registry.add(persistSection());
	registry.add({.name = "include", .inDocument = false}); // merged when the file is read (Screen)
	registry.add(permissionsSection());
	registry.add(imagesSection());
	registry.add(audioSection());
	registry.add({"features", checkFeaturesSection}); // installed features, merged by Composition (FeatureManifest.hpp)
	registry.add({"exports", checkExportsSection});   // what features read as app.<name>
	registry.add({"imports", checkImportsSection});   // aliases for features' components
	registry.add({.name = "points", .validate = checkPointsSection, .capability = true}); // ContributionPoints.hpp
	registry.add({"root", nullptr, "missing \"root\""});
}

} // namespace affresco::format
