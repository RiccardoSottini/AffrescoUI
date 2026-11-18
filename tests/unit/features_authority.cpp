/// @file
/// @brief The authority of a feature's code: it writes only its own state and runs its own commands and what the app
/// hands it, at run time too (Programs::checkStep()); what reaches a step from it is refused, naming the feature; and
/// its entries' authors.

#include "FeatureApp.hpp"

#include <affresco/core/FeatureOrigin.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/notation/Notation.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Program.hpp>
#include "Composition.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>

using affresco::RevisionStatus;
using featureapp::App;
using featureapp::app;
using featureapp::json;
using featureapp::record;
using featureapp::wordCount;

namespace {

/// The feature's file, as the program that runs names it (lisp::effectiveProgram()).
constexpr const char* featureFile = "features/wordCount/feature.lisp";

/// What the run-time check says, the start of it.
constexpr const char* ledHere = R"(the feature "wordCount" led here)";

/// @brief An app whose own program holds the feature's file, as the program that runs names it: no composition
/// judges it. With a record of the feature too, switched off so that its own file runs, the feature was installed:
/// only the run-time check stands between its code and a step. Without one, the app's own file shipped it.
/// @param flip The body of the feature's function.
/// @param installed Whether the document has the feature's record.
/// @return The document.
json shipping(const std::string& flip, bool installed = true) {
	json document = app();
	json definitions = wordCount(flip);
	definitions.erase("wordCount-label");
	definitions.erase("part.wordCount-views");
	document["program"][featureFile] = definitions;
	if(installed) {
		document["features"]["wordCount"] = record();
		document["features"]["wordCount"]["enabled"] = false;
	}
	return document;
}

/// @brief The app with a `save` action that runs the application's function.
/// @return The document, with no feature.
json saving() {
	json document = app();
	document["actions"]["save"] = json::parse(R"([{"call": "hit"}])");
	return document;
}

/// @brief The word count's record wrapping the app's `save` in advice, as an install leaves it.
/// @return The record.
json wrapping() {
	json definitions = wordCount();
	definitions["wordCount-wrap"] = R"((defn wordCount-wrap "Wraps it." [proceed] (proceed)))";
	definitions["advice save around wordCount-wrap"] = "(advice save :around wordCount-wrap)";
	return record(definitions);
}

/// @brief The app with a `save` action and the word count installed, wrapping it.
/// @return The document.
json advising() {
	json document = saving();
	document["features"]["wordCount"] = wrapping();
	return document;
}

/// @brief A folder of its own, removed with everything in it.
struct TemporaryFolder {
	std::filesystem::path path = std::filesystem::temp_directory_path() / "affrescoui-features-authority-test"; ///< It.
	/// @brief Makes it empty.
	TemporaryFolder() {
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path);
	}
	/// @brief Removes it.
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	TemporaryFolder(TemporaryFolder&&) = delete;
	TemporaryFolder& operator=(TemporaryFolder&&) = delete;
};

/// @brief Installs a feature whose function does something, by an agent, scanned or not.
/// @param flip The function's body.
/// @param scan Whether revisions are scanned.
/// @return Whether the install was refused.
bool installRefused(const std::string& flip, bool scan) {
	App host(app(), scan);
	const json revision = {
	    {"author", "agent/x"}, {"message", "m"}, {"set", {{"/features/wordCount", record(wordCount(flip))}}}};
	return host.repository.commit(revision).has_value();
}

/// @brief Runs a feature's function that does something, its file in the app's own program beside the record of its
/// install (shipping()), and tells what was reported.
/// @param flip The function's body.
/// @param scan Whether revisions are scanned (nothing is revised: the same either way).
/// @return The errors.
std::vector<std::string> runShipped(const std::string& flip, bool scan) {
	App host(shipping(flip), scan);
	host.run("wordCount.toggle");
	CHECK(host.state("secret") == "kept");
	CHECK(host.state("text") == "one two three");
	CHECK(host.hits == 0);
	return host.errors;
}

