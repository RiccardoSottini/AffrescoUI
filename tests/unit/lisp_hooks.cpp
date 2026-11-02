/// @file
/// @brief Unit tests for hooks and advice (RFC 0005, stage 5; SECURITY.md, "Hooks and advice"), and the attacks on
/// them, one per route, written as an attacker would, with the revision's scan off unless a test says: the check at run
/// time has to hold by itself.

#include "LispApp.hpp"

using affresco::json;
using affresco::Key;
using affresco::RevisionStatus;
using lispapp::App;
namespace lisp = affresco::lisp;

namespace {

/// The person's commands: `save` reaches outside, runs a hook point, and reaches outside again.
constexpr const char* kBase = R"lisp(
(defhook after-save "Runs after a save.")
(defn save "The persons: reaches outside." [path] (call "hit") path)
(defn save-and-hooks "" [] (call "hit") (run-hooks after-save "name") (call "hit"))
(defn quiet "Does nothing outside." [] (set "state.quiet" 1))
(defn agent-fn "An agent may replace this." [] nil)
(defn other-fn "An agent may replace this." [] nil)
(defcommand a "" :title "A" [] (save "doc"))
(defcommand h "" :title "H" [] (save-and-hooks))
(defkey "ctrl+a" a)
(defkey "ctrl+h" h)
(defscreen main :state {:n 0 :out nil :seen nil :order []} (panel#root))
)lisp";

} // namespace

TEST_CASE("lisp hooks: a program declares a hook, adds to it, runs it, and removing or undoing takes it away") {
	App app(R"lisp(
(defhook changed "After a change.")
(defn first-one "" [what] (push "state.order" (str "first " what)))
(defn second-one "" [what] (push "state.order" (str "second " what)))
(add-hook changed "The first." first-one)
(add-hook changed second-one)
(defcommand a "" :title "A" [] (run-hooks changed "x") (run-hooks "nobody-added-to-this"))
(defkey "ctrl+a" a)
(defscreen main :state {:order []} :actions {:from-json [{:runHooks "changed" :with ["j"]}]} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.errors.empty());
	CHECK(app.state("order") == json::array({"first x", "second x"}));
	app.runtime->runAction("from-json", app.window->root(), json::object());
	CHECK(app.state("order").size() == 4);
	CHECK(app.repository.sourceDocument()["program"]["app.lisp"].contains("add-hook changed second-one"));
	// A person's remove-hook is its own entry
	app.write("remove-hook changed first-one", "(remove-hook changed first-one)", "user");
	app.repository.state()["order"] = json::array();
	app.press(Key::A);
	CHECK(app.state("order") == json::array({"second x"}));
	// Undoing that gives it back
	app.commit(app.repository.revertRevision("HEAD", "user"));
	app.repository.state()["order"] = json::array();
	app.press(Key::A);
	CHECK(app.state("order").size() == 2);
	// Removing the entry removes the hook
	const json remove = {{"author", "user"},
	                     {"message", "m"},
	                     {"remove", json::array({"/program/app.lisp/add-hook changed second-one"})}};
	app.commit(remove);
	app.repository.state()["order"] = json::array();
	app.press(Key::A);
	CHECK(app.state("order") == json::array({"first x"}));
}

TEST_CASE("lisp hooks: several authors' hooks merge, and blame shows who added which") {
	App app(kBase);
	app.write("add-hook after-save quiet", "(add-hook after-save quiet)", "agent/one");
	app.write("add-hook after-save other-fn", "(add-hook after-save other-fn)", "agent/two");
	const json authors = app.repository.programAuthors()["app.lisp"];
	CHECK(authors["add-hook after-save quiet"] == "agent/one");
	CHECK(authors["add-hook after-save other-fn"] == "agent/two");
	app.press(Key::H);
	CHECK(app.state("quiet") == 1);
}

TEST_CASE("lisp advice: before, after and around, on a function, by name and however it is called") {
	App app(R"lisp(
(defn inner "" [x] (push "state.order" (str "inner " x)) (* 2 x))
(defn note-before "" [x] (push "state.order" (str "before " x)))
(defn note-after "" [x] (push "state.order" (str "after " x)))
(defn twice "" [proceed x] (push "state.order" "around") (+ (proceed x) (proceed (inc x))))
(advice inner :before note-before)
(advice inner :after note-after)
(advice inner "Doubles it." :around twice)
(defcommand a "" :title "A" [] (set "state.got" (inner 1)))
(defcommand b "Through apply, and map." :title "B" [] (apply inner [5]) (map inner [6]))
(defcommand c "Later: it stays a definitions function." :title "C" [] (after 10 inner 7))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defkey "ctrl+c" c)
(defscreen main :state {:order [] :got 0} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.errors.empty());
	CHECK(app.state("order") == json::array({"before 1", "around", "inner 1", "inner 2", "after 1"}));
	CHECK(app.state("got") == 6);
	app.repository.state()["order"] = json::array();
	app.press(Key::B);
	CHECK(app.errors.empty());
	CHECK(app.state("order").size() == 10);
	app.repository.state()["order"] = json::array();
	app.press(Key::C);
	app.wait(50);
	CHECK(app.errors.empty());
	CHECK(app.state("order").size() == 5);
}

TEST_CASE("lisp advice: on an action of the document, and a command") {
	App app(R"lisp(
(defn note-before "" [] (push "state.order" "before"))
(defn note-after "" [] (push "state.order" "after"))
(defn wrap "" [proceed] (push "state.order" "around") (proceed))
(advice plain-action :before note-before)
(advice plain-action :after note-after)
(advice plain-action :around wrap)
(defcommand cmd "" :title "C" [] (push "state.order" "cmd"))
(advice cmd :before note-before)
(defkey "ctrl+c" cmd)
(defscreen main :state {:order []} :actions {:plain-action [{:push "state.order" :value "action"}]} (panel#root))
)lisp");
	app.runtime->runAction("plain-action", app.window->root(), json::object());
	CHECK(app.errors.empty());
	CHECK(app.state("order") == json::array({"before", "around", "action", "after"}));
	app.repository.state()["order"] = json::array();
	app.press(Key::C);
	CHECK(app.state("order") == json::array({"before", "cmd"}));
}

TEST_CASE("lisp hooks: a failing hook doesn't stop the others or the action, and it is reported naming its author") {
	App app(kBase);
	app.write("quiet", R"lisp((defn quiet "" [] (set "state.quiet" 1)))lisp", "agent/good");
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (first 12345)))lisp");
	app.write("add-hook after-save agent-fn", "(add-hook after-save agent-fn)");
	app.write("add-hook after-save quiet", "(add-hook after-save quiet)", "agent/good");
	app.press(Key::H);
	CHECK(app.reported("agent/x"));
	CHECK_FALSE(app.reported("12345"));
	CHECK(app.state("quiet") == 1);
	CHECK(app.hits == 2);
	// Advice likewise, and throwing
	app.errors.clear();
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (throw "stop it")))lisp");
	app.write("advice save before agent-fn", "(advice save :before agent-fn)");
	app.hits = 0;
	app.press(Key::A);
	CHECK(app.hits == 1);
	CHECK(app.reported("agent/x"));
}

