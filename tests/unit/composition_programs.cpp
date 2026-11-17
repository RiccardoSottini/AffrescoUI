/// @file
/// @brief Unit tests for the composition proof on a program: it plays on the host's source, so a scenario can change
/// the definition that builds the tree a feature sits in; the record installs by revision and leaves that definition
/// open to change; a built tree passed where a source belongs is refused; and a JSON host proves as it always did.

#include "CompositionProof.hpp"
#include "CompositionProofRevision.hpp"
#include "HistoryFiles.hpp" // TemporaryFolder

#include <affresco/automation/Scenarios.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

using affresco::json;
namespace format = affresco::format;

namespace {

/// @brief The program's file: the dock, which holds the contribution point, and the window it is in.
constexpr std::string_view program = R"lisp((defview dock "The dock: its point takes a feature's panel." []
  `(column#panels :point panels.dock (label#layers :text "Layers")))

(defscreen main "The window: a canvas and the dock."
  (row#main (column#canvas) ~(dock)))
)lisp";

/// @brief The dock's definition changed: a heading above the layers.
constexpr std::string_view changedDock = R"lisp((defview dock "The dock, with a heading." []
  `(column#panels :point panels.dock (label#heading :text "Panels") (label#layers :text "Layers")))
)lisp";

/// @brief The sections a host writes beside its program, or as a JSON document: state, exports, a point, actions.
/// @return Them.
json writtenSections() {
	return json::parse(R"json({
		"permissions": { "allow": ["studio", "features"] },
		"state": { "active": "a", "layers": [
			{ "id": "a", "name": "Sky", "depth": 0, "thumbnail": "layer-a" },
			{ "id": "b", "name": "Hill", "depth": 0, "thumbnail": "layer-b" }
		] },
		"exports": {
			"doc.layers": { "type": "array", "kind": "list", "value": "{{state.layers}}",
				"item": { "id": "string", "name": "string", "depth": "integer", "thumbnail": "image" } },
			"layers.select": { "type": "string", "kind": "action", "value": "{{'selectLayer'}}", "arguments": { "args": "object" } }
		},
		"points": { "panels.dock": { "slot": "dock", "receives": ["doc.layers", "layers.select"] } },
		"actions": { "selectLayer": [{ "set": "state.active", "to": "{{args.id}}" }] },
		"features": {}
	})json");
}

/// @brief A host that is a program: its dock and window built by definitions, its other sections written.
/// @return Its source, as version control keeps it.
json programHost() {
	json host = affresco::lisp::sourceDocument(program, "app.lisp");
	host.update(writtenSections());
	return host;
}

/// @brief The same host as a JSON document: the tree written out.
/// @return The document.
json jsonHost() {
	json host = {{"format", "affrescoui-screen"}, {"version", 2}};
	host.update(writtenSections());
	host["root"] = json::parse(R"json({ "type": "row", "id": "main", "children": [
		{ "type": "column", "id": "canvas" },
		{ "type": "column", "id": "panels", "point": "panels.dock", "children": [
			{ "type": "label", "id": "layers", "text": "Layers" } ] }
	] })json");
	return host;
}

/// @brief A feature that lists pages and picks one, docked: it needs a list, an action and a slot.
/// @return The manifest.
json picker() {
	return json::parse(R"json({
		"format": "affrescoui-feature",
		"version": 2,
		"name": "pagePicker",
		"release": "1.0.0",
		"title": "Page picker",
		"requires": { "format": 2 },
		"needs": {
			"pages": { "kind": "list", "item": { "id": "string", "name": "string" } },
			"select": { "kind": "action", "arguments": { "id": "string" } },
			"dock": { "kind": "slot", "slot": "dock" }
		},
		"provides": {
			"state": { "picked": "" },
			"actions": {
				"pick": { "do": [{ "set": "state.pagePicker.picked", "to": "{{id}}" }, { "run": "app.select", "with": { "id": "{{id}}" } }] }
			},
			"views": { "dock": { "pagePicker-count": { "type": "label", "text": "{{length(app.pages)}} pages" } } }
		}
	})json");
}

/// @brief Binds the picker to the host: the layers are pages, selecting one selects the layer.
/// @return The adapter.
json adapter() {
	return json::parse(R"json({
		"format": "affrescoui-adapter",
		"version": 1,
		"feature": "pagePicker",
		"host": "editor",
		"bind": {
			"pages": { "value": "{{map(app.doc.layers, {id: it.id, name: it.name})}}" },
			"select": { "action": "layers.select", "with": { "args": { "id": "{{id}}" } } }
		}
	})json");
}

/// @brief The revision a scenario proposes: the dock's definition changed.
/// @return It.
json dockRevision() {
	return {{"author", "studio"},
	        {"message", "A heading over the dock"},
	        {"set", {{"/program/app.lisp/dock", std::string(changedDock)}}}};
}

