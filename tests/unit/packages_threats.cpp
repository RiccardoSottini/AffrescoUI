/// @file
/// @brief The threats installing a package meets (SECURITY.md, "Packages"), one test each: a package's code never
/// gets more authority than a revision, nothing it holds runs before the person accepts it, and nothing of the
/// person's is replaced without the person choosing it.

#include "LispApp.hpp"
#include "ScreenPackages.hpp"

#include <affresco/affresco.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/PackageFiles.hpp>
#include <affresco/format/PackageInstall.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using lispapp::App;
using lispapp::Key;
namespace format = affresco::format;

namespace {

/// A person's app: a command that saves through a function with a control step, and one that runs a helper.
constexpr const char* personsApp = R"lisp(
(defn helper "Mine." [] 1)
(defn save "Saves, through the application." [what] (call "hit"))
(defcommand a "" :title "A" [] (save "doc"))
(defcommand c "" :title "C" [] (helper))
(defkey "ctrl+a" a)
(defkey "ctrl+c" c)
(defscreen main :state {:title "Notes" :seen nil} (panel#root))
)lisp";

/// @brief The app the packages are for.
/// @return Its manifest.
std::optional<format::AppManifest> notesApp() {
	format::AppManifest app;
	app.id = "notes";
	app.name = "Notes";
	app.release = "1.0.0";
	return app;
}

/// The author of the packages below.
constexpr const char* plugin = "plugin:helper-pack";

/// @brief A package of kind "changes" from a base to a version of it: generated test data only.
/// @param base The base's document.
/// @param document The version's document.
/// @param state The state keys it brings, or null.
/// @return Its file.
std::string packageOf(const json& base, const json& document, const json& state = json()) {
	json value = {{"format", "affrescoui-package"},
	              {"version", 1},
	              {"kind", "changes"},
	              {"name", "Helper pack"},
	              {"release", "1.0.0"},
	              {"engine", "1.0.0"},
	              {"base",
	               {{"app", "notes"},
	                {"name", "Notes"},
	                {"release", "1.0.0"},
	                {"notation", "lisp"},
	                {"tree", format::documentTree(base)},
	                {"delta", json::diff(document, base)}}},
	              {"document", document}};
	if(!state.is_null()) value["state"] = state;
	return value.dump();
}

/// @brief The person's app's document with definitions added or replaced.
/// @param app The app.
/// @param definitions The definitions, by address in app.lisp.
/// @return The document.
json withDefinitions(const App& app, const json& definitions) {
	json document = app.repository.sourceDocument();
	for(const auto& [address, text] : definitions.items()) document["program"]["app.lisp"][address] = text;
	return document;
}

/// @brief Installs a package and accepts it as the person.
/// @param app The app.
/// @param text The package's file.
void installAndAccept(App& app, const std::string& text) {
	std::string error;
	const affresco::RevisionResult result = affresco::detail::proposePackage(notesApp(), app.repository, text, &error);
	REQUIRE_MESSAGE(result.status == RevisionStatus::Proposed, error);
	const affresco::ProposalInfo proposal = app.repository.proposals().back();
	const auto refused = app.repository.accept(proposal.id, {"user", proposal.head, false});
	REQUIRE_MESSAGE(!refused, refused.value_or(""));
	app.settle();
}

/// @brief Writes a file, making its folder.
/// @param path The file.
/// @param text What it holds.
void write(const std::filesystem::path& path, const std::string& text) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream(path, std::ios::binary) << text;
}

/// @brief A fresh folder for a test.
/// @param name Its name.
/// @return It, empty.
std::filesystem::path freshFolder(const std::string& name) {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / name;
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	return std::filesystem::canonical(root);
}

} // namespace

TEST_CASE("package attack: a package of changes that adds a section only the app as a whole brings is refused") {
	App app(personsApp, true);
	const json base = app.repository.sourceDocument();
	for(const auto& [section, value] : {std::pair<std::string, json>{"permissions", {{"allow", {"agent"}}}},
	                                    {"persist", json::array({"state.title"})},
	                                    {"connections", {{"feed", {{"url", "https://example.org/feed"}}}}},
	                                    {"services", {{"weather", {{"url", "https://example.org"}}}}}}) {
		CAPTURE(section);
		json document = base;
		document[section] = value;
		std::string error;
		CHECK(affresco::detail::proposePackage(notesApp(), app.repository, packageOf(base, document), &error).status ==
		      RevisionStatus::Rejected);
		CHECK_MESSAGE(error.contains("document." + section + ": a package of changes can't change it"), error);
	}
	CHECK(app.repository.proposals().empty());
}