/* The attacks */

TEST_CASE("lisp attack: the agent's denial of service: its advice and hooks can't take a person's command down") {
	App app(kBase);
	// :before: the person's command keeps its authority, for its control step after it
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (set "state.seen" "ran")))lisp");
	app.write("advice save before agent-fn", "(advice save :before agent-fn)");
	app.press(Key::A);
	CHECK(app.state("seen") == "ran");
	CHECK(app.hits == 1);
	CHECK(app.errors.empty());
	// A hook between two control steps: both run
	app.hits = 0;
	app.write("add-hook after-save agent-fn", "(add-hook after-save agent-fn)");
	app.press(Key::H);
	CHECK(app.hits == 2);
	// Spinning takes only its share: the command still runs, and the error names who
	app.hits = 0;
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (loop [i 0] (recur (inc i)))))lisp");
	app.press(Key::A);
	CHECK(app.hits == 1);
	CHECK(app.reported("agent/x"));
	app.errors.clear();
	app.hits = 0;
	app.press(Key::H);
	CHECK(app.hits == 2);
	CHECK(app.reported("agent/x"));
	// Using up the steps too
	app.errors.clear();
	app.hits = 0;
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (loop [] (set "state.n" 1) (recur))))lisp");
	app.press(Key::A);
	CHECK(app.hits == 1);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: :around holds what it wraps: it can skip it, and that takes the person's review") {
	App app(kBase);
	app.write("agent-fn", R"lisp((defn agent-fn "" [proceed] nil))lisp");
	app.write("advice save around agent-fn", "(advice save :around agent-fn)");
	app.press(Key::A);
	CHECK(app.hits == 0); // skipped: what it does, it does as replacing the function would
	// And it is never by the agent alone: with the scan and the reviews on, it is a proposal
	App reviewed(std::string(kBase) + R"lisp((defn p "" [] nil))lisp", true);
	const json add = {{"author", "agent/x"},
	                  {"message", "m"},
	                  {"set", {{"/program/app.lisp/advice quiet around p", "(advice quiet :around p)"}}}};
	CHECK(reviewed.repository.submit(add).status == RevisionStatus::Proposed);
}

