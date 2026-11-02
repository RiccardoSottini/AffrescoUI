/// @file
/// @brief Unit tests for permission rules on `/actions` in a program (VERSION_CONTROL.md, "Programs"): a rule on the
/// actions also covers a change of a definition that can run when an action would, whose text is under `/program` and
/// isn't in what it builds. The attack is the one the stage 6 review found, written as an attacker would.

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::Mode;
using affresco::RevisionStatus;
using affresco::format::Rules;
using affresco::vcs::Repository;

namespace {

/// @brief A program whose permissions are given: a function, a command calling it, a shortcut and the main screen.
/// @param permissions The `:permissions` map, in the notation.
/// @return The program's text.
std::string programWith(const std::string& permissions) {
	return R"lisp((defn helper "A function." [] 1)
(defhook after-save "Runs after a save.")
(defcommand save "Saves." :title "Save" :category "File" [] (helper))
(defkey "mod+s" save)
(defscreen main :permissions )lisp" +
	       permissions + R"lisp( (column#list (label#a :text "x")))
)lisp";
}

/// @brief A path of the program's only file.
/// @param name The definition's address, escaped.
/// @return Its JSON Pointer.
std::string at(const std::string& name) { return "/program/app.lisp/" + name; }

/// @brief A revision that sets one path.
/// @param path The path.
/// @param value The value.
/// @param author Its author.
/// @return The revision.
json setting(const std::string& path, const json& value, const std::string& author = "agent/x") {
	return {{"author", author}, {"message", "m"}, {"set", {{path, value}}}};
}

/// @brief A repository on a program, in a mode.
struct Fixture {
	/// @brief Loads a program.
	/// @param text Its text, as the file "app.lisp".
	/// @param mode The reviewer's mode.
	explicit Fixture(const std::string& text, Mode mode = Mode::AcceptEdits)
	    : repository(rules, affresco::lisp::sourceDocument(text, "app.lisp")) {
		repository.setModeForThisRun(mode);
	}

	Rules rules;           ///< The format's rules.
	Repository repository; ///< The history.
};

/// The only rule the apps that moved from JSON kept: agents' changes to the actions are reviewed.
constexpr const char* kAskActions = R"p({:ask ["agent(/actions)"] :reviewers ["studio"]})p";

} // namespace

// The stage 6 review: a JSON app's rule on /actions stopped matching once its actions became a program, so in
// acceptEdits an agent's change to a function was applied unreviewed, where JSON asked for a change to the actions
TEST_CASE("lisp action rules, attack: a rule on /actions reviews an agent's change to the code an action runs") {
	const json changes[] = {
	    setting(at("extra"), R"lisp((defn extra "New." [] 2))lisp"),                              // a new function
	    setting(at("helper"), R"lisp((defn helper "A function." [] 2))lisp"),                     // a changed body
	    setting(at("save") + "/:category", "Edit"),                                               // a command's keyword
	    setting(at("add-hook after-save helper"), "(add-hook after-save helper)"),                // a new hook
	    setting(at("advice save before helper"), "(advice save :before helper)"),                 // new advice
	    setting(at("defkey mod+e"), R"lisp((defkey "mod+e" save))lisp"),                          // a new key
	    {{"author", "agent/x"}, {"message", "m"}, {"remove", json::array({at("defkey mod+s")})}}, // a key taken away
	};
	for(const json& change : changes) {
		CAPTURE(change.dump());
		Fixture f(programWith(kAskActions));
		const std::string head = f.repository.head().id;
		const auto result = f.repository.submit(change);
		CAPTURE(result.error);
		CHECK(result.status == RevisionStatus::Proposed);
		CHECK(f.repository.head().id == head);
	}
}

