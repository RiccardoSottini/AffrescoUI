/// @file
/// @brief Unit tests for features (rfcs/0004-features.md): manifests, composition, and installing, enabling,
/// disabling and removing a feature by revision, each step undoable.
///
/// The packages are Forge's worked examples (examples/forge/features/word-count and todo-tree); the app is a small
/// screen with the same contribution points and exports as Forge.

#include "Composition.hpp"
#include "FeatureManifest.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <fstream>
#include <string>

using affresco::json;
using affresco::RevisionStatus;

namespace {

/// @brief One of Forge's feature packages.
/// @param folder Its folder under examples/forge/features, e.g. "word-count".
/// @return Its manifest.
json package(const std::string& folder) {
	std::ifstream file(std::string(AFFRESCOUI_EXAMPLES_DIRECTORY) + "/forge/features/" + folder + "/feature.json");
	REQUIRE(file);
	return json::parse(file);
}

/// @brief An app with Forge's contribution points (a status bar, a side bar), its exports, a View menu and a key.
/// @return The document, with no feature installed.
json app() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"tokens": { "color.statusText": "#ffffff", "color.text": "#000000", "color.muted": "#808080" },
		"state": { "text": "one two three", "files": [{ "id": "a.cpp", "name": "a.cpp", "text": "// TODO: x" }] },
		"exports": {
			"editor.text": { "type": "string", "value": "{{state.text}}" },
			"workspace.files": { "type": "array", "value": "{{state.files}}" }
		},
		"actions": { "showTab": [{ "set": "state.text", "to": "{{id}}" }], "save": [{ "set": "state.text", "to": "" }] },
		"keys": { "mod+s": "save" },
		"menus": { "view": { "save": { "run": "save" } } },
		"features": {},
		"root": { "type": "column", "id": "main", "children": [
			{ "type": "column", "id": "side-bar", "point": "sidebar.views" },
			{ "type": "row", "id": "status-bar", "point": "statusBar.right", "children": [
				{ "type": "label", "id": "status-folder", "text": "folder" }
			] }
		] }
	})json");
}

/// @brief The revision that installs a package as a reviewer would: one record under /features.
/// @param manifest The package's manifest.
/// @param order Its place among the features.
/// @return The revision.
json install(const json& manifest, int order) {
	const std::string name = manifest["name"];
	return {{"author", "studio"},
	        {"message", "Install " + name},
	        {"set", {{"/features/" + name, affresco::format::recordFor(manifest, "revision", "", "", order)}}}};
}

/// @brief The composed screen of a repository's document.
/// @param repository The repository.
/// @return The screen.
json composed(const affresco::vcs::Repository& repository) {
	return affresco::format::compose(repository.document()).screen;
}

/// @brief The problems composing a document gives.
/// @param document The document.
/// @return Each with its place, joined by newlines.
std::string problems(const json& document) {
	std::string text;
	for(const std::string& problem : affresco::format::compose(document).problems) text += problem + "\n";
	return text;
}

/// @brief Whether checking a manifest throws an error containing a text.
/// @param manifest The manifest.
/// @param text The text.
/// @return true if it does.
bool refused(const json& manifest, const std::string& text) {
	try {
		affresco::format::checkManifest(manifest, "feature");
	} catch(const affresco::DocumentError& error) {
		INFO(error.what());
		return std::string(error.what()).contains(text);
	}
	return false;
}

} // namespace

TEST_CASE("features: releases and ranges have semver's meanings") {
	using affresco::format::satisfies;
	CHECK(satisfies("1.2.3", "^1.0.0"));
	CHECK_FALSE(satisfies("2.0.0", "^1.0.0"));
	CHECK(satisfies("0.2.9", "^0.2.0"));
	CHECK_FALSE(satisfies("0.3.0", "^0.2.0"));
	CHECK(satisfies("1.2.9", "~1.2.0"));
	CHECK_FALSE(satisfies("1.3.0", "~1.2.0"));
	CHECK(satisfies("3.0.0", ">=1.0.0"));
	CHECK(satisfies("1.0.0", "1.0.0"));
	CHECK_FALSE(satisfies("1.0", "^1.0.0"));
	CHECK_FALSE(affresco::format::parseRelease("1.2.3-beta"));
	CHECK_FALSE(affresco::format::parseRelease("1234567890.0.0")); // at most 9 digits: no overflow
	CHECK_FALSE(affresco::format::validRange("<2.0.0"));
}

