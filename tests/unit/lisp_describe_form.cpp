/// @file
/// @brief Unit tests for `describe-form`: what a text would define if the person evaluated it, told only to the
/// person's own code, and nothing changed by asking.

#include "LispApp.hpp"

#include <string>

using affresco::json;
using affresco::Key;
using lispapp::App;

namespace {

/// The person's commands that ask about texts, on their keys.
constexpr const char* kAsks = R"lisp(
(defn helper "Is there already." [] 1)
(defcommand look "Asks about a definition and an expression." :title "L" []
  (set "state.form" (describe-form "(defn helper \"Changed.\" [] 2) (defn added \"\" [] 3) (+ 1 2)")))
(defcommand look-bad "Asks about texts that aren't forms." :title "B" []
  (set "state.bad" [(describe-form "not lisp") (describe-form "(+ 1") (describe-form "(no-such-thing 1)")
                    (describe-form 42)]))
(defkey "ctrl+l" look)
(defkey "ctrl+b" look-bad)
(defscreen main :state {:form nil :bad nil :ran nil} (panel#root))
)lisp";

} // namespace

TEST_CASE("lisp_describe_form: the person's command learns what a text would define") {
	App app(kAsks);
	const int head = app.repository.head().number;
	app.press(Key::L);
	app.settle();
	const std::string firstError = app.errors.empty() ? std::string() : app.errors[0];
	INFO(firstError);
	const json form = app.state("form");
	REQUIRE(form.is_object());
	REQUIRE(form["definitions"].size() == 2);
	CHECK(form["definitions"][0]["name"] == "helper");
	CHECK(form["definitions"][0]["kind"] == "defn");
	CHECK(form["definitions"][0]["file"] == "app.lisp");
	CHECK(form["definitions"][0]["replaces"] == true);
	CHECK(form["definitions"][1]["name"] == "added");
	CHECK(form["definitions"][1]["replaces"] == false);
	CHECK(form["expressions"] == 1);
	CHECK(form["typed"] == false);
	// Asking changes nothing
	CHECK(app.repository.document()["program"]["app.lisp"]["helper"].get<std::string>().contains("Is there"));
	CHECK(app.repository.head().number == head);
	// What isn't forms of Lisp the program can run is nil
	app.press(Key::B);
	app.settle();
	CHECK(app.state("bad") == json::array({nullptr, nullptr, nullptr, nullptr}));
}

TEST_CASE("lisp_describe_form attack: an agent's code learns nothing") {
	App app(kAsks);
	app.write("peek", R"lisp((defcommand peek "An agent's." :title "P" []
  (set "state.ran" true) (set "state.form" (describe-form "(+ 1 2)"))))lisp");
	app.runtime->runAction("peek", app.window->root(), json::object());
	app.settle();
	CHECK(app.state("ran") == true);
	CHECK(app.state("form").is_null());
}
