/// @file
/// @brief Unit tests for composition across apps: contribution points, needs bound directly or through an adapter, kind
/// checks, refused widenings, adapters' limits and proof records, passing and failing.
///
/// The host is a small image editor with Tempera's exports and dock point; the feature is a slide sorter with
/// Sinopia's needs.

#include "AdapterMapping.hpp"
#include "Composition.hpp"
#include "CompositionProof.hpp"
#include "ContributionPoints.hpp"
#include "FeatureManifest.hpp"

#include <affresco/format/Rules.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
namespace format = affresco::format;

namespace {

/// @brief A host with Tempera's layer exports, three of its actions passed on by one dock point, and one kept back.
/// @return The document.
json host() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"state": { "active": "a", "layers": [
			{ "id": "a", "name": "Sky", "depth": 0, "kind": "pixel", "thumbnail": "layer-a" },
			{ "id": "b", "name": "Cloud", "depth": 1, "kind": "pixel", "thumbnail": "layer-b" },
			{ "id": "c", "name": "Hill", "depth": 0, "kind": "pixel", "thumbnail": "layer-c" }
		] },
		"exports": {
			"doc.layers": { "type": "array", "kind": "list", "value": "{{state.layers}}",
				"item": { "id": "string", "name": "string", "depth": "integer", "kind": "string", "thumbnail": "image" } },
			"doc.active": { "type": "string", "kind": "state", "value": "{{state.active}}" },
			"layers.move": { "type": "string", "kind": "action", "value": "{{'moveLayer'}}", "arguments": { "args": "object" } },
			"layers.select": { "type": "string", "kind": "action", "value": "{{'selectLayer'}}", "arguments": { "args": "object" } },
			"layers.duplicate": { "type": "string", "kind": "action", "value": "{{'duplicateLayer'}}", "arguments": { "args": "object" } },
			"layers.delete": { "type": "string", "kind": "action", "value": "{{'deleteLayer'}}", "arguments": { "args": "object" } }
		},
		"points": { "panels.dock": { "slot": "dock",
			"receives": ["doc.layers", "doc.active", "layers.move", "layers.select", "layers.duplicate"] } },
		"actions": {
			"moveLayer": [{ "set": "state.active", "to": "{{args.onto}}" }],
			"selectLayer": [{ "set": "state.active", "to": "{{args.id}}" }],
			"duplicateLayer": [{ "set": "state.active", "to": "{{args.id}}" }],
			"deleteLayer": [{ "set": "state.layers", "to": "{{[]}}" }]
		},
		"features": {},
		"root": { "type": "row", "id": "main", "children": [
			{ "type": "column", "id": "canvas" },
			{ "type": "column", "id": "panels", "point": "panels.dock" }
		] }
	})json");
}

/// @brief A slide sorter with Sinopia's needs: pages renderable to an image, reorder, select and duplicate, a dock.
/// @return The manifest.
json sorter() {
	return json::parse(R"json({
		"format": "affrescoui-feature",
		"version": 2,
		"name": "slideSorter",
		"release": "1.0.0",
		"title": "Slide sorter",
		"requires": { "format": 2 },
		"needs": {
			"pages": { "kind": "list", "item": { "id": "string", "name": "string", "image": "image" } },
			"reorder": { "kind": "action", "arguments": { "id": "string", "onto": "string" } },
			"select": { "kind": "action", "arguments": { "id": "string" } },
			"duplicate": { "kind": "action", "arguments": { "id": "string" } },
			"dock": { "kind": "slot", "slot": "dock" }
		},
		"provides": {
			"state": { "picked": "" },
			"actions": {
				"pick": { "do": [{ "set": "state.slideSorter.picked", "to": "{{id}}" }, { "run": "app.select", "with": { "id": "{{id}}" } }] },
				"later": { "do": [{ "run": "app.reorder", "with": { "id": "{{id}}", "onto": "{{onto}}" } }] }
			},
			"views": { "dock": { "slideSorter-count": { "type": "label", "text": "{{length(app.pages)}} pages" } } }
		}
	})json");
}