TEST_CASE("features: the worked examples are valid manifests") {
	CHECK_NOTHROW(affresco::format::checkManifest(package("word-count"), "wordCount"));
	CHECK_NOTHROW(affresco::format::checkManifest(package("todo-tree"), "todoTree"));
	// Forge's own four, built in: manifests that describe them and provide nothing
	for(const char* name : {"shell", "palette", "themes", "panels"}) {
		CAPTURE(name);
		CHECK_NOTHROW(affresco::format::checkManifest(package(name), name));
		CHECK(package(name)["provides"].empty());
	}
}

TEST_CASE("features: a manifest can never widen a capability") {
	json manifest = package("word-count");
	SUBCASE("a capability section") {
		manifest["provides"]["services"] = json::object();
		CHECK(refused(manifest, "a capability"));
	}
	SUBCASE("a control step, however deep") {
		manifest["provides"]["actions"]["toggle"]["do"] = json::parse(R"([{"if": "{{true}}", "then": [{"call": "x"}]}])");
		CHECK(refused(manifest, "'call'"));
	}
	SUBCASE("a sign-in step") { // it reaches a provider in the person's name
		manifest["provides"]["actions"]["toggle"]["do"] = json::parse(R"([{"signIn": "api"}])");
		CHECK(refused(manifest, "'signIn'"));
		manifest["provides"]["actions"]["toggle"]["do"] = json::parse(R"([{"signOut": "api"}])");
		CHECK(refused(manifest, "'signOut'"));
	}
	SUBCASE("a node id outside the namespace") {
		manifest["provides"]["views"]["statusBar.right"]["label"] = manifest["provides"]["views"]["statusBar.right"]
		                                                                    ["wordCount-label"];
		CHECK(refused(manifest, "must start with 'wordCount-'"));
	}
	SUBCASE("an unknown key") {
		manifest["provides"]["permissionz"] = json::object();
		CHECK(refused(manifest, "unknown key"));
	}
	SUBCASE("a name that isn't one camelCase word") {
		manifest["name"] = "Word-Count";
		CHECK(refused(manifest, "camelCase"));
	}
}

TEST_CASE("features: composing merges a feature into its namespace and its point") {
	json document = app();
	document["features"]["wordCount"] = affresco::format::recordFor(package("word-count"), "revision", "", "", 10);
	const affresco::format::Composition composition = affresco::format::compose(document);
	REQUIRE(composition.problems.empty());
	const json& screen = composition.screen;
	CHECK(screen["state"]["wordCount"] == json::parse(R"({"characters": false})"));
	CHECK(screen["actions"].contains("wordCount.toggle"));
	CHECK(screen["menus"]["view"].contains("wordCount.toggle"));
	const json& bar = screen["root"]["children"][1];
	CHECK_FALSE(bar.contains("point")); // read by composition only
	REQUIRE(bar["children"].size() == 2);
	CHECK(bar["children"][1]["id"] == "wordCount-label");
	CHECK(screen["computed"]["wordCount"].get<std::string>().find("state.text") != std::string::npos); // app.editor.text
	CHECK_FALSE(screen.contains("features"));
	CHECK_FALSE(screen.contains("exports"));
	CHECK(affresco::format::featureStates(document) == json::parse(R"({"wordCount": {"characters": false}})"));
	CHECK(affresco::format::findFeatureNode(document, "wordCount-label") ==
	      "/features/wordCount/provides/views/statusBar.right/wordCount-label");
}

