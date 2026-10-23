/// @file
/// @brief Forge's language servers (examples/forge/features/lsp): URIs kept inside the open folder, a server's results
/// turned into the screen's bounded shapes, edits applied all or none, and malformed answers from an untrusted
/// server, many of them random, that must neither throw nor leave the limits.

#include "features/lsp/LspProtocol.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

using affresco::json;
namespace lsp = forge::lsp;

namespace {

const std::filesystem::path root = "/folder"; ///< The open folder: only names are compared, nothing is read.

/// @brief A range from 0, as servers send it.
/// @param line The line.
/// @param start The first character.
/// @param end The character after the last.
/// @return The range.
json range(int line, int start, int end) {
	return {{"start", {{"line", line}, {"character", start}}}, {"end", {{"line", line}, {"character", end}}}};
}

/// @brief A location of a file of the folder, or of any URI.
/// @param uri The URI.
/// @param line The line, from 0.
/// @param character The character, from 0.
/// @return The location.
json location(const std::string& uri, int line, int character) {
	return {{"uri", uri}, {"range", range(line, character, character + 4)}};
}

/// @brief A random JSON value, nested at most `depth` deep: what a broken or hostile server might send.
/// @param random The generator.
/// @param depth How deep it may nest.
/// @return The value.
json randomValue(std::mt19937& random, int depth) {
	static const char* const keys[] = {"uri", "range", "start", "end", "line", "character", "contents", "items", "label",
	                                   "changes", "documentChanges", "newText", "data", "legend", "diagnostics",
	                                   "children", "location", "severity", "kind", "edits", "value", "signatures"};
	switch(std::uniform_int_distribution<int>(0, depth > 0 ? 7 : 4)(random)) {
	case 0: return nullptr;
	case 1: return std::uniform_int_distribution<int>(-3, 1 << 20)(random);
	case 2: return std::uniform_int_distribution<int>(0, 1)(random) == 1;
	case 3: return "file:///folder/" + std::to_string(random() % 7);
	case 4: return std::string(random() % 40, static_cast<char>('a' + random() % 26));
	case 5:
	case 6: {
		json object = json::object();
		for(auto n = static_cast<unsigned>(random() % 6); n > 0; --n)
			object[keys[random() % std::size(keys)]] = randomValue(random, depth - 1);
		return object;
	}
	default: {
		json list = json::array();
		for(unsigned n = random() % 8; n > 0; --n) list.push_back(randomValue(random, depth - 1));
		return list;
	}
	}
}

} // namespace

