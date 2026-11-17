/// @file
/// @brief The AffrescoUI catalogue (catalogue/ in the repository): an index that lists packages beside features, a
/// listed package reviewed and installed through the `packages` service as one from disk, a file that isn't the one
/// the index lists refused, an unsigned index refused unless the host was told to trust it, and `affrescoui catalogue
/// check` on a folder with one good entry and one broken. The keys are made when the tests run, every folder in the
/// temporary folder.

#include "FeatureCatalog.hpp"
#include "FeatureSignatures.hpp"
#include "packages/PackagesService.hpp"
#include "process/Process.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Package.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <utility>

using affresco::json;
using affresco::format::SigningKey;
namespace fs = std::filesystem;
namespace detail = affresco::detail;

namespace {

/// @brief Writes a file, making its folder.
/// @param path The file.
/// @param text What it holds.
void write(const fs::path& path, const std::string& text) {
	fs::create_directories(path.parent_path());
	std::ofstream(path, std::ios::binary) << text;
}

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its text, "" if there is none.
std::string read(const fs::path& path) {
	std::ifstream in(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/// @brief A fresh folder for a test.
/// @param name Its name.
/// @return It, empty.
fs::path freshFolder(const std::string& name) {
	const fs::path root = fs::temp_directory_path() / name;
	fs::remove_all(root);
	fs::create_directories(root);
	return fs::canonical(root);
}

/// @brief Writes an app "notes" of one label: generated test data only.
/// @param folder Its folder.
void writeNotes(const fs::path& folder) {
	write(folder / "app.json", R"({ "format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes",
		"release": "1.0.0", "entry": "index.screen.json" })");
	write(folder / "index.screen.json", R"json({ "format": "affrescoui-screen", "version": 2,
		"window": { "title": "Notes" },
		"state": { "title": "Notes" },
		"program": { "app.lisp": { "count": "(defn count [] 1)" } },
		"root": { "type": "label", "id": "title", "text": "{{state.title}}" } })json");
}

/// @brief The folders of a test: the examples (notes), the person's apps, the host's data and a catalogue.
struct Folders {
	fs::path top = {};       ///< All of them.
	fs::path examples = {};  ///< The examples' root.
	fs::path apps = {};      ///< The person's apps' root, writable.
	fs::path data = {};      ///< The host's data folder.
	fs::path catalogue = {}; ///< The catalogue's folder.