TEST_CASE("lisp action rules: a deny rule on /actions refuses a change to the code an action runs, in every mode") {
	for(const Mode mode : {Mode::Default, Mode::AcceptEdits, Mode::Plan}) {
		Fixture f(programWith(R"p({:allow ["agent"] :deny ["agent(/actions)"] :reviewers ["studio"]})p"), mode);
		const auto result = f.repository.submit(setting(at("helper"), R"lisp((defn helper "A function." [] 2))lisp"));
		CAPTURE(result.error);
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.contains("agent(/actions)"));
		// A rule on one action is judged the same: the code an action runs isn't told apart by action
		Fixture g(programWith(R"p({:allow ["agent"] :deny ["agent(/actions/save)"] :reviewers ["studio"]})p"), mode);
		CHECK(g.repository.submit(setting(at("extra"), R"lisp((defn extra "New." [] 2))lisp")).status ==
		      RevisionStatus::Rejected);
	}
}

TEST_CASE("lisp action rules: a change of the view alone is judged by what it builds, as in JSON") {
	Fixture f(programWith(kAskActions));
	// A screen's text that builds a new label: applied in acceptEdits, as a JSON node's change is
	const std::string screen = R"lisp((defscreen main :permissions )lisp" + std::string(kAskActions) +
	                           R"lisp( (column#list (label#a :text "y"))))lisp";
	const auto changed = f.repository.submit(setting(at("main"), screen));
	CAPTURE(changed.error);
	CHECK(changed.status == RevisionStatus::Applied);
}

TEST_CASE("lisp action rules: an allow rule on /actions doesn't allow a change to a program's code") {
	Fixture f(programWith(R"p({:allow ["agent(/actions)"] :reviewers ["studio"]})p"), Mode::Default);
	CHECK(f.repository.submit(setting(at("helper"), R"lisp((defn helper "A function." [] 2))lisp")).status ==
	      RevisionStatus::Proposed);
	// Named, it is
	Fixture g(programWith(R"p({:allow ["agent(/actions)" "agent(/program)"] :reviewers ["studio"]})p"), Mode::Default);
	CHECK(g.repository.submit(setting(at("helper"), R"lisp((defn helper "A function." [] 2))lisp")).status ==
	      RevisionStatus::Applied);
}

TEST_CASE("lisp action rules: the person's own evaluation is unaffected") {
	Fixture f(programWith(kAskActions));
	const json person = {
	    {"author", "user"},
	    {"message", "m"},
	    {"patch",
		 {{{"op", "replace"}, {"path", at("helper")}, {"value", R"lisp((defn helper "A function." [] 2))lisp"}}}}};
	const auto refused = f.repository.commitOwnInput(person);
	CHECK_FALSE(refused.has_value());
	CHECK(f.repository.sourceDocument()["program"]["app.lisp"]["helper"].get<std::string>().contains("2)"));
}

TEST_CASE("lisp action rules: a JSON app is judged as before") {
	const json document = {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"permissions", {{"ask", {"agent(/actions)"}}, {"reviewers", {"studio"}}}},
	    {"root", {{"id", "root"}, {"type", "column"}, {"children", {{{"id", "a"}, {"type", "label"}, {"text", "x"}}}}}},
	    {"actions", {{"plain", json::array({{{"set", "state.out"}, {"to", 1}}})}}}};
	Rules rules;
	Repository repository(rules, document);
	repository.setModeForThisRun(Mode::AcceptEdits);
	CHECK(repository.submit(setting("#a/text", "y")).status == RevisionStatus::Applied);
	CHECK(repository.submit(setting("/actions/plain", json::array({{{"set", "state.out"}, {"to", 2}}}))).status ==
	      RevisionStatus::Proposed);
}

TEST_CASE("lisp action rules: an owner of /actions is asked for a change to the code an action runs") {
	Fixture f(programWith(R"p({:reviewers ["studio" "human"] :review {:owners {"/actions" ["human"]}}})p"),
	          Mode::Default);
	const auto result = f.repository.propose(setting(at("helper"), R"lisp((defn helper "A function." [] 2))lisp"));
	REQUIRE(result.status == RevisionStatus::Proposed);
	const auto open = f.repository.proposals();
	REQUIRE(open.size() == 1);
	CAPTURE(open[0].policy);
	CHECK(open[0].policy.contains("an owner of /actions"));
}
