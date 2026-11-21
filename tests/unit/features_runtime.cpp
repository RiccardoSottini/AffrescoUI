/// @file
/// @brief Features that hold programs, running: their code in the app's interpreter, the actions the app hands them,
/// and their going when they are switched off or taken out.

#include "FeatureApp.hpp"
#include "Composition.hpp"

#include <doctest/doctest.h>

using featureapp::App;
using featureapp::app;
using featureapp::json;
using featureapp::record;
using featureapp::wordCount;

TEST_CASE("features runtime: a click on a feature's view runs its command, and its state changes") {
	App host(app());
	host.set("/features/wordCount", record());
	REQUIRE(host.runtime->find("wordCount-label") != nullptr);
	host.click("wordCount-label");
	INFO(host.errors);
	CHECK(host.state("wordCount")["characters"] == true);
	host.click("wordCount-label");
	CHECK(host.state("wordCount")["characters"] == false);
}

TEST_CASE("features runtime: app.<need> runs the action the app hands the feature") {
	App host(app());
	host.set("/features/wordCount", record(wordCount(R"((run "app.save"))")));
	host.click("wordCount-label");
	INFO(host.errors);
	CHECK(host.state("text") == "");
	// From any other code, app.<need> names nothing
	host.set("/state/text", "again", "user");
	host.run("app.save");
	CHECK(host.state("text") == "again");
}

TEST_CASE("features runtime: a feature switched off, or its install undone, takes its views and functions") {
	App host(app());
	host.set("/features/wordCount", record());
	REQUIRE(host.runtime->find("wordCount-label") != nullptr);
	host.set("/features/wordCount/enabled", false);
	CHECK(host.runtime->find("wordCount-label") == nullptr);
	host.run("wordCount.toggle");
	CHECK(host.state("wordCount").is_null());
	host.set("/features/wordCount/enabled", true);
	REQUIRE(host.runtime->find("wordCount-label") != nullptr);
	host.click("wordCount-label");
	CHECK(host.state("wordCount")["characters"] == true);
}

TEST_CASE("features runtime: undoing a feature's install takes its views and functions") {
	App host(app());
	host.set("/features/wordCount", record());
	REQUIRE(host.runtime->find("wordCount-label") != nullptr);
	host.commit(host.repository.undoRevision(host.repository.head().number, "user"));
	CHECK(host.runtime->find("wordCount-label") == nullptr);
	host.run("wordCount.toggle");
	CHECK(host.state("wordCount").is_null());
	CHECK_FALSE(host.repository.document()["features"].contains("wordCount"));
}

TEST_CASE("features runtime: an after callback of a feature taken out runs nothing") {
	App host(app());
	host.set("/features/wordCount", record(wordCount(R"((after 100 wordCount-later))")));
	host.click("wordCount-label");
	INFO(host.errors);
	host.set("/features/wordCount/enabled", false);
	host.wait(200);
	CHECK(host.state("wordCount").is_null());
	// Still installed, it runs
	host.set("/features/wordCount/enabled", true);
	host.click("wordCount-label");
	host.wait(200);
	CHECK(host.state("wordCount")["later"] == 1);
}

// Found by fuzzing (tests/fuzz, revision): composing a feature's program whose record has no "requires" threw from
// the composition itself
TEST_CASE("features runtime: a feature's program whose record requires nothing runs") {
	json feature = record(json{{"state.tally", R"((defstate tally {:n 0} "Its state."))"},
	                           {"tally.go", R"((defcommand tally.go "Counts." :title "Tally" [] (set "state.tally.n" 1)))"}});
	feature.erase("requires");
	feature.erase("needs");
	feature.erase("bindings");
	App host(app());
	host.set("/features/tally", feature);
	CHECK(affresco::format::compose(host.repository.document()).problems.empty());
	host.run("tally.go");
	INFO(host.errors);
	CHECK(host.state("tally")["n"] == 1);
}
