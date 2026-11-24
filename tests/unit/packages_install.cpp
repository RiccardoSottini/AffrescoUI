/// @file
/// @brief Installing a package (PackageInstall.cpp, Screen::proposePackage(), Repository::resolveLeftOut()): the
/// summary the person reviews, a proposal only the person accepts, what doesn't merge left out for the person to
/// choose, and what refuses one.

#include <affresco/affresco.hpp>
#include <affresco/format/PackageFiles.hpp>
#include <affresco/format/PackageInstall.hpp>

#include "FeatureSignatures.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
namespace format = affresco::format;

namespace {

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

/// @brief Writes an app "notes" whose screen holds a program of two definitions: generated test data only.
/// @param folder Its folder.
/// @param id Its id.
/// @return The folder.
std::filesystem::path writeApp(const std::filesystem::path& folder, const std::string& id = "notes") {
	write(folder / "app.json", R"({ "format": "affrescoui-app", "version": 1, "id": ")" + id +
	                               R"(", "name": "Notes", "release": "1.0.0", "entry": "index.screen.json" })");
	write(folder / "index.screen.json", R"json({ "format": "affrescoui-screen", "version": 2,
		"state": { "title": "Notes" },
		"program": { "app.lisp": { "count": "(defn count [] 1)", "label": "(defn label [] \"Notes\")" } },
		"root": { "type": "label", "id": "title", "text": "{{state.title}}" } })json");
	return folder;
}

/// @brief Commits a change by an author.
/// @param screen The screen.
/// @param author Its author.
/// @param patch Its operations.
void commitBy(affresco::Screen& screen, const std::string& author, const json& patch) {
	const auto error = screen.versions().commit({{"author", author}, {"message", "A change"}, {"patch", patch}});
	REQUIRE_MESSAGE(!error, error.value_or(""));
}

/// @brief The package of a version of "notes": its count changed, a definition added and a state key it brings.
/// @param root Where the sharer's app and data are made.
/// @param kind Its kind.
/// @return The package's file.
std::string sharedVersion(const std::filesystem::path& root, format::PackageKind kind = format::PackageKind::Changes) {
	const std::filesystem::path app = writeApp(root / "sharer" / "notes");
	affresco::Screen screen = affresco::Screen::fromApp(app, root / "sharer-data", affresco::Screen::History::OnDisk);
	commitBy(screen, "studio",
	         {{{"op", "replace"}, {"path", "/program/app.lisp/count"}, {"value", "(defn count [] 2)"}},
	          {{"op", "add"}, {"path", "/program/app.lisp/blocked"}, {"value", "(defn blocked [] \"Blocked\")"}},
	          {{"op", "add"}, {"path", "/state/blockedReason"}, {"value", "waiting"}}});
	format::PackageOptions options;
	options.kind = kind;
	options.name = "Notes with a Blocked column";
	options.release = "1.1.0";
	options.engine = "1.0.0";
	return format::packageJson(screen.exportPackage(options)).dump();
}

/// @brief The person's copy of "notes".
/// @param root Where it is made.
/// @param id Its app's id.
/// @return The screen, with its history in its own data folder.
affresco::Screen receiver(const std::filesystem::path& root, const std::string& id = "notes") {
	return affresco::Screen::fromApp(writeApp(root / "person" / id, id), root / "person-data",
	                                 affresco::Screen::History::OnDisk);
}

/// @brief A definition of the screen's program.
/// @param screen The screen.
/// @param name Its name in app.lisp.
/// @return Its text, or null.
json definition(affresco::Screen& screen, const std::string& name) {
	const json& program = screen.versions().sourceDocument()["program"]["app.lisp"];
	return program.contains(name) ? program[name] : json();
}

/// @brief Why the trusted publishers' file is refused.
/// @param text Its text.
/// @return The error, or "" if it is read.
std::string trustRefusal(const std::string& text) {
	try {
		static_cast<void>(format::parseTrustedPublishers(text));
		return "";
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
}

/// @brief Proposes a package and requires that it was.
/// @param screen The screen.
/// @param text The package's file.
/// @return The proposal.
affresco::ProposalInfo proposed(affresco::Screen& screen, const std::string& text) {
	std::string error;
	const affresco::RevisionResult result = screen.proposePackage(text, &error);
	REQUIRE_MESSAGE(result.status == RevisionStatus::Proposed, error);
	return screen.versions().proposals().back();
}

/// @brief Accepts a proposal as the person.
/// @param screen The screen.
/// @param proposal Its id.
void acceptAsPerson(affresco::Screen& screen, int proposal) {
	for(const affresco::ProposalInfo& open : screen.versions().proposals())
		if(open.id == proposal) {
			const auto error = screen.versions().accept(proposal, {"user", open.head, false});
			REQUIRE_MESSAGE(!error, error.value_or(""));
			return;
		}
	FAIL("no proposal " << proposal);
}

} // namespace