/// @brief The demo's adapter: top-level layers are pages, the sorter's actions are the layer actions.
/// @return The adapter.
json adapter() {
	return json::parse(R"json({
		"format": "affrescoui-adapter",
		"version": 1,
		"feature": "slideSorter",
		"host": "tempera",
		"bind": {
			"pages": { "value": "{{map(filter(app.doc.layers, it.depth == 0), {id: it.id, name: it.name, image: it.thumbnail})}}" },
			"reorder": { "action": "layers.move", "with": { "args": { "id": "{{id}}", "onto": "{{onto}}" } } },
			"select": { "action": "layers.select", "with": { "args": { "id": "{{id}}" } } },
			"duplicate": { "action": "layers.duplicate", "with": { "args": { "id": "{{id}}" } } }
		}
	})json");
}

/// @brief The why of an unmet need.
/// @param bound What binding gave.
/// @param need The need.
/// @return Its why, or empty if it is met.
std::string unmetWhy(const format::BoundNeeds& bound, const std::string& need) {
	for(const json& unmet : bound.unmet)
		if(unmet["need"] == need) return unmet["why"].get<std::string>();
	return {};
}

/// @brief Whether a call throws a DocumentError containing a text.
/// @param call The call.
/// @param text The text.
/// @return true if it does.
template <typename Call> bool throwsWith(const Call& call, const std::string& text) {
	try {
		call();
	} catch(const affresco::DocumentError& error) {
		INFO(error.what());
		return std::string(error.what()).contains(text);
	}
	return false;
}

/// @brief Proves the demo composition, with a runner whose one scenario passes.
/// @param adapted The adapter, or null.
/// @return The proven composition.
format::ProvenComposition prove(const json& adapted) {
	const format::ScenarioRunner runner = [](const json&, const json&) {
		return std::vector<format::ScenarioOutcome>{
		    {.file = "feature", .name = "sorts", .passed = true, .problem = {}}};
	};
	return format::proveComposition({.host = host(),
	                                 .manifest = sorter(),
	                                 .adapter = adapted,
	                                 .source = "app:sinopia",
	                                 .hostName = "tempera",
	                                 .base = "0123abcd",
	                                 .digest = {},
	                                 .publisher = {}},
	                                runner);
}

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

TEST_CASE("contributions: points and typed exports are checked and printed") {
	CHECK_NOTHROW(format::checkPointsSection(host()["points"], "points"));
	CHECK(
	    throwsWith([] { format::checkPointsSection(json::parse(R"({"a.b": {"slot": "roof"}})"), "points"); }, "slot"));
	CHECK(throwsWith([] { format::checkExportContract(json::parse(R"({"kind": "value", "arguments": {}})"), "x"); },
	                 "only an action"));
	CHECK(format::grantedActions(host()) ==
	      std::set<std::string, std::less<>>{"moveLayer", "selectLayer", "duplicateLayer"});
	const std::string text =
	    format::contributionsText(format::contributionsOf(host(), json::array({format::partOf(sorter())})));
	CHECK(text.contains("point panels.dock (dock)"));
	CHECK(text.contains("export doc.layers"));
	CHECK(text.contains("slideSorter"));
}

// Found by fuzzing (tests/fuzz, composition): a manifest nobody checked, or one whose need was a word, reached
// contributionsText as is, and its json type_error stopped the program
TEST_CASE("contributions: a part is listed only from a manifest whose needs check") {
	CHECK(throwsWith([] { (void)format::partOf(json()); }, "manifest"));
	CHECK(throwsWith([] { (void)format::partOf(json::parse(R"({"name": "a", "needs": {"pages": "list"}})")); },
	                 "manifest.needs.pages"));
}

TEST_CASE("contributions: needs are named and typed") {
	CHECK_NOTHROW(format::checkManifest(sorter(), "slideSorter"));
	CHECK(throwsWith([] { format::checkNeeds(json::parse(R"({"x": {"kind": "magic"}})"), "needs"); }, "kind"));
	CHECK(throwsWith([] { format::checkNeeds(json::parse(R"({"x": {"kind": "slot"}})"), "needs"); }, "slot"));
	CHECK(throwsWith([] { format::checkNeeds(json::parse(R"({"x": {"kind": "list", "arguments": {}}})"), "needs"); },
	                 "only an action"));
	CHECK(throwsWith(
	    [] { format::checkNeeds(json::parse(R"({"x": {"kind": "capability", "capability": "x"}})"), "needs"); },
	    "services."));
}