TEST_CASE("features: what keeps a feature out is named with its place") {
	json document = app();
	SUBCASE("a missing contribution point") {
		document["root"]["children"][1].erase("point");
		document["features"]["wordCount"] = affresco::format::recordFor(package("word-count"), "revision", "", "", 10);
		CHECK(problems(document).find("features.wordCount.requires.points: the app has no contribution point "
		                              "'statusBar.right' (it has: sidebar.views)") != std::string::npos);
	}
	SUBCASE("a missing export") {
		document["exports"].erase("editor.text");
		document["features"]["wordCount"] = affresco::format::recordFor(package("word-count"), "revision", "", "", 10);
		CHECK(problems(document).find("requires.exports.editor.text: the app doesn't export it") != std::string::npos);
	}
	SUBCASE("a feature it requires is missing") {
		document["features"]["todoTree"] = affresco::format::recordFor(package("todo-tree"), "revision", "", "", 20);
		CHECK(problems(document).find("features.todoTree.requires.features.wordCount") != std::string::npos);
	}
	SUBCASE("a key the app binds") {
		json manifest = package("todo-tree");
		manifest["provides"]["keys"] = json::parse(R"({"mod+s": "todoTree.toggle"})");
		document["features"]["wordCount"] = affresco::format::recordFor(package("word-count"), "revision", "", "", 10);
		document["features"]["todoTree"] = affresco::format::recordFor(manifest, "revision", "", "", 20);
		CHECK(problems(document).find("provides.keys.mod+s: the app binds it already") != std::string::npos);
	}
	SUBCASE("a namespace the app uses") {
		document["state"]["wordCount"] = 3;
		document["features"]["wordCount"] = affresco::format::recordFor(package("word-count"), "revision", "", "", 10);
		CHECK(problems(document).find("the namespace 'wordCount' is taken") != std::string::npos);
	}
}

TEST_CASE("features: a chain of features can't grow the exports without bound") {
	// Each feature exports its predecessor's export twice, so the expressions double from one to the next: without a
	// bound, thirty of them would compose a screen of gigabytes.
	json document = app();
	for(int index = 0; index < 30; ++index) {
		const std::string name = "double" + std::to_string(index);
		const std::string previous = index == 0 ? "editor.text" : "double" + std::to_string(index - 1) + ".text";
		json manifest = {{"format", "affrescoui-feature"},
		                 {"version", 2},
		                 {"name", name},
		                 {"release", "1.0.0"},
		                 {"title", name},
		                 {"requires", {{"format", 2}, {"exports", {{previous, {{"type", "string"}}}}}}},
		                 {"provides",
		                  {{"exports",
		                    {{"text", {{"type", "string"}, {"value", "{{app." + previous + " + app." + previous + "}}"}}}}}}}};
		document["features"][name] = affresco::format::recordFor(manifest, "revision", "", "", index);
	}
	const std::string text = problems(document);
	INFO(text);
	CHECK(text.contains("features.double15.provides: its uses of the exports grow it by more than 1048576 bytes"));
	CHECK_FALSE(text.contains("features.double14.provides"));
}

TEST_CASE("features: a malformed feature is a problem, never a crash") {
	// Composition runs before the document's check: each part it reads must be checked by composition itself.
	json document = app();
	const json valid = affresco::format::recordFor(package("word-count"), "revision", "", "", 10);
	SUBCASE("a record that isn't an object") { document["features"]["wordCount"] = 5; }
	SUBCASE("enabled that isn't a boolean") {
		document["features"]["wordCount"] = valid;
		document["features"]["wordCount"]["enabled"] = "yes";
	}
	SUBCASE("an order that isn't a number") {
		document["features"]["wordCount"] = valid;
		document["features"]["wordCount"]["order"] = "first";
	}
	SUBCASE("a required export without a type") {
		document["features"]["wordCount"] = valid;
		document["features"]["wordCount"]["requires"]["exports"]["editor.text"] = json::object();
	}
	SUBCASE("a required point that isn't a string") {
		document["features"]["wordCount"] = valid;
		document["features"]["wordCount"]["requires"]["points"] = json::array({5});
	}
	SUBCASE("a features section that isn't an object") { document["features"] = json::array({1}); }
	SUBCASE("an export that isn't an object") {
		document["features"]["wordCount"] = valid;
		document["exports"]["editor.text"] = 5;
	}
	SUBCASE("an import that isn't a string") { document["imports"] = {{"wc", 5}}; }
	SUBCASE("a section features merge into that isn't an object") {
		document["features"]["wordCount"] = valid;
		document["keys"] = json::array({1});
	}
	SUBCASE("a menu that isn't an object") {
		document["features"]["wordCount"] = valid;
		document["menus"]["view"] = 5;
	}
	affresco::format::Composition composition;
	REQUIRE_NOTHROW(composition = affresco::format::compose(document));
	CHECK_FALSE(composition.problems.empty());
	CHECK_FALSE(composition.screen.contains("features"));

	const affresco::format::Rules rules; // a revision that brings one is refused, with the checker's reason
	affresco::vcs::Repository repository(rules, app());
	json changes = json::object();
	for(const auto& [key, value] : document.items())
		if(value != repository.document().value(key, json())) changes["/" + key] = value;
	affresco::RevisionResult result;
	REQUIRE_NOTHROW(result = repository.submit({{"author", "studio"}, {"set", changes}}));
	CHECK(result.status == RevisionStatus::Rejected);
}