	/// @brief The grant the Launcher gets, with the catalogue and the keys it trusts to have signed its index.
	/// @param trusted Those keys.
	/// @param trustUnsigned Whether an unsigned index is read, as a development build may be told to.
	/// @return It.
	[[nodiscard]] affresco::PackagesGrant grant(std::vector<std::string> trusted, bool trustUnsigned = false) const {
		affresco::PackagesGrant grant;
		grant.roots = {{"examples", examples}, {"apps", apps}};
		grant.writable = "apps";
		grant.dataDirectory = data;
		grant.catalogue.folder = catalogue;
		grant.catalogue.trusted = std::move(trusted);
		grant.catalogue.trustUnsigned = trustUnsigned;
		return grant;
	}
};

/// @brief Makes the folders, and a package of notes: the person's copy changed by one commit and exported.
/// @param name The test's folder.
/// @param patch The commit's patch.
/// @param whole Whether the package is the whole app, else its changes.
/// @param package Set to the package.
/// @return The folders.
Folders withChange(const std::string& name, const json& patch, bool whole, json& package) {
	Folders folders;
	folders.top = freshFolder(name);
	folders.examples = folders.top / "examples";
	folders.apps = folders.top / "apps";
	folders.data = folders.top / "data";
	folders.catalogue = folders.top / "catalogue";
	writeNotes(folders.examples / "notes");
	writeNotes(folders.apps / "my-notes");
	affresco::Screen screen =
	    affresco::Screen::fromApp(folders.apps / "my-notes", folders.data, affresco::Screen::History::OnDisk);
	const auto error = screen.versions().commit({{"author", "user"}, {"message", "A change"}, {"patch", patch}});
	REQUIRE_MESSAGE(!error, error.value_or(""));
	const affresco::PackagesGrant grant = folders.grant({});
	package = detail::exportVersion(grant,
	                                {{"root", "apps"},
	                                 {"app", "my-notes"},
	                                 {"name", "Notes that count two"},
	                                 {"description", "Two."},
	                                 {"whole", whole}},
	                                {})["package"];
	fs::remove_all(folders.apps / "my-notes");
	return folders;
}

/// @brief Makes the folders, and a package of notes that counts two: the person's copy changed by one commit and
/// exported as its changes.
/// @param name The test's folder.
/// @param package Set to the package.
/// @return The folders.
Folders withPackage(const std::string& name, json& package) {
	return withChange(name, {{{"op", "replace"}, {"path", "/program/app.lisp/count"}, {"value", "(defn count [] 2)"}}},
	                  false, package);
}

/// @brief Writes a package into a catalogue's folder and an index that lists it, beside no feature.
/// @param folder The catalogue's folder.
/// @param package The package.
/// @return The index, unsigned.
json listPackage(const fs::path& folder, const json& package) {
	const std::string bytes = package.dump(1, '\t');
	write(folder / "notes-two" / "1.0.0.affresco.json", bytes);
	return {{"format", "affrescoui-catalog"},
	        {"version", 2},
	        {"name", "test"},
	        {"updated", "2026-11-17"},
	        {"features", json::object()},
	        {"packages",
	         {{"notes-two",
	           {{"title", "Notes that count two"},
	            {"description", "Two."},
	            {"base", "notes"},
	            {"releases",
	             {{{"release", "1.0.0"},
	               {"url", "notes-two/1.0.0.affresco.json"},
	               {"digest", affresco::format::digestOf(bytes)},
	               {"size", bytes.size()},
	               {"base", {{"app", "notes"}, {"release", "1.0.0"}}}}}}}}}}};
}

/// @brief The failure a call of the service ends with.
/// @param work The call.
/// @return Its code and message, or two empty texts if it succeeded.
std::pair<std::string, std::string> failure(const std::function<void()>& work) {
	try {
		work();
	} catch(const detail::PackagesFailure& refused) {
		return {refused.code(), refused.what()};
	}
	return {};
}

#ifdef AFFRESCOUI_CLI
/// @brief Runs `affrescoui catalogue check` on a folder, from a child started with no shell.
/// @param folder The catalogue's folder.
/// @param examples The examples' folder.
/// @param said Set to the lines it printed.
/// @return Its exit code.
int runCheck(const fs::path& folder, const fs::path& examples, std::string& said) {
	const auto child = affresco::services::ChildProcess::start(
	    {AFFRESCOUI_CLI, {"catalogue", "check", folder.string(), "--examples", examples.string()}, {}, std::nullopt});
	child->closeInput();
	said.clear();
	std::array<char, 4096> buffer{};
	for(std::ptrdiff_t count; (count = child->read(buffer.data(), buffer.size())) > 0;)
		said.append(buffer.data(), static_cast<std::size_t>(count));
	return child->wait();
}

/// @brief Lists a package of notes in a catalogue for the check: with a licence and a proof, its base the examples'
/// notes (`<examples>/notes/json`, as `examples/` holds an app), and an unsigned index.
/// @param folders The test's folders.
/// @param package The package.
/// @return The examples' folder the check reads.
fs::path listForCheck(const Folders& folders, json package) {
	package["license"] = "Apache-2.0";
	package["proof"] = {
	    {"scenarios", {{{"name", "opens"}, {"steps", {{{"expect", {{"state", "title"}, {"is", "Notes"}}}}}}}}}};
	write(folders.catalogue / "index.json", listPackage(folders.catalogue, package).dump(1, '\t'));
	const fs::path examples = folders.top / "check-examples";
	writeNotes(examples / "notes" / "json");
	return examples;
}
#endif

} // namespace

TEST_CASE("catalogue: an index with packages lists them, and a listed package is reviewed and installed as a file") {
	json package;
	const Folders folders = withPackage("affrescoui-catalogue-listed", package);
	const SigningKey release = SigningKey::generate();
	write(folders.catalogue / "index.json", release.sign(listPackage(folders.catalogue, package)).dump(1, '\t'));
	const affresco::PackagesGrant grant = folders.grant({release.publisher()});

	const json listed = detail::catalogueEntries(grant);
	CHECK(listed["name"] == "test");
	CHECK(listed["signed"] == true);
	REQUIRE(listed["entries"].size() == 1);
	CHECK(listed["entries"][0] == json{{"id", "notes-two"},
	                                   {"kind", "package"},
	                                   {"title", "Notes that count two"},
	                                   {"description", "Two."},
	                                   {"release", "1.0.0"},
	                                   {"base", "notes"},
	                                   {"installed", false}});

	const json reviewed = detail::inspectVersion(grant, {{"catalogue", "notes-two"}});
	CHECK(reviewed["summary"]["kind"] == "changes");
	CHECK(reviewed["summary"]["problems"].empty());
	CHECK(reviewed["listed"] == json{{"catalogue", "test"}, {"updated", "2026-11-17"}, {"signed", true}});

	const json installed = detail::installVersion(
	    grant, {{"catalogue", "notes-two"}, {"release", "1.0.0"}, {"id", "counting"}, {"name", "Counting"}}, {});
	CHECK(installed["proposal"] == true); // its changes wait for the person, as for a file
	CHECK(fs::exists(folders.apps / "counting" / "index.screen.json"));
	CHECK(detail::catalogueEntries(grant)["entries"][0]["installed"] == false); // another id than the entry's

	CHECK(failure([&] {
		      (void)detail::inspectVersion(grant, {{"catalogue", "notes-two"}, {"release", "2.0.0"}});
	      }).first == "notFound");
	CHECK(failure([&] { (void)detail::inspectVersion(grant, {{"catalogue", "nothing"}}); }).first == "notFound");
	CHECK(failure([&] {
		      (void)detail::inspectVersion(grant, {{"catalogue", "notes-two"}, {"package", package}});
	      }).first != "");
	const std::string all = listed.dump() + reviewed.dump() + installed.dump();
	CHECK(all.find(folders.top.string()) == std::string::npos);
}