TEST_CASE("contributions: needs bind through the adapter, the slot directly") {
	const format::BoundNeeds bound = format::bindNeeds(sorter()["needs"], host(), adapter());
	CHECK(bound.unmet.empty());
	REQUIRE(bound.records.size() == 5);
	CHECK(bound.records[0]["by"] == "adapter");
	CHECK(bound.records[4]["need"] == "dock");
	CHECK(bound.records[4]["by"] == "host");
	CHECK(bound.bindings["dock"] == json::parse(R"({"point": "panels.dock"})"));
}

TEST_CASE("contributions: without an adapter, each unmet need says why") {
	const format::BoundNeeds bound = format::bindNeeds(sorter()["needs"], host(), nullptr);
	CHECK(unmetWhy(bound, "pages").contains("exports no 'pages'"));
	CHECK(unmetWhy(bound, "reorder").contains("exports no 'reorder'"));
	CHECK(unmetWhy(bound, "dock").empty());
	json two = host();
	two["points"]["side.dock"] = {{"slot", "dock"}};
	CHECK(unmetWhy(format::bindNeeds(sorter()["needs"], two, nullptr), "dock").contains("must pick one"));
}

TEST_CASE("contributions: kinds must match") {
	json mapped = adapter();
	SUBCASE("a list need bound to an action") {
		mapped["bind"]["pages"] = {{"export", "layers.move"}};
		CHECK(unmetWhy(format::bindNeeds(sorter()["needs"], host(), mapped), "pages").contains("needs a list"));
	}
	SUBCASE("an action need bound to a value") {
		mapped["bind"]["reorder"] = {{"value", "{{app.doc.active}}"}};
		CHECK(unmetWhy(format::bindNeeds(sorter()["needs"], host(), mapped), "reorder").contains("can't bind"));
	}
	SUBCASE("items missing a field") {
		mapped["bind"]["pages"] = {{"export", "doc.layers"}}; // its items have a thumbnail, not an image
		CHECK(unmetWhy(format::bindNeeds(sorter()["needs"], host(), mapped), "pages").contains("no field 'image'"));
	}
	SUBCASE("a slot bound to a point of another slot") {
		json other = host();
		other["points"]["top.bar"] = {{"slot", "toolbar"}};
		mapped["bind"]["dock"] = {{"point", "top.bar"}};
		CHECK(unmetWhy(format::bindNeeds(sorter()["needs"], other, mapped), "dock").contains("needs a dock"));
	}
}

TEST_CASE("contributions: widenings are refused") {
	json mapped = adapter();
	SUBCASE("an action no point passes on") {
		mapped["bind"]["duplicate"] = {{"action", "layers.delete"}, {"with", {{"args", {{"id", "{{id}}"}}}}}};
		CHECK(unmetWhy(format::bindNeeds(sorter()["needs"], host(), mapped), "duplicate").contains("no point"));
	}
	SUBCASE("a capability from the adapter") {
		json needs = sorter()["needs"];
		needs["storage"] = {{"kind", "capability"}, {"capability", "services.storage"}};
		mapped["bind"]["storage"] = {{"capability", "services.storage"}};
		CHECK(unmetWhy(format::bindNeeds(needs, host(), mapped), "storage").contains("never an adapter's"));
	}
	SUBCASE("an adapter reading the host's state") {
		mapped["bind"]["pages"] = {{"value", "{{state.layers}}"}};
		CHECK(unmetWhy(format::bindNeeds(sorter()["needs"], host(), mapped), "pages").contains("reads only"));
	}
	SUBCASE("a feature reaching past its needs") {
		json manifest = sorter();
		manifest["provides"]["actions"]["wipe"] = {{"do", json::array({{{"run", "deleteLayer"}}})}};
		manifest["provides"]["views"]["dock"]["slideSorter-peek"] = {{"type", "label"}, {"text", "{{state.active}}"}};
		const auto problems = format::isolationProblems("slideSorter", manifest["provides"], manifest["needs"]);
		REQUIRE(problems.size() == 2);
		CHECK(problems[0].contains("'deleteLayer'"));
		CHECK(problems[1].contains("state.active"));
	}
	SUBCASE("a revision changing what a point passes on") {
		json after = host();
		after["points"]["panels.dock"]["receives"].push_back("layers.delete");
		CHECK(throwsWith([&] { format::Rules().checkChange(host(), after); }, "permission denied"));
	}
}

