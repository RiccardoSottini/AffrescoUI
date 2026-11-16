/// @file
/// @brief Writing a package (PackageExport.cpp, Repository::versionParts(), Screen::exportPackage()): a version holds
/// its document and the base it grew from, the state keys and assets its commits added, and nothing about the person:
/// not their records, saved or live state, history, paths or keys, nor a file of the app's folder its manifest doesn't
/// reach.

#include <affresco/affresco.hpp>
#include <affresco/core/Assets.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/PackageFiles.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using affresco::json;
namespace format = affresco::format;

namespace {

/// @brief Writes a file, making its folder.
/// @param path The file.
/// @param text What it holds.
void write(const std::filesystem::path& path, const std::string& text) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream(path, std::ios::binary) << text;
}

/// A secret-looking text in a stray file of the folder: generated test data only.
const std::string strayKey = "sk-" + std::string(24, 'q');

/// @brief Writes an app: an entry that includes a part (which includes another, from its own folder) and the shared
/// kit, a theme, data models with a seed file, an icon and a folder of pictures, and two stray files its manifest
/// doesn't reach. Generated test data only.
/// @param root The folder the app's folder and the shared folder are made in.
/// @return The app's folder.
std::filesystem::path writeApp(const std::filesystem::path& root) {
	const std::filesystem::path app = root / "apps" / "tasks";
	write(root / "apps" / "shared" / "kit.json", R"({ "tokens": { "kitGap": 2 } })");
	write(app / "app.json", R"json({ "format": "affrescoui-app", "version": 1, "id": "tasks", "name": "Tasks",
		"release": "1.0.0", "icon": "icon.svg", "entry": "index.screen.json", "themes": ["theme.json"],
		"data": { "models": "models.json", "seed": { "notes": "seed/notes.csv" } }, "assets": ["pictures"] })json");
	write(app / "index.screen.json", R"json({ "format": "affrescoui-screen", "version": 2,
		"include": ["parts/look.json", "../shared/kit.json"], "persist": ["state.draft"],
		"state": { "title": "Tasks", "draft": "", "columns": ["To do", "Done"] },
		"root": { "type": "label", "id": "title", "text": "{{state.title}}" } })json");
	write(app / "parts" / "look.json", R"({ "include": "strings.json", "tokens": { "gap": 4 } })");
	write(app / "parts" / "strings.json", R"({ "strings": { "en": { "hello": "Hello" } } })");
	write(app / "theme.json", R"({ "tokens": { "radius": 6 } })");
	write(app / "models.json", R"json({ "models": { "notes": { "version": 1, "fields": {
		"title": { "type": "string", "required": true } } } } })json");
	write(app / "seed" / "notes.csv", "id,Title\nn1,First\n");
	write(app / "icon.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"/>)");
	write(app / "pictures" / "dot.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2"/>)");
	write(app / ".env", "API_KEY=" + strayKey + "\n");
	write(app / "notes.txt", "Ada's own notes: not part of the app.\n");
	return app;
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

/// @brief Commits a change by an author.
/// @param screen The screen.
/// @param author Its author.
/// @param message Its message.
/// @param patch Its operations.
void commitBy(affresco::Screen& screen, const std::string& author, const std::string& message, const json& patch) {
	const auto error = screen.versions().commit({{"author", author}, {"message", message}, {"patch", patch}});
	REQUIRE_MESSAGE(!error, error.value_or(""));
}

/// @brief What the package's options are in these tests.
/// @return Them.
format::PackageOptions options() {
	format::PackageOptions result;
	result.name = "Tasks with a Blocked column";
	result.release = "1.1.0";
	result.engine = "1.0.0";
	return result;
}

} // namespace

TEST_CASE("packages export: an app's files are the ones its manifest reaches, never a stray file or a shared one") {
	const std::filesystem::path root = freshFolder("affrescoui-package-files-test");
	const std::filesystem::path app = writeApp(root);
	const json files = format::appFiles(app / "app.json");
	std::set<std::string> names;
	for(const auto& [name, unused] : files.items()) names.insert(name);
	CHECK(names == std::set<std::string>{"app.json", "icon.svg", "index.screen.json", "parts/look.json",
	                                     "parts/strings.json", "theme.json", "models.json", "seed/notes.csv",
	                                     "pictures/dot.svg"});
	CHECK(files["seed/notes.csv"]["text"] == "id,Title\nn1,First\n");

	// a file it reaches outside its folder (and not in the shared one) refuses the export, naming it
	write(root / "private.json", "{}");
	write(app / "parts" / "look.json", R"({ "include": "../../../private.json" })");
	CHECK_THROWS_WITH_AS(static_cast<void>(format::appFiles(app / "app.json")),
	                     doctest::Contains("parts/../../../private.json: outside the app's folder"),
	                     affresco::DocumentError);
	// and so does a link out of it, in a folder of assets
	write(app / "parts" / "look.json", "{}");
	std::filesystem::create_symlink(root / "private.json", app / "pictures" / "link.json");
	CHECK_THROWS_WITH_AS(static_cast<void>(format::appFiles(app / "app.json")),
	                     doctest::Contains("outside the app's folder"), affresco::DocumentError);
	std::filesystem::remove(app / "pictures" / "link.json");
	// a file the manifest names that isn't there
	std::filesystem::remove(app / "theme.json");
	CHECK_THROWS_WITH_AS(static_cast<void>(format::appFiles(app / "app.json")),
	                     doctest::Contains("theme.json: can't be read"), affresco::DocumentError);
	std::filesystem::remove_all(root);
}

