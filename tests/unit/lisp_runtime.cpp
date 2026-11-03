/// @file
/// @brief Unit tests for a document's live program (RFC 0005, stage 3; SECURITY.md, "The Lisp language"): handlers and
/// bindings call functions, and the attacks on the checks by author, written as an attacker would, one per route. An
/// attack passes when the application's function was never called (`hits`) and the reason names who and what.

#include "LispApp.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using affresco::json;
using affresco::Key;
using affresco::RevisionStatus;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::format::Rules;
using affresco::format::Runtime;
using affresco::vcs::Repository;
namespace lisp = affresco::lisp;

namespace {

using lispapp::App;

/// What every attack's program starts from: the person's commands on keys, reaching the application's function.
constexpr const char* kBase = R"lisp(
(defn save "The persons own: reaches outside." [] (call "hit"))
(defcommand a "Runs save." :title "A" [] (save))
(defn helper "An agent may replace this." [] 1)
(defcommand b "Calls helper, then reaches outside." :title "B" [] (helper) (call "hit"))
(defcommand c "Calls helper." :title "C" [] (helper))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defkey "ctrl+c" c)
(defscreen main :state {:n 0 :out nil} (panel#root))
)lisp";

} // namespace

TEST_CASE("lisp runtime: a person's key runs the person's function, which reaches outside") {
	const App app(kBase);
	app.press(Key::A);
	CHECK(app.hits == 1);
	CHECK(app.errors.empty());
}

TEST_CASE("lisp runtime: functions read through paths and change the world through steps") {
	const App app(R"lisp(
(defn step-it [by] (set "state.n" (+ state.n by)))
(defn nested [] (set "state.deep.a" 1) (push "state.list" {:id "x"}) (toggle "state.flag") (increment "state.n" :by 2))
(defcommand a "" :title "A" [] (step-it 3))
(defcommand b "" :title "B" [] (nested))
(defcommand c "By name, from a handlers variables." :title "C" [by event] (set "state.by" by) (set "state.type" event.type))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defkey "ctrl+c" c)
(defscreen main :state {:n 1} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.state("n") == 4);
	app.press(Key::B);
	CHECK(app.state("deep") == json{{"a", 1}});
	CHECK(app.state("list") == json::array({json{{"id", "x"}}}));
	CHECK(app.state("flag") == true);
	CHECK(app.state("n") == 6);
	app.press(Key::C);
	CHECK(app.state("by").is_null());
	CHECK(app.state("type") == "key");
}

