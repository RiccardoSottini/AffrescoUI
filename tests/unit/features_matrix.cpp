/// @file
/// @brief The four cases of a feature and the app it goes into, each JSON or Lisp: a JSON manifest installs into a
/// JSON app and into a program, a feature written as a program installs into a program and runs its command there,
/// and a static app refuses one that holds code. Each case runs with small hosts and features made here, and again
/// with Sinopia's real slide sorter (examples/sinopia/<notation>/features/slide-sorter). An install shows in history
/// and blame, undo removes it, and switching it off and on works.

#include "CompositionProof.hpp"
#include "CompositionProofRevision.hpp"
#include "FeatureApp.hpp"

#include <affresco/lisp/Document.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using featureapp::App;
using featureapp::json;
namespace format = affresco::format;
namespace lisp = affresco::lisp;

namespace {

/// @brief The sections every host writes beside its tree: three pages, the selected one, what it exports for them
/// and the point its dock is, which receives them all.
/// @return Them.
json hostSections() {
	return json::parse(R"json({
		"state": { "active": "a", "moved": "", "duplicated": "", "pages": [
			{ "id": "a", "name": "Sky", "image": "" },
			{ "id": "b", "name": "Hill", "image": "" },
			{ "id": "c", "name": "Lake", "image": "" }
		] },
		"exports": {
			"doc.pages": { "type": "array", "kind": "list", "value": "{{state.pages}}",
				"item": { "id": "string", "name": "string", "image": "image" } },
			"doc.active": { "type": "string", "kind": "state", "value": "{{state.active}}" },
			"pages.select": { "type": "string", "kind": "action", "value": "{{'selectPage'}}", "arguments": { "args": "object" } },
			"pages.reorder": { "type": "string", "kind": "action", "value": "{{'reorderPage'}}", "arguments": { "args": "object" } },
			"pages.duplicate": { "type": "string", "kind": "action", "value": "{{'duplicatePage'}}", "arguments": { "args": "object" } }
		},
		"points": { "pages.dock": { "slot": "dock",
			"receives": ["doc.pages", "doc.active", "pages.select", "pages.reorder", "pages.duplicate"] } },
		"actions": {
			"selectPage": [{ "set": "state.active", "to": "{{args.id}}" }],
			"reorderPage": [{ "set": "state.moved", "to": "{{args.id + '>' + args.onto}}" }],
			"duplicatePage": [{ "set": "state.duplicated", "to": "{{args.id}}" }]
		},
		"colors": { "text": "#202020", "muted": "#707070", "accent": "#2060d0", "border": "#c0c0c0" },
		"features": {}
	})json");
}

/// @brief The program host's file: the dock, which is the point, and the window it is in.
constexpr std::string_view hostProgram = R"lisp((defview dock "The dock: its point takes a feature's views." []
  `(column#panels :width "100%" :height 200 :point pages.dock (label#layers :height 20 :text "Layers")))

(defscreen main "The window: a canvas over the dock."
  (column#main (column#canvas :height 40) ~(dock)))
)lisp";

/// @brief The dock's definition changed: a heading above it, the tree the feature sits in built anew.
constexpr std::string_view changedDock = R"lisp((defview dock "The dock, with a heading." []
  `(column#panels :width "100%" :height 200 :point pages.dock (label#heading :height 20 :text "Panels")
     (label#layers :height 20 :text "Layers")))
)lisp";

/// @brief A host written as a program.
/// @return Its source, as version control keeps it.
json programHost() {
	json host = lisp::sourceDocument(hostProgram, "app.lisp");
	host.update(hostSections());
	return host;
}

/// @brief The same host as a static JSON app: its tree written out.
/// @return The document.
json jsonHost() {
	json host = {{"format", "affrescoui-screen"}, {"version", 2}};
	host.update(hostSections());
	host["root"] = json::parse(R"json({ "type": "column", "id": "main", "children": [
		{ "type": "column", "id": "canvas", "height": 40 },
		{ "type": "column", "id": "panels", "width": "100%", "height": 200, "point": "pages.dock", "children": [
			{ "type": "label", "id": "layers", "height": 20, "text": "Layers" } ] }
	] })json");
	return host;
}