TEST_CASE("contributions: adapters are bounded declarative mappings") {
	CHECK_NOTHROW(format::checkAdapter(adapter()));
	json mapped = adapter();
	SUBCASE("unknown key") {
		mapped["script"] = "x";
		CHECK(throwsWith([&] { format::checkAdapter(mapped); }, "unknown key"));
	}
	SUBCASE("a value that isn't one expression") {
		mapped["bind"]["pages"] = {{"value", "pages: {{app.doc.layers}}"}};
		CHECK(throwsWith([&] { format::checkAdapter(mapped); }, "one expression"));
	}
	SUBCASE("too deep") {
		json deep = "{{id}}";
		for(int level = 0; level < 10; ++level) deep = {{"x", deep}};
		mapped["bind"]["reorder"]["with"] = deep;
		CHECK(throwsWith([&] { format::checkAdapter(mapped); }, "deep"));
	}
	SUBCASE("too long") {
		mapped["bind"]["pages"] = {{"value", "{{'" + std::string(format::maxBindingExpression, 'x') + "'}}"}};
		CHECK(throwsWith([&] { format::checkAdapter(mapped); }, "longer than"));
	}
}

TEST_CASE("contributions: a passing proof installs the sorter, docked and adapted") {
	const format::ProvenComposition proven = prove(adapter());
	INFO(proven.proof.dump(1));
	REQUIRE(proven.proof["passed"] == true);
	CHECK(proven.proof["base"] == "0123abcd");
	CHECK(proven.proof["scenarios"].size() == 1);
	CHECK(proven.proof["adapter"]["digest"].get<std::string>().starts_with("sha512:"));
	CHECK(proven.record["proof"] == proven.proof);
	CHECK_NOTHROW(format::checkFeatureRecord("slideSorter", proven.record, "features.slideSorter"));
	const format::Composition composed = format::compose(proven.document);
	REQUIRE(composed.problems.empty());
	const json& actions = composed.screen["actions"];
	REQUIRE(actions.contains("slideSorter.adapted.reorder"));
	CHECK(actions["slideSorter.adapted.reorder"][0]["run"] == "moveLayer");
	CHECK(actions["slideSorter.later"].dump().contains("slideSorter.adapted.reorder"));
	CHECK(composed.screen["root"].dump().contains("slideSorter-count"));
	CHECK(composed.screen["root"].dump().contains("it.depth == 0"));
}

TEST_CASE("contributions: a failing proof names each unmet need and why") {
	const format::ProvenComposition proven = prove(nullptr);
	CHECK(proven.proof["passed"] == false);
	CHECK(proven.proof["unmet"].size() == 4);
	CHECK(checkNamed(proven.proof, "needs")["passed"] == false);
	CHECK(proven.proof["scenarios"].empty());
	const auto reasons = format::proofReasons(proven.proof);
	REQUIRE_FALSE(reasons.empty());
	CHECK(reasons[0].contains("not proven"));
	CHECK(reasons[1].contains("unmet need pages"));
	CHECK_NOTHROW(format::checkProofRecord(proven.proof, "proof"));
}

TEST_CASE("contributions: proof records read back are bounded") {
	json proof = prove(adapter()).proof;
	CHECK_NOTHROW(format::checkProofRecord(proof, "proof"));
	proof["extra"] = 1;
	CHECK(throwsWith([&] { format::checkProofRecord(proof, "proof"); }, "unknown key"));
	proof.erase("extra");
	proof["scenarios"] = json::array();
	for(std::size_t index = 0; index <= format::maxProofScenarios; ++index)
		proof["scenarios"].push_back(json::object());
	CHECK(throwsWith([&] { format::checkProofRecord(proof, "proof"); }, "too many"));
}