TEST_CASE("lisp attack: :around going on with the original can't reach its control step") {
	App app(kBase);
	app.write("agent-fn", R"lisp((defn agent-fn "" [proceed] (proceed)))lisp");
	app.write("advice save around agent-fn", "(advice save :around agent-fn)");
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	CHECK(app.reported("\"call\""));
}

TEST_CASE("lisp attack: :around changing the arguments to reach a capability, or reading the result") {
	App app(R"lisp(
(defn plain "Does as it is told." [what] (set "state.done" what) "secret result")
(defn agent-fn "" [proceed] nil)
(defcommand a "" :title "A" [] (plain "the persons"))
(defkey "ctrl+a" a)
(defscreen main :state {:done nil :seen nil} (panel#root))
)lisp");
	app.write("agent-fn", R"lisp((defn agent-fn "" [proceed] (set "state.seen" (proceed "the agents"))))lisp");
	app.write("advice plain around agent-fn", "(advice plain :around agent-fn)");
	app.press(Key::A);
	CHECK(app.errors.empty());
	CHECK(app.state("done") == "the persons");
	CHECK(app.state("seen").is_null());
	// An agent's function that takes arguments gets none of the person's
	app.errors.clear();
	app.write("agent-fn", R"lisp((defn agent-fn "" [proceed what] (set "state.seen" what)))lisp");
	app.press(Key::A);
	CHECK(app.state("seen").is_null());
	CHECK_FALSE(app.errors.empty());
}

TEST_CASE("lisp attack: advice can't read a person's arguments or their result") {
	App app(kBase);
	app.write("agent-fn", R"lisp((defn agent-fn "" [& seen] (set "state.seen" seen)))lisp");
	app.write("advice save before agent-fn", "(advice save :before agent-fn)");
	app.write("advice save after agent-fn", "(advice save :after agent-fn)");
	app.press(Key::A);
	CHECK(app.state("seen") == json::array());
	// A hook, likewise: the runner's values aren't an agent's to read, as what it may read is in the state, under masks
	app.write("add-hook after-save agent-fn", "(add-hook after-save agent-fn)");
	app.repository.state()["seen"] = nullptr;
	app.press(Key::H);
	CHECK(app.state("seen") == json::array());
	// The person's own do get them
	App own(kBase);
	own.write("quiet", R"lisp((defn quiet "" [what] (set "state.seen" what)))lisp", "user");
	own.write("add-hook after-save quiet", "(add-hook after-save quiet)", "user");
	own.press(Key::H);
	CHECK(own.state("seen") == "name");
}

TEST_CASE("lisp attack: an agent's advice or hook running a person's function under its own decision") {
	App app(kBase);
	// The entry is the agent's and the function the person's: who decided it runs here counts
	app.write("add-hook after-save save", "(add-hook after-save save)");
	app.errors.clear();
	app.hits = 0;
	app.press(Key::H);
	CHECK(app.hits == 2); // the person's own two; not the third, the hook's
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: an agent's advice on a person's function that has no control step, used to launder one") {
	App app(R"lisp(
(defn reach "" [& given] (call "hit"))
(defn mine "" [] 1)
(defcommand a "" :title "A" [] (mine))
(defkey "ctrl+a" a)
(defscreen main (panel#root))
)lisp");
	app.write("advice mine after reach", "(advice mine :after reach)");
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	app.errors.clear();
	app.write("advice mine around reach", "(advice mine :around reach)");
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: a hook or advice can't name a step, or anything but a function of the program") {
	App app(kBase);
	for(const char* text :
	    {"(add-hook after-save call)", "(advice save :before call)", "(advice call :before quiet)",
	     "(advice accept :before quiet)", "(add-hook after-save nothing-here)", "(advice nothing-here :after quiet)"}) {
		CAPTURE(text);
		const auto entries = lisp::splitEntries(text, "app.lisp");
		REQUIRE(entries.size() == 1);
		const auto error = app.repository.commit(
		    {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/" + entries[0].name, text}}}});
		CHECK(error.has_value());
	}
	// A wrong kind of advice, and a closure in a hook, don't read at all
	for(const char* text : {"(advice save :instead quiet)", "(add-hook after-save (fn [] 1))"})
		CHECK_THROWS_AS(
		    static_cast<void>(lisp::buildText(std::string(text) + "(defn save [] 1)(defn quiet [] 1)", "a")),
		    affresco::DocumentError);
}

TEST_CASE("lisp attack: a hook on a review step, and advice on a function holding one") {
	App app(R"lisp(
(defn agent-fn "" [] nil)
(defn approve "The persons: decides." [] (accept 1 :head "abc"))
(defcommand a "" :title "A" [] (approve))
(defkey "ctrl+a" a)
(defscreen main :state {:seen nil} (panel#root))
)lisp");
	// A hook of that name is only a name: nothing runs it
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (set "state.seen" "ran") (accept 1 :head "abc")))lisp");
	app.write("add-hook accept agent-fn", "(add-hook accept agent-fn)");
	app.press(Key::A);
	app.settle();
	CHECK(app.state("seen").is_null());
	// Advice runs, and has no way to decide
	app.write("advice approve before agent-fn", "(advice approve :before agent-fn)");
	app.errors.clear();
	app.press(Key::A);
	app.settle();
	CHECK(app.state("seen") == "ran");
	CHECK(app.reported("agent/x"));
	CHECK(app.reported("\"accept\""));
}

TEST_CASE("lisp attack: hook order used to launder") {
	App app(kBase);
	// The person's hook after an agent's has the person's authority, nothing more; an agent's after a person's has its
	// own
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (call "hit")))lisp");
	app.write("quiet", R"lisp((defn quiet "" [& given] (call "hit")))lisp", "user");
	app.write("add-hook after-save agent-fn", "(add-hook after-save agent-fn)");
	app.write("add-hook after-save quiet", "(add-hook after-save quiet)", "user");
	app.hits = 0;
	app.errors.clear();
	app.press(Key::H);
	CHECK(app.hits == 3); // the person's two and the person's hook; never the agent's
	CHECK(app.reported("agent/x"));
	// An agent's hook that runs the point again: the person's hook under it is no way up
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (run-hooks "second")))lisp");
	app.write("add-hook second quiet", "(add-hook second quiet)", "user");
	app.hits = 0;
	app.errors.clear();
	app.press(Key::H);
	CHECK(app.hits == 3);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: an agent's hook on the person's run keeps the agent's authority when it schedules") {
	App app(kBase);
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (after 10 save "x")))lisp");
	app.write("add-hook after-save agent-fn", "(add-hook after-save agent-fn)");
	app.press(Key::H);
	CHECK(app.hits == 2);
	app.errors.clear();
	app.wait(50);
	CHECK(app.hits == 2);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: a person's hook scheduled by agent code runs under the agent") {
	App app(R"lisp(
(defhook point "")
(defn mine "" [] (call "hit"))
(add-hook point mine)
(defn fire "" [] (run-hooks point))
(defn helper "" [] nil)
(defcommand c "" :title "C" [] (helper))
(defcommand f "" :title "F" [] (fire))
(defkey "ctrl+c" c)
(defkey "ctrl+f" f)
(defscreen main (panel#root))
)lisp");
	app.press(Key::F);
	CHECK(app.hits == 1); // the person's run
	app.hits = 0;
	app.write("helper", R"lisp((defn helper "" [] (after 10 fire)))lisp");
	app.press(Key::C);
	app.wait(50);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: nothing leaves a sealed run: closures and the like can't be kept") {
	App app(kBase);
	// The only ways out of a hook are the state, which takes no function, and a callback, which keeps its authors
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (set "state.seen" call)))lisp");
	app.write("add-hook after-save agent-fn", "(add-hook after-save agent-fn)");
	app.press(Key::H);
	CHECK(app.state("seen").is_null());
	CHECK(app.hits == 2);
	// `proceed` is no callback either
	app.errors.clear();
	app.write("other-fn", R"lisp((defn other-fn "" [proceed] (after 10 proceed)))lisp");
	app.write("advice save around other-fn", "(advice save :around other-fn)");
	app.hits = 0;
	app.press(Key::A);
	app.wait(50);
	CHECK(app.hits == 0);
	CHECK(app.reported("callback"));
}