TEST_CASE("packages export: five commits by three authors leave no trace, nor the records, the state or the paths") {
	const std::filesystem::path root = freshFolder("affrescoui-package-export-test");
	const std::filesystem::path app = writeApp(root);
	const std::filesystem::path data = root / "data";
	std::string dumped;
	std::vector<affresco::CommitInfo> log;
	{
		affresco::Screen screen = affresco::Screen::fromApp(app, data);
		commitBy(screen, "Ada", "Adds the Blocked column (note-1)",
		         json::parse(R"([{ "op": "add", "path": "/state/columns/1", "value": "Blocked" }])"));
		commitBy(screen, "Boris", "Keeps why a card waits (note-2)",
		         json::parse(R"([{ "op": "add", "path": "/state/blockedReason", "value": "" },
		                         { "op": "add", "path": "/state/limit", "value": 3 }])"));
		commitBy(screen, "Chiara", "A scratch key (note-3)",
		         json::parse(R"([{ "op": "add", "path": "/state/scratch", "value": true }])"));
		commitBy(screen, "Ada", "Raises the limit (note-4)",
		         json::parse(R"([{ "op": "replace", "path": "/state/limit", "value": 5 }])"));
		commitBy(screen, "Boris", "Drops the scratch key, renames (note-5)",
		         json::parse(R"([{ "op": "remove", "path": "/state/scratch" },
		                         { "op": "replace", "path": "/state/title", "value": "Team tasks" },
		                         { "op": "replace", "path": "/root/text", "value": "{{state.title}}: {{state.limit}}" }])"));
		// what the person keeps: a record, and the live (and saved) state
		static_cast<void>(screen.data().insert("notes", {{"title", "Ada's private record"}}, "Ada"));
		screen.versions().state()["draft"] = "Ada's private draft";
		log = screen.log();
		REQUIRE(log.size() == 6);

		// the state keys the commits added, as the last of them left each; never one the base had, nor one removed
		const format::Package package = screen.exportPackage(options());
		CHECK(package.state == json{{"blockedReason", ""}, {"limit", 5}});
		CHECK(package.document["root"]["text"] == "{{state.title}}: {{state.limit}}");
		CHECK(format::baseOf(package)["root"]["text"] == "{{state.title}}");
		CHECK(package.document["state"]["title"] == "Tasks"); // a key the base has changes the person's state only
		CHECK(package.base.app == "tasks");
		CHECK(package.base.name == "Tasks");
		CHECK(package.base.release == "1.0.0");
		CHECK(package.base.notation == "json");
		CHECK(package.base.files.is_null());
		CHECK(format::scrubProblems(package, std::vector<std::string>{root.string(), data.string(), app.string()})
		          .empty());
		dumped = format::packageJson(package).dump();

		// what the person has, the app as its files are, is the base exactly
		affresco::Screen fresh = affresco::Screen::fromApp(app, {}, affresco::Screen::History::InMemory);
		const json installed = fresh.versions().sourceDocument();
		CHECK(format::packageSummary(package, installed, json::object())["base"]["exact"] == true);
	}
	// history: no commit id, author, committer, time or message
	for(const affresco::CommitInfo& commit : log) {
		CHECK_FALSE(dumped.contains(commit.id));
		CHECK_FALSE(dumped.contains(commit.id.substr(0, 12)));
		if(!commit.time.empty()) CHECK_FALSE(dumped.contains(commit.time));
		CHECK_FALSE(dumped.contains(commit.message));
	}
	for(const std::string text : {"Ada", "Boris", "Chiara", "note-1", "note-5"}) CHECK_FALSE(dumped.contains(text));
	// records and state
	CHECK_FALSE(dumped.contains("private record"));
	CHECK_FALSE(dumped.contains("private draft"));
	// paths and stray files
	CHECK_FALSE(dumped.contains(root.string()));
	CHECK_FALSE(dumped.contains(strayKey));
	CHECK_FALSE(dumped.contains("own notes"));
	std::filesystem::remove_all(root);
}