TEST_SUITE("lsp") {
	TEST_CASE("URIs stay inside the open folder") {
		CHECK(lsp::uriOf(root, "src/main.cpp") == "file:///folder/src/main.cpp");
		CHECK(lsp::uriOf(root, "") == "file:///folder");
		CHECK(lsp::uriOf(root, "a b/ü.py") == "file:///folder/a%20b/%C3%BC.py");
		CHECK(lsp::idOfUri(root, "file:///folder/src/main.cpp") == "src/main.cpp");
		CHECK(lsp::idOfUri(root, "file:///folder/a%20b/%C3%BC.py") == "a b/ü.py");
		CHECK(lsp::idOfUri(root, "file://localhost/folder/x.cpp") == "x.cpp");
		CHECK_FALSE(lsp::idOfUri(root, "file:///folder/../etc/passwd"));
		CHECK_FALSE(lsp::idOfUri(root, "file:///folder%2F..%2Fetc"));
		CHECK_FALSE(lsp::idOfUri(root, "file:///elsewhere/x.cpp"));
		CHECK_FALSE(lsp::idOfUri(root, "file:///folderx/x.cpp"));
		CHECK_FALSE(lsp::idOfUri(root, "file://host/folder/x.cpp"));
		CHECK_FALSE(lsp::idOfUri(root, "https://example.com/folder/x.cpp"));
		CHECK_FALSE(lsp::idOfUri(root, "file:///folder"));
	}

	TEST_CASE("each language's server") {
		CHECK(lsp::serverFor("c++")["service"] == "lsp_cpp");
		CHECK(lsp::serverFor("python")["languageId"] == "python");
		CHECK(lsp::serverFor("typescript")["service"] == "lsp_typescript");
		CHECK(lsp::serverFor("markdown").is_null());
	}

	TEST_CASE("diagnostics: Problems rows from 1, squiggles from 0, outside files refused") {
		const json params = {{"uri", "file:///folder/src/main.cpp"},
		                     {"diagnostics",
		                      {{{"range", range(2, 4, 9)}, {"severity", 1}, {"message", "bad"}, {"source", "fake"}},
		                       {{"range", range(0, 0, 0)}, {"severity", 3}, {"message", "note"}}}}};
		const json shown = lsp::normalize("diagnostics", params, root);
		CHECK(shown["id"] == "src/main.cpp");
		REQUIRE(shown["items"].size() == 2);
		CHECK(shown["items"][0]["severity"] == "error");
		CHECK(shown["items"][0]["line"] == 3);
		CHECK(shown["items"][0]["column"] == 5);
		CHECK(shown["items"][0]["message"] == "bad (fake)");
		CHECK(shown["items"][1]["severity"] == "info");
		CHECK(shown["decorations"][0]["line"] == 2);
		CHECK(shown["decorations"][0]["kind"] == "squiggle");
		CHECK(shown["decorations"][1]["endColumn"] == 1); // an empty range still shows
		CHECK(lsp::normalize("diagnostics", {{"uri", "file:///etc/x"}, {"diagnostics", json::array()}}, root)
		          .contains("error"));
	}

	TEST_CASE("completion: snippets as plain text, cut to the limit") {
		const json result = {{"isIncomplete", true},
		                     {"items",
		                      {{{"label", "puts"}, {"insertTextFormat", 2}, {"insertText", "puts(${1:text}, $2)$0"}},
		                       {{"label", ""}},
		                       {{"label", "printf"}, {"textEdit", {{"newText", "printf"}, {"range", range(0, 0, 2)}}}}}}};
		const json shown = lsp::normalize("completion", result, root);
		REQUIRE(shown["items"].size() == 2); // the one with no label is left out
		CHECK(shown["items"][0]["insert"] == "puts(text, )");
		CHECK(shown["items"][1]["insert"] == "printf");
		CHECK(shown["incomplete"] == true);
		lsp::Limits limits;
		limits.completions = 1;
		CHECK(lsp::normalize("completion", result, root, {}, limits)["truncated"] == true);
		CHECK(lsp::plainSnippet("a\\$b ${2:x${3:y}}") == "a$b xy");
	}

	TEST_CASE("definitions and references: from 1, outside the folder dropped") {
		const json result = json::array({location("file:///folder/a.cpp", 0, 4), location("file:///elsewhere/b.cpp", 1, 1),
		                                  {{"targetUri", "file:///folder/c.cpp"}, {"targetSelectionRange", range(5, 2, 3)}}});
		const json shown = lsp::normalize("references", result, root);
		REQUIRE(shown["locations"].size() == 2);
		CHECK(shown["locations"][0]["id"] == "a.cpp");
		CHECK(shown["locations"][0]["column"] == 5);
		CHECK(shown["locations"][1]["id"] == "c.cpp");
		CHECK(shown["locations"][1]["line"] == 6);
		CHECK(lsp::normalize("definition", location("file:///folder/a.cpp", 3, 0), root)["locations"].size() == 1);
	}

	TEST_CASE("rename: every edit inside the folder, or none") {
		const json inside = {{"changes", {{"file:///folder/a.cpp", {{{"range", range(0, 4, 8)}, {"newText", "start"}}}},
		                                  {"file:///folder/b.cpp", {{{"range", range(1, 0, 4)}, {"newText", "start"}}}}}}};
		const json shown = lsp::normalize("rename", inside, root);
		CHECK(shown["files"] == 2);
		CHECK(shown["edits"].size() == 2);
		json outside = inside;
		outside["changes"]["file:///elsewhere/c.cpp"] = json::array({{{"range", range(0, 0, 1)}, {"newText", "x"}}});
		CHECK(lsp::normalize("rename", outside, root).contains("error"));
		const json creates = {{"documentChanges", {{{"kind", "create"}, {"uri", "file:///folder/new.cpp"}}}}};
		CHECK(lsp::normalize("rename", creates, root).contains("error"));
	}

	TEST_CASE("edits apply all or none") {
		std::string text = "int main() {\n\treturn 0;\n}\n";
		const json edits = {{{"line", 1}, {"column", 5}, {"endLine", 1}, {"endColumn", 9}, {"text", "start"}},
		                    {{"line", 3}, {"column", 2}, {"endLine", 3}, {"endColumn", 2}, {"text", "\treturn 0;\n"}}};
		CHECK_FALSE(lsp::applyEdits(text, edits));
		CHECK(text == "int start() {\n\treturn 0;\n}\treturn 0;\n\n");
		std::string unchanged = "ab\n";
		const json overlapping = {{{"line", 1}, {"column", 1}, {"endLine", 1}, {"endColumn", 3}, {"text", "x"}},
		                          {{"line", 1}, {"column", 2}, {"endLine", 1}, {"endColumn", 3}, {"text", "y"}}};
		CHECK(lsp::applyEdits(unchanged, overlapping));
		const json past = {{{"line", 9}, {"column", 1}, {"endLine", 9}, {"endColumn", 1}, {"text", "x"}}};
		CHECK(lsp::applyEdits(unchanged, past));
		CHECK(unchanged == "ab\n");
	}

	TEST_CASE("symbols nest, workspace symbols name their files") {
		const json document = json::array(
		    {{{"name", "main"}, {"kind", 12}, {"range", range(0, 0, 9)}, {"selectionRange", range(0, 4, 8)},
		      {"children", {{{"name", "count"}, {"kind", 13}, {"range", range(1, 1, 9)}, {"selectionRange", range(1, 5, 9)}}}}}});
		const json shown = lsp::normalize("symbols", document, root, "a.cpp");
		REQUIRE(shown["symbols"].size() == 2);
		CHECK(shown["symbols"][1]["depth"] == 1);
		CHECK(shown["symbols"][1]["id"] == "a.cpp");
		const json workspace = json::array({{{"name", "main"}, {"kind", 12}, {"location", location("file:///folder/b.cpp", 2, 0)}},
		                                    {{"name", "hidden"}, {"kind", 12}, {"location", location("file:///etc/x", 0, 0)}}});
		const json found = lsp::normalize("workspaceSymbols", workspace, root);
		REQUIRE(found["symbols"].size() == 1);
		CHECK(found["symbols"][0]["id"] == "b.cpp");
		CHECK(found["symbols"][0]["line"] == 3);
	}

	TEST_CASE("semantic tokens: relative positions, the legend's types as the grammar's styles") {
		const json legend = {{"tokenTypes", {"type", "function", "macro"}}, {"tokenModifiers", json::array()}};
		const json tokens = lsp::normalize("tokens", {{"data", {0, 0, 3, 0, 0, 0, 4, 4, 1, 0, 2, 1, 5, 2, 0}}, {"legend", legend}},
		                                   root, "a.cpp");
		REQUIRE(tokens.size() == 3);
		CHECK(tokens[1]["column"] == 4);
		CHECK(tokens[2]["line"] == 2);
		CHECK(tokens[2]["column"] == 1);
		CHECK(tokens[0]["style"] == lsp::styleOfToken("type"));
		CHECK(lsp::normalize("tokens", {{"data", {0, 0, 3, 99, 0}}, {"legend", legend}}, root, "a.cpp").empty());
	}

	TEST_CASE("columns in UTF-16 units are mapped") {
		const lsp::ColumnMapper halve = [](const std::string&, std::size_t, std::size_t column) { return column / 2; };
		const json shown = lsp::normalize("definition", location("file:///folder/a.cpp", 0, 8), root, {}, {}, halve);
		CHECK(shown["locations"][0]["column"] == 5);
	}

	TEST_CASE("texts are cut on a character boundary") {
		CHECK(lsp::cut("abc", 5) == "abc");
		const std::string cut = lsp::cut("ééééé", 5);
		CHECK(cut.size() <= 5 + 3);
		CHECK(cut.find("é") == 0);
		CHECK((static_cast<unsigned char>(cut[4]) & 0xC0) != 0x80);
	}

	TEST_CASE("an unknown kind is refused") {
		CHECK_THROWS_AS((void)lsp::normalize("everything", json::object(), root), std::invalid_argument);
	}

	TEST_CASE("random answers neither throw nor leave the limits") {
		std::mt19937 random(8);
		lsp::Limits limits;
		limits.diagnostics = limits.completions = limits.edits = limits.locations = limits.tokens = 5;
		limits.text = limits.documentation = 16;
		static const char* const kinds[] = {"hover",      "completion", "signature",        "definition", "references",
		                                    "rename",     "format",     "symbols",          "workspaceSymbols",
		                                    "tokens",     "diagnostics"};
		for(std::size_t n = 0; n < 4000; ++n) {
			const char* kind = kinds[n % std::size(kinds)];
			const json result = randomValue(random, 5);
			json shown;
			REQUIRE_NOTHROW(shown = lsp::normalize(kind, result, root, "a.cpp", limits));
			for(const char* list : {"items", "locations", "edits", "symbols"})
				if(shown.is_object() && shown.contains(list)) CHECK(shown[list].size() <= 5);
			if(shown.is_array()) CHECK(shown.size() <= 5);
			if(shown.is_object() && shown.contains("text")) CHECK(shown["text"].get<std::string>().size() <= 16 + 3);
		}
	}

	TEST_CASE("symbols nested past the depth are cut") {
		json symbol = {{"name", "leaf"}, {"kind", 12}, {"range", range(0, 0, 1)}, {"selectionRange", range(0, 0, 1)}};
		for(int n = 0; n < 200; ++n)
			symbol = {{"name", "s"}, {"kind", 12}, {"range", range(0, 0, 1)}, {"selectionRange", range(0, 0, 1)},
			          {"children", json::array({symbol})}};
		const json shown = lsp::normalize("symbols", json::array({symbol}), root, "a.cpp");
		for(const json& item : shown["symbols"]) CHECK(item["depth"].get<int>() < lsp::Limits{}.depth + 1);
	}
}
