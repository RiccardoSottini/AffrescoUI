/// @file
/// @brief Unit tests for what a binding that calls a program's function records (SCREEN_FORMAT.md, "Writing an app as
/// a program"): its function reads the running app through `%read`, the lookup an expression's path takes, so the
/// binding is evaluated again only when a place it read changes. Each case is a small program, a change of the state,
/// and the counters of the update that followed, checked in verify mode, where a read left unrecorded is a difference.

#include "LispApp.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::format::UpdateStatistics;
using lispapp::App;
using lispapp::json;

namespace {

/// @brief The actions every program here has: each sets the place its name says to the event's `v`, but `keep`, which
/// keeps what the function `same` gives in `state.kept`.
constexpr const char* actions = R"lisp(
  :actions {:count [{:set "state.count" :to "{{event.v}}"}]
            :other [{:set "state.other" :to "{{event.v}}"}]
            :item [{:set "state.items[event.i].v" :to "{{event.v}}"}]
            :box-a [{:set "state.box.a" :to "{{event.v}}"}]
            :box-x [{:set "state.box.inner.x" :to "{{event.v}}"}]
            :keep [{:set "state.kept" :to "{{lisp('same')}}"}]})lisp";

/// @brief A program's screen, in verify mode: after each partial update a full pass must give the same panels.
struct Checked {
	App app; ///< The app.

	/// @brief Builds it.
	/// @param definitions The program's definitions.
	/// @param state Its state, as Lisp.
	/// @param body The screen's root.
	Checked(const std::string& definitions, const std::string& state, const std::string& body)
	    : app(definitions + "\n(defscreen main :state " + state + actions + "\n  " + body + ")\n") {
		app.runtime->setVerifyUpdates(true);
		REQUIRE(app.errors.empty());
	}

	/// @brief Runs one of the actions, and the update that follows.
	/// @param action Its name.
	/// @param event Its event.
	/// @return What the update did.
	UpdateStatistics change(const char* action, const json& event) {
		app.runtime->runAction(action, app.window->root(), event);
		app.window->render();
		for(const std::string& error : app.errors) INFO(error);
		CHECK(app.errors.empty()); // in verify mode, an update that missed a read is reported here
		return app.runtime->lastUpdate();
	}

	/// @brief A label's text.
	/// @param id Its id.
	/// @return It.
	[[nodiscard]] std::string text(const char* id) const {
		const auto* label = dynamic_cast<affresco::Label*>(app.window->root().findById(id));
		REQUIRE(label);
		return label->text();
	}
};

} // namespace

TEST_CASE("lisp reads: a function's binding is evaluated when what it read changes, and only then") {
	Checked checked(R"lisp((defn counted "Reads the count." [] (str "n=" state.count)))lisp", "{:count 0 :other 0}",
	                "(column (label#a :text ~(counted)))");
	CHECK(checked.text("a") == "n=0");
	UpdateStatistics update = checked.change("count", {{"v", 5}});
	CHECK_FALSE(update.full);
	CHECK(update.nodesEvaluated == 1);
	CHECK(checked.text("a") == "n=5");
	update = checked.change("other", {{"v", 5}});
	CHECK_FALSE(update.full);
	CHECK(update.nodesEvaluated == 0);
}

TEST_CASE("lisp reads: a path into a list records the item it reads, not the list") {
	Checked checked(R"lisp((defn third "" [] state.items.3.v))lisp", "{:items [{:v 0} {:v 1} {:v 2} {:v 3} {:v 4}]}",
	                "(column (label#a :text ~(third)))");
	CHECK(checked.text("a") == "3");
	UpdateStatistics update = checked.change("item", {{"i", 3}, {"v", 30}});
	CHECK(update.nodesEvaluated == 1);
	CHECK(checked.text("a") == "30");
	update = checked.change("item", {{"i", 4}, {"v", 40}});
	CHECK_FALSE(update.full);
	CHECK(update.nodesEvaluated == 0);
}