TEST_CASE("packages install: on the exact base, a proposal only the person accepts, as the package's plugin") {
	const std::filesystem::path root = freshFolder("affrescoui-install-exact-test");
	const std::string text = sharedVersion(root);
	affresco::Screen screen = receiver(root);

	const json summary = screen.inspectPackage(text, json::object());
	CHECK(summary["base"]["exact"] == true);
	CHECK(summary["base"]["installedRelease"] == "1.0.0");
	CHECK(summary["problems"] == json::array());
	CHECK(summary["conflicts"] == json::array());
	CHECK(summary["impact"]["logic"].get<int>() > 0);
	CHECK(summary["definitions"]["added"] == json::array({"app.lisp/blocked"}));

	const affresco::ProposalInfo proposal = proposed(screen, text);
	CHECK(proposal.author == "plugin:notes-with-a-blocked-column");
	CHECK(proposal.message == "Install Notes with a Blocked column 1.1.0");
	CHECK(proposal.leftOut == json::array());
	CHECK(definition(screen, "count") == "(defn count [] 1)"); // nothing is applied before it is accepted

	// only the person accepts it: not its author, not another reviewer
	CHECK(screen.versions().accept(proposal.id, {"agent/helper", proposal.head, false}));
	acceptAsPerson(screen, proposal.id);
	CHECK(definition(screen, "count") == "(defn count [] 2)");
	CHECK(definition(screen, "blocked") == "(defn blocked [] \"Blocked\")");
	CHECK(screen.versions().state()["blockedReason"] == "waiting");
	CHECK(screen.versions().programAuthors()["app.lisp"]["blocked"] == "plugin:notes-with-a-blocked-column");

	// and the person undoes it as any revision
	const json undo = screen.versions().revertRevision(screen.versions().head().id, "user");
	REQUIRE_FALSE(screen.versions().commit(undo));
	CHECK(definition(screen, "count") == "(defn count [] 1)");
	CHECK_FALSE(screen.versions().sourceDocument()["program"]["app.lisp"].contains("blocked"));
}

TEST_CASE("packages install: on a newer base, everything merges, and both sides land") {
	const std::filesystem::path root = freshFolder("affrescoui-install-newer-test");
	const std::string text = sharedVersion(root);
	affresco::Screen screen = receiver(root);
	commitBy(screen, "user",
	         {{{"op", "replace"}, {"path", "/program/app.lisp/label"}, {"value", "(defn label [] \"Mine\")"}}});

	const json summary = screen.inspectPackage(text, json::object());
	CHECK(summary["base"]["exact"] == false);
	CHECK(summary["conflicts"] == json::array());
	const affresco::ProposalInfo proposal = proposed(screen, text);
	CHECK(proposal.leftOut == json::array());
	acceptAsPerson(screen, proposal.id);
	CHECK(definition(screen, "count") == "(defn count [] 2)");
	CHECK(definition(screen, "label") == "(defn label [] \"Mine\")");
}

TEST_CASE("packages install: a definition both sides changed is left out, and the rest applies") {
	const std::filesystem::path root = freshFolder("affrescoui-install-leftout-test");
	const std::string text = sharedVersion(root);
	affresco::Screen screen = receiver(root);
	commitBy(screen, "user",
	         {{{"op", "replace"}, {"path", "/program/app.lisp/count"}, {"value", "(defn count [] 3)"}}});

	const json summary = screen.inspectPackage(text, json::object());
	REQUIRE(summary["conflicts"].size() == 1);
	CHECK(summary["conflicts"][0]["path"] == "/program/app.lisp/count");
	const affresco::ProposalInfo proposal = proposed(screen, text);
	REQUIRE(proposal.leftOut.size() == 1);
	CHECK(proposal.leftOut[0]["main"] == "(defn count [] 3)");
	CHECK(proposal.leftOut[0]["proposal"] == "(defn count [] 2)");
	acceptAsPerson(screen, proposal.id);
	CHECK(definition(screen, "count") == "(defn count [] 3)"); // mine stays, never merged inside
	CHECK(definition(screen, "blocked") == "(defn blocked [] \"Blocked\")");
}