TEST_CASE("lisp attack: undo leaving a hook behind") {
	App app(kBase);
	app.write("agent-fn", R"lisp((defn agent-fn "" [] (increment "state.n")))lisp");
	app.write("add-hook after-save agent-fn", "(add-hook after-save agent-fn)");
	app.press(Key::H);
	CHECK(app.state("n") == 1);
	app.commit(app.repository.revertRevision("HEAD", "user"));
	CHECK_FALSE(app.repository.sourceDocument()["program"]["app.lisp"].contains("add-hook after-save agent-fn"));
	app.press(Key::H);
	CHECK(app.state("n") == 1);
	// And a restore to the start takes all of it
	app.write("add-hook after-save agent-fn", "(add-hook after-save agent-fn)");
	app.press(Key::H);
	CHECK(app.state("n") == 2);
	app.commit(app.repository.restoreRevision(0, "user"));
	app.press(Key::H);
	CHECK(app.state("n") == 2);
}

TEST_CASE("lisp attack: taking a function away with a hook on it") {
	App app(kBase, true);
	app.write("add-hook after-save quiet", "(add-hook after-save quiet)", "user");
	const json remove = {{"author", "user"}, {"message", "m"}, {"remove", json::array({"/program/app.lisp/quiet"})}};
	CHECK(app.repository.commit(remove).has_value());
}