TEST_CASE("catalogue: an index of features only reads as before") {
	const fs::path folder = freshFolder("affrescoui-catalogue-features-only");
	const json index = {
	    {"format", "affrescoui-catalog"}, {"version", 2}, {"name", "test"}, {"features", json::object()}};
	const affresco::format::CatalogIndex read = affresco::format::readUnsignedCatalog(index.dump(), folder);
	CHECK(read.packages.empty());
	CHECK(read.features.empty());
}

TEST_CASE("catalogue: a package that isn't the one the index lists is refused") {
	json package;
	const Folders folders = withPackage("affrescoui-catalogue-digest", package);
	const SigningKey release = SigningKey::generate();
	const json index = listPackage(folders.catalogue, package);
	write(folders.catalogue / "index.json", release.sign(index).dump(1, '\t'));
	const affresco::PackagesGrant grant = folders.grant({release.publisher()});
	// Its description changed, its size kept: only the digest tells
	const fs::path file = folders.catalogue / "notes-two" / "1.0.0.affresco.json";
	std::string bytes = read(file);
	const auto at = bytes.find("\"Two.\"");
	REQUIRE(at != std::string::npos);
	bytes.replace(at, 6, "\"Tw0.\"");
	write(file, bytes);
	const auto [code, message] = failure([&] { (void)detail::inspectVersion(grant, {{"catalogue", "notes-two"}}); });
	CHECK(code == "refused");
	CHECK(message.find("its digest isn't the catalogue's") != std::string::npos);
	CHECK(failure([&] {
		      (void)detail::installVersion(grant, {{"catalogue", "notes-two"}, {"id", "counting"}, {"name", "C"}}, {});
	      }).first == "refused");
	CHECK(!fs::exists(folders.apps / "counting"));
}

TEST_CASE("catalogue: an unsigned index is refused, unless the host was told to trust it") {
	json package;
	const Folders folders = withPackage("affrescoui-catalogue-unsigned", package);
	const SigningKey release = SigningKey::generate();
	write(folders.catalogue / "index.json", listPackage(folders.catalogue, package).dump(1, '\t'));

	const auto [code, message] = failure([&] { (void)detail::catalogueEntries(folders.grant({release.publisher()})); });
	CHECK(code == "refused");
	CHECK(message.find("unsigned") != std::string::npos);
	CHECK(failure([&] { (void)detail::inspectVersion(folders.grant({}), {{"catalogue", "notes-two"}}); }).first ==
	      "refused");

	const json listed = detail::catalogueEntries(folders.grant({}, true)); // a development build's explicit flag
	CHECK(listed["signed"] == false);
	CHECK(listed["entries"].size() == 1);

	// Signed by a key the host doesn't trust: refused as well
	write(folders.catalogue / "index.json",
	      SigningKey::generate().sign(listPackage(folders.catalogue, package)).dump(1, '\t'));
	CHECK(failure([&] { (void)detail::catalogueEntries(folders.grant({release.publisher()})); }).first == "refused");
	// No catalogue granted: the Launcher hides its page
	affresco::PackagesGrant none = folders.grant({});
	none.catalogue = {};
	CHECK(failure([&] { (void)detail::catalogueEntries(none); }).first == "notFound");
}