/// @brief A small feature's manifest, without what it provides: a list of pages it shows, docked, and picks from.
/// @return The manifest.
json pageListManifest() {
	return json::parse(R"json({
		"format": "affrescoui-feature", "version": 2, "name": "pageList", "release": "1.0.0", "title": "Page list",
		"requires": { "format": 2 },
		"needs": {
			"pages": { "kind": "list", "item": { "id": "string", "name": "string", "image": "image" } },
			"select": { "kind": "action", "arguments": { "id": "string" } },
			"dock": { "kind": "slot", "slot": "dock" }
		}
	})json");
}

/// @brief The small feature as a JSON manifest: its state, its command and its view written out.
/// @return The manifest.
json pageListJson() {
	json manifest = pageListManifest();
	manifest["provides"] = json::parse(R"json({
		"state": { "picked": "" },
		"actions": { "pick": { "title": "Pick Page",
			"do": [{ "set": "state.pageList.picked", "to": "{{id}}" }, { "run": "app.select", "with": { "id": "{{id}}" } }] } },
		"views": { "dock": { "pageList-view": { "type": "row", "width": "100%", "height": 24, "children": [
			{ "type": "label", "id": "pageList-page-{{page.id}}", "repeat": { "for": "page", "in": "{{app.pages}}", "key": "{{page.id}}" },
			  "width": 60, "height": 20, "text": "{{page.name}}",
			  "onClick": { "run": "pageList.pick", "with": { "id": "{{page.id}}" } } } ] } } }
	})json");
	return manifest;
}

/// @brief The small feature written as a program: the same state, command and view, as definitions.
constexpr std::string_view pageListProgram = R"lisp((defstate pageList {:picked ""} "The page picked last.")

(defcommand pageList.pick "Picks a page, and the app selects it." :title "Pick Page" [id]
  (set "state.pageList.picked" id)
  (run "app.select" :with {:id id}))

(defview pageList-page "One page's name, which picks it." [page]
  `(label :id ~(str "pageList-page-" page.id) :width 60 :height 20 :text ~page.name
     :onClick ~(handler pageList.pick page.id)))

(defview pageList-strip "The pages, in a row." []
  `(row :width "100%" :height 24 ~(repeat [page app.pages :key page.id] (pageList-page page))))

(defpart pageList-views "Where its views go." :views {:dock {:pageList-view ~(pageList-strip)}})
)lisp";

/// @brief A program file's definitions, by address, as a feature's `provides.program` holds them.
/// @param text The file's text.
/// @param file Its name.
/// @return The definitions.
json definitionsOf(std::string_view text, const std::string& file) {
	return lisp::sourceDocument(text, file).at("program").at(file);
}

/// @brief The small feature written as a program.
/// @return The manifest.
json pageListLisp() {
	json manifest = pageListManifest();
	manifest["provides"]["program"]["list.lisp"] = definitionsOf(pageListProgram, "list.lisp");
	return manifest;
}

/// @brief The examples' folder.
/// @return It.
std::filesystem::path examples() { return std::filesystem::path(AFFRESCOUI_FONTS_DIR).parent_path().parent_path(); }

/// @brief A file's text.
/// @param path The file.
/// @return Its text.
std::string textOf(const std::filesystem::path& path) {
	const std::ifstream file(path, std::ios::binary);
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

/// @brief Sinopia's slide sorter in one notation, read as `affrescoui check --compose` reads its folder: the
/// manifest, and the program files it lists in its `provides.program`.
/// @param notation "json" or "lisp".
/// @return The manifest.
json slideSorter(const std::string& notation) {
	const std::filesystem::path folder = examples() / "sinopia" / notation / "features" / "slide-sorter";
	const std::filesystem::path file = folder / ("feature." + notation);
	json manifest = lisp::readDocument(textOf(file), file);
	if(manifest.contains("program")) {
		const json files = manifest["program"]; // a copy: adding "provides" moves the manifest's members
		for(const json& name : files)
			manifest["provides"]["program"][name.get<std::string>()] =
			    definitionsOf(textOf(folder / name.get<std::string>()), name.get<std::string>());
		manifest.erase("program");
	}
	return manifest;
}

/// @brief Binds a feature's needs to the host's exports, as Tempera's adapter binds the sorter's.
/// @param manifest The feature, whose needs it binds.
/// @return The adapter.
json adapterFor(const json& manifest) {
	const json all = json::parse(R"json({
		"pages": { "export": "doc.pages" },
		"current": { "export": "doc.active" },
		"select": { "action": "pages.select", "with": { "args": { "id": "{{id}}" } } },
		"reorder": { "action": "pages.reorder", "with": { "args": { "id": "{{id}}", "onto": "{{onto}}" } } },
		"duplicate": { "action": "pages.duplicate", "with": { "args": { "id": "{{id}}" } } }
	})json");
	json bind = json::object();
	for(const auto& [need, binding] : all.items())
		if(manifest["needs"].contains(need)) bind[need] = binding;
	return {{"format", "affrescoui-adapter"},
	        {"version", 1},
	        {"feature", manifest["name"]},
	        {"host", "app:test"},
	        {"bind", bind}};
}