TEST_CASE("packages export: a whole app holds its files, and its base is the app's") {
	const std::filesystem::path root = freshFolder("affrescoui-package-whole-test");
	const std::filesystem::path app = writeApp(root);
	affresco::Screen screen = affresco::Screen::fromApp(app, root / "data");
	commitBy(screen, "Ada", "Blocked",
	         json::parse(R"([{ "op": "add", "path": "/state/columns/1", "value": "Blocked" }])"));
	format::PackageOptions whole = options();
	whole.kind = format::PackageKind::App;
	const format::Package package = screen.exportPackage(whole);
	CHECK(package.kind == format::PackageKind::App);
	CHECK(package.base.files.contains("app.json"));
	CHECK(package.base.files.contains("pictures/dot.svg"));
	CHECK_FALSE(package.base.files.contains(".env"));
	CHECK_FALSE(package.base.files.contains("notes.txt"));
	CHECK(format::packageJson(format::readPackage(format::packageJson(package).dump())) ==
	      format::packageJson(package));
	std::filesystem::remove_all(root);
}

TEST_CASE("packages export: a screen that isn't an app's has no package") {
	affresco::Screen screen = affresco::Screen::fromString(
	    R"({ "format": "affrescoui-screen", "version": 2, "root": { "type": "label", "id": "a", "text": "a" } })");
	CHECK_THROWS_WITH_AS(static_cast<void>(screen.exportPackage(options())), doctest::Contains("isn't an app's"),
	                     affresco::DocumentError);
}

TEST_CASE("packages export: a program's definitions, added and changed, are in its document; the base is the app's") {
	const std::filesystem::path root = freshFolder("affrescoui-package-program-test");
	const std::filesystem::path folder = std::filesystem::path(AFFRESCOUI_EXAMPLES_ASSETS) / "flow" / "lisp";
	affresco::Screen screen = affresco::Screen::fromApp(folder, root / "data");
	const json program = screen.versions().sourceDocument()["program"];
	REQUIRE(program.is_object());
	const std::string file = program.begin().key();
	REQUIRE(program[file].is_object());
	commitBy(screen, "Ada", "A definition",
	         {{{"op", "add"},
	           {"path", "/program/" + file + "/blockedName"},
	           {"value", "(defn blockedName [] \"Blocked\")"}}});
	const format::Package package = screen.exportPackage(options());
	CHECK(package.base.notation == "lisp");
	const json summary = format::packageSummary(package, format::baseOf(package), json::object());
	CHECK(summary["base"]["exact"] == true);
	CHECK(summary["definitions"]["added"] == json::array({file + "/blockedName"}));
	CHECK(summary["definitions"]["removed"].empty());
	std::filesystem::remove_all(root);
}

TEST_CASE("packages export: the scan refuses the person's paths and anything that looks like a key, naming only the "
          "place") {
	const std::filesystem::path root = freshFolder("affrescoui-package-scrub-test");
	const std::filesystem::path app = writeApp(root);
	affresco::Screen screen = affresco::Screen::fromApp(app, root / "data");
	const format::Package clean = screen.exportPackage(options());
	const std::vector<std::string> privateTexts{root.string()};
	REQUIRE(format::scrubProblems(clean, privateTexts).empty());

	const auto problemsWith = [&](const std::string& text) {
		format::Package package = clean;
		package.document["state"]["leak"] = text;
		return format::scrubProblems(package, privateTexts);
	};
	const std::vector<std::pair<std::string, std::string>> leaks = {
	    {(root / "data" / "tasks").string(), "one of your own paths"},
	    {"-----BEGIN OPENSSH PRIVATE KEY-----", "a private key"},
	    {"key " + strayKey, "an API key"},
	    {"ghp_" + std::string(36, 'x'), "a code host's token"},
	    {"xoxb-" + std::string(20, '1'), "a chat service's token"},
	    {"AKIA" + std::string(16, 'Z'), "a cloud access key"},
	    {"Authorization: Bearer " + std::string(24, 'a'), "a bearer token"}};
	for(const auto& [text, kind] : leaks) {
		const std::vector<std::string> problems = problemsWith(text);
		REQUIRE_MESSAGE(problems.size() == 1, kind);
		CHECK(problems[0].starts_with("document.state.leak: holds "));
		CHECK(problems[0].contains(kind));
		CHECK_FALSE(problems[0].contains(text.substr(text.size() - 12))); // no part of the match is printed
	}
	// what only resembles one is fine
	for(const std::string text : {"sk-short", "AKIA-not", "Bearer token", "xox", "a task", "boxed-in"})
		CHECK_MESSAGE(problemsWith(text).empty(), text);
	// a key that is a path is named by the place it is in, not by itself
	format::Package keyed = clean;
	keyed.document["state"][root.string()] = 1;
	const std::vector<std::string> problems = format::scrubProblems(keyed, privateTexts);
	REQUIRE(problems.size() == 1);
	CHECK(problems[0].starts_with("document.state, a key: holds one of your own paths"));
	// bytes of a file or an asset are scanned as they are, not as base64
	format::Package whole = clean;
	whole.base.files = {{"seed/notes.csv", {{"data", affresco::encodeBase64("x," + strayKey)}}}};
	CHECK(format::scrubProblems(whole, privateTexts).size() == 1);
	std::filesystem::remove_all(root);
}