/// @brief Tells whether one of some errors holds a text, and none a value.
/// @param errors The errors.
/// @param step The step named.
/// @return true if the run-time check refused the step, naming the feature, its definition and the step.
bool refusedAtRunTime(const std::vector<std::string>& errors, const std::string& step) {
	return std::ranges::any_of(errors, [&](const std::string& error) {
		return error.contains(ledHere) && error.contains("wordCount-flip") && error.contains("\"" + step + "\"") &&
		       !error.contains("gone");
	});
}

} // namespace

TEST_CASE("features authority attack: a feature's code reaches only its own state, its commands and what it's handed") {
	const std::vector<std::pair<std::string, std::string>> attacks = {
	    {R"((set "state.secret" "gone"))", "set"}, // the host's state
	    {R"((run "save"))", "run"},                // the host's action by name
	    {R"((invoke "files.read"))", "invoke"},    // a service
	    {R"((call "hit"))", "call"}};              // a control step
	for(const bool scan : {true, false})           // "false": the static checks off
		for(const auto& [flip, step] : attacks) {
			CAPTURE(scan);
			CAPTURE(flip);
			CHECK(installRefused(flip, scan)); // the first line: composition leaves it out, the revision is refused
			CHECK(refusedAtRunTime(runShipped(flip, scan), step)); // the second
		}
}

TEST_CASE("features authority: the feature's own state and commands, and what the app hands it, run") {
	for(const bool scan : {true, false}) {
		CAPTURE(scan);
		App host(shipping(R"((set "state.wordCount.characters" true) (run "app.save"))"), scan);
		host.run("wordCount.toggle");
		INFO(host.errors);
		CHECK(host.state("wordCount")["characters"] == true);
		CHECK(host.state("text") == "one two three"); // no wordCount.adapted.save: not composed, nothing handed
		CHECK(std::ranges::any_of(host.errors, [](const std::string& error) {
			return error.contains(R"(the app hands the feature "wordCount" no action "save")");
		}));
	}
}

TEST_CASE("features authority attack: advice on the person's command doesn't lend it the person's control step") {
	for(const bool scan : {true, false}) {
		CAPTURE(scan);
		json document = app();
		document["actions"]["save"] = json::parse(R"([{"call": "hit"}])");
		App host(document, scan);
		json definitions = wordCount();
		definitions["wordCount-wrap"] = R"((defn wordCount-wrap "Wraps it." [proceed] (proceed)))";
		definitions["advice save around wordCount-wrap"] = "(advice save :around wordCount-wrap)";
		const json revision = {
		    {"author", "agent/x"}, {"message", "m"}, {"set", {{"/features/wordCount", record(definitions)}}}};
		// Advice on the person's action is the feature's offer to make: the second line holds whatever the first let in
		if(host.repository.commit(revision)) continue;
		host.run("save");
		CHECK(host.hits == 0);
		CHECK(host.reported(R"(the feature "wordCount" led here)"));
		CHECK(host.reported("\"call\""));
	}
}