/// @brief Proves a feature into a host, playing no scenario.
/// @param host The host.
/// @param manifest The feature.
/// @return The proven composition.
format::ProvenComposition prove(const json& host, const json& manifest) {
	const format::ScenarioRunner none = [](const json&, const json&) { return std::vector<format::ScenarioOutcome>(); };
	return format::proveComposition({.host = host,
	                                 .manifest = manifest,
	                                 .adapter = adapterFor(manifest),
	                                 .source = "app:sinopia",
	                                 .hostName = "app:test",
	                                 .base = "",
	                                 .digest = {},
	                                 .publisher = {}},
	                                none);
}

/// @brief The proof's reasons, in one text.
/// @param proven The proven composition.
/// @return The proof, as text.
std::string reasonsOf(const format::ProvenComposition& proven) { return proven.proof.dump(1); }

/// @brief Proves a feature into a host running on a headless window, and installs it: the revision the `features`
/// service would submit. The install shows in history and blame, and the feature's first page is there.
/// @param host The running host.
/// @param manifest The feature.
/// @param page The id of its view of the first page.
/// @return The install's commit number.
int install(App& host, const json& manifest, const std::string& page) {
	const format::ProvenComposition proven = prove(host.repository.sourceDocument(), manifest);
	INFO(reasonsOf(proven));
	REQUIRE(proven.proof.value("passed", false));
	const std::string name = manifest["name"];
	host.commit(affresco::vcs::compositionRevision(name, proven.record, "features", host.repository.sourceDocument()));
	host.settle();
	INFO(host.errors);
	REQUIRE(host.runtime->find(page) != nullptr);
	const affresco::CommitInfo installed = host.repository.head();
	CHECK(installed.author == "features");
	CHECK(installed.message.contains(name));
	CHECK(host.repository.log().back().id == installed.id);
	CHECK(host.repository.blame("/features/" + name).id == installed.id);
	return installed.number;
}

/// @brief Switches an installed feature off and on, then undoes its install: its view goes, comes back, and goes.
/// @param host The running host.
/// @param name The feature.
/// @param page The id of its view of the first page.
/// @param installed The install's commit number.
void switchOffOnAndUndo(App& host, const std::string& name, const std::string& page, int installed) {
	host.set("/features/" + name + "/enabled", false, "user");
	CHECK(host.runtime->find(page) == nullptr);
	host.set("/features/" + name + "/enabled", true, "user");
	CHECK(host.runtime->find(page) != nullptr);
	host.commit(host.repository.undoRevision(installed, "user"));
	host.settle();
	CHECK(host.runtime->find(page) == nullptr);
	CHECK_FALSE(host.repository.document()["features"].contains(name));
}

/// @brief The case of a JSON manifest in a JSON app: it installs, and a click on a page selects it in the app.
/// @param manifest The feature.
/// @param page The id of its view of the second page.
/// @param picked Where it keeps what was picked, in its state.
void jsonIntoJson(const json& manifest, const std::string& page, const std::string& picked) {
	App host(jsonHost());
	const std::string name = manifest["name"];
	const int installed = install(host, manifest, page);
	CHECK(host.state(name.c_str())[picked] == ""); // its state, as it starts
	host.click(page);
	INFO(host.errors);
	CHECK(host.state("active") == "b");
	CHECK(host.state(name.c_str())[picked] == "b");
	switchOffOnAndUndo(host, name, page, installed);
}

/// @brief The case of a JSON manifest in a program: it installs, and then a revision that changes the definition
/// building the tree it sits in applies, the feature still in its place.
/// @param manifest The feature.
/// @param page The id of its view of the second page.
void jsonIntoProgram(const json& manifest, const std::string& page) {
	App host(programHost());
	const int installed = install(host, manifest, page);
	host.commit({{"author", "studio"},
	             {"message", "A heading over the dock"},
	             {"set", {{"/program/app.lisp/dock", std::string(changedDock)}}}});
	host.settle();
	CHECK(host.runtime->find("heading") != nullptr);
	CHECK(host.runtime->find(page) != nullptr);
	host.click(page);
	CHECK(host.state("active") == "b");
	switchOffOnAndUndo(host, manifest["name"], page, installed);
}

