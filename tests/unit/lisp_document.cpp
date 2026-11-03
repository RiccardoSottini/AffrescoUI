/// @file
/// @brief Unit tests for programs in the document (RFC 0005, stage 2; VERSION_CONTROL.md, "Programs"): loading, version
/// control on `/program/...`, review by what is built, the keyword edit and authors, with the attacks written as an
/// attacker would.

#include "FeatureManifest.hpp"
#include "HistoryFiles.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/notation/Notation.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::format::Rules;
using affresco::test::TemporaryFolder;
using affresco::vcs::Repository;
namespace lisp = affresco::lisp;
namespace notation = affresco::notation;

namespace {

/// A program: a function, a command with a comment, a shortcut whose address holds a slash, and a screen.
constexpr const char* kProgram = R"lisp(;;; app.lisp --- the fixture

;; The rooms.
(def rooms "The rooms." [{:id "a" :name "Kitchen"}])

(defn card "One room." [room]
  `(label :id ~(str "room-" (:id room)) :text ~(:name room)))

(defn unused "Nothing builds with it." [] (other))

(defn other "What unused calls." [] 1)

;; Kills the line.
(defcommand kill-line "Kills." :title "Kill Line" :category "Killing" [times] (list times)) ; end of line

(defkey "mod+/" kill-line)

(defscreen main :window {:title "Rooms" :width 300} :state {:n 1}
  (column#list ~@(map card rooms) (label#title :text "Hello")))
)lisp";

/// A screen's permissions that send agents' changes to review.
constexpr const char* kGoverned = R"lisp((defn card "One." [] `(label#title :text "Hello"))
(defn helper "Only a function." [] 1)
(defcommand save "Saves." :title "Save" [] nil)
(defscreen main :permissions {:allow ["studio"] :reviewers ["studio"]} (column#list (label#locked :text "x" :locked true) ~(card)))
)lisp";

/// @brief A path of the fixture's only file.
/// @param name The definition's address, escaped.
/// @return Its JSON Pointer.
std::string at(const std::string& name) { return "/program/app.lisp/" + name; }

/// @brief A revision that sets one path.
/// @param path The path.
/// @param value The value.
/// @param author Its author.
/// @return The revision.
json setting(const std::string& path, const json& value, const std::string& author = "app") {
	return {{"author", author}, {"message", "m"}, {"set", {{path, value}}}};
}

/// @brief A repository on a program.
struct Fixture {
	/// @brief Loads a text as the document of the file "app.lisp".
	/// @param text The text.
	explicit Fixture(const char* text = kProgram) : repository(rules, lisp::sourceDocument(text, "app.lisp")) {}
	/// @brief The text of a definition.
	/// @param name Its address.
	/// @return The text.
	std::string text(const std::string& name) const {
		return repository.sourceDocument()["program"]["app.lisp"].at(name).get<std::string>();
	}
	/// @brief Submits a revision that must be refused.
	/// @param revision The revision.
	/// @param reason What its reason holds.
	/// @return The reason.
	std::string refused(const json& revision, const std::string& reason) {
		const std::string head = repository.head().id;
		const auto result = repository.submit(revision);
		CHECK(result.status == RevisionStatus::Rejected);
		INFO(result.error);
		CHECK(result.error.contains(reason));
		CHECK(repository.head().id == head);
		return result.error;
	}

	Rules rules;           ///< The format's rules.
	Repository repository; ///< The history.
};

} // namespace

TEST_CASE("lisp document: a program loads as definitions, and the document shows what it builds") {
	Fixture f;
	const json& source = f.repository.sourceDocument();
	CHECK_FALSE(source.contains("root"));
	CHECK(source["program"]["app.lisp"].contains("defkey mod+/"));
	CHECK(f.text("kill-line").starts_with(";; Kills the line.\n(defcommand"));
	CHECK(f.text("kill-line").ends_with("; end of line"));
	const json& shown = f.repository.document();
	CHECK(shown["root"]["id"] == "list");
	CHECK(shown["keys"]["mod+/"] == "kill-line");
	CHECK(shown["actions"]["kill-line"]["title"] == "Kill Line");
	CHECK(shown["program"] == source["program"]);
	CHECK(f.repository.state()["n"] == 1); // its state is the live state
}

TEST_CASE("lisp document: a document can be read again as itself") {
	const json source = lisp::sourceDocument(kProgram, "app.lisp");
	const json built = lisp::buildDocument(source);
	CHECK(lisp::buildDocument(built) == built);
}

TEST_CASE("lisp document: a data file reads as before, and a bare word the language runs is an error with the hint") {
	CHECK(notation::read("(column :gap 2 (label :text x))", notation::Notation::Lisp)["type"] == "column");
	CHECK(notation::read("(list :gap 2)", notation::Notation::Lisp)["type"] == "list"); // a node type
	CHECK(notation::read("(node :type not :x 1)", notation::Notation::Lisp)["type"] == "not");
	try {
		static_cast<void>(notation::read("{:a\n (not x)}", notation::Notation::Lisp));
		FAIL("it reads");
	} catch(const notation::NotationError& error) {
		CHECK(error.line() == 2);
		CHECK(error.message() ==
		      R"lisp("not" is not a node type; to run it, write ~(not ...); for a node of that type, )lisp"
		      "write (node :type not ...)");
	}
}

TEST_CASE("lisp document: no generated example has a word the language runs in a data position") {
	std::size_t files = 0;
	for(const auto& entry : std::filesystem::recursive_directory_iterator(AFFRESCOUI_EXAMPLES_DIRECTORY)) {
		if(entry.path().extension() != ".lisp") continue;
		std::ifstream stream(entry.path(), std::ios::binary);
		const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		INFO(entry.path().string());
		if(lisp::isProgram(text)) continue; // written by hand: an app that is a program
		CHECK_NOTHROW(static_cast<void>(notation::read(text, notation::Notation::Lisp)));
		++files;
	}
	CHECK(files > 100);
}

TEST_CASE("lisp document: a revision on a definition changes what is shown, and undo gives the exact text back") {
	Fixture f;
	const std::string before = f.text("rooms");
	const std::string sourceDump = f.repository.sourceDocument().dump();
	const auto applied = f.repository.submit(setting(
	    at("rooms"),
	    ";; More rooms.\n(def rooms \"The rooms.\" [{:id \"a\" :name \"Kitchen\"} {:id \"b\" :name \"Hall\"}])"));
	REQUIRE(applied.status == RevisionStatus::Applied);
	CHECK(f.repository.document()["root"]["children"][1]["id"] == "room-b");
	// What the history keeps is the source: a plain RFC 6902 patch, and a diff of the source
	const json diff = f.repository.diff("HEAD~1", "HEAD");
	REQUIRE(diff.size() == 1);
	CHECK(diff[0]["path"] == at("rooms"));
	CHECK(diff[0]["op"] == "replace");

	const auto undone = f.repository.submit(f.repository.revertRevision("HEAD", "app"));
	REQUIRE(undone.status == RevisionStatus::Applied);
	CHECK(f.text("rooms") == before);
	CHECK(f.repository.sourceDocument().dump() == sourceDump);
	CHECK(f.repository.document()["root"]["children"].size() == 2);

	// Restoring the first revision and blaming a definition work on the source as well
	CHECK(f.repository.blame(at("rooms")).number == 2);
	CHECK(f.repository.blame(at("card")).number == 0);
	CHECK(f.repository.blame(at("defkey mod+~1")).number == 0);
}

TEST_CASE("lisp document: an address with a slash or a tilde round-trips through JSON Pointer") {
	const std::string text = "(defn f \"F.\" [] 1)\n(defkey \"a/b~c\" f)\n(defscreen main (label#x :text \"T\"))\n";
	Fixture f(text.c_str());
	const std::string key = at("defkey a~1b~0c");
	REQUIRE(f.repository.submit(setting(key, R"lisp((defkey "a/b~c" "Other." f))lisp")).status ==
	        RevisionStatus::Applied);
	CHECK(f.repository.blame(key).number == 1);
	CHECK(f.repository.programAuthors()["app.lisp"]["defkey a/b~c"] == "app");
	REQUIRE(f.repository.submit(f.repository.revertRevision("HEAD", "app")).status == RevisionStatus::Applied);
	CHECK(f.text("defkey a/b~c") == R"lisp((defkey "a/b~c" f))lisp");
	// A file's name holds slashes too
	const Rules rules;
	Repository nested(rules, lisp::sourceDocument(text, "parts/x.lisp"));
	CHECK(nested.submit(setting("/program/parts~1x.lisp/f", R"lisp((defn f "F." [] 2))lisp")).status ==
	      RevisionStatus::Applied);
}

TEST_CASE("lisp document: the keyword edit keeps every other byte, and history holds a plain replace") {
	Fixture f;
	const std::string before = f.text("kill-line");
	const auto applied = f.repository.submit(setting(at("kill-line") + "/:title", R"lisp(Kill "to" end)lisp"));
	REQUIRE(applied.status == RevisionStatus::Applied);
	std::string expected = before;
	expected.replace(expected.find(R"lisp("Kill Line")lisp"), 11, R"("Kill \"to\" end")");
	CHECK(f.text("kill-line") == expected);
	CHECK(f.repository.document()["actions"]["kill-line"]["title"] == R"lisp(Kill "to" end)lisp");
	const json diff = f.repository.diff("HEAD~1", "HEAD");
	REQUIRE(diff.size() == 1);
	CHECK(diff[0]["op"] == "replace");
	CHECK(diff[0]["path"] == at("kill-line"));
	REQUIRE(f.repository.submit(f.repository.revertRevision("HEAD", "app")).status == RevisionStatus::Applied);
	CHECK(f.text("kill-line") == before);

	// Inside a map; a number; a form written out; through a plain JSON Patch
	REQUIRE(f.repository.submit(setting(at("main") + "/:window/:title", "New")).status == RevisionStatus::Applied);
	CHECK(f.repository.document()["window"]["title"] == "New");
	CHECK(f.text("main").contains(R"(:window {:title "New" :width 300} :state {:n 1})"));
	REQUIRE(f.repository.submit(setting(at("main") + "/:window/:width", 400)).status == RevisionStatus::Applied);
	CHECK(f.repository.document()["window"]["width"] == 400);
	REQUIRE(f.repository.submit(setting(at("main") + "/:window/:width", json{{"$form", "~(+ 400 100)"}})).status ==
	        RevisionStatus::Applied);
	CHECK(f.repository.document()["window"]["width"] == 500);
	const json patch = {{"author", "app"},
	                    {"patch", {{{"op", "replace"}, {"path", at("kill-line") + "/:category"}, {"value", "Other"}}}}};
	REQUIRE(f.repository.submit(patch).status == RevisionStatus::Applied);
	CHECK(f.repository.document()["actions"]["kill-line"]["category"] == "Other");
}

TEST_CASE("lisp document: a keyword edit is refused cleanly") {
	Fixture f;
	const std::string kill = at("kill-line");
	f.refused(setting(kill + "/:nope", "x"), "no keyword :nope");
	f.refused(setting(at("nothing") + "/:title", "x"), "no definition there");
	f.refused(setting(at("card") + "/:title", "x"), "only defcommand, defscreen and defcustom");
	f.refused(setting(kill + "/title", "x"), "");
	f.refused(setting(at("main") + "/:window/:nope", 1), "no keyword :nope");
	f.refused(setting(kill + "/:title/:deeper", 1), "isn't a map");
	// A body's keyword isn't an option: its address doesn't reach it
	Fixture g("(defcommand c \"Doc.\" [] (kill :span \"x\"))\n(defscreen main (label#a :text \"T\"))\n");
	g.refused(setting(at("c") + "/:span", "y"), "no keyword :span");

	const json remove = {{"author", "app"}, {"remove", {kill + "/:title"}}};
	f.refused(remove, R"lisp(only "set" and "replace")lisp");
	const json odd = {{"author", "app"}, {"patch", {{{"op", 5}, {"path", kill + "/:title"}, {"value", "x"}}}}};
	f.refused(odd, R"lisp(only "set" and "replace")lisp");
	const json none = {{"author", "app"}, {"patch", {{{"op", "replace"}, {"path", kill + "/:title"}}}}};
	f.refused(none, "expected a value");
}

TEST_CASE("lisp document, attack: a keyword edit that smuggles a second form") {
	Fixture f;
	const std::string title = at("kill-line") + "/:title";
	for(const char* form :
	    {R"("x") (defn evil [] 1) ("y")", R"(1 2)", R"(1 ; a comment)", R"( 1)", R"(1) (x)", R"(#_x 1)", ""})
		f.refused(setting(title, json{{"$form", form}}), "");
	// As a string it stays one string, whatever it holds
	REQUIRE(f.repository.submit(setting(title, R"(x") (defn evil [] 1) (")")).status == RevisionStatus::Applied);
	CHECK(f.repository.document()["actions"]["kill-line"]["title"] == R"(x") (defn evil [] 1) (")");
	CHECK_FALSE(f.repository.document()["program"]["app.lisp"].contains("evil"));
	CHECK_FALSE(lisp::builtProgram(f.repository.sourceDocument()["program"])->tree.contains("evil"));
}

TEST_CASE("lisp document, attack: what a definition's text says must be what its address says") {
	Fixture f;
	f.refused(setting(at("card"), R"lisp((defn other2 "Doc." [] 1))lisp"), "defines its own name");
	f.refused(setting(at("card"), "(defn card [] 1) (defn evil [] 2)"), "one form");
	f.refused(setting(at("card"), "; only a comment"), "one form");
	f.refused(setting(at("card"), "(not a definition"), "");
	f.refused(setting(at("card"), 42), "expected the definition's text");
	f.refused(setting("/program/app.lisp", "text"), "expected an object");
	// A second file defining a name another has
	f.refused(setting("/program/b.lisp", json{{"card", R"lisp((defn card "Doc." [] 1))lisp"}}), "is defined twice");
	// Too large
	f.refused(setting(at("card"),
	                  R"lisp((defn card "Doc." [] ")lisp" + std::string(lisp::maxEntryBytes, 'x') + R"lisp("))lisp"),
	          "at most 1 MiB");
	// The built tree is never edited itself
	const std::string hint = f.refused(setting("#title/text", "x"), "no node with id");
	CHECK(hint.contains("under /program"));
	f.refused(setting("/root/id", "x"), "under /program");
	f.refused(setting("/root", json{{"type", "panel"}}), "built by the program");
	f.refused(setting("/actions/kill-line", json{{"do", json::array()}}), "under /program");
}

TEST_CASE("lisp document, attack: taking away what another definition uses") {
	Fixture f;
	const json remove = {{"author", "app"}, {"remove", {at("other")}}};
	f.refused(remove, R"lisp(no longer defines "other", which "unused" uses)lisp");
	const json build = {{"author", "app"}, {"remove", {at("card")}}};
	f.refused(build, ""); // the build itself fails, naming it
	const json both = {{"author", "app"}, {"remove", {at("unused"), at("other")}}};
	CHECK(f.repository.submit(both).status == RevisionStatus::Applied);
	CHECK(f.repository.submit(f.repository.revertRevision("HEAD", "app")).status == RevisionStatus::Applied);
	CHECK(f.text("other").contains("(defn other"));
}

TEST_CASE("lisp document, attack: a build that builds a capability, however it is computed, is refused") {
	Fixture f(kGoverned);
	const std::string main = at("main");
	const auto screen = [](const std::string& options) {
		return "(defscreen main " + options +
		       R"lisp( (column#list (label#locked :text "x" :locked true) ~(card))))lisp";
	};
	const std::string grants = R"lisp(:permissions {:allow ["studio"] :reviewers ["studio"]})lisp";

	// Written out
	f.refused(
	    setting(main, screen(R"lisp(:permissions {:allow ["studio" "agent"] :reviewers ["studio"]})lisp"), "agent"),
	    "permission denied");
	// Computed
	f.refused(
	    setting(main, screen(R"lisp(:permissions ~(assoc {:reviewers ["studio"]} :allow ["agent"]))lisp"), "agent"),
	    "permission denied");
	// Out of the keyword edit
	f.refused(setting(main + "/:permissions/:reviewers", json::array({"agent"}), "agent"), "permission denied");
	// Through a macro: its call stands where the definition was, and expands to it
	const json macro = {
	    {"author", "agent"},
	    {"set",
		 {{at("sneak"), R"lisp((defmacro sneak [x] `(defscreen ~x :permissions {:allow ["agent"] )lisp"
		                R"lisp(:reviewers ["studio"]} (column#list (label#locked :text "x" :locked )lisp"
		                "true) ~(card))))"},
		  {main, "(sneak main)"}}}};
	f.refused(macro, "permission denied");
	// A function that writes a protected step into the tree
	const json computed = {
	    {"author", "agent"},
	    {"set",
		 {{main, "(defscreen main " + grants + " ~(innocent))"},
		  {at("innocent"), R"lisp((defn innocent [] `(column#list (label#locked :text "x" :locked )lisp"
		                   "true) ~(card) (label#new :onClick [~(assoc {} :accept 1)])))"}}}};
	f.refused(computed, "permission denied");
	// A capability
	f.refused(setting(main, screen(grants + R"lisp( :channels {:out {:socket "x.sock"}})lisp"), "agent"), "channels");
	f.refused(setting(main, screen(grants + R"lisp( :persist ["state.x"] :state {:x 1})lisp"), "agent"), "persist");
	// A protected step, built
	f.refused(setting(at("card"),
	                  R"lisp((defn card "One." [] `(label#title :text "Hello" :onClick [{:undo true}])))lisp", "agent"),
	          "permission denied");
	f.refused(
	    setting(
	        at("card"),
	        R"lisp((defn card "One." [] `(label#title :text "Hello" :onClick ~(vec (list {:send {:channel "x"}})))))lisp",
	        "agent"),
	    "permission denied");
	// The first screen's locked node, through a definition
	f.refused(setting(main,
	                  "(defscreen main " + grants +
	                      R"lisp( (column#list (label#locked :text "changed" :locked true) ~(card))))lisp",
	                  "agent"),
	          "may not change");
	// A section's own text beside the program
	f.refused(setting("/permissions", json{{"allow", json::array({"agent"})}}, "agent"), "built by the program");
	f.refused(setting("/channels", json{{"out", {{"socket", "x.sock"}}}}, "agent"), "permission denied");
	// What a program may never build
	for(const char* key : {R"lisp(:include ["x.lisp"])lisp", ":program {:a {}}", ":assets {}", ":features {}"}) {
		INFO(key);
		f.refused(setting(main, screen(grants + " " + key), "app"), "a program can't build");
	}
}

TEST_CASE("lisp document, attack: a build that doesn't end, or fails, refuses the revision and names the definition") {
	Fixture f;
	const std::string spin = R"lisp((def rooms "The rooms." (loop [n 0] (recur (inc n)))))lisp";
	const std::string error = f.refused(setting(at("rooms"), spin), "rooms");
	CHECK(error.contains("AffrescoUI lisp"));
	f.refused(setting(at("card"), "(defn card [room] (first 1))"), "in card");
	f.refused(setting(at("rooms"), R"lisp((def rooms "The rooms." (vec (range 100000000))))lisp"), "rooms");
	// A macro call that runs forever as it expands
	const json macro = {
	    {"author", "app"},
	    {"set", {{at("spin"), "(defmacro spin [x] (loop [] (recur)))"}, {at("whatever"), "(spin whatever)"}}}};
	f.refused(macro, "");
	CHECK(f.repository.document()["root"]["id"] == "list");

	// What the budget costs: a judged revision, with one build on each side at most
	for(const char* costly : {"(loop [n 0] (recur (inc n)))", R"lisp((loop [s "xxxxxxxx"] (recur (str s s))))lisp",
	                          "(loop [v []] (recur (conj v {:a 1 :b [1 2 3]})))", "(sort (vec (range 100000)))",
	                          "(reduce (fn [a b] (assoc a (str b) b)) {} (range 100000))"}) {
		const auto start = std::chrono::steady_clock::now();
		f.refused(setting(at("rooms"), std::string(R"lisp((def rooms "The rooms." )lisp") + costly + ")"), "");
		MESSAGE(std::string(costly), " took ",
		        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(),
		        " ms");
	}
}

TEST_CASE("lisp document: rules judge what is built: deny, allow and ask, on /program and on the tree") {
	Fixture f(kGoverned);
	// No rule allows it: a proposal, which shows on preview, and merges
	const auto proposed = f.repository.submit(setting(at("save") + "/:title", "Store", "agent"));
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	const auto open = f.repository.proposals();
	REQUIRE(open.size() == 1);
	CHECK(f.repository.proposalDiff(proposed.proposal)[0]["path"] == at("save"));
	CHECK(open[0].touches == std::vector<std::string>{at("save")});
	CHECK(f.repository.document()["actions"]["save"]["title"] == "Save");
	REQUIRE_FALSE(f.repository.preview(proposed.proposal).has_value());
	CHECK(f.repository.document()["actions"]["save"]["title"] == "Store");
	CHECK(f.repository.sourceDocument()["program"]["app.lisp"]["save"].get<std::string>().contains("Store"));
	f.repository.endPreview();
	CHECK(f.repository.document()["actions"]["save"]["title"] == "Save");
	const auto accepted = f.repository.accept(proposed.proposal, {"studio", proposed.commit});
	REQUIRE_FALSE(accepted.has_value());
	CHECK(f.repository.document()["actions"]["save"]["title"] == "Store");
	CHECK(f.repository.blame(at("save")).author == "agent");
	CHECK(f.repository.blame(at("save")).committer == "studio");

	// Allowed: a change of a file only allowed if what it builds is too
	const char* text = R"lisp((defn helper "Only a function." [] 1)
(defn body "A body." [] 1)
(defcommand save "Saves." :title "Save" [] nil)
(defscreen main :permissions {:allow ["agent(/program/app.lisp)"] :reviewers ["studio"]} (column#list (label#a :text "T")))
)lisp";
	Fixture g(text);
	CHECK(g.repository.submit(setting(at("helper"), R"lisp((defn helper "Doc." [] 2))lisp", "agent")).status ==
	      RevisionStatus::Applied);
	CHECK(g.repository.submit(setting(at("save") + "/:title", "Store", "agent")).status == RevisionStatus::Proposed);

	// Denied by the rule on the tree, and by the rule on the source
	const char* deny = R"lisp((defn helper "Only a function." [] "T")
(defn locked "Never." [] 1)
(defscreen main :permissions {:allow ["agent"] :deny ["agent(#a)" "agent(/program/app.lisp/locked)"] :reviewers ["studio"]}
  (column#list (label#a :text ~(helper))))
)lisp";
	Fixture h(deny);
	h.refused(setting(at("helper"), R"lisp((defn helper "Doc." [] "U"))lisp", "agent"), "may not change #a");
	h.refused(setting(at("locked"), R"lisp((defn locked "Doc." [] 2))lisp", "agent"), "may not change");
	CHECK(h.repository.judge(setting(at("locked"), R"lisp((defn locked "Doc." [] 2))lisp", "agent")).decision ==
	      "deny");
}

TEST_CASE("lisp document: a limited reader doesn't read a program it isn't given, and can't write one") {
	const char* text = R"lisp(;; The secret is right here in the text
(def secret "Masked." "s3cret")
(defn open "Fine to read." [] 1)
(defn writable "Fine to write." [] 1)
(defscreen main :state {:token ~secret :count 0}
  :permissions {:allow ["agent" "writer" "reader"] :reviewers ["studio"] :data [
    {:author "agent" :never ["/state/token"]}
    {:author "reader" :read ["/program/app.lisp/open" "/state/count"]}
    {:author "writer" :write ["/program/app.lisp/writable"]}]}
  (column#list (label#a :text "T")))
)lisp";
	Fixture f(text);
	// Not named: not there, whatever else it is told
	const json agent = f.repository.readable("agent").value_or(json());
	REQUIRE(agent.is_object());
	CHECK_FALSE(agent.contains("program"));
	CHECK_FALSE(agent.dump().contains("s3cret"));
	CHECK(agent["root"]["id"] == "list"); // what is built, as masked as any tree
	// Named: that much
	const json reader = f.repository.readable("reader").value_or(json());
	REQUIRE(reader.is_object());
	CHECK(reader["program"]["app.lisp"].contains("open"));
	CHECK_FALSE(reader["program"]["app.lisp"].contains("secret"));
	CHECK_FALSE(reader.dump().contains("s3cret"));

	// A limited reader's program could build what it may not read, into what it may
	const std::string leak = "(defscreen main (column#list (label#a :text ~secret)))";
	f.refused(setting(at("main"), leak, "agent"), "may not change a program while its reads are limited");
	f.refused(setting(at("open"), R"lisp((defn open "Doc." [] secret))lisp", "reader"), "while its reads are limited");
	const json test = {
	    {"author", "agent"},
	    {"patch",
		 {{{"op", "test"}, {"path", at("secret")}, {"value", "x"}}, {{"op", "remove"}, {"path", "/state/count"}}}}};
	f.refused(test, "may not read");
	const json copy = {{"author", "agent"},
	                   {"patch", {{{"op", "copy"}, {"from", at("secret")}, {"path", "/state/stolen"}}}}};
	f.refused(copy, "may not read");

	// Writing: what "write" names, by effect
	CHECK_FALSE(f.repository.readable("writer").has_value());
	CHECK(f.repository.submit(setting(at("writable"), R"lisp((defn writable "Doc." [] 2))lisp", "writer")).status ==
	      RevisionStatus::Applied);
	f.refused(setting(at("open"), R"lisp((defn open "Doc." [] 2))lisp", "writer"), "may write only");
}

TEST_CASE("lisp document: a limit on the built tree catches a definition's change") {
	const char* text = R"lisp((defn state-value "The initial value." [] 1)
(defscreen main :state {:x ~(state-value)} :permissions {:allow ["agent"] :reviewers ["studio"] :data [{:author "agent" :write ["/program/app.lisp/state-value"]}]}
  (column#list (label#a :text "T")))
)lisp";
	Fixture f(text);
	// The live state is scoped to "write": what the definition does to it counts
	f.refused(setting(at("state-value"), R"lisp((defn state-value "Doc." [] 2))lisp", "agent"), "may write only");
	CHECK(f.repository.state()["x"] == 1);
}

TEST_CASE("lisp document: a definition's changed initial state reaches the live state, and undo takes it back") {
	Fixture f;
	REQUIRE(f.repository.submit(setting(at("main") + "/:state/:n", 5)).status == RevisionStatus::Applied);
	CHECK(f.repository.state()["n"] == 5);
	REQUIRE(f.repository.submit(f.repository.revertRevision("HEAD", "app")).status == RevisionStatus::Applied);
	CHECK(f.repository.state()["n"] == 1);
	// A revision on the live state stays as ever
	REQUIRE(f.repository.submit(setting("/state/n", 9)).status == RevisionStatus::Applied);
	CHECK(f.repository.state()["n"] == 9);
	CHECK(f.repository.sourceDocument()["program"]["app.lisp"]["main"].get<std::string>().contains(":state {:n 1}"));
	f.refused(setting(at("main") + "/:state", 3), "must be an object");
}

TEST_CASE("lisp document: proposals on different definitions merge, and on the same one conflict") {
	Fixture f(kGoverned);
	const auto first = f.repository.submit(setting(at("save") + "/:title", "One", "agent/a"));
	const auto other = f.repository.submit(setting(at("helper"), R"lisp((defn helper "Doc." [] 2))lisp", "agent/b"));
	const auto same = f.repository.submit(setting(at("save") + "/:title", "Two", "agent/c"));
	REQUIRE(first.status == RevisionStatus::Proposed);
	REQUIRE(other.status == RevisionStatus::Proposed);
	REQUIRE_FALSE(f.repository.accept(first.proposal, {"studio", first.commit}).has_value());
	const auto open = f.repository.proposals();
	REQUIRE(open.size() == 2);
	CHECK(open[0].conflicts.empty());
	CHECK_FALSE(open[1].conflicts.empty());
	CHECK(open[1].id == same.proposal);
	REQUIRE_FALSE(f.repository.accept(other.proposal, {"studio", open[0].head}).has_value());
	CHECK(f.text("helper").contains("[] 2"));
	CHECK(f.text("save").contains("One"));
}

TEST_CASE("lisp document: history on disk, and a history written by hand is judged by what it builds") {
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "h";
	const json source = lisp::sourceDocument(kGoverned, "app.lisp");
	const Rules rules;
	std::string head;
	{
		Repository first(rules, source);
		REQUIRE(first.openHistory(history).memoryOnly.empty());
		REQUIRE(first.submit(setting(at("save") + "/:title", "Stored", "studio")).status == RevisionStatus::Applied);
		head = first.head().id;
	}
	Repository second(rules, source);
	CHECK(second.openHistory(history).loaded);
	CHECK(second.head().id == head);
	CHECK(second.document()["actions"]["save"]["title"] == "Stored");
	CHECK(second.sourceDocument()["program"] != source["program"]);
	CHECK(second.programAuthors()["app.lisp"]["save"] == "studio");
}

namespace {

/// @brief Rules that let anything through, as whoever wrote a history by hand wouldn't have been stopped.
class AnythingGoes final : public affresco::DocumentRules {
public:
	/// @brief Nothing is checked.
	/// @param document The document.
	void validate([[maybe_unused]] const json& document) const override {}
	/// @brief No node is found.
	/// @param document The document.
	/// @param id The id.
	/// @return Nothing.
	[[nodiscard]] std::optional<std::string> findNode([[maybe_unused]] const json& document,
	                                                  [[maybe_unused]] std::string_view id) const override {
		return std::nullopt;
	}
	/// @brief No rules.
	/// @param document The document.
	/// @return None.
	[[nodiscard]] affresco::Permissions permissions([[maybe_unused]] const json& document) const override { return {}; }
	/// @brief Nothing is forbidden.
	/// @param before The document before.
	/// @param after The document after.
	void checkChange([[maybe_unused]] const json& before, [[maybe_unused]] const json& after) const override {}
};

} // namespace

TEST_CASE("lisp document, attack: a history written by hand is judged by what its program builds") {
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "h";
	const json source = lisp::sourceDocument(kGoverned, "app.lisp");
	{
		const AnythingGoes anything;
		Repository forged(anything, source);
		REQUIRE(forged.openHistory(history).memoryOnly.empty());
		const std::string widened =
		    R"lisp((defscreen main :permissions {:allow ["agent"] :reviewers ["agent"]} (column#list )lisp"
		    R"lisp((label#locked :text "x" :locked true) ~(card))))lisp";
		REQUIRE(forged.submit(setting(at("main"), widened, "agent")).status == RevisionStatus::Applied);
	}
	const Rules rules;
	Repository real(rules, source);
	CHECK_THROWS_WITH_AS(static_cast<void>(real.openHistory(history)), doctest::Contains("permission denied"),
	                     affresco::DocumentError);
}

TEST_CASE("lisp document: an entry's author is the commit's that last changed it, and a file is the app's own") {
	Fixture f;
	CHECK(f.repository.programAuthors()["app.lisp"]["card"] == "file");
	REQUIRE(f.repository.submit(setting(at("other"), R"lisp((defn other "Doc." [] 2))lisp", "agent/a")).status ==
	        RevisionStatus::Applied);
	REQUIRE(f.repository.submit(setting(at("other"), R"lisp((defn other "Doc." [] 3))lisp", "person")).status ==
	        RevisionStatus::Applied);
	REQUIRE(
	    f.repository
	        .submit(setting(at("card"), R"lisp((defn card "Doc." [room] `(label :id ~(str "room-" (:id room)))))lisp",
			                "agent/b"))
	        .status == RevisionStatus::Applied);
	const json authors = f.repository.programAuthors();
	CHECK(authors["app.lisp"]["other"] == "person");
	CHECK(authors["app.lisp"]["card"] == "agent/b");
	CHECK(authors["app.lisp"]["rooms"] == "file");

	// What the next stage gives its interpreter
	const lisp::Program program = lisp::programOf(f.repository.sourceDocument()["program"], authors);
	lisp::Interpreter interpreter;
	program.load(interpreter);
	CHECK(lisp::Interpreter::authorOf(interpreter.global("card").value_or(lisp::Value())) == "agent/b");
	CHECK(lisp::Interpreter::authorOf(interpreter.global("other").value_or(lisp::Value())) == "person");
}

TEST_CASE("lisp document: impact and behaviour count what the tree doesn't show") {
	Fixture f(kGoverned);
	const auto proposed = f.repository.submit(setting(at("helper"), R"lisp((defn helper "Doc." [] 2))lisp", "agent"));
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	const auto open = f.repository.proposals();
	REQUIRE(open.size() == 1);
	CHECK(open[0].impact["logic"] == 1);
	CHECK(open[0].impact["view"] == 0);
	CHECK(open[0].behaviour == std::vector<std::string>{"changes the definition 'helper' of app.lisp"});
	const auto title = f.repository.submit(
	    setting(at("card"), R"lisp((defn card "One." [] `(label#title :text "Bye")))lisp", "agent"));
	REQUIRE(title.status == RevisionStatus::Proposed);
	CHECK(f.repository.proposals()[1].impact["view"] == 1);
}

TEST_CASE("lisp document: a change set on a definition applies to another copy") {
	Fixture f;
	REQUIRE(f.repository.submit(setting(at("kill-line") + "/:title", "Plugged")).status == RevisionStatus::Applied);
	const json changes = f.repository.exportChanges(0, "plugin");
	Fixture other;
	const auto problem = other.repository.importChanges(changes);
	INFO(problem.value_or(""));
	CHECK_FALSE(problem.has_value());
	CHECK(other.repository.document()["actions"]["kill-line"]["title"] == "Plugged");
}

TEST_CASE("lisp document, attack: a JSON screen stays static: no revision brings it a program") {
	const json screen = {{"format", "affrescoui-screen"},
	                     {"version", 2},
	                     {"permissions", {{"allow", json::array({"studio"})}, {"reviewers", json::array({"studio"})}}},
	                     {"root", {{"type", "panel"}, {"id", "root"}}}};
	const Rules rules;
	Repository repository(rules, screen);
	for(const char* text : {R"lisp((defscreen main :channels {:out {:socket "x"}} (panel#root)))lisp",
	                        "(defscreen main (panel#other))", R"lisp((defcommand go "Go." :title "Go" [] nil))lisp"}) {
		CAPTURE(text);
		const json name = std::string(text).contains("defcommand") ? "go" : "main";
		const auto result = repository.submit(setting("/program", json{{"x.lisp", {{name, text}}}}, "agent"));
		CHECK(result.status == RevisionStatus::Rejected);
		if(name == "go") CHECK(result.error.contains("static"));
	}
	CHECK_FALSE(repository.sourceDocument().contains("program"));
}

TEST_CASE("lisp document: a JSON section beside a program merges, and may not disagree with it") {
	json source =
	    lisp::sourceDocument("(defcommand c \"Doc.\" [] nil)\n(defscreen main (label#a :text \"T\"))", "x.lisp");
	source["colors"] = {{"accent", "#112233"}};
	source["actions"] = {{"other", {{"do", json::array()}}}};
	const json built = lisp::buildDocument(source);
	CHECK(built["actions"].contains("c"));
	CHECK(built["actions"].contains("other"));
	CHECK(built["colors"]["accent"] == "#112233");
	source["actions"]["c"] = {{"title", "T"}, {"do", json::array()}};
	CHECK_THROWS_WITH_AS(static_cast<void>(lisp::buildDocument(source)), doctest::Contains("built by the program"),
	                     lisp::LispError);
}

TEST_CASE("lisp document: a screen, its include and an app's entry load a program") {
	const TemporaryFolder folder;
	const auto write = [&](const std::string& name, const std::string& text) {
		std::ofstream(folder.path() / name, std::ios::binary) << text;
	};
	write("entry.screen.lisp", kProgram);
	const affresco::Screen screen = affresco::Screen::fromFile(
	    folder.path() / "entry.screen.lisp", folder.path() / "data", affresco::Screen::History::InMemory);
	CHECK(json::parse(screen.documentJson())["root"]["id"] == "list");

	// A JSON screen that includes a program: its nodes are the JSON's, its commands the program's
	write("lib.lisp", "(defcommand hello \"Hi.\" :title \"Hello\" [] nil)\n(defkey \"mod+h\" hello)\n");
	write("json.screen.json",
	      R"({"format": "affrescoui-screen", "version": 2, "include": ["lib.lisp"], "root": {"type": "panel"}})");
	const affresco::Screen included = affresco::Screen::fromFile(
	    folder.path() / "json.screen.json", folder.path() / "data", affresco::Screen::History::InMemory);
	const json merged = json::parse(included.documentJson());
	CHECK(merged["actions"]["hello"]["title"] == "Hello");
	CHECK(merged["program"]["lib.lisp"].contains("defkey mod+h"));

	// An app whose entry is a program
	write("app.lisp",
	      "{:format affrescoui-app :version 1 :id \"rooms\" :name \"Rooms\" :entry \"entry.screen.lisp\"}\n");
	const affresco::Screen app = affresco::Screen::fromApp(folder.path(), {}, affresco::Screen::History::InMemory);
	CHECK(json::parse(app.documentJson())["keys"]["mod+/"] == "kill-line");
}

TEST_CASE("lisp document: the file view shows definitions under their file, with comments, and the built tree beside") {
	const TemporaryFolder folder;
	const std::filesystem::path file = folder.path() / "entry.screen.lisp";
	std::ofstream(file, std::ios::binary) << kProgram;
	affresco::Screen screen =
	    affresco::Screen::fromFile(file, folder.path() / "data", affresco::Screen::History::InMemory);
	const json revision =
	    setting(at("rooms"), ";; The rooms, grown.\n(def rooms \"The rooms.\" [{:id \"b\" :name \"Hall\"}])");
	// The file is named as the program calls it
	const std::string pointer = "/program/entry.screen.lisp/rooms";
	std::string error;
	REQUIRE(screen.applyRevision(json{{"author", "app"}, {"set", {{pointer, revision["set"][at("rooms")]}}}}.dump(),
	                             &error));
	const json changes = screen.fileChanges({{"to", "HEAD"}, {"file", "entry.screen.lisp"}});
	REQUIRE(changes["files"].size() == 2);
	const json& row = changes["files"][0];
	CHECK(row["file"] == "entry.screen.lisp");
	CHECK(row["before"].get<std::string>().contains(";; The rooms.\n(def rooms"));
	CHECK(row["after"].get<std::string>().contains(";; The rooms, grown."));
	CHECK(row["added"] == 2);
	const json& built = changes["files"][1];
	CHECK(built["built"] == true);
	CHECK(built["added"].get<int>() > 0);
	CHECK(changes["impact"]["view"].get<int>() > 0);
}

TEST_CASE("lisp document: a feature's manifest can't hold a program yet") {
	json manifest = {{"format", "affrescoui-feature"},
	                 {"version", 2},
	                 {"name", "f"},
	                 {"release", "1.0.0"},
	                 {"title", "f"},
	                 {"requires", {{"format", 2}}},
	                 {"provides", json::object()}};
	CHECK_NOTHROW(affresco::format::checkManifest(manifest, "f"));
	json top = manifest;
	top["program"] = {{"x.lisp", json::object()}};
	CHECK_THROWS_WITH_AS(affresco::format::checkManifest(top, "f"), doctest::Contains("can't hold a program"),
	                     affresco::DocumentError);
	manifest["provides"]["program"] = {{"x.lisp", json::object()}};
	CHECK_THROWS_WITH_AS(affresco::format::checkManifest(manifest, "f"), doctest::Contains("can't hold a program"),
	                     affresco::DocumentError);
}

TEST_CASE("lisp document: who wrote a program's entries, asked of a document with no program, is nobody") {
	// Found by the revision fuzzer: the first writers were read from a key a JSON document doesn't have
	const Rules rules;
	Repository repository(
	    rules, json{{"format", "affrescoui-screen"}, {"version", 2}, {"root", {{"type", "panel"}, {"id", "views"}}}});
	static_cast<void>(repository.submit(json{{"author", "agent"}, {"remove", json::array({"#views"})}}));
	CHECK(repository.programAuthors().empty());
}