TEST_CASE("lisp attack: a hook adding a hook, recursion among hooks, and budgets are bounded") {
	App app(R"lisp(
(defhook point "")
(defhook other "")
(defn adder "Adds itself another, as an evaluation." [] (evaluate "(add-hook point noop)"))
(defn noop "" [] (increment "state.noops"))
(defn again "" [] (increment "state.again") (run-hooks point))
(defn across "" [] (run-hooks other))
(defn back "" [] (run-hooks point))
(defn deep "" [n] (deep (inc n)))
(defn deep-before "" [n] 1)
(advice deep :before deep-before)
(add-hook point adder)
(defcommand a "" :title "A" [] (run-hooks point))
(defkey "ctrl+a" a)
(defcommand d "" :title "D" [] (deep 0))
(defkey "ctrl+d" d)
(defscreen main :state {:noops 0 :again 0} (panel#root))
)lisp");
	// A hook adding a hook: it is for the next run, in a new version of the program, and what it evaluates isn't what
	// the person typed, so it waits for them
	const int head = app.repository.head().number;
	app.press(Key::A);
	app.settle();
	CHECK(app.errors.empty());
	CHECK(app.state("noops") == 0);
	CHECK(app.repository.head().number == head);
	REQUIRE(app.repository.proposals().size() == 1);
	REQUIRE_FALSE(
	    app.repository.accept(app.repository.proposals()[0].id, {"user", app.repository.proposals()[0].head, false})
	        .has_value());
	app.write("adder", "(defn adder \"\" [] nil)", "user");
	app.press(Key::A);
	CHECK(app.state("noops") == 1);
	// Itself, and through another point: runs once at a time
	app.write("add-hook point again", "(add-hook point again)", "user");
	app.errors.clear();
	app.press(Key::A);
	CHECK(app.state("again") == 1);
	CHECK(app.reported("once at a time"));
	app.write("add-hook point across", "(add-hook point across)", "user");
	app.write("add-hook other back", "(add-hook other back)", "user");
	app.errors.clear();
	app.press(Key::A);
	CHECK(app.reported("once at a time"));
	// Advice on what recurses ends at a limit that names what ran out
	app.errors.clear();
	app.press(Key::D);
	CHECK_FALSE(app.errors.empty());
}

TEST_CASE("lisp attack: adding advice or a hook to what has control steps takes the review changing it takes") {
	App app(std::string(kBase) + R"lisp((defn p "" [] nil))lisp", true);
	const auto submit = [&](const std::string& address, const std::string& text) {
		return app.repository.submit(
		    {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/" + address, text}}}});
	};
	// Any advice, on what reaches a control step or not
	CHECK(submit("advice save before p", "(advice save :before p)").status == RevisionStatus::Proposed);
	CHECK(submit("advice quiet after p", "(advice quiet :after p)").status == RevisionStatus::Proposed);
	// A hook where the hook point meets a control step
	CHECK(submit("add-hook after-save p", "(add-hook after-save p)").status == RevisionStatus::Proposed);
	// Where nothing does, it is as any function: a hook point no control step comes near
	App free(R"lisp(
(defhook tidy "")
(defn p "" [] nil)
(defcommand c "" :title "C" [] (run-hooks tidy))
(defkey "ctrl+c" c)
(defscreen main :permissions {:allow ["agent"] :reviewers ["studio"]} (panel#root))
)lisp",
	         true);
	const auto harmless =
	    free.repository.submit({{"author", "agent/x"},
		                        {"message", "m"},
		                        {"set", {{"/program/app.lisp/add-hook tidy p", "(add-hook tidy p)"}}}});
	CHECK(harmless.status == RevisionStatus::Applied);
	// Advice that would make a key's function reach a control step: the scan refuses it outright
	App scanned(R"lisp(
(defn reach "" [] (call "hit"))
(defn mine "" [] 1)
(defcommand a "" :title "A" [] (mine))
(defkey "ctrl+a" a)
(defscreen main (panel#root))
)lisp",
	            true);
	const auto refused = scanned.repository.submit(
	    {{"author", "agent/x"},
		 {"message", "m"},
		 {"set", {{"/program/app.lisp/advice mine after reach", "(advice mine :after reach)"}}}});
	CHECK(refused.status == RevisionStatus::Rejected);
	CHECK(refused.error.contains("permission denied"));
}

TEST_CASE("lisp attack: a proposal previewed that registers a hook which runs while previewing") {
	App app(R"lisp(
(defhook point "")
(defn fire "" [] (run-hooks point))
(defn start "" [] (after 10 fire))
(defn mine "" [] (set "state.seen" "ran") (call "hit") (accept 1 :head "abc"))
(defcommand f "" :title "F" [] (fire))
(defcommand s "" :title "S" [] (start))
(defkey "ctrl+f" f)
(defkey "ctrl+s" s)
(defscreen main :permissions {:allow ["studio"] :reviewers ["studio"]} :state {:seen nil} (panel#root))
)lisp");
	const auto proposed =
	    app.repository.propose({{"author", "agent/x"},
		                        {"message", "m"},
		                        {"set", {{"/program/app.lisp/add-hook point mine", "(add-hook point mine)"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	app.press(Key::S);
	REQUIRE_FALSE(app.repository.preview(proposed.proposal).has_value());
	app.errors.clear();
	app.wait(50); // fires by itself in the preview: the hook is the proposal's
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	app.errors.clear();
	app.press(Key::F); // the person's key in the preview: the hook is still the proposal's
	app.settle();
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	app.repository.endPreview();
	// When it isn't accepted, nothing of it stays
	app.errors.clear();
	app.repository.state()["seen"] = nullptr;
	app.press(Key::F);
	CHECK(app.state("seen").is_null());
	CHECK(app.errors.empty());
}

TEST_CASE("lisp hooks: a value read for the screen can't run a hook's steps") {
	App app(R"lisp(
(defhook point "")
(defn mine "" [] (set "state.n" 9))
(add-hook point mine)
(defn base "" [] state.n)
(defn before-it "" [] (set "state.n" 9))
(advice base :before before-it)
(defscreen main :state {:n 0} (column (label#a :text ~(base))))
)lisp");
	CHECK(app.state("n") == 0);
	CHECK(app.reported("only reads"));
	const auto* label = dynamic_cast<affresco::Label*>(app.window->root().findById("a"));
	REQUIRE(label);
	CHECK(label->text() == "0");
	// run-hooks itself is a step
	const App direct(R"lisp(
(defhook point "")
(defn mine "" [] (set "state.n" 9))
(add-hook point mine)
(defn reader "" [] (try (run-hooks point) (catch e nil)) state.n)
(defscreen main :state {:n 0} (column (label#a :text ~(reader))))
)lisp");
	CHECK(direct.state("n") == 0);
}