TEST_CASE("packages install: what was left out is settled by the person, either way, and moves the head") {
	for(const bool theirs : {true, false}) {
		CAPTURE(theirs);
		const std::filesystem::path root = freshFolder("affrescoui-install-resolve-test");
		const std::string text = sharedVersion(root);
		affresco::Screen screen = receiver(root);
		commitBy(screen, "user",
		         {{{"op", "replace"}, {"path", "/program/app.lisp/count"}, {"value", "(defn count [] 3)"}}});
		const affresco::ProposalInfo proposal = proposed(screen, text);
		affresco::VersionControl& versions = screen.versions();

		// not its author, nor an agent, nor at a path it didn't leave out
		CHECK(versions.resolveLeftOut(proposal.id, "/program/app.lisp/count", theirs, proposal.author)
		          .value_or("")
		          .contains("its author can't settle"));
		CHECK(versions.resolveLeftOut(proposal.id, "/program/app.lisp/count", theirs, "agent/helper")
		          .value_or("")
		          .contains("only the person settles"));
		CHECK(versions.resolveLeftOut(proposal.id, "/program/app.lisp/label", theirs, "user")
		          .value_or("")
		          .contains("left nothing out at"));

		const auto error = versions.resolveLeftOut(proposal.id, "/program/app.lisp/count", theirs, "user");
		REQUIRE_MESSAGE(!error, error.value_or(""));
		const affresco::ProposalInfo settled = versions.proposals().back();
		CHECK(settled.leftOut == json::array());
		if(theirs) {
			CHECK(settled.head != proposal.head);
			// an approval of the head before it is refused
			CHECK(versions.accept(proposal.id, {"user", proposal.head, false}));
		}
		acceptAsPerson(screen, proposal.id);
		CHECK(definition(screen, "count") == (theirs ? "(defn count [] 2)" : "(defn count [] 3)"));
		if(theirs) CHECK(versions.programAuthors()["app.lisp"]["count"] == "plugin:notes-with-a-blocked-column");
	}
}

TEST_CASE("packages install: refused for an app that isn't here, a delta that doesn't give its tree, a bad signature") {
	const std::filesystem::path root = freshFolder("affrescoui-install-refused-test");
	const std::string text = sharedVersion(root);
	affresco::Screen other = receiver(root, "tasks");
	const json summary = other.inspectPackage(text, json::object());
	CHECK(summary["problems"] == json::array({"base.app: made for Notes, which isn't here"}));
	std::string error;
	CHECK(other.proposePackage(text, &error).status == RevisionStatus::Rejected);
	CHECK(error.contains("made for Notes, which isn't here"));
	CHECK(other.versions().proposals().empty());

	affresco::Screen screen = receiver(root);
	json value = json::parse(text);
	value["base"]["delta"] = json::array();
	CHECK(screen.proposePackage(value.dump(), &error).status == RevisionStatus::Rejected);
	CHECK(error.contains("base.tree"));

	const format::SigningKey key = format::SigningKey::generate();
	json signedValue = key.sign(json::parse(text));
	signedValue["release"] = "1.2.0";
	CHECK(screen.proposePackage(signedValue.dump(), &error).status == RevisionStatus::Rejected);
	CHECK(error.contains("doesn't verify"));
	CHECK(screen.versions().proposals().empty());
}

TEST_CASE("packages install: a version whose program can't be built is refused, as its summary says") {
	// Found by fuzzing (tests/fuzz, package): the summary listed the problem, yet installing it on a copy without the
	// program made a proposal. The package is the fuzz target's (corpus.py, package_seeds()), with an entry that
	// defines another name
	const std::filesystem::path folder = freshFolder("affrescoui-install-unbuilt-test") / "notes";
	write(folder / "app.json", R"({"format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes",
		"release": "1.0.0", "entry": "index.screen.json"})");
	write(folder / "index.screen.json", R"json({"format": "affrescoui-screen", "version": 2,
		"state": {"title": "Notes"}, "root": {"type": "label", "id": "title", "text": "{{state.title}}"}})json");
	affresco::Screen screen = affresco::Screen::fromApp(folder, {}, affresco::Screen::History::InMemory);
	const std::string text = R"json({"format": "affrescoui-package", "version": 1, "kind": "changes",
		"name": "Notes with Blocked", "release": "1.1.0", "engine": "1.0.0", "author": "Ada",
		"base": {"app": "notes", "name": "Notes", "release": "1.0.0", "notation": "lisp",
			"tree": "sha256:9930931f708a2e40da53e382f96492722e08cf47c6513d732634cda008827a8e",
			"delta": [{"op": "remove", "path": "/state/columns/1"},
				{"op": "replace", "path": "/program/app.lisp/count", "value": "(defn count [] 1)"},
				{"op": "remove", "path": "/assets"}]},
		"document": {"format": "affrescoui-screen", "version": 2,
			"state": {"title": "Notes", "columns": ["To do", "Blocked", "Done"]},
			"program": {"app.lisp": {"count": "(defn total)"}},
			"assets": {"dot.svg": {"type": "image/svg+xml", "size": 62,
				"sha256": "5b10d22019dbf0238b178c09957cac2af302655093bbe81a741d207a4e58e53a"}},
			"root": {"type": "label", "id": "title", "text": "{{state.title}}"}},
		"state": {"blockedReason": ""},
		"assets": {"dot.svg": {"type": "image/svg+xml",
			"text": "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"4\" height=\"4\"/>"}}})json";
	const json summary = screen.inspectPackage(text, json::object());
	REQUIRE(summary["problems"].size() == 1);
	CHECK(summary["problems"][0].get<std::string>().contains("document.program: can't be built"));
	std::string error;
	CHECK(screen.proposePackage(text, &error).status == RevisionStatus::Rejected);
	CHECK_MESSAGE(error.contains("document.program: can't be built"), error);
	CHECK(screen.versions().proposals().empty());
}