/// @brief Applies a revision to a document as a host would, and says why not.
/// @param document The document, as version control keeps it.
/// @param revision The revision.
/// @return Why it wasn't applied, or std::nullopt.
std::optional<std::string> refusalOf(const json& document, const json& revision) {
	const format::Rules rules;
	try {
		affresco::vcs::Repository repository(rules, document);
		return repository.commit(revision);
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
}

/// @brief Proves the picker into a host, with a scenario that proposes the dock's change on what it is given.
/// @param host The host.
/// @param played Receives the scenario's document.
/// @return The proven composition.
format::ProvenComposition prove(const json& host, json* played = nullptr) {
	const format::ScenarioRunner runner = [played](const json& document, const json&) {
		if(played) *played = document;
		const std::optional<std::string> refused = refusalOf(document, dockRevision());
		return std::vector<format::ScenarioOutcome>{{.file = "index.scenarios.json",
		                                             .name = "heads the dock",
		                                             .passed = !refused,
		                                             .problem = refused.value_or("")}};
	};
	return format::proveComposition({.host = host,
	                                 .manifest = picker(),
	                                 .adapter = adapter(),
	                                 .source = "app:pages",
	                                 .hostName = "app:editor",
	                                 .base = "0123abcd",
	                                 .digest = {},
	                                 .publisher = {}},
	                                runner);
}

/// The record a proof of the picker into the JSON host gave before proofs took a program's source, byte for byte:
/// installed records carry it, so a JSON host's must not change.
constexpr std::string_view recordBeforeText = R"json(
	{
		"release": "1.0.0",
		"title": "Page picker",
		"source": "app:pages",
		"digest": null,
		"publisher": null,
		"enabled": true,
		"order": 10,
		"requires": {
			"format": 2
		},
		"provides": {
			"state": {
				"picked": ""
			},
			"actions": {
				"pick": {
					"do": [
						{
							"set": "state.pagePicker.picked",
							"to": "{{id}}"
						},
						{
							"run": "app.select",
							"with": {
								"id": "{{id}}"
							}
						}
					]
				}
			},
			"views": {
				"dock": {
					"pagePicker-count": {
						"type": "label",
						"text": "{{length(app.pages)}} pages"
					}
				}
			}
		},
		"needs": {
			"pages": {
				"kind": "list",
				"item": {
					"id": "string",
					"name": "string"
				}
			},
			"select": {
				"kind": "action",
				"arguments": {
					"id": "string"
				}
			},
			"dock": {
				"kind": "slot",
				"slot": "dock"
			}
		},
		"bindings": {
			"pages": {
				"value": "{{map(app.doc.layers, {id: it.id, name: it.name})}}"
			},
			"select": {
				"action": "layers.select",
				"with": {
					"args": {
						"id": "{{id}}"
					}
				}
			},
			"dock": {
				"point": "panels.dock"
			}
		},
		"proof": {
			"format": "affrescoui-proof",
			"version": 1,
			"feature": "pagePicker",
			"release": "1.0.0",
			"source": "app:pages",
			"host": "app:editor",
			"base": "0123abcd",
			"adapter": {
				"digest": "sha512:H0RKgH6mxcqS1EMphbjECLtV8Sk4OPh-4L1xz4SWo8qmyjJDEABmnawM8DFyFehBTtEtEYxAmFvnvTsuvz-l3g",
				"note": ""
			},
			"bindings": [
				{
					"need": "pages",
					"kind": "list",
					"by": "adapter",
					"to": {
						"value": "{{map(app.doc.layers, {id: it.id, name: it.name})}}"
					}
				},
				{
					"need": "select",
					"kind": "action",
					"by": "adapter",
					"to": {
						"action": "layers.select",
						"with": {
							"args": {
								"id": "{{id}}"
							}
						}
					}
				},
				{
					"need": "dock",
					"kind": "slot",
					"by": "host",
					"to": {
						"point": "panels.dock"
					}
				}
			],
			"checks": [
				{
					"name": "manifest",
					"passed": true,
					"reasons": []
				},
				{
					"name": "adapter",
					"passed": true,
					"reasons": []
				},
				{
					"name": "needs",
					"passed": true,
					"reasons": []
				},
				{
					"name": "composition",
					"passed": true,
					"reasons": []
				},
				{
					"name": "isolation",
					"passed": true,
					"reasons": []
				},
				{
					"name": "permissions",
					"passed": true,
					"reasons": []
				},
				{
					"name": "document",
					"passed": true,
					"reasons": []
				},
				{
					"name": "scenarios",
					"passed": false,
					"reasons": [
						"heads the dock (index.scenarios.json): AffrescoUI screen: revision.patch[0] (set /program/app.lisp/dock): key 'program' not found at /program/app.lisp/dock ('/' has no 'program' (it has: format, version, permissions, state, exports, points, actions, features, root))"
					]
				}
			],
			"scenarios": [
				{
					"file": "index.scenarios.json",
					"name": "heads the dock",
					"passed": false,
					"problem": "AffrescoUI screen: revision.patch[0] (set /program/app.lisp/dock): key 'program' not found at /program/app.lisp/dock ('/' has no 'program' (it has: format, version, permissions, state, exports, points, actions, features, root))"
				}
			],
			"passed": false,
			"unmet": []
		}
	}
	)json";

