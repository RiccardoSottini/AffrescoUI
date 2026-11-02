/// @file
/// @brief Unit tests for a running program's loop (RFC 0005, stage 4; SECURITY.md, "The loop"): evaluating a form is a
/// revision, what a person types has their authority and what arrives from elsewhere is a proposal, and describing what
/// is defined obeys read masks. The attacks are written as an attacker would, one per route.

#include "LispApp.hpp"

#include <affresco/lisp/Program.hpp>

#include <filesystem>

using affresco::json;
using affresco::Key;
using affresco::RevisionStatus;
using lispapp::App;
namespace lisp = affresco::lisp;

namespace {

/// The person's editor: a key evaluates the text in the state, and the function it runs shows the result.
constexpr const char* kLoop = R"lisp(
(defn shown "Shows what was evaluated." [result error] (set "state.result" result) (set "state.error" error))
(defcommand eval-it "Evaluates the text." :title "Eval" [] (evaluate state.text :file "init.lisp" :then shown))
(defn save "Reaches nothing yet." [] 1)
(defcommand a "Runs save." :title "A" [] (save))
(defn helper "An agent may replace this." [] 1)
(defcommand c "Calls helper." :title "C" [] (helper))
(defkey "ctrl+e" eval-it)
(defkey "ctrl+a" a)
(defkey "ctrl+c" c)
(defscreen main :state {:text "" :result nil :error nil :out nil} (panel#root))
)lisp";

/// @brief Puts a text in the state and evaluates it with the person's key.
/// @param app The app.
/// @param text The text.
void typeAndEvaluate(App& app, const std::string& text) {
	app.repository.state()["text"] = text;
	app.press(Key::E);
	app.settle();
}

} // namespace

TEST_CASE("lisp forms: the form before a position, the one around it, and all of them") {
	const std::string text = "(defn a [] 1)\n\n(foo (+ 1 2) bar \"é\")  ; note\n(unfinished ";
	CHECK(lisp::formBefore(text, 1, 14)->text == "(defn a [] 1)");
	CHECK(lisp::formBefore(text, 1, 14)->kind == "defn");
	CHECK(lisp::formBefore(text, 1, 14)->name == "a");
	CHECK(lisp::formBefore(text, 3, 13)->text == "(+ 1 2)");
	CHECK(lisp::formBefore(text, 3, 17)->text == "bar");
	CHECK(lisp::formBefore(text, 3, 21)->text == "\"é\"");
	CHECK(lisp::formBefore(text, 3, 22)->text == "(foo (+ 1 2) bar \"é\")");
	CHECK(lisp::formBefore(text, 3, 22)->endColumn == 22);
	CHECK(lisp::formBefore(text, 3, 30)->text == "(foo (+ 1 2) bar \"é\")"); // in the comment
	CHECK_FALSE(lisp::formBefore(text, 1, 1).has_value());
	CHECK(lisp::formAround(text, 3, 8)->text == "(foo (+ 1 2) bar \"é\")");
	CHECK(lisp::formAround(text, 2, 1)->name == "a");      // the last before it
	CHECK_FALSE(lisp::formAround(text, 4, 5).has_value()); // not finished
	CHECK(lisp::formBefore("'x", 1, 3)->text == "'x");
	const auto forms = lisp::formsOf("(defn a [] 1) (+ 1 2)", "");
	REQUIRE(forms.size() == 2);
	CHECK(forms[0].name == "a");
	CHECK(forms[1].kind.empty());
	CHECK_THROWS_AS(static_cast<void>(lisp::formsOf("(a", "")), affresco::DocumentError);
}

TEST_CASE("lisp forms: an expression can read them, and print a value as Lisp") {
	App app(R"lisp(
(defn read-them "" [text]
  (set "state.before" (:text (formBefore text 1 8)))
  (set "state.around" (:name (formAround text 1 3)))
  (set "state.printed" (printForm {:a [1 "x" nil true]}))
  (set "state.printedHere" (pr-str {:a [1 "x" nil true]} 2)))
(defcommand a "" :title "A" [] (read-them "(defn f [] 1)"))
(defkey "ctrl+a" a)
(defscreen main (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.errors.empty());
	CHECK(app.state("before") == "f");
	CHECK(app.state("around") == "f");
	CHECK(app.state("printed") == "{:a [1 \"x\" nil true]}");
	CHECK(app.state("printedHere") == "{:a [1 \"x\" nil true]} 2");
}

TEST_CASE("lisp loop: evaluating a definition replaces it, the next key uses it, and it lands in history") {
	App app(kLoop, true);
	const int before = app.repository.head().number;
	typeAndEvaluate(app, R"lisp((defn save "Mine now." [] (set "state.out" "redefined")))lisp");
	CHECK(app.errors.empty());
	CHECK(app.state("result") == "save");
	CHECK(app.repository.head().number == before + 1);
	CHECK(app.repository.programAuthors()["app.lisp"]["save"] == "user");
	CHECK(app.repository.sourceDocument()["program"]["app.lisp"]["save"].get<std::string>().contains("Mine now."));
	app.press(Key::A);
	CHECK(app.state("out") == "redefined");
	// Undo gives the definition back, exactly
	app.commit(app.repository.revertRevision("HEAD", "user"));
	app.repository.state()["out"] = nullptr;
	app.press(Key::A);
	CHECK(app.state("out").is_null());
	CHECK(
	    app.repository.sourceDocument()["program"]["app.lisp"]["save"].get<std::string>().contains("Reaches nothing"));
}

TEST_CASE(
    "lisp loop: a new definition goes in the file the step names, and what isn't a definition changes no history") {
	App app(kLoop, true);
	typeAndEvaluate(app, R"lisp((defn fresh "New." [] 40))lisp");
	CHECK(app.repository.sourceDocument()["program"]["init.lisp"].contains("fresh"));
	const int head = app.repository.head().number;
	typeAndEvaluate(app, "(+ 1 (fresh) 1)");
	CHECK(app.state("result") == "42");
	CHECK(app.repository.head().number == head);
	typeAndEvaluate(app, R"lisp({:a [1 "x"]})lisp");
	CHECK(app.state("result") == "{:a [1 \"x\"]}");
	// A region: its definitions first, then what uses them, and the last form's value
	typeAndEvaluate(app, "(defn g \"\" [] 5) (* 2 (g))");
	CHECK(app.state("result") == "10");
	// What goes wrong is shown, with no value in it
	app.errors.clear();
	typeAndEvaluate(app, "(first 12345)");
	CHECK_FALSE(app.state("error").is_null());
	CHECK_FALSE(app.state("error").get<std::string>().contains("12345"));
	typeAndEvaluate(app, "(unfinished");
	CHECK_FALSE(app.state("error").is_null());
}

TEST_CASE("lisp loop: a person's expression has the person's authority, and effects when it says so") {
	App app(kLoop, true);
	typeAndEvaluate(app, R"lisp((do (set "state.out" 7) (call "hit") state.out))lisp");
	CHECK(app.errors.empty());
	CHECK(app.hits == 1);
	CHECK(app.state("out") == 7);
	CHECK(app.state("result") == "7");
}

TEST_CASE("lisp attack: what only the person's input may do: a person's revision may reach a control step") {
	App app(kLoop, true); // the scan is on
	const std::string reaches = R"lisp((defn save "Reaches outside." [] (call "hit")))lisp";
	// Nobody else's revision may, the person's name or not
	const json byAgent = {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/save", reaches}}}};
	const json claimed = {{"author", "user"}, {"message", "m"}, {"set", {{"/program/app.lisp/save", reaches}}}};
	CHECK(app.repository.submit(byAgent).status == RevisionStatus::Rejected);
	CHECK(app.repository.submit(claimed).status == RevisionStatus::Rejected);
	CHECK(app.repository.commit(claimed).has_value());
	// Their own input does
	typeAndEvaluate(app, reaches);
	CHECK(app.errors.empty());
	app.press(Key::A);
	CHECK(app.hits == 1);
	// And the same hand can undo it, by a step from their input
	App second(std::string(kLoop) + R"lisp((defcommand un "" :title "U" [] (undo)) (defkey "ctrl+u" un))lisp", true);
	typeAndEvaluate(second, reaches);
	second.press(Key::A);
	CHECK(second.hits == 1);
	second.press(Key::U);
	second.settle();
	CHECK(second.errors.empty());
	second.press(Key::A);
	CHECK(second.hits == 1);
	// An agent's undo of it stays refused
	typeAndEvaluate(second, reaches);
	const auto reverted = second.repository.submit(second.repository.revertRevision("HEAD", "agent/x"));
	CHECK(reverted.status == RevisionStatus::Rejected);
}

TEST_CASE("lisp attack: nothing else gets past with the person's own input") {
	App app(kLoop, true);
	for(const char* form :
	    {R"lisp((defscreen main :permissions {:allow ["agent"]} :state {:text ""} (panel#root)))lisp",
	     R"lisp((defscreen main :channels {:x {:path "x"}} :state {:text ""} (panel#root)))lisp",
	     R"lisp((defscreen main :persist ["state.text"] :state {:text ""} (panel#root)))lisp",
	     R"lisp((defscreen main :services {:x {}} :state {:text ""} (panel#root)))lisp"}) {
		CAPTURE(form);
		const int head = app.repository.head().number;
		app.errors.clear();
		typeAndEvaluate(app, form);
		CHECK(app.repository.head().number == head);
		CHECK_FALSE(app.state("error").is_null());
	}
	// The way in takes a patch of the program, and only that
	const auto outside = [&](const json& revision) { return app.repository.commitOwnInput(revision).has_value(); };
	CHECK(outside({{"author", "user"}, {"message", "m"}, {"set", {{"/state/out", 1}}}}));
	CHECK(outside({{"author", "user"},
	               {"message", "m"},
	               {"patch", {{{"op", "replace"}, {"path", "/state/out"}, {"value", 1}}}}}));
	CHECK(outside({{"author", "user"},
	               {"message", "m"},
	               {"patch", {{{"op", "copy"}, {"from", "/state"}, {"path", "/program/app.lisp/x"}}}}}));
	CHECK(outside({{"author", "user"},
	               {"message", "m"},
	               {"patch", {{{"op", "add"}, {"path", "/permissions"}, {"value", json::object()}}}}}));
}

TEST_CASE("lisp attack: evaluate from a timer, a watch, a handler or agent code") {
	App app(R"lisp(
(defn shown "" [result error] (set "state.result" result))
(defn go "" [] (evaluate "(defn save [] (call \"hit\"))" :then shown))
(defn save "" [] 1)
(defn helper "" [] 1)
(defcommand later "Starts a timer: the key is the persons, the firing isnt." :title "L" [] (after 10 go))
(defcommand poke "Sets off the watch." :title "P" [] (increment "state.n"))
(defcommand a "" :title "A" [] (save))
(defcommand c "" :title "C" [] (helper))
(defkey "ctrl+l" later)
(defkey "ctrl+p" poke)
(defkey "ctrl+a" a)
(defkey "ctrl+c" c)
(defscreen main :state {:n 0 :result nil}
  :watch {:state.n "go"}
  :onLoad [{:lisp "go"}]
  :actions {:timed [{:after 10 :do [{:evaluate "(defn save [] 2)"}]}]}
  (panel#root :every {:ms 20 :do "go"}))
)lisp");
	const int head = app.repository.head().number;
	CHECK(app.reported("own input")); // onLoad, on its own
	app.errors.clear();
	app.wait(50); // the panel's timer
	CHECK(app.reported("own input"));
	app.errors.clear();
	app.press(Key::L);
	app.wait(50); // a person's own timer
	CHECK(app.reported("own input"));
	app.errors.clear();
	app.press(Key::P);
	app.settle();
	app.wait(1); // a watch
	CHECK(app.reported("own input"));
	app.errors.clear();
	app.runtime->runAction("timed", app.window->root(), json::object());
	app.wait(50); // a JSON action's own step, on a timer
	CHECK(app.reported("own input"));
	app.runtime->runAction("go", app.window->root(), json::object());
	CHECK(app.repository.head().number == head);
	// Agent code, from the person's key
	app.write("helper", R"lisp((defn helper "" [] (evaluate "(defn save [] (call \"hit\"))")))lisp");
	app.errors.clear();
	app.press(Key::C);
	app.settle();
	CHECK(app.reported("agent/x"));
	CHECK(app.repository.head().number == head + 1);
	// Pressing the person's key from a timer isn't the person's input
	app.write("helper", R"lisp((defn helper "" [] 1))lisp", "file");
	CHECK(app.hits == 0);
}

TEST_CASE("lisp attack: a key pressed for the screen isn't the person's input") {
	App app(R"lisp(
(defn shown "" [result error] (set "state.result" result))
(defcommand eval-it "" :title "E" [] (evaluate "(+ 1 1)" :then shown))
(defn presser "" [] (press "ctrl+e"))
(defn start "" [] (after 10 presser))
(defcommand a "" :title "A" [] (start))
(defkey "ctrl+e" eval-it)
(defkey "ctrl+a" a)
(defscreen main :state {:result nil} (panel#root))
)lisp");
	app.press(Key::A);
	app.wait(50);
	CHECK(app.state("result").is_null());
}

TEST_CASE("lisp attack: a person's eval of a form holding an agent's macro") {
	App app(std::string(kLoop) + R"lisp((defmacro written "An agent may replace this." [] nil))lisp");
	app.write("written", R"lisp((defmacro written "Mine." [] `(call "hit")))lisp");
	// As a definition: the person's function holds what the agent's macro wrote
	typeAndEvaluate(app, R"lisp((defn save "The persons." [] (written)))lisp");
	CHECK(app.repository.programAuthors()["app.lisp"]["save"] == "user");
	app.errors.clear();
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	// As an expression
	app.errors.clear();
	typeAndEvaluate(app, "(written)");
	CHECK(app.hits == 0);
	CHECK(app.state("error").get<std::string>().contains("agent/x"));
	// A macro running while it is read reaches nothing, whoever wrote it, and the person's key doesn't help it
	app.write("while-read", R"lisp((defmacro while-read "" [] (call "hit") nil))lisp");
	typeAndEvaluate(app, "(while-read)");
	CHECK(app.hits == 0);
	CHECK(app.state("error").get<std::string>().contains("reaches outside"));
	// A macro that defines is a definition: it lands as an entry, and the agent's authority holds at run time
	app.write("defining", R"lisp((defmacro defining "" [n] `(defn ~n "" [] (call "hit"))))lisp");
	typeAndEvaluate(app, "(defining save)");
	CHECK(app.state("result") == "save");
	app.errors.clear();
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: exactly the text an agent wrote stays the agent's, whoever evaluates it") {
	App app(kLoop);
	const std::string text = R"lisp((defn save "An agents." [] (call "hit")))lisp";
	app.write("save", text);
	app.write("save", R"lisp((defn save "Someone elses." [] 1))lisp", "user");
	typeAndEvaluate(app, text);
	CHECK(app.repository.programAuthors()["app.lisp"]["save"] == "agent/x");
	app.errors.clear();
	app.press(Key::A);
	CHECK(app.hits == 0);
}

TEST_CASE("lisp attack: evaluating while a proposal is previewed is refused") {
	App app(std::string(kLoop) + R"lisp((defn extra "" [] 1))lisp", true);
	const auto proposed = app.repository.propose(
	    {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/extra", "(defn extra \"\" [] 2)"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	REQUIRE_FALSE(app.repository.preview(proposed.proposal).has_value());
	const int head = app.repository.head().number;
	app.errors.clear();
	typeAndEvaluate(app, "(defn extra \"\" [] 3)");
	CHECK(app.reported("previewed"));
	CHECK(app.repository.head().number == head);
}

TEST_CASE("lisp loop: a change of the program a person made survives a restart of the history") {
	const std::filesystem::path folder = std::filesystem::temp_directory_path() /
	                                     ("affresco-own-" + std::to_string(reinterpret_cast<std::uintptr_t>(&kLoop)));
	std::filesystem::remove_all(folder);
	const std::string reaches = R"lisp((defn save "Reaches outside." [] (call "hit")))lisp";
	{
		App app(kLoop, true);
		static_cast<void>(app.repository.openHistory(folder));
		typeAndEvaluate(app, reaches);
		CHECK(app.errors.empty());
	}
	{
		App app(kLoop, true);
		const auto opened = app.repository.openHistory(folder);
		CHECK(opened.loaded);
		app.press(Key::A);
		CHECK(app.hits == 1);
	}
	std::filesystem::remove_all(folder);
}

/* init.lisp: what arrives from elsewhere */

TEST_CASE(
    "lisp init: a downloaded file arrives as a proposal, in its own name, with no more authority for being accepted") {
	App app(std::string(kLoop) + R"lisp(
(defcommand load-it "" :title "L" [] (propose state.text :as "init.lisp" :file "init.lisp" :then opened :onError refused))
(defn opened "" [proposal] (set "state.proposal" proposal))
(defn refused "" [error] (set "state.refused" error))
(defkey "ctrl+l" load-it)
)lisp",
	        true);
	app.repository.state()["text"] = R"lisp(
;; Mine.
(defn hello "Says hello." [] (set "state.out" "hello"))
(defcommand greet "Greets." :title "Greet" [] (hello))
)lisp";
	const int head = app.repository.head().number;
	app.press(Key::L);
	CHECK(app.errors.empty());
	CHECK(app.repository.head().number == head); // nothing landed
	REQUIRE(app.repository.proposals().size() == 1);
	CHECK(app.repository.proposals()[0].author == "plugin:init.lisp");
	const int id = app.state("proposal").get<int>();
	REQUIRE_FALSE(app.repository.accept(id, {"user", app.repository.proposals()[0].head, false}).has_value());
	CHECK(app.repository.programAuthors()["init.lisp"]["hello"] == "plugin:init.lisp");
	app.runtime->runAction("greet", app.window->root(), json::object());
	CHECK(app.state("out") == "hello");
	// Undoing it takes it away
	app.commit(app.repository.revertRevision("HEAD", "user"));
	CHECK_FALSE(app.repository.sourceDocument()["program"].contains("init.lisp"));
}

TEST_CASE("lisp attack: an init.lisp with control steps") {
	App app(std::string(kLoop) + R"lisp(
(defcommand load-it "" :title "L" [] (propose state.text :as "init.lisp" :file "init.lisp" :onError refused))
(defn refused "" [error] (set "state.refused" error))
(defkey "ctrl+l" load-it)
)lisp",
	        true);
	// What reaches a control step from a key, a timer or onLoad it adds: the scan refuses it, as any revision's
	app.repository.state()["text"] = R"lisp((defn evil "" [] (call "hit")) (defkey "ctrl+z" evil))lisp";
	app.press(Key::L);
	CHECK(app.repository.proposals().empty());
	CHECK(app.state("refused").get<std::string>().contains("permission denied"));
	// A form that would run by itself isn't a definition, and doesn't run
	app.repository.state()["refused"] = nullptr;
	app.repository.state()["text"] = R"lisp((call "hit"))lisp";
	app.press(Key::L);
	CHECK(app.repository.proposals().empty());
	CHECK(app.state("refused").get<std::string>().contains("definitions"));
	CHECK(app.hits == 0);
	// A function with a control step no key reaches lands, accepted, and its step is refused when it runs at all
	app.repository.state()["text"] = R"lisp((defn evil "" [] (call "hit")))lisp";
	app.press(Key::L);
	REQUIRE(app.repository.proposals().size() == 1);
	REQUIRE_FALSE(
	    app.repository.accept(app.repository.proposals()[0].id, {"user", app.repository.proposals()[0].head, false})
	        .has_value());
	app.errors.clear();
	app.runtime->runAction("evil", app.window->root(), json::object());
	CHECK(app.hits == 0);
	CHECK(app.reported("plugin:init.lisp"));
}

TEST_CASE("lisp init: a person's definitions export as a change set, and import under no more authority") {
	App app(std::string(kLoop) + R"lisp(
(defcommand export-it "" :title "X" [] (exportChanges "my init" :only "/program/init.lisp" :into "state.changes"))
(defkey "ctrl+x" export-it)
)lisp",
	        true);
	typeAndEvaluate(app, R"lisp((defn reach "" [] (call "hit")))lisp");
	typeAndEvaluate(app, R"lisp((defcommand use-it "" :title "U" [] (set "state.used" 1)))lisp");
	app.press(Key::X);
	const json changes = app.state("changes");
	REQUIRE(changes.is_object());
	CHECK(changes["format"] == "affrescoui-changes");
	CHECK(changes["revisions"].size() == 2);
	// Another copy of the app: as a change set does, it lands in the set's name
	App other(std::string(kLoop), true);
	REQUIRE_FALSE(other.repository.importChanges(changes).has_value());
	CHECK(other.repository.programAuthors()["init.lisp"]["reach"] == "plugin:my init");
	other.errors.clear();
	other.runtime->runAction("reach", other.window->root(), json::object());
	CHECK(other.hits == 0);
	CHECK(other.reported("plugin:my init"));
	other.runtime->runAction("use-it", other.window->root(), json::object());
	CHECK(other.state("used") == 1);
}

/* Describing */

TEST_CASE("lisp describe: docstrings, authors, keys and commands, with no value") {
	App app(R"lisp(
(defn shown "Shows a thing." [what how] nil)
(defcommand act "Does a thing." :title "Act" :category "Things" [times] (set "state.times" times))
(defcustom limit 60 "How many." :type integer)
(def secret-value "s3cret" )
(defkey "ctrl+k" act)
(defn run-it "" []
  (set "state.fn" (describe-function "shown"))
  (set "state.cmd" (describe-function "act"))
  (set "state.none" (describe-function "nothing"))
  (set "state.var" (describe-variable "limit"))
  (set "state.val" (describe-variable "secret-value"))
  (set "state.key" (describe-key "ctrl+k"))
  (set "state.nokey" (describe-key "ctrl+y"))
  (set "state.apropos" (map :name (apropos "sho")))
  (set "state.file" (program-file "app.lisp")))
(defcommand a "" :title "A" [] (run-it))
(defkey "ctrl+a" a)
(defscreen main :actions {:plain {:title "Plain" :description "A plain command." :category "P" :do [{:set "state.x" :to 1}]}} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.errors.empty());
	CHECK(app.state("fn")["doc"] == "Shows a thing.");
	CHECK(app.state("fn")["kind"] == "defn");
	CHECK(app.state("fn")["author"] == "file");
	CHECK(app.state("fn")["arguments"] == json::array({"what", "how"}));
	CHECK(app.state("cmd")["title"] == "Act");
	CHECK(app.state("cmd")["category"] == "Things");
	CHECK(app.state("cmd")["arguments"] == json::array({"times"}));
	CHECK(app.state("none").is_null());
	CHECK(app.state("var")["doc"] == "How many.");
	CHECK_FALSE(app.state("val").dump().contains("s3cret"));
	CHECK(app.state("key")["runs"] == "act");
	CHECK(app.state("key")["command"]["doc"] == "Does a thing.");
	CHECK(app.state("nokey").is_null());
	CHECK(app.state("apropos") == json::array({"shown"}));
	CHECK(app.state("file").get<std::string>().contains("Shows a thing."));
}

TEST_CASE("lisp commands: callable by name with their arguments, and listed with them") {
	App app(R"lisp(
(defcommand act "Does a thing." :title "Act" :category "Things" [times] (set "state.times" times))
(defscreen main :state {:times 0}
  :actions {:by-name [{:run "act" :with {:times 3}}]}
  (panel#root))
)lisp");
	CHECK(app.repository.document()["actions"]["act"]["arguments"] == json::array({"times"}));
	app.runtime->runAction("by-name", app.window->root(), json::object());
	CHECK(app.state("times") == 3);
	app.window->render();
}

TEST_CASE("lisp attack: describe-* and apropos over masked definitions") {
	App app(R"lisp(
(defn open-fn "Anyone may read this." [] 1)
(defn masked-fn "The password is hunter2." [] (call "hit"))
(defcommand masked-cmd "A masked command." :title "Masked" [] 1)
(defkey "ctrl+m" masked-cmd)
(defn helper "" [] 1)
(defcommand c "" :title "C" [] (helper))
(defkey "ctrl+c" c)
(defscreen main :state {:got nil}
  :permissions {:allow ["reader"] :reviewers ["studio"] :data [
    {:author "reader" :read ["/program/app.lisp/open-fn" "/program/app.lisp/helper" "/state/got"]}]}
  (panel#root))
)lisp");
	// Code by a limited reader: no revision of theirs can do this, and the second line holds without the first
	app.write("helper", R"lisp((defn helper "" []
  (set "state.named" (describe-function "open-fn"))
  (set "state.masked" (describe-function "masked-fn"))
  (set "state.variable" (describe-variable "masked-fn"))
  (set "state.command" (describe-function "masked-cmd"))
  (set "state.key" (describe-key "ctrl+m"))
  (set "state.apropos" (map :name (apropos "masked")))
  (set "state.all" (map :name (apropos "")))
  (set "state.file" (program-file "app.lisp"))))lisp",
	          "reader");
	app.press(Key::C);
	CHECK(app.state("named")["doc"] == "Anyone may read this.");
	CHECK(app.state("masked").is_null());
	CHECK(app.state("variable").is_null());
	CHECK(app.state("command").is_null());
	CHECK(app.state("key")["command"].is_null()); // the key is in what it reads; what it runs isn't
	CHECK(app.state("apropos") == json::array());
	CHECK_FALSE(app.state("all").dump().contains("masked"));
	CHECK_FALSE(app.state("file").get<std::string>().contains("hunter2"));
	CHECK(app.state("file").get<std::string>().contains("Anyone may read this."));
	// The person's own describe sees it
	App own(R"lisp(
(defn masked-fn "The password is hunter2." [] 1)
(defcommand c "" :title "C" [] (set "state.masked" (describe-function "masked-fn")))
(defkey "ctrl+c" c)
(defscreen main :permissions {:allow ["reader"] :reviewers ["studio"] :data [{:author "reader" :read ["/state/got"]}]} (panel#root))
)lisp");
	own.press(Key::C);
	CHECK(own.state("masked")["doc"] == "The password is hunter2.");
}
