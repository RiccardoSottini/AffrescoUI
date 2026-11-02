/// @file
/// @brief Unit tests for what `evaluate` gives the person's authority to (RFC 0005, decision 5; SECURITY.md, "The
/// loop"): only what the person typed themselves, proved from the keystrokes and never assumed. The attacks are written
/// as an agent would, one per way text gets into a buffer.

#include "LispApp.hpp"

#include <string>

using affresco::json;
using affresco::Key;
using lispapp::App;

namespace {

/// A person's editor, its commands on the person's keys, the buffer bound to the state.
constexpr const char* kEditor = R"lisp(
(def word "A word that gives a value." 5)
(defn shown "Shows what was evaluated." [result error proposal]
  (set "state.result" result) (set "state.error" error) (set "state.proposal" proposal))
(defcommand eval-text "Evaluates the buffer." :title "Eval" [] (evaluate state.text :file "init.lisp" :then shown))
(defcommand eval-copy "Evaluates a copy." :title "Copy" [] (evaluate state.copied :file "init.lisp" :then shown))
(defcommand copy-text "The person's own command that copies text." :title "C" [] (set "state.copied" state.text))
(defcommand insert-text "A key that types for the person." :title "I" [] (edit "type" :text state.given :in "typed"))
(defn press-word "A key that presses a key for the person." [] (press "w" :in "typed"))
(defcommand press-it "" :title "P" [] (press-word))
(defn save "Reaches nothing yet." [] 1)
(defcommand use-save "" :title "S" [] (save))
(defkey "ctrl+e" eval-text)
(defkey "ctrl+g" eval-copy)
(defkey "ctrl+t" copy-text)
(defkey "ctrl+i" insert-text)
(defkey "ctrl+p" press-it)
(defkey "ctrl+s" use-save)
(defscreen main :state {:text "" :copied "" :given "" :word "" :result nil :error nil :proposal nil :out nil}
  (panel#root (editor#typed :bind "state.text" :autofocus true :width 300 :height 100)))
)lisp";

/// @brief A revision of an agent that writes a state value, as any agent may.
/// @param app The app.
/// @param key The state key.
/// @param value What it holds now.
void agentWrites(App& app, const char* key, const std::string& value) {
	app.commit({{"author", "agent/x"}, {"message", "m"}, {"set", {{std::string("/state/") + key, value}}}});
	app.settle();
}

/// @brief Presses the key that evaluates the buffer, and lets it happen.
/// @param app The app.
void evaluateBuffer(App& app) {
	app.press(Key::E);
	app.settle();
}

constexpr const char* kReaches = R"lisp((call "hit"))lisp"; ///< A form that has the person's authority or nothing.

} // namespace

TEST_CASE("lisp typed: what the person types has their authority") {
	App app(kEditor);
	app.type(kReaches);
	evaluateBuffer(app);
	CHECK(app.errors.empty());
	CHECK(app.hits == 1);
	// Over several lines, with a mistake put right, and put in the middle of what it had typed
	App second(kEditor);
	second.type("(do\n(set \"state.out\" 1)\n(call \"hx");
	second.window->routeKey({Key::Backspace, 0, affresco::Action::Press, affresco::Mods{}});
	second.type("it\"))");
	evaluateBuffer(second);
	CHECK(second.errors.empty());
	CHECK(second.hits == 1);
	CHECK(second.state("out") == 1);
	// Beside a comment it didn't write: a comment runs nothing
	App third(kEditor);
	agentWrites(third, "text", ";; an agent's comment\n");
	third.window->routeKey({Key::End, 0, affresco::Action::Press, affresco::Mods{0x2}}); // the end of the text
	third.type(kReaches);
	evaluateBuffer(third);
	CHECK(third.hits == 1);
	// A definition lands as the person's
	App fourth(kEditor);
	fourth.type(R"lisp((defn save "Mine." [] (call "hit")))lisp");
	evaluateBuffer(fourth);
	CHECK(fourth.repository.programAuthors()["app.lisp"]["save"] == "user");
	fourth.press(Key::S);
	CHECK(fourth.hits == 1);
	// And its copy, by the person's own command, is still what they typed
	const App fifth(kEditor);
	fifth.type("(+ 1 1)");
	fifth.press(Key::T);
	fifth.press(Key::G);
	fifth.settle();
	CHECK(fifth.state("result") == "2");
}

TEST_CASE("lisp attack: a form an agent wrote into a buffer, past what the screen shows") {
	App app(kEditor);
	const int head = app.repository.head().number;
	agentWrites(app, "text", std::string(500, '\n') + kReaches);
	const int written = app.repository.head().number;
	CHECK(written == head + 1);
	evaluateBuffer(app);
	CHECK(app.hits == 0);
	CHECK(app.repository.head().number == written);
	CHECK(app.repository.proposals().empty());
	CHECK(app.state("error").get<std::string>().contains("typed"));
}

TEST_CASE("lisp attack: a definition an agent wrote into a buffer is a proposal, in no one's name but its own") {
	App app(kEditor, true);
	// One a key reaches is refused as it arrives, as any revision's would be
	agentWrites(app, "text", R"lisp((defn save "An agent's." [] (call "hit")))lisp");
	const int head = app.repository.head().number;
	evaluateBuffer(app);
	CHECK(app.repository.head().number == head);
	CHECK(app.repository.proposals().empty());
	CHECK(app.state("error").get<std::string>().contains("permission denied"));
	// One no key reaches waits
	agentWrites(app, "text", R"lisp((defn fresh "An agent's." [] (call "hit")))lisp");
	const int written = app.repository.head().number;
	evaluateBuffer(app);
	CHECK(app.repository.head().number == written);
	REQUIRE(app.repository.proposals().size() == 1);
	CHECK(app.repository.proposals()[0].author == "plugin:evaluated");
	CHECK(app.state("proposal") == app.repository.proposals()[0].id);
	CHECK(app.state("error").is_null());
	// Accepted, it has no authority of the person's
	REQUIRE_FALSE(
	    app.repository.accept(app.repository.proposals()[0].id, {"user", app.repository.proposals()[0].head, false})
	        .has_value());
	CHECK(app.repository.programAuthors()["init.lisp"]["fresh"] == "plugin:evaluated");
	app.errors.clear();
	app.runtime->runAction("fresh", app.window->root(), json::object());
	CHECK(app.hits == 0);
	CHECK(app.reported("plugin:evaluated"));
}

TEST_CASE("lisp attack: a form an agent adds to what the person typed") {
	App app(kEditor);
	app.type("(+ 1 1)");
	agentWrites(app, "text", "(+ 1 1)\n" + std::string(kReaches));
	evaluateBuffer(app);
	CHECK(app.hits == 0);
	// Inside what the person typed: both sides are theirs, and the middle isn't
	App inside(kEditor);
	inside.type("(do  (+ 1 1))");
	agentWrites(inside, "text", "(do " + std::string(kReaches) + " (+ 1 1))");
	evaluateBuffer(inside);
	CHECK(inside.hits == 0);
}

TEST_CASE("lisp attack: an agent makes a form by taking out what the person typed") {
	App app(kEditor);
	app.type(R"lisp((do ZZZ(call "hit")))lisp");
	agentWrites(app, "text", kReaches);
	evaluateBuffer(app);
	CHECK(app.hits == 0);
	// Joining two forms the person typed, each of which is theirs
	App joined(kEditor);
	joined.type(R"lisp((do (call ZZZ"hit")))lisp");
	agentWrites(joined, "text", R"lisp((do (call "hit")))lisp");
	evaluateBuffer(joined);
	CHECK(joined.hits == 0);
}

TEST_CASE("lisp attack: synthetic input isn't the person typing") {
	// A key pressed for the person, from the person's own key
	App pressed(kEditor);
	pressed.press(Key::P);
	pressed.settle();
	REQUIRE(pressed.state("text") == "w");
	pressed.commit({{"author", "agent/x"},
	                {"message", "m"},
	                {"set", {{"/program/app.lisp/word", R"lisp((def word "As a word." 5))lisp"}}}});
	const int head = pressed.repository.head().number;
	evaluateBuffer(pressed);
	CHECK(pressed.state("error").get<std::string>().contains("typed"));
	CHECK(pressed.repository.head().number == head);
	// A key's step that types for the person
	App typed(kEditor);
	typed.repository.state()["given"] = kReaches;
	typed.press(Key::I);
	typed.settle();
	REQUIRE(typed.state("text") == kReaches);
	evaluateBuffer(typed);
	CHECK(typed.hits == 0);
}

TEST_CASE("lisp attack: an editor's own handler that types while the person types") {
	App app(R"lisp(
(defn shown "" [result error] (set "state.error" error))
(defcommand eval-text "" :title "E" [] (evaluate state.text :then shown))
(defkey "ctrl+e" eval-text)
(defscreen main :state {:text "" :error nil}
  (panel#root (editor#typed :bind "state.text" :autofocus true :width 300 :height 100
                :onChange [{:if "{{state.text == ' '}}" :then [{:edit "type" :text "(call \"hit\")" :in "typed"}]}])))
)lisp");
	app.type(" ");
	app.settle();
	REQUIRE(app.state("text").get<std::string>().contains("call"));
	evaluateBuffer(app);
	CHECK(app.hits == 0);
}

TEST_CASE("lisp attack: text pasted isn't typed, whoever put it on the clipboard") {
	App app(kEditor);
	app.window->setClipboardText(kReaches); // a `copy` step is free, so anyone's code may have
	app.window->routeKey({Key::V, 0, affresco::Action::Press, affresco::Mods{lispapp::controlKey}});
	app.settle();
	REQUIRE(app.state("text") == kReaches);
	evaluateBuffer(app);
	CHECK(app.hits == 0);
}

TEST_CASE("lisp attack: undo brings back what an agent wrote") {
	App app(kEditor);
	agentWrites(app, "text", kReaches);
	// The person selects it and takes it out, by their own keys, and undoes that
	app.window->routeKey({Key::A, 0, affresco::Action::Press, affresco::Mods{lispapp::controlKey}});
	app.window->routeKey({Key::Backspace, 0, affresco::Action::Press, affresco::Mods{}});
	app.settle();
	REQUIRE(app.state("text") == "");
	app.window->routeKey({Key::Z, 0, affresco::Action::Press, affresco::Mods{lispapp::controlKey}});
	app.settle();
	REQUIRE(app.state("text") == kReaches);
	evaluateBuffer(app);
	CHECK(app.hits == 0);
}

TEST_CASE("lisp attack: copying what an agent wrote through the person's own command") {
	App app(kEditor);
	agentWrites(app, "text", kReaches);
	app.press(Key::T);
	REQUIRE(app.state("copied") == kReaches);
	app.press(Key::G);
	app.settle();
	CHECK(app.hits == 0);
	// Nor is a value the agent left anywhere else, read by the person's own command
	agentWrites(app, "copied", kReaches);
	app.press(Key::G);
	app.settle();
	CHECK(app.hits == 0);
}

TEST_CASE("lisp attack: what a person typed in an earlier run is no proof") {
	const App first(kEditor);
	first.type(kReaches);
	const json kept = first.state("text"); // a persisted state: anyone who can write that file has this
	App second(kEditor);
	second.repository.state()["text"] = kept;
	second.settle();
	evaluateBuffer(second);
	CHECK(second.hits == 0);
}

TEST_CASE("lisp attack: a key that presses the person's evaluate for them") {
	// `press` names no action, so nothing says what it reaches: what it reaches has to say no by itself
	App app(R"lisp(
(defn shown "" [result error] (set "state.error" error))
(defcommand eval-text "" :title "E" [] (evaluate state.text :then shown))
(defscreen main :state {:text "" :error nil}
  :keys {"ctrl+j" [{:press "ctrl+e" :in "typed"}]}
  (panel#root (editor#typed :bind "state.text" :autofocus true :width 300 :height 100
                :keys {"ctrl+e" eval-text})))
)lisp");
	app.type(kReaches);
	app.errors.clear();
	app.press(Key::J);
	app.settle();
	CHECK(app.hits == 0);
	// The same key, pressed by the person, is theirs
	app.errors.clear();
	app.press(Key::E);
	app.settle();
	CHECK(app.errors.empty());
	CHECK(app.hits == 1);
}

TEST_CASE("lisp attack: what a bound value's place holds now is whatever an agent made it") {
	App app(R"lisp(
(defn shown "" [result error] (set "state.result" result) (set "state.error" error))
(defcommand eval-other "" :title "E" [] (evaluate state.other :then shown))
(defkey "ctrl+e" eval-other)
(defscreen main :state {:buffers {"99999999999999999999999" {:text ""}} :other "(+ 1 1)" :result nil :error nil}
  (panel#root (editor#typed :bind "state.buffers.99999999999999999999999.text" :autofocus true :width 300 :height 100)))
)lisp");
	app.type("(+ 1 1)");
	REQUIRE(app.state("buffers")["99999999999999999999999"]["text"] == "(+ 1 1)");
	app.commit({{"author", "agent/x"}, {"message", "m"}, {"set", {{"/state/buffers", json::array({1, 2})}}}});
	app.press(Key::E);
	app.settle();
	CHECK(app.state("result") == ""); // what it held went with it, so this is no longer what they typed
	CHECK(app.state("error").get<std::string>().contains("typed"));
}