TEST_CASE("packages install: a trusted key names its publisher, an unknown one its start") {
	const std::filesystem::path root = freshFolder("affrescoui-install-trust-test");
	const format::SigningKey key = format::SigningKey::generate();
	const std::string text = key.sign(json::parse(sharedVersion(root))).dump();
	const affresco::Screen screen = receiver(root);
	CHECK(screen.inspectPackage(text, {{key.publisher(), "Studio"}})["signature"] == "trusted:Studio");
	CHECK(screen.inspectPackage(text, json::object())["signature"] == "unknown:" + key.publisher().substr(8, 16));

	// the file of trusted publishers: keys at their fixed length, names, and its limits; refused whole
	const std::string file = json{{key.publisher(), "Studio"}}.dump();
	CHECK(format::parseTrustedPublishers(file) == json{{key.publisher(), "Studio"}});
	CHECK(trustRefusal(json{{key.publisher() + "x", "Studio"}}.dump()).contains("isn't a public key"));
	CHECK(trustRefusal(json{{key.publisher(), ""}}.dump()).contains("expected a name"));
	CHECK(trustRefusal(json{{key.publisher(), 7}}.dump()).contains("expected a name"));
	CHECK(trustRefusal("[]").contains("expected"));
	CHECK(trustRefusal(std::string(format::maxTrustedPublishersBytes + 1, ' ')).contains("larger than 64 KiB"));
	json many = json::object();
	for(std::size_t i = 0; i <= format::maxTrustedPublishers; ++i)
		many[format::SigningKey::generate().publisher()] = "Studio";
	CHECK(trustRefusal(many.dump()).contains("more than 256 keys"));
	CHECK(format::readTrustedPublishers(root / "none.json") == json::object());
	write(root / "publishers.json", file);
	CHECK(format::readTrustedPublishers(root / "publishers.json") == json{{key.publisher(), "Studio"}});
}

TEST_CASE("packages install: the same file twice is already installed, accepted or not") {
	const std::filesystem::path root = freshFolder("affrescoui-install-twice-test");
	const std::string text = sharedVersion(root);
	{
		affresco::Screen screen = receiver(root);
		static_cast<void>(proposed(screen, text));
		CHECK(screen.hasImported(text));
		std::string error;
		CHECK(screen.proposePackage(text, &error).status == RevisionStatus::Rejected);
		CHECK(error.contains("is already installed"));
	}
	// and after a restart, from the history on disk
	const affresco::Screen again =
	    affresco::Screen::fromApp(root / "person" / "notes", root / "person-data", affresco::Screen::History::OnDisk);
	CHECK(again.hasImported(text));
}

TEST_CASE("packages install: a whole app goes into a new folder, then proposes its version there") {
	const std::filesystem::path root = freshFolder("affrescoui-install-app-test");
	const std::string text = sharedVersion(root, format::PackageKind::App);
	const format::Package package = format::readPackage(text);
	const std::filesystem::path folder = root / "installed" / "notes";
	format::installAppFiles(package, folder);
	CHECK(std::filesystem::exists(folder / "app.json"));
	CHECK(std::filesystem::exists(folder / "index.screen.json"));

	affresco::Screen screen =
	    affresco::Screen::fromApp(folder, root / "installed-data", affresco::Screen::History::OnDisk);
	CHECK(screen.inspectPackage(text, json::object())["base"]["exact"] == true);
	const affresco::ProposalInfo proposal = proposed(screen, text);
	acceptAsPerson(screen, proposal.id);
	CHECK(definition(screen, "count") == "(defn count [] 2)");

	// never into a folder that exists, nor a package of changes
	CHECK_THROWS_WITH(format::installAppFiles(package, folder), doctest::Contains("already exists"));
	CHECK_THROWS_WITH(
	    format::installAppFiles(format::readPackage(sharedVersion(freshFolder("affrescoui-install-app2-test"))),
		                        root / "other"),
	    doctest::Contains("kind \"app\""));
}
