/// @file
/// @brief The format's reference (Reference.hpp): the JSON Schema and SCREEN_FORMAT.md's tables generated from the
/// registries, and the reference covering every key the registries have.

#include "Reference.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <stdexcept>
#include <string>

using affresco::json;
using namespace affresco::format;

namespace {

/// @brief A screen with only the built-in registries.
/// @return The screen.
affresco::Screen emptyScreen() {
	return affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 1, "root": {}})");
}

/// @brief Whether a reference has an entry of a name.
/// @param reference The reference.
/// @param name The name.
/// @return Whether it has.
bool hasEntry(std::span<const KeyReference> reference, std::string_view name) {
	return std::ranges::find(reference, name, &KeyReference::name) != reference.end();
}

/// @brief Checks that the keys a schema generates and those a reference lists are the same: every registered key has
/// its entry, and every entry in a table is a registered key (or stands for several, "…").
/// @param properties The generated schema's properties.
/// @param reference The reference.
void checkSameKeys(const json& properties, std::span<const KeyReference> reference) {
	for(const auto& [name, schema] : properties.items()) {
		INFO("a registered key without a reference entry: ", name);
		CHECK(hasEntry(reference, name));
	}
	for(const KeyReference& key : reference) {
		if(key.table.empty() || key.name.find("…") != std::string_view::npos) continue;
		INFO("a reference entry the registries don't have: ", std::string(key.name));
		CHECK(properties.contains(key.name));
	}
}

} // namespace

TEST_CASE("reference: every key the registries have is in the reference, and the reference has no other") {
	const json v1 = emptyScreen().schema()["$defs"]["v1"];
	checkSameKeys(v1["node"]["properties"], nodeKeys());
	checkSameKeys(v1["step"]["properties"], stepKeys());
	checkSameKeys(v1["screen"]["properties"], sectionKeys());
}

TEST_CASE("reference: every \"$ref\" in the generated schema names a definition") {
	const json schema = emptyScreen().schema();
	const json& v1 = schema["$defs"]["v1"];
	const std::string prefix = "#/$defs/v1/";
	std::vector<const json*> pending{&schema};
	while(!pending.empty()) {
		const json* value = pending.back();
		pending.pop_back();
		if(!value->is_structured()) continue;
		if(value->is_object() && value->contains("$ref")) {
			const std::string target = (*value)["$ref"].get<std::string>();
			INFO("$ref ", target);
			REQUIRE(target.starts_with(prefix));
			CHECK(v1.contains(target.substr(prefix.size())));
		}
		for(const json& child : *value) pending.push_back(&child);
	}
}

TEST_CASE("reference: SCREEN_FORMAT.md's tables are written between their markers, and bad markers are refused") {
	const affresco::Screen screen = emptyScreen();
	const std::string doc = "intro\n<!-- generated: steps -->\nold\n<!-- end generated -->\nafter\n";
	const std::string tables = screen.referenceTables(doc);
	CHECK(tables.starts_with("intro\n<!-- generated: steps -->\n| Step | Example |\n|---|---|\n| `set`"));
	CHECK(tables.ends_with("|\n<!-- end generated -->\nafter\n"));
	CHECK(screen.referenceTables(tables) == tables);
	CHECK(screen.referenceTables("no markers") == "no markers");
	CHECK_THROWS_AS((void)screen.referenceTables("<!-- generated: steps -->\nold\n"), std::invalid_argument);
	CHECK_THROWS_AS((void)screen.referenceTables("<!-- generated: none -->\n<!-- end generated -->"),
	                std::invalid_argument);
}