/// @brief The record a proof of the picker into the JSON host gave before proofs took a program's source.
/// @return It, parsed from recordBeforeText.
json recordBefore() { return json::parse(recordBeforeText); }

/// @brief A proof's check by name.
/// @param proof The proof.
/// @param name The check.
/// @return It, or null.
json checkNamed(const json& proof, const std::string& name) {
	for(const json& check : proof["checks"])
		if(check["name"] == name) return check;
	return nullptr;
}

} // namespace

TEST_CASE("composition: a proof on a program plays on its source, so a scenario can change a definition") {
	json played;
	const format::ProvenComposition proven = prove(programHost(), &played);
	INFO(proven.proof.dump(1));
	CHECK(proven.proof["passed"] == true);
	REQUIRE(proven.proof["scenarios"].size() == 1);
	CHECK(proven.proof["scenarios"][0]["passed"] == true);
	CHECK(proven.document.contains("program"));
	CHECK_FALSE(proven.document.contains("root")); // the record went into the source, beside the program
	CHECK(proven.document["features"].contains("pagePicker"));
	CHECK(played.contains("program"));
	CHECK_FALSE(played.contains("root"));
	const std::optional<json> built = format::Rules().built(proven.document); // and it builds, the feature docked
	REQUIRE(built.has_value());
	const json screen = built.value_or(json::object());
	CHECK(screen["root"]["children"][1]["id"] == "panels");
}

TEST_CASE("composition: a proof on a program installs by revision, and the definition it sits in still changes") {
	const json host = programHost();
	const format::ProvenComposition proven = prove(host);
	REQUIRE(proven.proof["passed"] == true);
	const format::Rules rules;
	affresco::vcs::Repository repository(rules, host);
	const json install = affresco::vcs::compositionRevision("pagePicker", proven.record, "features", host);
	const std::optional<std::string> installRefused = repository.commit(install);
	INFO(installRefused.value_or(""));
	REQUIRE_FALSE(installRefused.has_value());
	CHECK(repository.sourceDocument()["features"].contains("pagePicker"));
	const std::optional<std::string> changeRefused = repository.commit(dockRevision());
	INFO(changeRefused.value_or(""));
	CHECK_FALSE(changeRefused.has_value());
	CHECK(repository.sourceDocument()["program"]["app.lisp"]["dock"] == std::string(changedDock));
}

TEST_CASE("composition: a proof given what a program built, in place of its source, is refused") {
	const std::optional<json> builtTree = format::Rules().built(programHost());
	REQUIRE(builtTree.has_value());
	const json built = builtTree.value_or(json::object());
	REQUIRE(format::isBuiltTree(built));
	CHECK_FALSE(format::isBuiltTree(programHost()));
	CHECK_FALSE(format::isBuiltTree(jsonHost()));
	const format::ProvenComposition proven = prove(built);
	CHECK(proven.proof["passed"] == false);
	const json host = checkNamed(proven.proof, "host");
	REQUIRE(host.is_object());
	CHECK(host["reasons"][0].get<std::string>().contains(format::builtTreeProblem));
	CHECK(proven.record.is_null());
	CHECK(proven.proof["scenarios"].empty());
}

TEST_CASE("composition: a scenario played on a program's source passed in changes its definitions") {
	const affresco::test::TemporaryFolder folder;
	const json scenarios = {{"format", "affrescoui-scenarios"},
	                        {"version", 2},
	                        {"screen", "index.screen.lisp"},
	                        {"scenarios",
	                         {{{"name", "heads-the-dock"},
	                           {"steps",
	                            {{{"expect", {{"node", "#heading"}, {"exists", false}}}},
	                             {{"propose", dockRevision()}},
	                             {{"expect", {{"node", "#heading"}, {"exists", true}}}}}}}}}};
	affresco::automation::Options options;
	options.pictures = false;
	options.document = prove(programHost()).document.dump();
	const affresco::automation::Report report = affresco::automation::run(
	    affresco::automation::ScenarioFile::parse(scenarios.dump(), folder.path() / "index.scenarios.json"), options);
	INFO(report.text());
	REQUIRE(report.scenarios.size() == 1);
	CHECK(report.scenarios[0].passed());
}

TEST_CASE("composition: a JSON host proves exactly as before") {
	const json before = recordBefore();
	const format::ProvenComposition proven = prove(jsonHost());
	REQUIRE(proven.record.is_object());
	for(const auto& [key, value] : before.items()) {
		CAPTURE(key);
		CHECK(proven.record[key] == value);
	}
	CHECK(proven.record.size() == before.size());
	CHECK(proven.record.dump() == before.dump());
	CHECK(proven.proof == before["proof"]);
	CHECK(before["proof"]["passed"] == false); // its scenario edits a program the JSON host hasn't: every check ran
	json document = jsonHost();
	document["features"]["pagePicker"] = before;
	document["features"]["pagePicker"].erase("proof"); // a failed proof's document holds the record without it
	CHECK(proven.document == document);
}
