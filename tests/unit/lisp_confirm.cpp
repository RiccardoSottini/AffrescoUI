/// @file
/// @brief Unit tests for the question the window asks before text the person didn't type runs as theirs (`evaluate`
/// with `confirm`; SECURITY.md, "The loop"): only their own click or key on it, which no document draws, gives that
/// text their authority, once, and exactly the text it showed. The attacks are written as an agent or a document
/// would try them.

#include "LispApp.hpp"

#include <affresco/render/TextEditor.hpp>

#include <string>

using affresco::json;
using affresco::Key;
using affresco::Panel;
using affresco::Vec2;
using lispapp::App;

namespace {

/// A person's editor: its commands on the person's keys, the buffer bound to the state, evaluation that asks.
constexpr const char* kEditor = R"lisp(
(defn shown "Shows what was evaluated." [result error proposal]
  (set "state.result" result) (set "state.error" error) (set "state.proposal" proposal))
(defcommand eval-text "Evaluates the buffer, asking first if it isn't what was typed." :title "Eval" []
  (evaluate state.text :file "init.lisp" :confirm true :then shown))
(defn save "Reaches nothing yet." [] 1)
(defcommand use-save "" :title "S" [] (save))
(defn press-enter "Presses Enter for the person, on the buffer." [] (press "enter" :in "typed"))
(defcommand arm "Presses Enter a little later." :title "A" [] (after 50 press-enter))
(defn eval-later "Evaluates by itself." [] (evaluate state.text :confirm true :then shown))
(defcommand arm-eval "Evaluates a little later, by a timer." :title "T" [] (after 50 eval-later))
(defkey "ctrl+e" eval-text)
(defkey "ctrl+s" use-save)
(defkey "ctrl+a" arm)
(defkey "ctrl+t" arm-eval)
(defscreen main :state {:text "" :result nil :error nil :proposal nil :out nil :clicked 0}
  (panel#root (editor#typed :bind "state.text" :autofocus true :width 300 :height 100)))
)lisp";

constexpr const char* kReaches = R"lisp((call "hit"))lisp"; ///< A form that has the person's authority or nothing.

constexpr double answerDelayMs = 600.0; ///< More than the question waits before a yes counts (WindowConfirm.cpp).

/// @brief Puts a text on the clipboard and pastes it into the buffer with the person's own key.
/// @param app The app.
/// @param text The text.
void paste(App& app, const std::string& text) {
	app.window->setClipboardText(text);
	app.window->routeKey({Key::V, 0, affresco::Action::Press, affresco::Mods{lispapp::controlKey}});
	app.settle();
}

/// @brief A panel of the question with a role and an accessible name.
/// @param panel Where to look.
/// @param name The name.
/// @return It, or nullptr.
Panel* button(Panel* panel, const std::string& name) {
	if(!panel) return nullptr;
	if(panel->accessibleRole() == "button" && panel->accessibleLabel() == name) return panel;
	for(const auto& child : panel->children())
		if(Panel* found = button(child.get(), name)) return found;
	return nullptr;
}

/// @brief The text the question shows.
/// @param panel Where to look.
/// @return It; "" if none.
std::string shownText(Panel* panel) {
	if(!panel) return "";
	if(const auto* editor = dynamic_cast<affresco::TextEditor*>(panel)) return editor->text();
	for(const auto& child : panel->children())
		if(std::string text = shownText(child.get()); !text.empty()) return text;
	return "";
}

/// @brief The middle of a panel, in window units.
/// @param panel The panel.
/// @return It.
Vec2 middle(const Panel& panel) {
	const affresco::Rect& frame = panel.frame();
	return {(frame.left + frame.right) / 2.0f, (frame.top + frame.bottom) / 2.0f};
}

/// @brief Clicks a point with the person's own mouse.
/// @param app The app.
/// @param at The point.
void click(App& app, Vec2 at) {
	app.window->routeMouseMove(at);
	app.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
	app.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
	app.settle();
}

/// @brief Clicks one of the question's buttons, as the person does.
/// @param app The app.
/// @param name Its name.
void answer(App& app, const std::string& name) {
	app.wait(answerDelayMs);                    // shown a moment: a yes sooner isn't one
	static_cast<void>(app.window->panelAt({})); // laid out
	Panel* found = button(app.window->confirmation(), name);
	REQUIRE(found != nullptr);
	click(app, middle(*found));
}

/// @brief Presses one key with no modifier, the person's own input.
/// @param app The app.
/// @param key The key.
void key(App& app, Key key) {
	app.wait(answerDelayMs); // shown a moment: a yes sooner isn't one
	app.window->routeKey({key, 0, affresco::Action::Press, affresco::Mods{}});
	app.window->routeKey({key, 0, affresco::Action::Release, affresco::Mods{}});
	app.settle();
}

/// @brief Evaluates the buffer with the person's key, and lets the question come up.
/// @param app The app.
void evaluateBuffer(App& app) {
	app.press(Key::E);
	app.settle();
}

} // namespace

TEST_CASE("lisp confirm: a pasted definition, run by the person's own click on the question") {
	App app(kEditor);
	const std::string text = R"lisp((defn save "Mine now." [] (call "hit")))lisp";
	paste(app, text);
	evaluateBuffer(app);
	REQUIRE(app.window->confirmation() != nullptr);
	CHECK(shownText(app.window->confirmation()) == text); // the whole of it, as it is
	answer(app, "Run as Me");
	CHECK(app.window->confirmation() == nullptr);
	CHECK(app.errors.empty());
	CHECK(app.repository.programAuthors()["app.lisp"]["save"] == "user");
	CHECK(app.repository.proposals().empty());
	app.press(Key::S);
	CHECK(app.hits == 1);
	// An expression, by Enter on the question
	App second(kEditor);
	paste(second, kReaches);
	evaluateBuffer(second);
	REQUIRE(second.window->confirmation() != nullptr);
	key(second, Key::Enter);
	CHECK(second.hits == 1);
	// Once: the same text asks again
	evaluateBuffer(second);
	CHECK(second.window->confirmation() != nullptr);
	CHECK(second.hits == 1);
}

TEST_CASE("lisp confirm: saying no runs nothing, and makes nothing") {
	App app(kEditor);
	paste(app, R"lisp((defn save "Not mine." [] (call "hit")) (call "hit"))lisp");
	const int head = app.repository.head().number;
	evaluateBuffer(app);
	REQUIRE(app.window->confirmation() != nullptr);
	answer(app, "Don't Run");
	CHECK(app.window->confirmation() == nullptr);
	CHECK(app.hits == 0);
	CHECK(app.repository.head().number == head);
	CHECK(app.repository.proposals().empty());
	CHECK(app.state("error").get<std::string>().contains("no"));
	// Escape says no too
	evaluateBuffer(app);
	REQUIRE(app.window->confirmation() != nullptr);
	key(app, Key::Escape);
	CHECK(app.window->confirmation() == nullptr);
	CHECK(app.hits == 0);
}

TEST_CASE("lisp confirm: what the person typed doesn't ask") {
	App app(kEditor);
	app.type(kReaches);
	evaluateBuffer(app);
	CHECK(app.window->confirmation() == nullptr);
	CHECK(app.hits == 1);
}

TEST_CASE("lisp confirm attack: an agent's revision can't answer, and can't change what runs") {
	// The buffer changes while the question waits: what runs is what it showed
	App app(kEditor);
	paste(app, R"lisp((set "state.out" 1))lisp");
	evaluateBuffer(app);
	REQUIRE(app.window->confirmation() != nullptr);
	app.commit({{"author", "agent/x"}, {"message", "m"}, {"set", {{"/state/text", kReaches}}}});
	app.settle();
	CHECK(app.window->confirmation() != nullptr); // a revision answers nothing
	answer(app, "Run as Me");
	CHECK(app.hits == 0);
	CHECK(app.state("out") == 1);
	// The program changes while it waits: what the text calls could mean something else, so nothing runs
	App changed(kEditor);
	paste(changed, "(save)");
	evaluateBuffer(changed);
	REQUIRE(changed.window->confirmation() != nullptr);
	changed.write("save", R"lisp((defn save "An agent's." [] (call "hit")))lisp");
	changed.settle();
	answer(changed, "Run as Me");
	CHECK(changed.hits == 0);
	CHECK(changed.state("error").get<std::string>().contains("changed"));
}

TEST_CASE("lisp confirm attack: an agent's code can't ask") {
	App app(kEditor);
	app.write("sneak", R"lisp((defcommand sneak "" :title "X" [] (evaluate "(call \"hit\")" :confirm true)))lisp");
	app.settle();
	app.errors.clear();
	app.runtime->runAction("sneak", app.window->root(), json::object());
	app.settle();
	CHECK(app.window->confirmation() == nullptr);
	CHECK(app.hits == 0);
	CHECK_FALSE(app.errors.empty());
}

TEST_CASE("lisp confirm attack: a timer or a synthetic event can neither ask nor answer") {
	// A timer that evaluates by itself asks nothing
	App timer(kEditor);
	paste(timer, kReaches);
	timer.errors.clear();
	timer.press(Key::T);
	timer.wait(100);
	CHECK(timer.window->confirmation() == nullptr);
	CHECK(timer.hits == 0);
	CHECK(timer.reported("own input"));
	// An action the app runs, with no input at all, asks nothing
	App action(kEditor);
	paste(action, kReaches);
	action.errors.clear();
	action.runtime->runAction("eval-text", action.window->root(), json::object());
	action.settle();
	CHECK(action.window->confirmation() == nullptr);
	CHECK(action.hits == 0);
	// A `press` step from a timer, while the question waits, answers nothing
	App pressed(kEditor);
	pressed.press(Key::A); // the person's command sets the timer that presses Enter
	paste(pressed, kReaches);
	evaluateBuffer(pressed);
	REQUIRE(pressed.window->confirmation() != nullptr);
	pressed.wait(100);
	CHECK(pressed.window->confirmation() != nullptr);
	CHECK(pressed.hits == 0);
	// Nor does a key pressed straight on the question for the screen
	pressed.window->pressKey(*pressed.window->confirmation(), {Key::Enter, 0, affresco::Action::Press, {}}, 0);
	pressed.settle();
	CHECK(pressed.window->confirmation() != nullptr);
	CHECK(pressed.hits == 0);
	// The person's own key does
	key(pressed, Key::Enter);
	CHECK(pressed.hits == 1);
}

TEST_CASE("lisp confirm attack: a document's overlay on top of everything is under the question") {
	App app(R"lisp(
(defn shown "" [result error] (set "state.error" error))
(defcommand eval-text "" :title "E" [] (evaluate state.text :confirm true :then shown))
(defn clicked "" [] (set "state.clicked" (inc state.clicked)))
(defkey "ctrl+e" eval-text)
(defscreen main :state {:text "" :error nil :clicked 0}
  (panel#root (editor#typed :bind "state.text" :autofocus true :width 300 :height 100)
    (panel#cover :overlay {:placement window} :width 400 :height 300 :onClick {:run "clicked"})))
)lisp");
	paste(app, kReaches);
	evaluateBuffer(app);
	REQUIRE(app.window->confirmation() != nullptr);
	// Wherever the pointer is, what is hit is the question, never the overlay
	for(const Vec2 at : {Vec2{1.0f, 1.0f}, Vec2{200.0f, 150.0f}, Vec2{399.0f, 299.0f}}) {
		const Panel* hit = app.window->panelAt(at);
		bool inQuestion = false;
		for(const Panel* p = hit; p; p = p->parent()) inQuestion = inQuestion || p == app.window->confirmation();
		CHECK(inQuestion);
	}
	click(app, {2.0f, 2.0f});
	CHECK(app.state("clicked") == 0);
	CHECK(app.window->confirmation() != nullptr);
	answer(app, "Run as Me");
	CHECK(app.hits == 1);
	CHECK(app.state("clicked") == 0);
}

TEST_CASE("lisp confirm attack: the key that brings the question up doesn't answer it") {
	App app(R"lisp(
(defn shown "" [result error] (set "state.error" error))
(defcommand eval-text "" :title "E" [] (evaluate state.text :confirm true :then shown))
(defscreen main :state {:text "" :error nil}
  (panel#root (editor#typed :bind "state.text" :autofocus true :width 300 :height 100 :keys {"enter" eval-text})))
)lisp");
	paste(app, kReaches);
	app.window->routeKey({Key::Enter, 0, affresco::Action::Press, affresco::Mods{}});
	app.settle();
	REQUIRE(app.window->confirmation() != nullptr);
	app.window->routeKey({Key::Enter, 0, affresco::Action::Release, affresco::Mods{}});
	app.settle();
	CHECK(app.window->confirmation() != nullptr); // held down from before, it isn't an answer
	CHECK(app.hits == 0);
	key(app, Key::Enter);
	CHECK(app.hits == 1);
}

TEST_CASE("lisp confirm attack: text that wouldn't show as it is isn't asked about") {
	App app(kEditor);
	// A right-to-left override makes what is drawn read other than what runs
	const std::string rightToLeft{'\xE2', '\x80', '\xAE'}; // U+202E, built byte by byte, as the attack is the character
	paste(app, "(call \"hit\") ;; " + rightToLeft + ")\"tih\" llac(");
	evaluateBuffer(app);
	CHECK(app.window->confirmation() == nullptr);
	CHECK(app.hits == 0);
	CHECK(app.state("error").get<std::string>().contains("doesn't show"));
	// A letter that draws as a space, in a text that then isn't the one it looks like (the reader takes no such word)
	App filler(kEditor);
	paste(filler, "(set \"state.out\" \"ok\xE3\x85\xA4\")");
	evaluateBuffer(filler);
	CHECK(filler.window->confirmation() == nullptr);
	CHECK(filler.state("out").is_null());
	CHECK(filler.state("error").get<std::string>().contains("doesn't show"));
}

TEST_CASE("lisp confirm: one question at a time, and it goes with the screen") {
	App app(kEditor);
	paste(app, kReaches);
	evaluateBuffer(app);
	REQUIRE(app.window->confirmation() != nullptr);
	// While it is shown the screen hears no key: the evaluating key doesn't reach it
	evaluateBuffer(app);
	CHECK(app.window->confirmation() != nullptr);
	app.runtime.reset();
	CHECK(app.window->confirmation() == nullptr);
	app.window->render();
}

// The question comes up after the evaluating press: a person's next press, already on its way (the second click of a
// double click on a button where Run as Me then shows, Enter typed on after the evaluating key), would say yes to a
// text never read. Saying yes waits until the question has been shown a moment; saying no doesn't
TEST_CASE("lisp confirm attack: a press that follows the evaluating one too soon doesn't say yes") {
	App app(kEditor);
	paste(app, kReaches);
	evaluateBuffer(app);
	REQUIRE(app.window->confirmation() != nullptr);
	app.window->routeKey({Key::Enter, 0, affresco::Action::Press, affresco::Mods{}});
	app.window->routeKey({Key::Enter, 0, affresco::Action::Release, affresco::Mods{}});
	app.settle();
	CHECK(app.window->confirmation() != nullptr);
	CHECK(app.hits == 0);
	static_cast<void>(app.window->panelAt({})); // laid out
	Panel* yes = button(app.window->confirmation(), "Run as Me");
	REQUIRE(yes != nullptr);
	click(app, middle(*yes));
	CHECK(app.window->confirmation() != nullptr);
	CHECK(app.hits == 0);
	// A press begun too soon and let go later isn't a yes either
	app.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, middle(*yes)});
	app.wait(1000);
	app.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, middle(*yes)});
	app.settle();
	CHECK(app.window->confirmation() != nullptr);
	CHECK(app.hits == 0);
	// Once it has been shown a moment, the person's own press says yes
	answer(app, "Run as Me");
	CHECK(app.window->confirmation() == nullptr);
	CHECK(app.hits == 1);
	// No can be said at once
	App declined(kEditor);
	paste(declined, kReaches);
	evaluateBuffer(declined);
	REQUIRE(declined.window->confirmation() != nullptr);
	declined.window->routeKey({Key::Escape, 0, affresco::Action::Press, affresco::Mods{}});
	declined.window->routeKey({Key::Escape, 0, affresco::Action::Release, affresco::Mods{}});
	declined.settle();
	CHECK(declined.window->confirmation() == nullptr);
	CHECK(declined.hits == 0);
}
