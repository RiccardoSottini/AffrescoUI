/// @file
/// @brief Unit tests for node ids (D15): readable ids for nodes without one, ids for the nodes a revision adds (in
/// the commit), duplicate ids reported by the checker, and the hints of a revision whose path finds nothing.

#include "Helpers.hpp"

#include <affresco/format/NodeIds.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::format::NodeNamer;
using affresco::test::permissionsScreen;

namespace {

/// @brief A screen whose nodes mostly have no id: labels, a bound field, a repeat, a component and its uses.
/// @return The document.
json unnamedScreen() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"state": { "board": "Launch", "cards": [], "newCard": "" },
		"components": { "moveButton": { "params": { "by": 1 }, "node": { "type": "label", "text": "{{by}}" } } },
		"root": { "children": [
			{ "id": "topBar", "children": [
				{ "type": "label", "text": "Blocked" },
				{ "type": "label", "text": "{{state.board}}" },
				{ "width": 1, "color": "#374151" },
				{ "width": 1, "color": "#374151" }
			] },
			{ "type": "textarea", "bind": "state.newCard", "placeholder": "Add a card..." },
			{ "type": "column", "repeat": { "in": "state.cards", "for": "card" }, "children": [
				{ "use": "moveButton", "with": { "by": -1 } },
				{ "use": "moveButton" }
			] },
			{ "type": "label", "text": "Blocked" }
		] }
	})json");
}

} // namespace

TEST_CASE("ids: nodes without one get readable ids from their type and what they show") {
	json document = unnamedScreen();
	const auto named = NodeNamer::nameAll(document);

	const json& root = document["root"];
	CHECK(root["id"] == "panel-root");
	CHECK(root["children"][0]["id"] == "topBar"); // ids that are there stay
	CHECK(root["children"][0]["children"][0]["id"] == "label-blocked");
	CHECK(root["children"][0]["children"][1]["id"] == "label-board"); // "state" says nothing
	CHECK(root["children"][0]["children"][2]["id"] == "panel-top-bar"); // shows nothing: named after its parent
	CHECK(root["children"][0]["children"][3]["id"] == "panel-top-bar-2");
	CHECK(root["children"][1]["id"] == "textarea-add-a-card"); // the placeholder, before the bound path
	CHECK(root["children"][2]["id"] == "column-card");          // the repeat's variable
	CHECK(root["children"][2]["children"][0]["id"] == "move-button-card"); // a use: after its component
	CHECK(root["children"][2]["children"][1]["id"] == "move-button-card-2");
	CHECK(root["children"][3]["id"] == "label-blocked-2");
	CHECK(document["components"]["moveButton"]["node"]["id"] == "label-by");

	REQUIRE(named.size() == 11);
	CHECK(named[1] == std::pair<std::string, std::string>("/root/children/0/children/0", "label-blocked"));
	CHECK(affresco::format::duplicateIds(document).empty());

	// Naming again changes nothing: every node has one
	json again = document;
	CHECK(NodeNamer::nameAll(again).empty());
	CHECK(again == document);
}

TEST_CASE("ids: where nodes go, and places as errors write them") {
	using affresco::format::isNodePlace;
	using affresco::format::placeOf;
	CHECK(isNodePlace("/root"));
	CHECK(isNodePlace("/root/children/0"));
	CHECK(isNodePlace("/root/children/2/children/-"));
	CHECK(isNodePlace("/screens/home/children/1"));
	CHECK(isNodePlace("/templates/card"));
	CHECK(isNodePlace("/components/chip/node/children/0"));
	CHECK_FALSE(isNodePlace(""));
	CHECK_FALSE(isNodePlace("/root/children"));
	CHECK_FALSE(isNodePlace("/root/children/x"));
	CHECK_FALSE(isNodePlace("/root/text"));
	CHECK_FALSE(isNodePlace("/state/tree/children/0"));
	CHECK_FALSE(isNodePlace("/components/chip"));
	CHECK(placeOf("/root/children/2/children/0") == "root.children[2].children[0]");
	CHECK(placeOf("/components/chip/node") == "components.chip.node");
}

