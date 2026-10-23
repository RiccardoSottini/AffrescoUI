#pragma once

/// @file
/// @brief format::SectionRegistry: the top-level sections a screen document can have, how each is checked, and which
/// grant capabilities.

#include "affresco/core/Json.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief Checks a section's value.
/// @param value The value.
/// @param name The section's name, which is its place for errors.
/// @throws DocumentError If it is malformed; what() names the place.
using SectionValidator = void (*)(const json& value, const std::string& name);

/// @brief One top-level section.
struct SectionType {
	std::string name;                    ///< Its key in the document.
	SectionValidator validate = nullptr; ///< Checks its value; null: anything goes (or the runtime checks it).
	/// Whether a document must have it ("version", "root"); the error then says what is missing.
	const char* missing = nullptr;
	bool capability =
	    false;          ///< It grants a capability (programs to talk to, files to write): a revision can't change it.
	bool listed = true; ///< Whether the catalog lists it under "sections".
	bool inDocument = true; ///< false for "include": merged when the file is read, so never in a live document.
};

/// @brief The sections a screen document can have, in the order they are checked and listed.
///
/// Invariants: a name is registered once. builtInSections() fills one with registerBuiltInSections(), the definitions
/// coming from their own files, and shares it read-only. Callers: format::Rules (validate(), the capabilities in
/// checkChange()) and the runtime's catalog (sections()).
class SectionRegistry {
public:
	/// @brief Adds a section after the others.
	/// @param section The section.
	/// @throws std::logic_error If one of that name is registered.
	void add(SectionType section);
	/// @brief Checks a document's top-level sections: none unknown, the required ones there, each valid.
	/// @param document The document.
	/// @throws DocumentError If something is wrong; what() names the place.
	void validate(const json& document) const;
	/// @brief Finds a section.
	/// @param name Its name.
	/// @return The section, or nullptr.
	[[nodiscard]] const SectionType* find(std::string_view name) const;
	/// @brief Every section, in registration order.
	[[nodiscard]] const std::vector<SectionType>& sections() const noexcept { return sections_; }

private:
	std::vector<SectionType> sections_; ///< See sections().
};

/// @brief Registers the built-in sections, in the order the catalog lists them (Sections.cpp).
/// @param registry The registry.
void registerBuiltInSections(SectionRegistry& registry);

/// @brief The built-in sections, built once and shared: they never change, so every Rules and runtime reads the same.
/// @return The registry.
[[nodiscard]] std::shared_ptr<const SectionRegistry> builtInSections();

/// @brief "channels": programs the screen talks to over local sockets; a capability (CapabilitySections.cpp).
/// @return The section.
[[nodiscard]] SectionType channelsSection();
/// @brief "services": programs and folders the screen calls by method name; a capability (CapabilitySections.cpp).
/// @return The section.
[[nodiscard]] SectionType servicesSection();
/// @brief "connections": servers the screen talks to over HTTP and WebSocket, and what it may ask them; a capability
/// (ConnectionSpec.cpp).
/// @return The section.
[[nodiscard]] SectionType connectionsSection();
/// @brief "persist": state paths kept on disk; a capability (CapabilitySections.cpp).
/// @return The section.
[[nodiscard]] SectionType persistSection();
/// @brief "permissions": who may change what, and who reviews; a capability (CapabilitySections.cpp).
/// @return The section.
[[nodiscard]] SectionType permissionsSection();

} // namespace affresco::format