TEST_CASE("catalogue: a feature is reviewed from the catalogue, and installed only from inside an app") {
	const fs::path folder = freshFolder("affrescoui-catalogue-feature");
	const std::string bytes = read(fs::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "sinopia" / "json" / "features" /
	                               "slide-sorter" / "feature.json");
	REQUIRE(!bytes.empty());
	write(folder / "slideSorter" / "1.0.0.json", bytes);
	const json index = {{"format", "affrescoui-catalog"},
	                    {"version", 2},
	                    {"name", "test"},
	                    {"features",
	                     {{"slideSorter",
	                       {{"title", "Slide sorter"},
	                        {"releases",
	                         {{{"release", "1.0.0"},
	                           {"url", "slideSorter/1.0.0.json"},
	                           {"digest", affresco::format::digestOf(bytes)},
	                           {"size", bytes.size()}}}}}}}}};
	write(folder / "index.json", index.dump(1, '\t'));
	affresco::PackagesGrant grant;
	grant.catalogue.folder = folder;
	grant.catalogue.trustUnsigned = true;
	const json listed = detail::catalogueEntries(grant);
	REQUIRE(listed["entries"].size() == 1);
	CHECK(listed["entries"][0]["kind"] == "feature");
	const json reviewed = detail::inspectVersion(grant, {{"catalogue", "slideSorter"}});
	CHECK(reviewed["summary"]["kind"] == "feature");
	CHECK(reviewed["summary"]["license"] == "Apache-2.0");
	CHECK(!reviewed["summary"]["definitions"]["added"].empty());
	CHECK(failure([&] {
		      (void)detail::installVersion(grant, {{"catalogue", "slideSorter"}, {"id", "s"}, {"name", "S"}}, {});
	      }).first != "");
}

#ifdef AFFRESCOUI_CLI
TEST_CASE("catalogue: the check passes a good entry and names the broken one") {
	const fs::path folder = freshFolder("affrescoui-catalogue-check") / "catalogue";
	const fs::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	const std::string sorter = read(examples / "sinopia" / "json" / "features" / "slide-sorter" / "feature.json");
	json broken = json::parse(read(examples / "forge" / "json" / "features" / "word-count" / "feature.json"));
	broken.erase("signature");
	broken.erase("license"); // what the check must name
	const std::string brokenBytes = broken.dump(1, '\t');
	write(folder / "slideSorter" / "1.0.0.json", sorter);
	write(folder / "wordCount" / "1.0.0.json", brokenBytes);
	const auto release = [](const std::string& url, const std::string& bytes) {
		return json{
		    {"release", "1.0.0"}, {"url", url}, {"digest", affresco::format::digestOf(bytes)}, {"size", bytes.size()}};
	};
	json index = {
	    {"format", "affrescoui-catalog"},
	    {"version", 2},
	    {"name", "test"},
	    {"features",
		 {{"slideSorter", {{"title", "Slide sorter"}, {"releases", {release("slideSorter/1.0.0.json", sorter)}}}},
		  {"wordCount", {{"title", "Word count"}, {"releases", {release("wordCount/1.0.0.json", brokenBytes)}}}}}}};
	write(folder / "index.json", index.dump(1, '\t'));
	std::string said;
	const auto check = [&] { return runCheck(folder, examples, said); };
	CHECK(check() == 1);
	INFO(said);
	CHECK(said.find("wordCount 1.0.0") != std::string::npos);
	CHECK(said.find("licen") != std::string::npos);
	CHECK(said.find("slideSorter") == std::string::npos); // the good entry has no line

	index["features"].erase("wordCount");
	write(folder / "index.json", index.dump(1, '\t'));
	CHECK(check() == 0);
}

TEST_CASE("catalogue: the check names a service an entry's changes use and its base app doesn't") {
	json package;
	const Folders folders = withPackage("affrescoui-catalogue-check-uses", package);
	REQUIRE(package["kind"] == "changes");
	// An action that calls a service, written into the file: its base, rebuilt from the delta, has none
	package["document"]["actions"] = {{"look", {{{"invoke", "packages.catalogue"}}}}};
	package["base"]["delta"].push_back({{"op", "remove"}, {"path", "/actions"}});
	const fs::path examples = listForCheck(folders, package);
	std::string said;
	CHECK(runCheck(folders.catalogue, examples, said) == 1); // a problem named, not a check that stops
	INFO(said);
	CHECK(said.find("notes-two 1.0.0: it uses packages.catalogue") != std::string::npos);
}

TEST_CASE("catalogue: the check refuses a whole app that changes its base app's permissions") {
	json whole;
	const Folders folders = withChange(
	    "affrescoui-catalogue-check-permissions",
	    {{{"op", "replace"}, {"path", "/program/app.lisp/count"}, {"value", "(defn count [] 2)"}}}, true, whole);
	REQUIRE(whole["kind"] == "app");
	// Agents' changes would land unreviewed: the base, rebuilt from the delta, is still the examples' notes
	whole["document"]["permissions"] = {{"allow", {"agent"}}};
	whole["base"]["delta"].push_back({{"op", "remove"}, {"path", "/permissions"}});
	const fs::path examples = listForCheck(folders, whole);
	std::string said;
	CHECK(runCheck(folders.catalogue, examples, said) == 1);
	INFO(said);
	CHECK(said.find("notes-two 1.0.0: it changes permissions") != std::string::npos);
}
#endif