TEST_CASE("features: two packages install, are disabled, enabled and removed by revision, each step undoable") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, app());
	const json original = repository.document();

	REQUIRE(repository.submit(install(package("word-count"), 10)).status == RevisionStatus::Applied);
	const affresco::RevisionResult todo = repository.submit(install(package("todo-tree"), 20));
	REQUIRE_MESSAGE(todo.status == RevisionStatus::Applied, todo.error);
	CHECK(composed(repository)["root"]["children"][0]["children"][0]["id"] == "todoTree-view");
	CHECK(composed(repository)["keys"].contains("mod+shift+t"));

	SUBCASE("disable and enable") {
		REQUIRE(repository.submit({{"author", "studio"}, {"set", {{"/features/todoTree/enabled", false}}}}).status ==
		        RevisionStatus::Applied);
		CHECK_FALSE(composed(repository)["keys"].contains("mod+shift+t"));
		CHECK(composed(repository)["root"]["children"][0]["children"].empty());
		REQUIRE(repository.submit({{"author", "studio"}, {"set", {{"/features/todoTree/enabled", true}}}}).status ==
		        RevisionStatus::Applied);
		CHECK(composed(repository)["keys"].contains("mod+shift+t"));
	}
	SUBCASE("a feature others require can't be disabled under them") {
		const affresco::RevisionResult refused =
		    repository.submit({{"author", "studio"}, {"set", {{"/features/wordCount/enabled", false}}}});
		CHECK(refused.status == RevisionStatus::Rejected);
		CHECK(refused.error.find("todoTree") != std::string::npos);
	}
	SUBCASE("remove, then undo the removal") {
		REQUIRE(repository.submit({{"author", "studio"}, {"remove", {"/features/todoTree"}}}).status ==
		        RevisionStatus::Applied);
		REQUIRE(repository.submit({{"author", "studio"}, {"remove", {"/features/wordCount"}}}).status ==
		        RevisionStatus::Applied);
		CHECK(composed(repository) == affresco::format::compose(original).screen);
		REQUIRE(repository.submit(repository.revertRevision("HEAD", "studio")).status == RevisionStatus::Applied);
		CHECK(repository.document()["features"].contains("wordCount"));
	}
	SUBCASE("undo both installs") {
		REQUIRE(repository.submit(repository.revertRevision("HEAD", "studio")).status == RevisionStatus::Applied);
		REQUIRE(repository.submit(repository.revertRevision(1, "studio")).status == RevisionStatus::Applied);
		CHECK(repository.document() == original);
	}
}

TEST_CASE("features: a revision can't install a feature that brings a control step") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, app());
	json manifest = package("word-count");
	manifest["provides"]["actions"]["toggle"]["do"] = json::parse(R"([{"call": "forge.openFolder"}])");
	json revision = install(package("word-count"), 10);
	revision["set"]["/features/wordCount"]["provides"] = manifest["provides"];
	const affresco::RevisionResult result = repository.submit(revision);
	CHECK(result.status == RevisionStatus::Rejected);
	CHECK(result.error.find("call") != std::string::npos); // the revision rule or the manifest's, whichever is first
}

TEST_CASE("features: imports name an installed feature's exports under an alias") {
	json document = app();
	document["features"]["wordCount"] = affresco::format::recordFor(package("word-count"), "revision", "", "", 10);
	SUBCASE("a range the release meets") {
		document["imports"] = json::parse(R"({"words": "wordCount@^1.0.0"})");
		CHECK(problems(document).empty());
	}
	SUBCASE("a range it doesn't") {
		document["imports"] = json::parse(R"({"words": "wordCount@^2.0.0"})");
		CHECK(problems(document).find("imports.words: needs ^2.0.0, 1.0.0 is installed") != std::string::npos);
	}
	SUBCASE("a feature that isn't installed") {
		document["imports"] = json::parse(R"({"todo": "todoTree@^1.0.0"})");
		CHECK(problems(document).find("imports.todo: the feature 'todoTree' isn't installed") != std::string::npos);
	}
	CHECK_THROWS_AS(affresco::format::checkImportsSection(json::parse(R"({"x": "wordCount"})"), "imports"),
	                affresco::DocumentError);
}