/// @brief The case of a feature written as a program in a program: it installs, a click runs the feature's own
/// command, which keeps what it picked in its state and runs the action the app bound to its need.
/// @param manifest The feature.
/// @param page The id of its view of the second page.
/// @param picked Where it keeps what was picked, in its state.
void programIntoProgram(const json& manifest, const std::string& page, const std::string& picked) {
	App host(programHost());
	const std::string name = manifest["name"];
	const int installed = install(host, manifest, page);
	CHECK(host.state(name.c_str())[picked] == ""); // its state, as it starts
	host.click(page);
	INFO(host.errors);
	CHECK(host.state(name.c_str())[picked] == "b");
	CHECK(host.state("active") == "b");
	switchOffOnAndUndo(host, name, page, installed);
}

/// @brief The case of a feature written as a program in a static app: the proof refuses it, saying which version
/// to install.
/// @param manifest The feature.
void programIntoJson(const json& manifest) {
	const format::ProvenComposition proven = prove(jsonHost(), manifest);
	INFO(reasonsOf(proven));
	CHECK_FALSE(proven.proof.value("passed", true));
	CHECK(reasonsOf(proven).contains("this feature holds code, and this app is static: install its JSON version"));
}

} // namespace

TEST_CASE("features matrix: a JSON manifest installs into a JSON app, and a click selects a page") {
	jsonIntoJson(pageListJson(), "pageList-page-b", "picked");
}

TEST_CASE("features matrix: a JSON manifest installs into a program, which then changes the tree around it") {
	jsonIntoProgram(pageListJson(), "pageList-page-b");
}

TEST_CASE("features matrix: a feature written as a program installs into a program, and its command runs") {
	programIntoProgram(pageListLisp(), "pageList-page-b", "picked");
}

TEST_CASE("features matrix: a static app refuses a feature that holds code") { programIntoJson(pageListLisp()); }

TEST_CASE("features matrix: Sinopia's slide sorter has two versions, a JSON manifest and a program") {
	CHECK(slideSorter("json")["release"] == "1.1.0");
	const json program = slideSorter("lisp");
	CHECK(program["release"] == "2.1.0");
	CHECK(program["provides"].contains("program"));
}

TEST_CASE("features matrix: Sinopia's JSON slide sorter installs into a JSON app") {
	jsonIntoJson(slideSorter("json"), "slideSorter-page-b", "picked");
}

TEST_CASE("features matrix: Sinopia's JSON slide sorter installs into a program") {
	jsonIntoProgram(slideSorter("json"), "slideSorter-page-b");
}

TEST_CASE("features matrix: Sinopia's slide sorter written as a program installs into a program") {
	programIntoProgram(slideSorter("lisp"), "slideSorter-page-b", "picked");
}

TEST_CASE("features matrix: a static app refuses Sinopia's slide sorter written as a program") {
	programIntoJson(slideSorter("lisp"));
}

TEST_CASE("features matrix: the slide sorter's two versions build the same views") {
	// Same pixels: what the program builds is what the manifest writes, but for how a handler is written
	App fromJson(jsonHost());
	static_cast<void>(install(fromJson, slideSorter("json"), "slideSorter-page-b"));
	App fromProgram(programHost());
	static_cast<void>(install(fromProgram, slideSorter("lisp"), "slideSorter-page-b"));
	for(const char* id : {"slideSorter-title", "slideSorter-image-a", "slideSorter-name-c", "slideSorter-duplicate"}) {
		const affresco::Panel* left = fromJson.runtime->find(id);
		const affresco::Panel* right = fromProgram.runtime->find(id);
		REQUIRE(left != nullptr);
		REQUIRE(right != nullptr);
		CHECK(left->bounds().right - left->bounds().left == right->bounds().right - right->bounds().left);
		CHECK(left->bounds().bottom - left->bounds().top == right->bounds().bottom - right->bounds().top);
	}
	// Its button duplicates the selected page: the view passes it (a drop, which reorders, is in Tempera's end-to-end
	// test)
	fromProgram.click("slideSorter-duplicate");
	INFO(fromProgram.errors);
	CHECK(fromProgram.state("duplicated") == "a");
}