TEST_CASE("package attack: advice on a person's command is listed and reviewed, and can't reach its control step") {
	App app(personsApp, true);
	const json document = withDefinitions(
	    app, {{"wrap", R"((defn wrap "" [proceed] (proceed)))"}, {"advice a around wrap", "(advice a :around wrap)"}});
	const std::string text = packageOf(app.repository.sourceDocument(), document);
	const json summary = affresco::detail::inspectPackage(notesApp(), app.repository, text, json::object());
	REQUIRE(summary["extensions"]["added"].size() == 1);
	CHECK(summary["extensions"]["added"][0]["kind"] == "advice");
	CHECK(summary["extensions"]["added"][0]["target"] == "a");
	CHECK(summary["extensions"]["removed"] == json::array());

	installAndAccept(app, text);
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK_MESSAGE(app.reported(plugin), (app.errors.empty() ? "" : app.errors.front()));
}

TEST_CASE("package attack: a definition the person changed is left out, never replaced") {
	App app(personsApp, true);
	const json base = app.repository.sourceDocument();
	app.write("helper", R"((defn helper "Mine, changed." [] 2))", "user");
	const std::string text = packageOf(base, withDefinitions(app, {{"helper", R"((defn helper "Theirs." [] 3))"}}));
	std::string error;
	REQUIRE(affresco::detail::proposePackage(notesApp(), app.repository, text, &error).status ==
	        RevisionStatus::Rejected);
	CHECK(error.contains("nothing merges cleanly"));
	// with something else that merges, the proposal opens, the definition left out for the person to choose
	json document = withDefinitions(app, {{"helper", R"((defn helper "Theirs." [] 3))"}});
	document["program"]["app.lisp"]["other"] = R"((defn other "" [] 4))";
	document = json(base).patch(json::diff(base, document));
	document["program"]["app.lisp"]["helper"] = R"((defn helper "Theirs." [] 3))";
	installAndAccept(app, packageOf(base, document));
	const json& program = app.repository.sourceDocument()["program"]["app.lisp"];
	CHECK(program["helper"] == R"((defn helper "Mine, changed." [] 2))");
	CHECK(program.contains("other"));
}

TEST_CASE("package attack: a state key the person has stays as it is") {
	App app(personsApp, true);
	app.repository.state()["title"] = "My own title";
	const json base = app.repository.sourceDocument();
	const std::string text = packageOf(base, withDefinitions(app, {{"other", R"((defn other "" [] 4))"}}),
	                                   {{"title", "Theirs"}, {"seen", "theirs"}, {"added", "theirs"}});
	installAndAccept(app, text);
	CHECK(app.state("title") == "My own title");
	CHECK(app.state("seen").is_null());
	CHECK(app.state("added") == "theirs");
}

TEST_CASE("package attack: an app's files that leave its folder, or a folder that is a link, are refused") {
	const std::filesystem::path root = freshFolder("affrescoui-package-files-attack-test");
	format::Package package;
	package.kind = format::PackageKind::App;
	for(const std::string path : {"../outside.json", "/etc/outside.json", "parts/../../outside.json", "./app.json"}) {
		CAPTURE(path);
		package.base.files = {{"app.json", {{"text", "{}"}}}, {path, {{"text", "{}"}}}};
		CHECK_THROWS_WITH(format::installAppFiles(package, root / "app"), doctest::Contains("inside the app's folder"));
		CHECK_FALSE(std::filesystem::exists(root / "app"));
		CHECK_FALSE(std::filesystem::exists(root / "outside.json"));
	}
	// a folder that is a link, even to nowhere, is a folder that exists
	package.base.files = {{"app.json", {{"text", "{}"}}}};
	std::filesystem::create_directory_symlink(root / "elsewhere", root / "link");
	CHECK_THROWS_WITH(format::installAppFiles(package, root / "link"), doctest::Contains("already exists"));
	CHECK_FALSE(std::filesystem::exists(root / "elsewhere"));
}