TEST_CASE("ids: the checker reports an id that two nodes have") {
	json document = permissionsScreen();
	document["root"]["children"].push_back({{"type", "label"}, {"id", "board"}, {"text", "Again"}});
	const affresco::format::Rules rules;
	const std::vector<std::string> findings = rules.findings(document, document["state"]);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0] == "root.children[3]: id 'board' is taken by root.children[0] (an id names one node: give this "
	                     "one another)");

	const auto duplicates = affresco::format::duplicateIds(document);
	REQUIRE(duplicates.size() == 1);
	CHECK(duplicates[0].pointers == std::vector<std::string>{"/root/children/0", "/root/children/3"});
}

TEST_CASE("ids: a node a revision adds without an id gets one, in the commit too") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());

	const affresco::RevisionResult added = repository.submit(
	    {{"author", "studio"},
	     {"add", {{"#main/children/-", {{"type", "label"}, {"text", "Blocked"}, {"children", {{{"width", 4}}}}}}}}});
	REQUIRE(added.status == RevisionStatus::Applied);
	const json& node = repository.document()["root"]["children"][3];
	CHECK(node["id"] == "label-blocked");
	CHECK(node["children"][0]["id"] == "panel-blocked");

	// The commit holds the ids, so a change set replays them and a diff shows them
	const json changes = repository.exportChanges(0, "ids");
	CHECK(changes["revisions"][0]["patch"][0]["path"] == "#main/children/-"); // as written
	CHECK(changes["revisions"][0]["patch"][0]["value"]["id"] == "label-blocked");
	CHECK(repository.diff("HEAD~1", "HEAD")[0]["value"]["id"] == "label-blocked");
}

TEST_CASE("ids: a node that replaces another keeps its id; an id already taken is refused") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());

	REQUIRE(repository.submit({{"author", "studio"}, {"set", {{"#title", {{"type", "label"}, {"text", "Today"}}}}}})
	            .status == RevisionStatus::Applied);
	CHECK(repository.document()["root"]["children"][1] == json{{"id", "title"}, {"type", "label"}, {"text", "Today"}});

	const affresco::RevisionResult taken =
	    repository.submit({{"author", "studio"}, {"add", {{"/root/children/-", {{"id", "board"}}}}}});
	REQUIRE(taken.status == RevisionStatus::Rejected);
	CHECK(taken.error.find("id 'board' is taken by root.children[0]") != std::string::npos);
}

TEST_CASE("ids: a rejected revision names the operation and the path, with a hint") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	auto error = [&](const json& edits) {
		json revision = edits;
		revision["author"] = "studio";
		const affresco::RevisionResult result = repository.submit(revision);
		CHECK(result.status == RevisionStatus::Rejected);
		return result.error;
	};

	CHECK(error({{"set", {{"#titel/text", "Done"}}}}) ==
	      "AffrescoUI screen: revision.patch[0] (set #titel/text).path: no node with id 'titel' (did you mean "
	      "'#title'?)");
	CHECK(error({{"remove", {"#title/colr"}}}) ==
	      "AffrescoUI screen: revision.patch[0] (remove #title/colr): key 'colr' not found at /root/children/1/colr "
	      "('#title' has no 'colr' (it has: type, id, text))");
	CHECK(error({{"set", {{"/root/children/7/text", "Done"}}}}).ends_with(
	    "at /root/children/7/text ('/root/children' is a list of 3: use 0 to 2, or '-' to append)"));
	CHECK(error({{"add", {{"#title/text/x", 1}}}}) == "AffrescoUI screen: revision.patch[0] (add #title/text/x): "
	                                                  "'/root/children/1/text' is a string: nothing can be added "
	                                                  "inside it");
}