TEST_CASE("lisp reads: a function given a repeat's item evaluates only the row whose item changed") {
	Checked checked(R"lisp((defn shown "" [item] (str "v=" (:v item))))lisp",
	                "{:items [{:id 0 :v 0} {:id 1 :v 1} {:id 2 :v 2} {:id 3 :v 3} {:id 4 :v 4} {:id 5 :v 5}]}",
	                "(column#list ~(repeat [item state.items :key item.id] `(label :text ~(shown item))))");
	const UpdateStatistics update = checked.change("item", {{"i", 2}, {"v", 20}});
	CHECK_FALSE(update.full);
	CHECK(update.nodesEvaluated == 1);
	CHECK(update.panelsApplied == 1);
	const auto& rows = checked.app.window->root().findById("list")->children();
	REQUIRE(rows.size() == 6);
	CHECK(dynamic_cast<const affresco::Label&>(*rows[2]).text() == "v=20");
	CHECK(dynamic_cast<const affresco::Label&>(*rows[3]).text() == "v=3");
}

TEST_CASE("lisp reads: through another function, a let, a destructured map and get-in, the paths read are recorded") {
	Checked checked(R"lisp(
(defn inner "" [] state.box.a)
(defn outer "" [] (inner))
(defn with-let "" [] (let [b state.box] (:a b)))
(defn destructured "" [] (let [{:keys [a]} state.box] a))
(defn deep "" [] (get-in state.box [:inner :x])))lisp",
	                "{:box {:a 1 :inner {:x 2}} :other 0}",
	                "(column (label#outer :text ~(outer)) (label#let :text ~(with-let))"
	                " (label#destructured :text ~(destructured)) (label#deep :text ~(deep)))");
	CHECK(checked.text("deep") == "2");
	// state.box is read whole by three of them: a change inside it evaluates them, coarse but right
	UpdateStatistics update = checked.change("box-x", {{"v", 20}});
	CHECK(update.nodesEvaluated == 3);
	CHECK(checked.text("deep") == "20");
	CHECK(checked.text("outer") == "1");
	update = checked.change("box-a", {{"v", 10}});
	CHECK(update.nodesEvaluated == 4);
	CHECK(checked.text("outer") == "10");
	CHECK(checked.text("let") == "10");
	CHECK(checked.text("destructured") == "10");
	update = checked.change("other", {{"v", 1}});
	CHECK(update.nodesEvaluated == 0);
}

TEST_CASE("lisp reads: an expression's function called from code records what it reads") {
	// today() reads the clock, which no change announces: the binding reads always, as the expression would
	Checked checked(R"lisp((defn dated "" [] (str state.count " " (today))))lisp", "{:count 0 :other 0}",
	                "(column (label#a :text ~(dated)))");
	const UpdateStatistics update = checked.change("other", {{"v", 1}});
	CHECK_FALSE(update.full);
	CHECK(update.nodesEvaluated == 1);
	CHECK(update.alwaysReaders == 1);
	CHECK(checked.text("a").starts_with("0 "));
}

TEST_CASE("lisp reads: advice around a function, and before it, records what each reads") {
	Checked checked(R"lisp(
(defn base "" [] state.box.a)
(defn wrapped "" [proceed] (str (proceed) "/" state.count))
(advice base :around wrapped)
(defn looked "" [] state.box.inner.x)
(advice base :before looked))lisp",
	                "{:box {:a 1 :inner {:x 2}} :count 0 :other 0}", "(column (label#a :text ~(base)))");
	CHECK(checked.text("a") == "1/0");
	CHECK(checked.change("count", {{"v", 3}}).nodesEvaluated == 1);
	CHECK(checked.text("a") == "1/3");
	CHECK(checked.change("box-a", {{"v", 4}}).nodesEvaluated == 1);
	CHECK(checked.text("a") == "4/3");
	CHECK(checked.change("box-x", {{"v", 5}}).nodesEvaluated == 1); // what the advice before it read
	CHECK(checked.change("other", {{"v", 1}}).nodesEvaluated == 0);
}

