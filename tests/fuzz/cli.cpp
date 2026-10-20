/// @file
/// @brief Fuzzes what the affrescoui tool reads from a screen file (tools/affrescoui), which people, agents and CI run
/// on files nobody reviewed: the text reader and printer behind `fmt` and `ids`, the places `ids` writes ids at, and
/// the schema check behind `check`.
///
/// Any text either isn't JSON (parseText throws std::runtime_error, which the tool reports) or: printText writes it
/// as the same JSON value, and formatting that again changes nothing; and the schema check reports problems, never
/// throws.

#include "../../tools/affrescoui/JsonText.hpp" // by their paths: the tool's sources, not a library's
#include "../../tools/affrescoui/Schema.hpp"
#include "Fuzz.hpp"

#include <affresco/format/NodeIds.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

using affresco::json;

namespace {

/// @brief The screen format's schema, read once from AFFRESCOUI_FUZZ_SCHEMA (CMakeLists.txt), as `check` builds it in.
/// @return The schema, ready to check with.
affresco::cli::Schema& screenSchema() {
	static affresco::cli::Schema schema = [] {
		std::ifstream file(AFFRESCOUI_FUZZ_SCHEMA);
		if(!file) affresco::fuzz::broken("the schema " AFFRESCOUI_FUZZ_SCHEMA " can be read");
		std::stringstream content;
		content << file.rdbuf();
		return affresco::cli::Schema(json::parse(content.str()));
	}();
	return schema;
}

/// @brief Finds, in the text tree, each node `ids` would name, as the command does before it writes the ids.
/// @param tree The text tree.
/// @param document The same document, as the library reads it.
void findNamedNodes(affresco::cli::TextValue& tree, json document) {
	try {
		for(const auto& [pointer, id] : affresco::format::NodeNamer::nameAll(document))
			static_cast<void>(affresco::cli::valueAt(tree, pointer));
	} catch(const json::exception&) { // what `ids` reports as a failure
	} catch(const affresco::DocumentError&) {
	}
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	affresco::cli::TextValue tree;
	try {
		tree = affresco::cli::parseText(text);
	} catch(const std::runtime_error&) {
		return 0; // not JSON, or nested too deep: fmt and ids report it
	}
	const json document = affresco::parseUntrusted(text); // parseText read it with this, so it can't throw

	const std::string formatted = affresco::cli::printText(tree);
	if(affresco::parseUntrusted(formatted) != document) affresco::fuzz::broken("fmt writes the same JSON value");
	if(affresco::cli::printText(affresco::cli::parseText(formatted)) != formatted)
		affresco::fuzz::broken("formatting a formatted file changes nothing");

	findNamedNodes(tree, document);
	try {
		static_cast<void>(screenSchema().check(document));
	} catch(const std::exception&) {
		affresco::fuzz::broken("the schema check reports problems, never throws");
	}
	return 0;
}
