/// @file
/// @brief The shipped kits and themes (modules/themes) and the lookup the format reads them through (core's
/// ComponentLibrary): every component well formed, the tokens' variants, a library swapped in, a document's own
/// component winning, and "runsActions" declared by any component.

#include "Checker.hpp"
#include "Library.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;

namespace {

/// @brief A library of one component, "greeting", and one token, "brand.accent", standing for another theme.
class OneComponentLibrary final : public affresco::ComponentLibrary {
public:
	/// @copydoc affresco::ComponentLibrary::component
	[[nodiscard]] const json* component(std::string_view name) const override {
		return name == "greeting" ? &components_["greeting"] : nullptr;
	}
	/// @copydoc affresco::ComponentLibrary::components
	[[nodiscard]] const json& components() const override { return components_; }
	/// @copydoc affresco::ComponentLibrary::tokens
	[[nodiscard]] const json& tokens() const override { return tokens_; }
	/// @copydoc affresco::ComponentLibrary::tokenDefaults
	[[nodiscard]] const json& tokenDefaults(bool themed) const override { return themed ? themed_ : plain_; }

private:
	/// Name → definition.
	json components_ = json::parse(R"({"greeting": {"params": {"name": "you"},
		"node": {"type": "label", "text": "Hello, {{name}}"}}})");
	/// Name → description.
	json tokens_ = json::parse(R"({"brand.accent": {"dark": "#ff8800", "light": "#cc6600", "about": "Accents"}})");
	json plain_ = json::parse(R"({"brand.accent": "#ff8800"})");                                ///< Base values.
	json themed_ = json::parse(R"({"brand.accent": {"dark": "#ff8800", "light": "#cc6600"}})"); ///< Both variants.
};

/// @brief A screen whose root holds one node.
/// @param node The node.
/// @return The document.
json screenWith(const json& node) {
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"root", {{"type", "panel"}, {"id", "main"}, {"children", json::array({node})}}}};
}

/// @brief Whether the format accepts a document: its rules, and its checker without a finding.
/// @param document The document.
/// @return true if it does.
bool valid(const json& document) {
	try {
		affresco::format::Rules().validate(document);
	} catch(const affresco::DocumentError&) {
		return false;
	}
	const auto findings = affresco::format::check(document, json::object());
	return findings && findings->empty();
}

/// @brief Installs a library for one test, and the shipped one again after it.
struct InstalledFor {
	/// @brief Installs it.
	/// @param library The library (nullptr: none).
	explicit InstalledFor(const affresco::ComponentLibrary* library) { affresco::installComponentLibrary(library); }
	/// @brief Installs the shipped kits and themes again.
	~InstalledFor() { affresco::themes::install(); }
	InstalledFor(const InstalledFor&) = delete;            ///< Not copied.
	InstalledFor& operator=(const InstalledFor&) = delete; ///< Not assigned.
};

} // namespace

TEST_CASE("themes: every shipped component has a node, and its runsActions names one of its parameters or is true") {
	const json& components = affresco::themes::library().components();
	CHECK(components.size() >= 27);
	for(const auto& [name, definition] : components.items()) {
		CAPTURE(name);
		REQUIRE(definition.is_object());
		CHECK(definition.contains("node"));
		const json params = definition.value("params", json::object());
		if(const auto runs = definition.find("runsActions"); runs != definition.end())
			CHECK((runs->is_boolean() || (runs->is_string() && params.contains(runs->get<std::string>()))));
		CHECK(affresco::themes::library().component(name) == &definition);
	}
	CHECK(affresco::themes::library().component("noSuchComponent") == nullptr);
}

TEST_CASE("themes: each token has a dark and a light colour, or a size; the base values are the dark ones") {
	const auto& library = affresco::themes::library();
	CHECK(library.tokens().contains("kit.surface"));
	for(const auto& [name, token] : library.tokens().items()) {
		CAPTURE(name);
		if(token.contains("dark")) {
			CHECK(library.tokenDefaults(false)[name] == token["dark"]);
			CHECK(library.tokenDefaults(true)[name]["light"].is_string());
		} else {
			CHECK(token["value"].is_number());
			CHECK(library.tokenDefaults(true)[name] == token["value"]);
		}
	}
}

TEST_CASE("themes: a document uses the installed library's components and tokens, and only those") {
	const json greeting = {{"use", "greeting"}, {"with", {{"name", "Ada"}}}};
	const json toolbar = {{"use", "toolbar"}, {"with", {{"items", json::array()}}}};
	CHECK(valid(screenWith(toolbar)));
	CHECK_FALSE(valid(screenWith(greeting)));
	{
		const OneComponentLibrary other;
		const InstalledFor installed(&other);
		CHECK(valid(screenWith(greeting)));
		CHECK_FALSE(valid(screenWith(toolbar)));
		CHECK(affresco::format::isLibraryToken("brand.accent"));
		CHECK_FALSE(affresco::format::isLibraryToken("kit.surface"));
		CHECK(affresco::format::followsLibraryThemes(json{{"tokens", {{"brand.panel", "#000000"}}}}));
		CHECK_FALSE(affresco::format::followsLibraryThemes(json{{"tokens", {{"kit.surface", "#000000"}}}}));
		CHECK(affresco::format::libraryTokenTable().find("| `brand.accent` | `#ff8800` | `#cc6600` | Accents |") !=
		      std::string::npos);
	}
	{
		const InstalledFor none(nullptr);
		CHECK_FALSE(valid(screenWith(toolbar)));
		CHECK(affresco::format::libraryTokens(true).empty());
	}
	CHECK(valid(screenWith(toolbar)));
}

TEST_CASE("themes: a document's own component wins over the library's of its name") {
	const json use = {{"use", "toolbar"}, {"with", {{"items", json::array()}}}};
	json document = screenWith(use);
	document["components"] = {
	    {"toolbar",
		 {{"params", {{"items", json::array()}}}, {"node", {{"type", "label"}, {"text", "{{state.missing}}"}}}}}};
	CHECK_FALSE(valid(document)); // its own body is checked, and reads what the state doesn't have
	CHECK(valid(screenWith(use)));
}

TEST_CASE("themes: runsActions is the name of a parameter or true") {
	json document = screenWith({{"type", "label"}, {"text", "Hi"}});
	document["components"] = {
	    {"bar", {{"params", {{"items", json::array()}}}, {"runsActions", "items"}, {"node", {{"type", "panel"}}}}}};
	CHECK(valid(document));
	document["components"]["bar"]["runsActions"] = true;
	CHECK(valid(document));
	document["components"]["bar"]["runsActions"] = 3;
	CHECK_FALSE(valid(document));
}