TEST_CASE("package attack: two of an app's files that this disk takes as one are refused, not one over the other") {
	const std::filesystem::path root = freshFolder("affrescoui-package-files-case-test");
	std::ofstream(root / "probe").put('x');
	const bool caseBlind = std::filesystem::exists(root / "PROBE");
	format::Package package;
	package.kind = format::PackageKind::App;
	package.base.files = {{"app.json", {{"text", "{}"}}}, {"APP.json", {{"text", R"({"hidden": true})"}}}};
	if(caseBlind) {
		CHECK_THROWS_WITH(format::installAppFiles(package, root / "app"), doctest::Contains("the same file on this disk"));
		CHECK_FALSE(std::filesystem::exists(root / "app"));
	} else {
		format::installAppFiles(package, root / "app");
		CHECK(std::filesystem::exists(root / "app" / "APP.json"));
	}
	// and a path with a "." part, which install would refuse, is refused when the package is read
	json app = json::parse(packageOf(json::object(), json::object()));
	app["kind"] = "app";
	app["base"]["files"] = {{"./app.json", {{"text", "{}"}}}, {"app.json", {{"text", "{}"}}}};
	CHECK_THROWS_WITH(static_cast<void>(format::readPackage(app.dump())), doctest::Contains("inside the app's folder"));
}

TEST_CASE("package attack: a package over a limit is refused before it is parsed") {
	App app(personsApp, true);
	std::string error;
	// not even JSON: the size is what refuses it
	const std::string huge(format::maxPackageBytes + 1, '[');
	CHECK(affresco::detail::proposePackage(notesApp(), app.repository, huge, &error).status ==
	      RevisionStatus::Rejected);
	CHECK_MESSAGE(error.contains("larger than 16 MiB"), error);
	// nested deeper than a screen may be: refused as it is parsed, never built
	const std::string deep = std::string(1000, '[') + std::string(1000, ']');
	CHECK(affresco::detail::proposePackage(notesApp(), app.repository, deep, &error).status ==
	      RevisionStatus::Rejected);
	CHECK_FALSE(error.empty());
	// the trusted publishers' file likewise
	const std::filesystem::path root = freshFolder("affrescoui-package-limits-attack-test");
	write(root / "publishers.json", std::string(format::maxTrustedPublishersBytes + 1, '{'));
	CHECK_THROWS_WITH(static_cast<void>(format::readTrustedPublishers(root / "publishers.json")),
	                  doctest::Contains("larger than 64 KiB"));
	CHECK(app.repository.proposals().empty());
}

TEST_CASE("package attack: an agent can't accept an install, nor settle what it left out") {
	App app(personsApp, true);
	const json base = app.repository.sourceDocument();
	app.write("helper", R"((defn helper "Mine, changed." [] 2))", "user");
	json document = base;
	document["program"]["app.lisp"]["helper"] = R"((defn helper "Theirs." [] 3))";
	document["program"]["app.lisp"]["other"] = R"((defn other "" [] 4))";
	std::string error;
	REQUIRE_MESSAGE(
	    affresco::detail::proposePackage(notesApp(), app.repository, packageOf(base, document), &error).status ==
	        RevisionStatus::Proposed,
	    error);
	const affresco::ProposalInfo proposal = app.repository.proposals().back();
	REQUIRE(proposal.leftOut.size() == 1);
	CHECK(app.repository.accept(proposal.id, {"agent/x", proposal.head, false}));
	const std::string path = proposal.leftOut[0]["path"];
	CHECK(app.repository.resolveLeftOut(proposal.id, path, true, "agent/x").value_or("").contains("only the person"));
	CHECK(app.repository.resolveLeftOut(proposal.id, path, true, plugin).value_or("").contains("its author"));
	CHECK(app.repository.proposals().back().leftOut.size() == 1);
	CHECK(app.repository.sourceDocument()["program"]["app.lisp"]["helper"] == R"((defn helper "Mine, changed." [] 2))");
}