TEST_CASE("lisp runtime: a handler runs a function by name, with `run` and `lisp`, and its variables") {
	App app(R"lisp(
(defn put [value] (set "state.got" value))
(defscreen main :state {:got 0}
  :onLoad [{:run "put" :with {:value 5}}]
  :actions {:again [{:lisp "put" :with [7]}]}
  (panel#root))
)lisp");
	app.window->render();
	CHECK(app.state("got") == 5);
	app.runtime->runAction("again", app.window->root(), json::object());
	CHECK(app.state("got") == 7);
}

TEST_CASE("lisp runtime: a binding reads through the expression machine, and through a function") {
	App app(R"lisp(
(defn shown "Reads the state itself." [] (str "n=" state.n))
(defn doubled [x] (* 2 x))
(defcommand a "" :title "A" [] (increment "state.n"))
(defkey "ctrl+a" a)
(defscreen main :state {:n 1}
  (column#root
    (label#plain :text ~(not state.n))
    (label#read :text ~(shown))
    (label#call :text ~(doubled state.n))
    (label#cond :text ~(if (> state.n 1) "big" "small"))))
)lisp");
	const json& children = app.repository.document()["root"]["children"];
	CHECK(children[0]["text"] == "{{!(state.n)}}");
	CHECK(children[1]["text"] == "{{lisp('shown')}}");
	CHECK(children[2]["text"] == "{{lisp('doubled', state.n)}}");
	CHECK(children[3]["text"] == "{{((state.n > 1) ? 'big' : 'small')}}");
	const auto label = [&](const char* id) { return dynamic_cast<affresco::Label*>(app.window->root().findById(id)); };
	REQUIRE(label("read"));
	CHECK(label("read")->text() == "n=1");
	CHECK(label("call")->text() == "2");
	CHECK(label("cond")->text() == "small");
	app.press(Key::A); // the binding read state.n through the function, and updates with it
	CHECK(label("read")->text() == "n=2");
	CHECK(label("call")->text() == "4");
	CHECK(label("cond")->text() == "big");
}

TEST_CASE("lisp runtime: a data file's ~ is read when an app loads it, and stays data elsewhere") {
	const json tree =
	    lisp::readDocument(R"lisp((column :when ~(not state.x) (label :text ~(t "title"))))lisp", "a.lisp");
	CHECK(tree["when"] == "{{!(state.x)}}");
	CHECK(tree["children"][0]["text"] == "{{t('title')}}");
	const json plain = lisp::readDocument("(column (label :text \"~streak\"))", "a.lisp");
	CHECK(plain["children"][0]["text"] == "~streak");
	CHECK_THROWS_AS(static_cast<void>(lisp::readDocument("(column (not x))", "a.lisp")), affresco::DocumentError);
	// What the build knows is worked out; a form with both is a binding holding what was worked out
	const json mixed = lisp::readDocument(R"lisp((label :text ~(+ state.n (* 2 3)) :id ~(str "a" "b")))lisp", "a.lisp");
	CHECK(mixed["text"] == "{{(state.n + 6)}}");
	CHECK(mixed["id"] == "ab");
}

/* The attacks: each route, one test */

TEST_CASE("lisp attack: an agent's code reaching control and review steps directly") {
	App app(kBase);
	for(const char* form :
	    {R"((call "hit"))", R"((send "x" :data 1))", R"((connect "x"))", R"((patch []))", R"((accept 1 :head "abc"))",
	     R"((reject 1))", R"((undo))", R"((restore 0))", R"((revert 0))", R"((preview 1))", R"((endPreview))",
	     R"((ask "x"))", R"((signIn "x"))", R"((saveFile "x"))", R"((fullscreen true))",
	     R"((invoke (str "files." "write")))", R"((importCsv "h" :text "a"))", R"((image "open"))"}) {
		CAPTURE(form);
		app.write("helper", std::string("(defn helper \"Mine.\" [] ") + form + ")");
		app.errors.clear();
		app.press(Key::C);
		CHECK(app.hits == 0);
		CHECK(app.reported("agent/x"));
	}
}

TEST_CASE("lisp attack: through apply, a computed name, a variable and map") {
	App app(kBase);
	for(const char* body :
	    {R"((apply call ["hit"]))", R"((let [f call] (f "hit")))", R"((map call ["hit"]))", R"((run (str "sa" "ve")))",
	     R"((run "a"))", R"((reduce (fn [a b] (call b)) nil ["hit"]))"}) {
		CAPTURE(body);
		app.write("helper", std::string("(defn helper \"\" [] ") + body + ")");
		app.errors.clear();
		app.press(Key::C);
		CHECK(app.hits == 0);
		CHECK(app.reported("agent/x"));
	}
}

TEST_CASE("lisp attack: an agent's run of a JSON action, or a native one, that holds a control step") {
	App app(R"lisp(
(defn helper "" [] 1)
(defcommand c "" :title "C" [] (helper))
(defkey "ctrl+c" c)
(defscreen main :actions {:json-action [{:call "hit"}] :later-action [{:after 10 :do [{:call "hit"}]}]} (panel#root))
)lisp");
	app.runtime->onAction("native", [](affresco::Panel&) { FAIL("a native action ran"); });
	for(const char* action : {"json-action", "native", "later-action"}) {
		CAPTURE(action);
		app.write("helper", std::string(R"((defn helper "" [] (run ")") + action + R"(")))");
		app.errors.clear();
		app.press(Key::C);
		app.wait(50);
		CHECK(app.hits == 0);
		CHECK(app.reported("agent/x"));
	}
}

TEST_CASE("lisp attack: an agent's macro writes a person's function") {
	App app(R"lisp(
(defmacro written "An agent may replace this." [] nil)
(defn mine "The persons: its text never changes." [] (written))
(defcommand c "" :title "C" [] (mine))
(defkey "ctrl+c" c)
(defscreen main (panel#root))
)lisp");
	app.write("written", R"lisp((defmacro written "Mine." [] `(call "hit")))lisp");
	app.press(Key::C);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: a closure, or a primitive, an agent hands to a person's function") {
	App app(R"lisp(
(defn each "The persons: calls what it is given." [f] (f "hit"))
(defn helper "" [] nil)
(defcommand c "The persons: applies what the helper gives." :title "C" [] (let [f (helper)] (when f (f "hit"))))
(defcommand d "" :title "D" [] (each (helper)))
(defkey "ctrl+c" c)
(defkey "ctrl+d" d)
(defscreen main (panel#root))
)lisp");
	// The agent's code has returned by the time the person's calls what it gave: it counts all the same
	app.write("helper", R"lisp((defn helper "" [] call))lisp");
	app.press(Key::C);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	app.errors.clear();
	app.write("helper", R"lisp((defn helper "" [] (fn [x] (call x))))lisp");
	app.press(Key::D);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: a callback an agent's code schedules keeps the agent's authority") {
	App app(kBase);
	// save is the person's, and fires by itself later: the agent scheduled it
	app.write("helper", R"lisp((defn helper "" [] (after 10 save)))lisp");
	app.press(Key::C);
	CHECK(app.hits == 0);
	app.wait(50);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	app.errors.clear();
	// A second hop keeps it too: the person's function reschedules what the agent started
	app.write("save", R"lisp((defn save "Mine." [] (after 10 helper)))lisp", "user");
	app.write("helper", R"lisp((defn helper "" [] (after 10 save)))lisp");
	app.press(Key::C);
	app.wait(50);
	app.wait(50);
	CHECK(app.hits == 0);
}

TEST_CASE("lisp attack: a closure can't be a callback, and a person's timer has no review step") {
	App app(R"lisp(
(defn closure "" [] (after 10 (fn [] (call "hit"))))
(defn tick "" [] (accept 1 :head "abc"))
(defn start "" [] (after 10 tick))
(defn fire "" [] (call "hit"))
(defn start-fire "" [] (after 10 fire))
(defcommand a "" :title "A" [] (closure))
(defcommand b "" :title "B" [] (start))
(defcommand c "" :title "C" [] (start-fire))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defkey "ctrl+c" c)
(defscreen main (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.reported("a callback is a function a definition gives"));
	app.errors.clear();
	app.press(Key::B);
	app.wait(50);
	CHECK(app.reported("runs only from the person's own input"));
	// The person's own scheduled function does reach outside, as the screen's own timer step does
	app.press(Key::C);
	app.wait(50);
	CHECK(app.hits == 1);
}

TEST_CASE("lisp runtime: a path's member that is a number is an index, as in a binding") {
	const App app(R"lisp(
(defn read-them "Reads items by their index." [] (set "state.out" [state.l.0 state.m.0.k state.l.9]))
(defcommand a "" :title "A" [] (read-them))
(defkey "ctrl+a" a)
(defscreen main :state {:l [1 2] :m [{:k 3}] :out nil} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.state("out") == json::array({1, 3, nullptr}));
}

TEST_CASE("lisp runtime: a callback given its first arguments takes the rest by name") {
	const App app(R"lisp(
(defn failed "Its first argument given, `error` by name." [label error] (set "state.out" [label error]))
(defn begin "Calls what the host doesn't answer, then failed with the label." [label]
  (call "nothing" :onError [failed label]))
(defcommand a "" :title "A" [] (begin "file"))
(defcommand b "A vector that doesn't start with a function is refused." :title "B" [] (call "hit" :then ["failed"]))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defscreen main :state {:out nil} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.state("out") == json::array({"file", "no native action 'nothing' registered"}));
	app.press(Key::B);
	CHECK(app.hits == 0);
	CHECK(app.reported("takes a function of the program"));
}

TEST_CASE("lisp attack: a timer or a watch that runs an agent's function") {
	App app(R"lisp(
(defn on-change "" [] nil)
(defn tick "" [] nil)
(defcommand a "" :title "A" [] (increment "state.n"))
(defkey "ctrl+a" a)
(defscreen main :state {:n 0}
  :watch {:state.n "on-change"}
  (panel#root :every {:ms 10 :do "tick"}))
)lisp");
	app.write("on-change", R"lisp((defn on-change "" [] (call "hit")))lisp");
	app.write("tick", R"lisp((defn tick "" [] (call "hit")))lisp");
	app.press(Key::A);
	app.wait(50);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: an agent changes the state that a file's watch reads: whose authority?") {
	// The watch is the file's and does what the file wrote, as any revision on the state may set it off: a change of
	// the state is no authority, and the steps are the file's
	App app(R"lisp(
(defn saver "The files." [] (call "hit"))
(defn helper "" [] nil)
(defcommand c "" :title "C" [] (helper))
(defkey "ctrl+c" c)
(defscreen main :state {:n 0} :watch {:state.n "saver"} (panel#root))
)lisp");
	app.write("helper", R"lisp((defn helper "" [] (set "state.n" 1)))lisp");
	app.press(Key::C);
	app.wait(10);
	CHECK(app.hits == 1);
}

TEST_CASE("lisp attack: a person's command calling an agent's helper is refused, naming the author and the step") {
	App app(kBase);
	app.write("helper", R"lisp((defn helper "Only reads." [] 2))lisp");
	app.press(Key::B);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	CHECK(app.reported("\"call\""));
}

TEST_CASE("lisp attack: a tail call doesn't launder authority") {
	App app(kBase);
	app.write("helper", R"lisp((defn helper "Ends in the persons." [] (save)))lisp");
	app.press(Key::C);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: replacing a person's function so the person's key runs agent code") {
	App app(kBase);
	app.write("save", R"lisp((defn save "Mine now." [] (call "hit")))lisp");
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	// Advice by redefinition: the wrapper calls the person's function under another name
	app.errors.clear();
	app.write("save", R"lisp((defn save "Wraps." [] (original-save)))lisp");
	app.write("original-save", R"lisp((defn original-save "The persons text, moved." [] (call "hit")))lisp");
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: replacing a primitive by name") {
	App app(kBase);
	app.write("call", R"lisp((defn call "Shadows the step." [x] (set "state.out" x)))lisp");
	app.press(Key::A); // the person's save now calls the agent's: what it does is the agent's to do
	CHECK(app.hits == 0);
	CHECK(app.state("out") == "hit");
}

TEST_CASE("lisp attack: re-entrance through a primitive") {
	App app(R"lisp(
(defn person "The persons." [] (call "hit"))
(defn helper "" [] nil)
(defcommand c "" :title "C" [] (helper))
(defkey "ctrl+c" c)
(defscreen main :actions {:reenter [{:lisp "person"}] :named [{:run "person"}]} (panel#root))
)lisp");
	for(const char* action : {"reenter", "named"}) {
		CAPTURE(action);
		app.write("helper", std::string(R"((defn helper "" [] (run ")") + action + R"(")))");
		app.errors.clear();
		app.press(Key::C);
		CHECK(app.hits == 0);
		CHECK(app.reported("agent/x"));
	}
}

TEST_CASE("lisp attack: deep re-entry ends at a limit, on the native stack too") {
	App app(R"lisp(
(defn again "Through a step." [] (run "again"))
(defn deep "Through a read." [] computed.loop)
(defn value "" [] (lisp-read))
(defn lisp-read "Itself, through a value." [] (deep))
(defcommand a "" :title "A" [] (again))
(defcommand b "" :title "B" [] (lisp-read))
(defn recurse "On the machines stack." [n] (+ 1 (recurse (inc n))))
(defcommand c "" :title "C" [] (recurse 0))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defkey "ctrl+c" c)
(defscreen main :computed {:loop "{{lisp(deep)}}"} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK_FALSE(app.errors.empty());
	app.errors.clear();
	app.press(Key::B);
	CHECK_FALSE(app.errors.empty());
	app.errors.clear();
	app.press(Key::C);
	CHECK(app.reported("nested more than"));
}

TEST_CASE("lisp attack: a binding has no way to cause effects") {
	const std::string functions = R"lisp(
(defn set-it "" [] (set "state.n" 9) 1)
(defn call-it "" [] (call "hit") 1)
(defn schedule-it "" [] (after 0 call-it) 1)
(defn run-it "" [] (run "act") 1)
(defn try-it "A try gets nothing past it." [] (try (set "state.n" 9) (catch e 2)))
(defn agent-run "" [] (apply set ["state.n" 9]) 1)
)lisp";
	for(const char* binding :
	    {"~(set-it)", "~(call-it)", "~(schedule-it)", "~(run-it)", "~(agent-run)", "\"{{lisp('set', 'state.n', 9)}}\"",
	     "\"{{lisp('call', 'hit')}}\"", "\"{{lisp('accept', 1)}}\"", "\"{{lisp('run', 'act')}}\""}) {
		CAPTURE(binding);
		std::string why;
		int hits = 0;
		ManualClock clock;
		const auto window = createHeadlessWindow(400, 300, clock);
		const Rules rules;
		Repository repository(rules, lisp::sourceDocument(functions +
		                                                      "(defscreen main :state {:n 0} :actions {:act [{:set "
		                                                      "\"state.n\" :to 9}]} (label#a :text " +
		                                                      binding + "))",
		                                                  "app.lisp"));
		Runtime runtime(repository, "");
		runtime.onCall("hit", [&](const json&) {
			++hits;
			return json();
		});
		runtime.onError.connect([&](const std::string& message) { why += message; });
		try {
			runtime.build(*window);
			window->render();
			clock.advance(100);
			window->render();
		} catch(
		    const affresco::DocumentError& error) { // a value that can't be read at the first frame: the load's error
			why += error.what();
		}
		CHECK(hits == 0);
		CHECK(repository.state()["n"] == 0);
		CHECK(why.contains("only reads"));
	}
	// A try sees the refusal as any condition, and still nothing happened
	App app(functions + R"lisp((defscreen main :state {:n 0} (column (label#a :text ~(try-it)))))lisp");
	CHECK(app.state("n") == 0);
	const auto* label = dynamic_cast<affresco::Label*>(app.window->root().findById("a"));
	REQUIRE(label);
	CHECK(label->text() == "2");
}

TEST_CASE("lisp attack: a try swallowing a refusal, then another route") {
	App app(kBase);
	app.write("helper", R"lisp((defn helper "" []
  (try (call "hit") (catch e nil))
  (try (apply call ["hit"]) (catch e nil))
  (try (save) (catch e nil))
  (try (run "a") (catch e nil))
  (after 1 save)
  (set "state.out" "survived")))lisp");
	app.press(Key::C);
	app.wait(50);
	CHECK(app.hits == 0);
	CHECK(app.state("out") == "survived"); // a refusal is a condition it may catch: what it does next is checked too
}

TEST_CASE("lisp attack: a function or a value in the state is never run") {
	App app(R"lisp(
(defn keep-fn "" [] (set "state.f" (fn [] 1)))
(defn keep-prim "" [] (set "state.f" call))
(defn keep-named "" [] (set "state.f" keep-prim))
(defn keep-text "" [] (set "state.text" "{{state.secret}}") (push "state.list" "{{state.secret}}") (set "state.map" {:$concat ["a" "{{state.secret}}"]}))
(defn copy-text "" [] (set "state.copied" state.text))
(defcommand a "" :title "A" [] (keep-fn))
(defcommand b "" :title "B" [] (keep-prim))
(defcommand c "" :title "C" [] (keep-text) (copy-text))
(defcommand d "" :title "D" [] (keep-named))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defkey "ctrl+c" c)
(defkey "ctrl+d" d)
(defscreen main :state {:secret "s3cret"} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.reported("function"));
	CHECK(app.state("f").is_null());
	app.errors.clear();
	app.press(Key::B);
	CHECK(app.reported("function"));
	CHECK(app.state("f").is_null());
	app.errors.clear();
	app.press(Key::D);
	CHECK(app.reported("function"));
	CHECK(app.state("f").is_null());
	app.errors.clear();
	app.press(Key::C);
	CHECK(app.errors.empty());
	CHECK(app.state("text") == "{{state.secret}}");
	CHECK(app.state("list") == json::array({"{{state.secret}}"}));
	CHECK(app.state("copied") == "{{state.secret}}");
	CHECK(app.state("map") == json{{"$concat", json::array({"a", "{{state.secret}}"})}});
}

TEST_CASE("lisp attack: the build, a macro and a value are pure") {
	// A macro runs when the live program loads; a `def` is worked out then: neither reaches anything
	for(const char* text : {R"lisp((defmacro m [] (call "hit") nil) (defn f [] (m)) (defscreen main (panel#root)))lisp",
	                        R"lisp((def x (call "hit")) (defscreen main (panel#root)))lisp",
	                        R"lisp((def x state.n) (defscreen main (panel#root)))lisp",
	                        R"lisp((defscreen main (panel#root :text ~(call "hit"))))lisp"}) {
		CAPTURE(text);
		int hits = 0;
		const ManualClock clock;
		const auto window = createHeadlessWindow(400, 300, clock);
		try {
			affresco::Screen screen = affresco::Screen::fromString(lisp::sourceDocument(text, "app.lisp").dump());
			screen.onCall("hit", [&](const json&) {
				++hits;
				return json();
			});
			screen.build(*window);
			window->render();
		} catch(const affresco::DocumentError&) { // NOLINT(bugprone-empty-catch): refusing it is the rule
		}
		CHECK(hits == 0);
	}
}

TEST_CASE("lisp attack: a preview's function runs under today's rule, and changes nothing's author") {
	App app(R"lisp(
(defn save "" [] (call "hit"))
(defn start "" [] (after 10 save))
(defcommand a "" :title "A" [] (save))
(defcommand b "" :title "B" [] (start))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defscreen main :permissions {:allow ["studio"] :reviewers ["studio"]} :state {:n 0} (panel#root))
)lisp");
	// A proposal: the agent changes nothing but a label's worth of text in the person's function
	const json revision = {{"author", "agent/x"},
	                       {"message", "m"},
	                       {"set", {{"/program/app.lisp/save", R"lisp((defn save "Changed." [] (call "hit")))lisp"}}}};
	const auto first = app.repository.submit(revision);
	REQUIRE(first.status == RevisionStatus::Proposed);
	REQUIRE_FALSE(app.repository.preview(first.proposal).has_value());
	// The person presses the key: the proposal's text isn't the person's
	app.press(Key::A);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	app.repository.endPreview();
	app.errors.clear();
	// A proposal that changes something else: the file's own function, started by the person, fires by itself in the
	// preview
	const json other = {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/main/:state/:n", 1}}}};
	const auto second = app.repository.submit(other);
	REQUIRE(second.status == RevisionStatus::Proposed);
	app.press(Key::B);
	REQUIRE_FALSE(app.repository.preview(second.proposal).has_value());
	app.wait(50);
	CHECK(app.hits == 0);
	CHECK(app.reported("while a proposal is previewed"));
}

TEST_CASE("lisp attack: an agent adds a way to run a person's control command") {
	App app(kBase, true);
	const auto refused = [&](const std::string& name, const std::string& text) {
		const json revision = {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/" + name, text}}}};
		const auto result = app.repository.submit(revision);
		CAPTURE(text);
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.contains("permission denied"));
	};
	refused("defkey ctrl+e", R"lisp((defkey "ctrl+e" save))lisp");
	refused("main", R"lisp((defscreen main :state {:n 0 :out nil} (panel#root :onClick "a")))lisp");
	refused("main", R"lisp((defscreen main :state {:n 0 :out nil} :onLoad [{:run "save"}] (panel#root)))lisp");
	refused("main", R"lisp((defscreen main :state {:n 0 :out nil} (panel#root :every {:ms 1 :do "save"})))lisp");
	refused("main", R"lisp((defscreen main :state {:n 0 :out nil} :onLoad [{:lisp "save"}] (panel#root)))lisp");
	CHECK(app.hits == 0);
}

TEST_CASE("lisp attack: an agent's handler passes a name to a person's function that runs what it is told") {
	App app(R"lisp(
(defn save "" [] (call "hit"))
(defn go "A persons dispatcher." [name] (run name))
(defscreen main (panel#root))
)lisp",
	        true);
	const json revision = {
	    {"author", "agent/x"},
	    {"message", "m"},
	    {"set",
		 {{"/program/app.lisp/defkey ctrl+e", R"lisp((defkey "ctrl+e" {:run "go" :with {:name "save"}}))lisp"}}}};
	const auto result = app.repository.submit(revision);
	CHECK(result.status == RevisionStatus::Rejected);
	CHECK(result.error.contains("permission denied"));
}

TEST_CASE("lisp attack: an agent's new function that writes records is always reviewed") {
	App app(R"lisp(
(defn helper "" [] 1)
(defscreen main :permissions {:allow ["agent"] :reviewers ["studio"]} (panel#root))
)lisp",
	        true);
	const auto harmless = app.repository.submit(
	    {{"author", "agent/x"},
		 {"message", "m"},
		 {"set", {{"/program/app.lisp/helper", R"lisp((defn helper "" [] (set "state.a" 2)))lisp"}}}});
	CHECK(harmless.status == RevisionStatus::Applied);
	const auto writes = app.repository.submit(
	    {{"author", "agent/x"},
		 {"message", "m"},
		 {"set", {{"/program/app.lisp/helper", R"lisp((defn helper "" [] (delete "songs" :where {})))lisp"}}}});
	CHECK(writes.status == RevisionStatus::Proposed);
}

TEST_CASE("lisp runtime: a running function keeps its version and its authors while the program changes under it") {
	App app(R"lisp(
(defn helper "" [] 1)
(defn twice "" [] (call "swap") (helper) (call "hit"))
(defcommand a "" :title "A" [] (twice))
(defkey "ctrl+a" a)
(defscreen main (panel#root))
)lisp");
	app.runtime->onCall("swap", [&](const json&) {
		const json revision = {{"author", "agent/x"},
		                       {"message", "m"},
		                       {"set", {{"/program/app.lisp/helper", "(defn helper \"\" [] 2)"}}}};
		CHECK(app.repository.commit(revision) == std::nullopt);
		return json();
	});
	app.press(Key::A);
	CHECK(app.hits == 1); // the version it started on goes on: the agent's helper isn't in it
	CHECK(app.repository.programAuthors()["app.lisp"]["helper"] == "agent/x");
	app.press(Key::A); // the next press calls the agent's, and is refused
	CHECK(app.hits == 1);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: a person's undo doesn't make an agent's text the person's") {
	App app(kBase);
	app.write("helper", R"lisp((defn helper "The agents." [] (call "hit")))lisp");
	app.write("helper", R"lisp((defn helper "The persons." [] 1))lisp", "user");
	CHECK(app.repository.programAuthors()["app.lisp"]["helper"] == "user");
	// The person undoes their change: the text is the agent's again
	app.commit(app.repository.revertRevision("HEAD", "user"));
	CHECK(app.repository.programAuthors()["app.lisp"]["helper"] == "agent/x");
	app.press(Key::C);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
	// An accepted proposal stays its author's too, and a person's own text theirs
	app.commit(app.repository.restoreRevision(0, "user"));
	CHECK(app.repository.programAuthors()["app.lisp"]["helper"] == "file");
}

TEST_CASE("lisp attack: an author's name that only looks like the person's counts for least") {
	App app(kBase);
	for(const char* author : {"agent/user", "studio/file", "users", "app", "filename"}) {
		CAPTURE(author);
		app.write("save", std::string(R"((defn save ")") + author + R"(" [] (call "hit")))", author);
		app.errors.clear();
		app.press(Key::A);
		CHECK(app.hits == 0);
		CHECK(app.reported(author));
	}
	app.press(Key::C); // a command with no reach outside still runs
}

TEST_CASE("lisp runtime: redefinition, removal and undo reach the running app, and live state stays") {
	App app(R"lisp(
(defn show "" [] (set "state.got" "one"))
(defn tick "" [] (increment "state.ticks") (after 10 tick))
(defn extra "" [] 1)
(defcommand a "" :title "A" [] (show))
(defcommand b "" :title "B" [] (tick))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defscreen main :state {:got "" :count 1 :ticks 0} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.state("got") == "one");
	app.write("show", R"lisp((defn show "" [] (set "state.got" "two")))lisp", "user");
	app.press(Key::A);
	CHECK(app.state("got") == "two");
	// A running timer goes on through a change of something else
	app.press(Key::B);
	app.wait(20);
	app.wait(20);
	const auto ticks = app.state("ticks").get<int>();
	CHECK(ticks >= 2);
	app.write("extra", R"lisp((defn extra "" [] 2))lisp", "user");
	app.wait(20);
	CHECK(app.state("ticks").get<int>() > ticks);
	// Undo gives the definition back, exactly
	app.commit(app.repository.revertRevision("HEAD~1", "user"));
	CHECK(app.state("got") == "two"); // what a function did stays: state changes aren't undone
	app.press(Key::A);
	CHECK(app.state("got") == "one");
	// Taking the timer's definition away ends it, and its callback goes with it
	const json remove = {{"author", "user"},
	                     {"message", "m"},
	                     {"patch",
	                      {{{"op", "remove"}, {"path", "/program/app.lisp/tick"}},
	                       {{"op", "remove"}, {"path", "/program/app.lisp/b"}},
	                       {{"op", "remove"}, {"path", "/program/app.lisp/defkey ctrl+b"}}}}};
	app.commit(remove);
	const auto last = app.state("ticks").get<int>();
	app.wait(20);
	app.wait(20);
	CHECK(app.state("ticks").get<int>() == last);
}

TEST_CASE("lisp runtime: a definition's state is what a key starts from: it never takes what the person set") {
	App app(R"lisp(
(defcommand a "" :title "A" [] (set "state.mine" 5))
(defkey "ctrl+a" a)
(defscreen main :state {:mine 1 :other 1} (panel#root))
)lisp");
	app.press(Key::A);
	const std::string main = "/program/app.lisp/main";
	const json change = {{"author", "user"},
	                     {"message", "m"},
	                     {"set", {{main + "/:state", json{{"mine", 2}, {"other", 2}, {"added", 3}}}}}};
	app.commit(change);
	CHECK(app.state("mine") == 5);  // the person set it
	CHECK(app.state("other") == 2); // still the old default: it follows
	CHECK(app.state("added") == 3); // no live value: the default applies
	app.commit(app.repository.revertRevision("HEAD", "user"));
	CHECK(app.state("mine") == 5);
	CHECK(app.state("other") == 1);
}

TEST_CASE("lisp runtime: limits: a loop can't freeze the app, in a handler or a binding, and an event leaves nothing") {
	App app(R"lisp(
(defn spin "" [] (loop [i 0] (recur (inc i))))
(defn hoard "" [] (loop [v []] (recur (conj v (str "x" (count v))))))
(defn steps "" [] (loop [] (set "state.n" 1) (recur)))
(defcommand a "" :title "A" [] (spin))
(defcommand b "" :title "B" [] (hoard))
(defcommand c "" :title "C" [] (steps))
(defcommand d "" :title "D" [] (try (spin) (catch e (set "state.caught" true))))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defkey "ctrl+c" c)
(defkey "ctrl+d" d)
(defscreen main :state {:n 0} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK(app.reported("budget for work ran out"));
	app.errors.clear();
	app.press(Key::B);
	CHECK(app.reported("ran out"));
	app.errors.clear();
	app.press(Key::C);
	CHECK(app.reported("steps"));
	app.errors.clear();
	app.press(Key::D); // no `try` catches a limit
	CHECK(app.state("caught").is_null());
	CHECK(app.reported("budget for work ran out"));
}

TEST_CASE("lisp runtime: a binding that doesn't end stops at the update's budget, every frame") {
	const App app(R"lisp(
(defn spin "" [n] (when (> n 0) (loop [i 0] (recur (inc i)))))
(defcommand a "" :title "A" [] (set "state.n" 1))
(defkey "ctrl+a" a)
(defscreen main :state {:n 0} (column (label#a :text ~(spin state.n))))
)lisp");
	app.press(Key::A);
	CHECK(app.reported("budget for work ran out"));
}

TEST_CASE("lisp runtime: an event's objects go when it ends, however many events there are") {
	lisp::Interpreter interpreter;
	const lisp::Value function =
	    interpreter.eval(R"lisp((defn work [n] (vec (map (fn [x] (str x)) (range n)))))lisp", "a.lisp", "file");
	static_cast<void>(function);
	interpreter.workOutDefinitions();
	const lisp::Value work = interpreter.global("work").value_or(lisp::Value());
	const std::size_t before = interpreter.objects();
	const std::vector<lisp::Value> arguments = {lisp::Value::integer(100)};
	for(int event = 0; event < 100; ++event) {
		const lisp::Interpreter::Transient transient(interpreter);
		interpreter.setBudget({});
		CHECK(interpreter.call(work, arguments).count() == 100);
	}
	CHECK(interpreter.objects() == before);
	// A vector of the program isn't given what an event made
	static_cast<void>(
	    interpreter.eval(R"lisp((def kept [1 2]) (defn grow [] (conj kept (str "x"))))lisp", "a.lisp", "file"));
	interpreter.workOutDefinitions();
	const lisp::Value grow = interpreter.global("grow").value_or(lisp::Value());
	for(int event = 0; event < 2; ++event) {
		const lisp::Interpreter::Transient transient(interpreter);
		CHECK(interpreter.print(interpreter.call(grow, {})) == "[1 2 \"x\"]");
	}
	CHECK(interpreter.print(interpreter.global("kept").value_or(lisp::Value())) == "[1 2]");
}

TEST_CASE("lisp runtime: an error's text carries no value, and describes nothing it shouldn't") {
	const App app(R"lisp(
(defn leak "" [] (throw (str "secret " state.secret)))
(defn bad "" [] (first state.secret))
(defcommand a "" :title "A" [] (leak))
(defcommand b "" :title "B" [] (bad))
(defkey "ctrl+a" a)
(defkey "ctrl+b" b)
(defscreen main :state {:secret 12345} (panel#root))
)lisp");
	app.press(Key::A);
	CHECK_FALSE(app.reported("12345"));
	app.press(Key::B);
	CHECK_FALSE(app.reported("12345"));
	CHECK(app.errors.size() == 2);
}