TEST_CASE("features authority attack: the person's command calling a feature's function loses its control step") {
	for(const bool scan : {true, false}) {
		CAPTURE(scan);
		App host(
		    app({{"host.go", R"((defcommand host.go "The person's." :title "Go" [] (wordCount-flip) (call "hit")))"}}),
		    scan);
		host.set("/features/wordCount", record());
		host.run("host.go");
		INFO(host.errors);
		CHECK(host.state("wordCount")["characters"] == true);
		CHECK(host.hits == 0);
		CHECK(host.reported(R"(the feature "wordCount" led here)"));
		// Without the feature's function, the person's command runs its step
		host.errors.clear();
		App plain(app({{"host.go", R"((defcommand host.go "The person's." :title "Go" [] (call "hit")))"}}), scan);
		plain.run("host.go");
		CHECK(plain.hits == 1);
	}
}

TEST_CASE("features authority attack: an installed feature kept in the app's own file has a revision's authority") {
	for(const bool scan : {true, false}) {
		CAPTURE(scan);
		// As a save leaves it: the document written to its file and read back, in either notation
		for(const auto notation : {affresco::notation::Notation::Json, affresco::notation::Notation::Lisp}) {
			const json saved = affresco::notation::read(affresco::notation::write(advising(), notation), notation);
			REQUIRE(saved["features"]["wordCount"].contains("release"));
			App host(saved, scan);
			REQUIRE(host.runtime->find("wordCount-label") != nullptr);
			CHECK(host.repository.programAuthors()[featureFile]["wordCount-flip"] == "feature:wordCount");
			host.run("save");
			CHECK(host.hits == 0);
			CHECK(host.reported(R"(the feature "wordCount" led here)"));
		}
	}
}

TEST_CASE("features authority: what the app's own file ships of a feature it never installed is the app's own code") {
	for(const bool scan : {true, false}) {
		CAPTURE(scan);
		App host(shipping(R"((set "state.secret" "gone") (call "hit"))", false), scan);
		CHECK(host.repository.programAuthors()[featureFile]["wordCount-flip"] == "file");
		host.run("wordCount.toggle");
		INFO(host.errors);
		CHECK(host.state("secret") == "gone");
		CHECK(host.hits == 1);
		CHECK_FALSE(host.reported("led here"));
	}
}

TEST_CASE("features authority attack: the person's text in a feature's own file is the feature's") {
	App host(shipping(R"((toggle "state.wordCount.characters"))", false));
	host.set("/program/features~1wordCount~1feature.lisp/wordCount-flip",
	         R"((defn wordCount-flip "Flips it." [] (set "state.secret" "gone")))", "user");
	CHECK(host.repository.programAuthors()[featureFile]["wordCount-flip"] == "feature:wordCount");
	CHECK(host.repository.programAuthors()[featureFile]["wordCount.toggle"] == "file"); // the rest stays the file's
	host.run("wordCount.toggle");
	CHECK(host.state("secret") == "kept");
	CHECK(host.reported(R"(the feature "wordCount" led here)"));
}

TEST_CASE("features authority attack: an installed feature stays the feature's after a restart") {
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path / "app.history";
	{
		App first(saving(), false, history);
		first.set("/features/wordCount", wrapping(), "features"); // as the features service installs it
		CHECK(first.repository.programAuthors()[featureFile]["wordCount-wrap"] != "file");
	} // the host quits
	App second(saving(), false, history);
	REQUIRE(second.repository.head().number == 1); // the install came back from the folder
	REQUIRE(second.runtime->find("wordCount-label") != nullptr);
	CHECK(second.repository.programAuthors()[featureFile]["wordCount-wrap"] != "file");
	second.run("save");
	CHECK(second.hits == 0);
	CHECK(second.reported(R"(the feature "wordCount" led here)"));
}

TEST_CASE("features authority attack: taking a record's origin away doesn't make its feature the app's own") {
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path / "app.history";
	// Installed, and kept in the app's own file: then a commit takes away what it can of where it came from
	json document = shipping(R"((set "state.secret" "gone"))");
	document["features"]["wordCount"]["digest"] = "sha256:0";
	{
		App first(document, false, history);
		const json strip = {{"author", "agent/x"},
		                    {"message", "m"},
		                    {"remove", {"/features/wordCount/release", "/features/wordCount/source"}}};
		const std::optional<std::string> refused = first.repository.commit(strip);
		CHECK(refused.value_or("").contains("release")); // a record always says its release
		first.commit({{"author", "agent/x"}, {"message", "m"}, {"remove", {"/features/wordCount/digest"}}});
		CHECK(first.repository.programAuthors()[featureFile]["wordCount-flip"] == "feature:wordCount");
	}
	App second(document, false, history);
	REQUIRE(second.repository.head().number == 1);
	CHECK(second.repository.programAuthors()[featureFile]["wordCount-flip"] == "feature:wordCount");
	second.run("wordCount.toggle");
	CHECK(second.state("secret") == "kept");
	CHECK(second.reported(R"(the feature "wordCount" led here)"));
}

TEST_CASE("features authority: a record says it was installed by any of the keys an install writes") {
	json bare = {{"enabled", true}, {"provides", json::object()}};
	CHECK_FALSE(affresco::holdsFeatureOrigin(bare));
	for(const std::string_view key : affresco::featureOriginKeys) {
		CAPTURE(key);
		json installed = bare;
		installed[std::string(key)] = nullptr; // an install writes a key with no value too ("digest", "publisher")
		CHECK(affresco::holdsFeatureOrigin(installed));
		CHECK(affresco::installedFeature({{"features", {{"wordCount", installed}}}}, "wordCount"));
	}
	CHECK(affresco::holdsFeatureOrigin(json::array())); // a record of the wrong shape: never the app's own
	CHECK_FALSE(affresco::installedFeature(json::object(), "wordCount"));
	CHECK_FALSE(affresco::installedFeature({{"features", json::object()}}, "wordCount"));
	CHECK(affresco::installedFeature({{"features", json::array()}}, "wordCount"));
}

TEST_CASE("features authority: a feature's entries' authors, after an accepted install, a restore and in a preview") {
	json document = app();
	document["permissions"] = json::parse(R"({"allow": ["studio"], "reviewers": ["studio"]})");
	App host(document);
	// An agent's install, the person accepts it: the author is the agent, never the person
	const json install = {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/features/wordCount", record()}}}};
	const auto proposed = host.repository.submit(install);
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	REQUIRE_FALSE(
	    host.repository.accept(proposed.proposal, {"user", host.repository.proposals()[0].head, false}).has_value());
	json authors = host.repository.programAuthors();
	CHECK(authors[featureFile]["wordCount-flip"] == "agent/x");
	CHECK(authors["app.lisp"]["after-save"] == "file");
	// An agent changes one definition; another's author stays
	const json edit = {{"author", "agent/y"},
	                   {"message", "m"},
	                   {"set",
	                    {{"/features/wordCount/provides/program/feature.lisp/wordCount-flip",
	                      R"((defn wordCount-flip "Flips it twice." [] (toggle "state.wordCount.characters")))"}}}};
	const auto edited = host.repository.submit(edit);
	REQUIRE(edited.status == RevisionStatus::Proposed);
	// In a preview, the proposal's entry is its author's
	REQUIRE_FALSE(host.repository.preview(edited.proposal).has_value());
	authors = host.repository.programAuthors();
	CHECK(authors[featureFile]["wordCount-flip"] == "agent/y");
	CHECK(authors[featureFile]["wordCount.toggle"] == "agent/x");
	host.repository.endPreview();
	// The person's restore of the app before the install, then of the install: never the person's
	const auto installed = host.repository.head().number;
	host.commit(host.repository.restoreRevision(installed - 1, "user"));
	CHECK_FALSE(host.repository.programAuthors().contains(featureFile));
	host.commit(host.repository.restoreRevision(installed, "user"));
	authors = host.repository.programAuthors();
	CHECK(authors[featureFile]["wordCount-flip"] != "user");
	const std::string restored = authors[featureFile]["wordCount-flip"].get<std::string>();
	CHECK((restored.starts_with("agent/") || restored == "feature:wordCount"));
	for(const auto& [file, entries] : authors.items())
		if(file.starts_with("features/"))
			for(const auto& [name, author] : entries.items()) {
				CAPTURE(name);
				CHECK(author != "file");
				CHECK(author != "user");
			}
}

TEST_CASE("features authority: judging a revision on an app with eight features builds no section twice") {
	json document = app();
	for(int i = 0; i < 8; ++i) {
		const std::string name = "count" + std::to_string(i);
		json definitions = {
		    {"state." + name, std::format(R"((defstate {} {{:on false}} "Its state."))", name)},
		    {name + "-flip", std::format(R"((defn {0}-flip "Flips it." [] (toggle "state.{0}.on")))", name)}};
		json feature = record(definitions);
		feature["order"] = i;
		feature.erase("needs");
		feature.erase("bindings");
		document["features"][name] = feature;
	}
	const std::size_t start = affresco::lisp::programBuilds();
	REQUIRE(affresco::format::compose(document).problems.empty()); // builds each feature's program alone
	App host(document);
	const std::size_t opened = affresco::lisp::programBuilds() - start;
	host.set("/program/app.lisp/after-save", R"((defhook after-save "Runs after a save, changed."))");
	const std::size_t judged = affresco::lisp::programBuilds() - start - opened;
	MESSAGE("builds to open the app: " << opened << "; to judge, commit and load one revision: " << judged);
	// Opening builds the app's own section and each feature's alone, at most once each (an earlier test may have)
	CHECK(opened <= 9);
	// The revision: its own section after (the document's build), and the effective program before and after; each
	// feature's alone and the section before stay kept
	CHECK(judged <= 3);
}

TEST_CASE("features authority: an install, or switching it off, is judged as a change of the definitions") {
	const affresco::format::Rules rules;
	json hooked = wordCount();
	hooked["add-hook after-save wordCount-flip"] = "(add-hook after-save wordCount-flip)";
	const json before = app();
	json on = before;
	on["features"]["wordCount"] = record(hooked);
	const auto holds = [](const std::vector<std::string>& lines, const std::string& text) {
		return std::ranges::any_of(lines, [&](const std::string& line) { return line.contains(text); });
	};
	// A hook through a feature is always reviewed, whatever it meets
	CHECK(holds(rules.reviewLines(before, on), "adds 'add-hook after-save wordCount-flip'"));
	CHECK(
	    holds(rules.behaviourLines(before, on), "adds the definition 'wordCount-flip' of " + std::string(featureFile)));
	CHECK(rules.impact(before, on)["logic"] == 5);
	json off = on;
	off["features"]["wordCount"]["enabled"] = false;
	CHECK(holds(rules.reviewLines(on, off), "removes 'add-hook after-save wordCount-flip'"));
	CHECK(rules.impact(on, off)["logic"] == 5);
	// Taking away what the app's own code calls is refused, as taking away a definition of its own
	json calls = app({{"host.go", R"((defcommand host.go "The person's." :title "Go" [] (wordCount-flip)))"}});
	calls["features"]["wordCount"] = record();
	App host(calls);
	const std::optional<std::string> removed =
	    host.repository.commit({{"author", "agent/x"}, {"message", "m"}, {"remove", {"/features/wordCount"}}});
	REQUIRE(removed.has_value());
	CHECK(removed.value_or("").contains(R"(the program no longer defines "wordCount-flip", which "host.go" uses)"));
	// A definition named like a control step: a feature's names start with its own, so composition refuses it first
	json named = wordCount();
	named["accept"] = R"((defn accept "Named like a review step." [] 1))";
	App plain(app());
	CHECK(plain.repository
	          .commit({{"author", "agent/x"}, {"message", "m"}, {"set", {{"/features/wordCount", record(named)}}}})
	          .has_value());
}

TEST_CASE("features authority attack: an interpreter changed in place keeps a feature's code without the person's reach") {
	// No hook in the app: a hook makes every revision load a new interpreter, and this is the path that doesn't
	json document = app({{"host.go", R"((defcommand host.go "The person's." :title "Go" [] (wordCount-flip) (call "hit")))"},
	                     {"host.other", R"((defn host.other "The person's." [] 1))"}});
	document["program"]["app.lisp"].erase("after-save");
	document["points"]["statusBar.right"].erase("hooks");
	App host(document, true);
	host.set("/features/wordCount", record());
	const std::size_t before = affresco::lisp::rebuildStatistics().redefined;
	// The person's own definition changed: the interpreter is changed in place, the feature's function kept
	host.set("/program/app.lisp/host.other", R"((defn host.other "The person's." [] 2))", "user");
	CHECK(affresco::lisp::rebuildStatistics().redefined == before + 1);
	host.run("host.go");
	INFO(host.errors);
	CHECK(host.hits == 0);
	CHECK(host.reported(ledHere));
	// The feature's function changed: defined again in place, still the feature's
	host.errors.clear();
	json definitions = wordCount(R"((toggle "state.wordCount.characters") nil)");
	host.set("/features/wordCount/provides/program/feature.lisp/wordCount-flip", definitions["wordCount-flip"]);
	CHECK(affresco::lisp::rebuildStatistics().redefined == before + 2);
	// The history credits who edited it; it runs as the feature's (Programs credits it so), as the refusal shows
	host.run("host.go");
	CHECK(host.hits == 0);
	CHECK(host.reported(ledHere));
	CHECK(host.runtime->find("wordCount-label") != nullptr);
}