TEST_CASE("package attack: nothing runs before it is accepted: a definition with a form that runs is refused") {
	App app(personsApp, true);
	const json base = app.repository.sourceDocument();
	for(const std::string text :
	    {R"((defn x "" [] 1) (call "hit"))", R"((set "state.seen" "ran"))", "(call \"hit\")"}) {
		CAPTURE(text);
		std::string error;
		CHECK(affresco::detail::proposePackage(notesApp(), app.repository,
		                                       packageOf(base, withDefinitions(app, {{"x", text}})), &error)
		          .status == RevisionStatus::Rejected);
		CHECK_MESSAGE(error.contains("document.program.app.lisp/x: holds a form that isn't a definition"), error);
	}
	CHECK(app.hits == 0);
	CHECK(app.state("seen").is_null());
	CHECK(app.repository.proposals().empty());
}

TEST_CASE("package attack: after it is accepted, a control step its code reaches is refused when it runs") {
	App app(personsApp, true);
	// a control step written in it is refused as a revision's would be
	std::string error;
	CHECK(affresco::detail::proposePackage(
	          notesApp(), app.repository,
	          packageOf(app.repository.sourceDocument(),
			            withDefinitions(app, {{"helper", R"((defn helper "Theirs." [] (call "hit")))"}})),
	          &error)
	          .status == RevisionStatus::Rejected);
	CHECK_MESSAGE(error.contains("call"), error);
	// advice on the person's function that holds one: run through the package's code, it is refused
	installAndAccept(app,
	                 packageOf(app.repository.sourceDocument(),
	                           withDefinitions(app, {{"twice", R"((defn twice "" [proceed] (proceed) (proceed)))"},
	                                                 {"advice save around twice", "(advice save :around twice)"}})));
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK_MESSAGE(app.reported(plugin), (app.errors.empty() ? "" : app.errors.front()));
	CHECK(app.repository.sourceDocument()["program"]["app.lisp"]["save"] ==
	      R"((defn save "Saves, through the application." [what] (call "hit")))");
}

TEST_CASE("package attack: a model change is listed, and no record is touched") {
	const std::filesystem::path root = freshFolder("affrescoui-package-models-attack-test");
	const auto writeApp = [](const std::filesystem::path& folder) {
		write(folder / "app.json", R"({ "format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes",
			"release": "1.0.0", "entry": "index.screen.json", "data": { "seed": { "notes": "seed/notes.csv" } } })");
		write(folder / "index.screen.json", R"json({ "format": "affrescoui-screen", "version": 2,
			"models": { "notes": { "version": 1, "fields": { "title": { "type": "string" } } } },
			"root": { "type": "label", "id": "title", "text": "Notes" } })json");
		write(folder / "seed" / "notes.csv", "id,title\nn1,First\n");
		return folder;
	};
	affresco::Screen sharer =
	    affresco::Screen::fromApp(writeApp(root / "sharer"), root / "sharer-data", affresco::Screen::History::OnDisk);
	const json migrations = json::parse(R"([{ "from": 1, "to": 2, "steps": [{ "rename": "title", "to": "name" }] }])");
	REQUIRE_FALSE(sharer.versions().commit(
	    {{"author", "studio"},
		 {"message", "Rename"},
		 {"patch",
		  {{{"op", "replace"}, {"path", "/models/notes/version"}, {"value", 2}},
		   {{"op", "replace"}, {"path", "/models/notes/fields"}, {"value", {{"name", {{"type", "string"}}}}}},
		   {{"op", "add"}, {"path", "/models/notes/migrations"}, {"value", migrations}}}}}));
	format::PackageOptions options;
	options.engine = "1.0.0";
	const std::string text = format::packageJson(sharer.exportPackage(options)).dump();

	affresco::Screen screen =
	    affresco::Screen::fromApp(writeApp(root / "person"), root / "person-data", affresco::Screen::History::OnDisk);
	const auto before = screen.data().get("notes", "n1");
	REQUIRE(before);
	const std::uint64_t changes = screen.data().changes();
	const json summary = screen.inspectPackage(text, json::object());
	CHECK(summary["models"] ==
	      json::array({{{"collection", "notes"}, {"from", 1}, {"to", 2}, {"migrations", migrations}}}));
	CHECK(summary["writes"] == json::array());
	std::string error;
	REQUIRE_MESSAGE(screen.proposePackage(text, &error).status == RevisionStatus::Proposed, error);
	CHECK(screen.data().changes() == changes);
	const auto after = screen.data().get("notes", "n1");
	REQUIRE(after);
	CHECK((after ? after->toJson() : json()) == (before ? before->toJson() : json()));
}