TEST_CASE("lisp reads: a hook's change reaches the binding whose function read it") {
	// A hook never runs in a value read for the screen (run-hooks is a step): it runs from a command, and what it
	// writes is a change like any other
	Checked checked(R"lisp(
(defhook changed "")
(defn on-changed "" [] (set "state.box.a" 7))
(add-hook changed on-changed)
(defcommand go "" :title "Go" [] (run-hooks changed))
(defkey "ctrl+g" go)
(defn base "" [] state.box.a))lisp",
	                "{:box {:a 1} :other 0}", "(column (label#a :text ~(base)) (label#b :text ~state.other))");
	checked.app.press(affresco::Key::G);
	CHECK(checked.app.errors.empty());
	CHECK(checked.text("a") == "7");
	CHECK_FALSE(checked.app.runtime->lastUpdate().full);
	CHECK(checked.app.runtime->lastUpdate().nodesEvaluated == 1);
}

TEST_CASE("lisp reads: an app variable read from code is read as an expression reads it") {
	Checked checked(R"lisp(
(defn listed "" [] (count commands))
(defn set-to "" [] settings.x)
(defn where "" [] (count context))
(defn whole "" [] (:count state)))lisp",
	                "{:count 0 :other 0}",
	                "(column (label#commands :text ~(listed)) (label#settings :text ~(set-to))"
	                " (label#context :text ~(where)) (label#state :text ~(whole)) (toggle#go))");
	CHECK(checked.text("state") == "0");
	// commands, settings and context are places, read through the lookup an expression takes: a change of the state
	// evaluates only #state, which reads the state whole, and no binding is read on every update
	const UpdateStatistics update = checked.change("other", {{"v", 1}});
	CHECK_FALSE(update.full);
	CHECK(update.alwaysReaders == 0);
	CHECK(update.nodesEvaluated == 1);
	CHECK(checked.change("count", {{"v", 2}}).nodesEvaluated == 1);
	CHECK(checked.text("state") == "2");
	// a change of a variable's own place evaluates what read it: the focus is the context's
	checked.app.window->setFocus(checked.app.runtime->find("go"));
	CHECK(checked.change("other", {{"v", 3}}).nodesEvaluated == 2); // #context, and #state, which reads the state whole
	CHECK(checked.app.runtime->lastUpdate().alwaysReaders == 0);
}

TEST_CASE("lisp reads: a function that reads nothing of the app is evaluated once") {
	Checked checked(R"lisp((defn constant "" [] (str "a" "b")))lisp", "{:count 0}", "(column (label#a :text ~(constant)))");
	CHECK(checked.text("a") == "ab");
	CHECK(checked.change("count", {{"v", 1}}).nodesEvaluated == 0);
	CHECK(checked.change("count", {{"v", 2}}).nodesEvaluated == 0);
}

TEST_CASE("lisp reads: what a read viewed in place never outlives it") {
	// The read gives its own tree: the state changed after it, the value it gave stays as it was
	Checked checked(R"lisp(
(defn same "" [] state.box)
(defn twice "" [] (+ (count state.items) (count state.items) (count (:inner state.box)))))lisp",
	                "{:box {:a 1 :inner {:x 2}} :items [{:v 0} {:v 1}] :kept nil}",
	                "(column (label#a :text ~(twice)))");
	CHECK(checked.text("a") == "5");
	checked.change("keep", json::object()); // a handler's expression reads for the step: viewed in place too
	checked.app.runtime->runAction("box-a", checked.app.window->root(), {{"v", 9}});
	checked.app.window->render();
	CHECK(checked.app.state("kept") == json{{"a", 1}, {"inner", {{"x", 2}}}});
	CHECK(checked.app.state("box")["a"] == 9);
	// state.items is read whole, twice, and charged twice: a change inside it evaluates the binding
	CHECK(checked.change("item", {{"i", 1}, {"v", 5}}).nodesEvaluated == 1);
}
