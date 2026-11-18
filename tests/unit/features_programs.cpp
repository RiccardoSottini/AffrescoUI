/// @file
/// @brief Unit tests for features written as programs (SCREEN_FORMAT.md, "A feature written as a program"): the
/// manifest, the names, the build, what the code reaches, hooks and advice, and the composed screen.

#include "Composition.hpp"
#include "FeatureManifest.hpp"
#include "FeaturePrograms.hpp"

#include <doctest/doctest.h>

#include <string>

using affresco::json;
namespace format = affresco::format;

namespace {

/// @brief The word count's program: its state, a computed value, a function, a view, a command, a part and a key.
/// @return Its definitions, by address.
json wordCount() {
	return {
	    {"state.wordCount", R"((defstate wordCount {:characters false} "The word count's state."))"},
	    {"computed.wordCount", R"((defcomputed wordCount "Its derived values." {:words (count app.editor.text)}))"},
	    {"wordCount-flip", R"((defn wordCount-flip "Flips it." [] (toggle "state.wordCount.characters")))"},
	    {"wordCount-label",
		 R"((defview wordCount-label "The label." [] `(label#wordCount-label :text ~(str computed.wordCount.words) :onClick wordCount.toggle)))"},
	    {"wordCount.toggle", R"((defcommand wordCount.toggle "Toggles." :title "Toggle" [] (wordCount-flip)))"},
	    {"part.wordCount-views",
		 R"((defpart wordCount-views "Where its views go." :views {:statusBar.right {:wordCount-label ~(wordCount-label)}}))"},
	    {"defkey mod+k", R"((defkey "mod+k" "Toggles the count." wordCount.toggle))"}};
}

/// @brief A program of one file, `feature.lisp`.
/// @param definitions Its definitions, by address.
/// @return The program.
json programOf(const json& definitions) { return {{"feature.lisp", definitions}}; }

/// @brief The word count's program with a definition added or changed.
/// @param address The definition's address.
/// @param text Its text.
/// @return The program.
json withDefinition(const std::string& address, const std::string& text) {
	json definitions = wordCount();
	definitions[address] = text;
	return programOf(definitions);
}

/// @brief A feature's record holding a program.
/// @param program The program.
/// @param order Its place among the features.
/// @return The record, enabled, placed in the status bar.
json record(const json& program, int order = 0) {
	return {{"release", "1.0.0"},
	        {"source", "revision"},
	        {"enabled", true},
	        {"order", order},
	        {"requires", {{"points", {"statusBar.right"}}, {"exports", {{"editor.text", {{"type", "string"}}}}}}},
	        {"provides", {{"program", program}}}};
}

/// @brief An app written as a program: a status bar point that lists the hook point `after-save` and receives the
/// export `editor.save`, an action, a key and a View menu.
/// @return The document, with no feature.
json app() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"program": { "app.lisp": { "after-save": "(defhook after-save \"Runs after a save.\")" } },
		"state": { "text": "one two three" },
		"points": { "statusBar.right": { "slot": "toolbar", "receives": ["editor.save"], "hooks": ["after-save"] } },
		"exports": {
			"editor.text": { "type": "string", "value": "{{state.text}}" },
			"editor.save": { "type": "string", "kind": "action", "value": "{{'save'}}" }
		},
		"actions": { "save": [{ "set": "state.text", "to": "" }] },
		"keys": { "mod+s": "save" },
		"menus": { "view": { "save": { "run": "save" } } },
		"root": { "type": "row", "id": "status-bar", "point": "statusBar.right", "children": [
			{ "type": "label", "id": "status-folder", "text": "folder" }
		] }
	})json");
}

/// @brief The app with one feature installed.
/// @param featureRecord The feature's record.
/// @param name The feature's name.
/// @return The document.
json appWith(const json& featureRecord, const std::string& name = "wordCount") {
	json document = app();
	document["features"][name] = featureRecord;
	return document;
}

/// @brief The problems composing a document gives.
/// @param document The document.
/// @return Each with its place, joined by newlines.
std::string problems(const json& document) {
	std::string text;
	const format::Composition composed = format::compose(document); // the loop outlives a temporary
	for(const std::string& problem : composed.problems) text += problem + "\n";
	return text;
}

/// @brief Whether composing the app with the word count, one definition added or changed, gives a problem with a text.
/// @param address The definition's address.
/// @param definition Its text.
/// @param text The text the problem must hold.
/// @return true if it does.
bool refused(const std::string& address, const std::string& definition, const std::string& text) {
	const std::string found = problems(appWith(record(withDefinition(address, definition))));
	if(!found.contains(text)) MESSAGE(found);
	return found.contains(text);
}

/// @brief A manifest holding a program.
/// @param program The program.
/// @return The manifest.
json manifest(const json& program) {
	return {{"format", "affrescoui-feature"},
	        {"version", 2},
	        {"name", "wordCount"},
	        {"release", "1.0.0"},
	        {"title", "Word count"},
	        {"description", "Counts the words of the text being edited."},
	        {"license", "Apache-2.0"},
	        {"requires", {{"points", {"statusBar.right"}}}},
	        {"provides", {{"program", program}}}};
}

} // namespace

TEST_CASE("feature programs: a manifest may hold a program, and its record keeps it whole") {
	const json written = manifest(programOf(wordCount()));
	CHECK_NOTHROW(format::checkManifest(written, "wordCount"));
	const json installed = format::recordFor(written, "revision", "", "", 0);
	CHECK(installed["provides"]["program"] == programOf(wordCount()));
	CHECK_NOTHROW(format::checkFeatureRecord("wordCount", installed, "features.wordCount"));
	// Its shape is a program's: files of definitions as text
	CHECK_THROWS_WITH(format::checkManifest(manifest({{"feature.lisp", 42}}), "wordCount"),
	                  doctest::Contains("wordCount.provides.program"));
	CHECK_THROWS(format::checkManifest(manifest({{"feature.lisp", {{"x", 1}}}}), "wordCount"));
}

TEST_CASE("feature programs: the names") {
	CHECK(format::programNameProblems("wordCount", programOf(wordCount())).empty());
	const auto named = [](const std::string& address, const std::string& text, const std::string& why) {
		const std::vector<std::string> found = format::programNameProblems("wordCount", withDefinition(address, text));
		REQUIRE(found.size() == 1);
		CHECK(found.front().starts_with("features.wordCount.provides.program.feature.lisp." + address + ": "));
		CHECK(found.front().contains(why));
	};
	named("state.other", R"((defstate other {}))", "named 'wordCount'");
	named("computed.counts", R"((defcomputed counts 1))", "named 'wordCount'");
	named("toggle", R"((defcommand toggle "T." [] 1))", "must start with 'wordCount.'");
	named("wordCount.adapted.save", R"((defcommand wordCount.adapted.save "S." [] 1))", "adapter's");
	named("flip", R"((defn flip "F." [] 1))", "must start with 'wordCount-'");
	named("wordCount.flip", R"((defn wordCount.flip "F." [] 1))", "must start with 'wordCount-'");
	named("styles.badge", R"((defstyle badge {:color "#fff"}))", "must start with 'wordCount-'");
	named("part.views", R"((defpart views :keys {}))", "must start with 'wordCount-'");
	named("wordCount-screen", R"((defscreen wordCount-screen :root {:type "column"}))", "can't hold a defscreen");
	named("menus.wordCount-menu", R"((defmenu wordCount-menu {}))", "can't hold a defmenu");
	named("tokens.wordCount-tokens", R"((deftokens wordCount-tokens {}))", "can't hold a deftokens");
	named("defaults.wordCount-defaults", R"((defdefaults wordCount-defaults :styles {}))", "can't hold a defdefaults");
	// Keys and extensions are named as written
	CHECK(format::programNameProblems("wordCount",
	                                  withDefinition("defkey ctrl+w", R"((defkey "ctrl+w" wordCount.toggle))"))
	          .empty());
}

TEST_CASE("feature programs: the composed screen holds its members under their full names, after the app's") {
	const format::Composition composed = format::compose(appWith(record(programOf(wordCount()))));
	REQUIRE(composed.problems.empty());
	const json& screen = composed.screen;
	CHECK(screen["state"]["wordCount"] == json{{"characters", false}});
	CHECK(screen["computed"]["wordCount"] == "{{{'words': length((state.text))}}}");
	CHECK(screen["actions"]["wordCount.toggle"]["title"] == "Toggle");
	CHECK(screen["keys"]["mod+k"] == "wordCount.toggle");
	const json& children = screen["root"]["children"];
	REQUIRE(children.size() == 2);
	CHECK(children[0]["id"] == "status-folder");
	CHECK(children[1]["id"] == "wordCount-label");
	CHECK(children[1]["text"] == "{{('' + computed.wordCount.words)}}");
	// After the app's own members
	CHECK(screen["actions"].begin().key() == "save");
	CHECK(screen["keys"].begin().key() == "mod+s");
	CHECK_FALSE(screen.contains("features"));
	CHECK(composed.extensions.empty());
}

TEST_CASE("feature programs: a static app installs a feature's JSON version") {
	json document = appWith(record(programOf(wordCount())));
	document.erase("program");
	CHECK(problems(document).contains("features.wordCount.provides.program: this feature holds code, and this app is "
	                                  "static: install its JSON version"));
}

TEST_CASE("feature programs: what the build gives stays in the feature's namespace and sections") {
	CHECK(refused("part.wordCount-more", R"((defpart wordCount-more :actions {:other.go {:do []}}))",
	              "the name must start with 'wordCount.'"));
	CHECK(refused("part.wordCount-more", R"((defpart wordCount-more :styles {:badge {:color "#fff"}}))",
	              "must start with 'wordCount-' or 'wordCount.'"));
	CHECK(refused("part.wordCount-more", R"((defpart wordCount-more :tokens {:color.x "#fff"}))",
	              "builds 'tokens', which a feature can't give"));
	CHECK(refused("part.wordCount-views",
	              R"((defpart wordCount-views :views {:statusBar.right {:other ~(wordCount-label)}}))",
	              "id 'other' must start with 'wordCount-'"));
	CHECK(refused("part.wordCount-more", R"((defpart wordCount-more :actions {:wordCount.go {:do [{:call "x"}]}}))",
	              "is a control step"));
}

TEST_CASE("feature programs: a member both the JSON parts and the program give must be the same") {
	json featureRecord = record(programOf(wordCount()));
	featureRecord["provides"]["actions"] = {{"toggle", {{"title", "Another"}}}};
	CHECK(
	    problems(appWith(featureRecord))
	        .contains("features.wordCount.provides.actions.wordCount.toggle: both the JSON parts and the program give "
			          "it, differently"));
	featureRecord["provides"]["actions"] = json::object();
	featureRecord["provides"]["state"] = {{"characters", false}};
	CHECK(problems(appWith(featureRecord)).empty());
	featureRecord["provides"]["state"] = {{"characters", true}};
	CHECK(problems(appWith(featureRecord)).contains("provides.state.wordCount: both"));
}

TEST_CASE("feature programs: a build that fails, or spins, is refused with the definition") {
	CHECK(refused("wordCount-broken", R"((defn wordCount-broken "B." [] (let [x] x)))",
	              "features.wordCount.provides.program.feature.lisp.wordCount-broken: "));
	CHECK(refused("wordCount-spin", R"((def wordCount-spin (reduce + (range 100000000))))",
	              "features.wordCount.provides.program.feature.lisp.wordCount-spin: its build ran out of its budget"));
}

TEST_CASE("feature programs: what its code reaches") {
	// Its own state, written out
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] (toggle "state.other")))",
	              "'toggle' writes \"state.other\": a feature writes only under \"state.wordCount\""));
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] (set (str "state." "wordCount") 1)))",
	              "'set' must name its path, \"state.wordCount...\", written out"));
	// Its own commands, and the actions it needs
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] (run "save")))", "runs \"save\""));
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] (run "app.save")))", "runs \"app.save\""));
	// No other step, nothing undefined
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] (call "x" {})))", "uses the step 'call'"));
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] (frobnicate 1)))",
	              "uses 'frobnicate', which the feature doesn't define"));
	// A function reads what it is given
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] (count app.editor.text)))",
	              "wordCount-flip: reads 'app.editor.text': pass it to the function as an argument from the view"));
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] state.text))", "reads 'state.text'"));
	CHECK(refused("wordCount.toggle", R"((defcommand wordCount.toggle "T." [] (println computed.other.x)))",
	              "reads 'computed.other.x'"));
	// Through the primitive a path compiles to, called by name
	CHECK(refused("wordCount-flip", R"((defn wordCount-flip "F." [] (%read (str "state." "text"))))",
	              "uses '%read', a name the language keeps"));
	CHECK(refused("wordCount-v", R"((defview wordCount-v "V." [] (text (str (map %read ["auth"])))))",
	              "uses '%read', a name the language keeps"));
	CHECK(refused("wordCount-m", R"((defmacro wordCount-m "M." [] (list (symbol "%read") "auth")))", "uses 'symbol'"));
	CHECK(problems(appWith(record(withDefinition(
	          "wordCount-flip", R"((defn wordCount-flip "F." [xs] (map #(inc %) xs) (str "100%" " done")))"))))
	          .empty());
	// What it may do
	const json allowed = withDefinition(
	    "wordCount-flip",
	    R"((defn wordCount-flip "F." [] (toggle "state.wordCount.characters") (run "wordCount.toggle") (log (str (count state.wordCount.characters)))))");
	CHECK(problems(appWith(record(allowed))).empty());
	json needing = record(withDefinition("wordCount-flip", R"((defn wordCount-flip "F." [] (run "app.save")))"));
	needing["needs"] = {{"save", {{"kind", "action"}}}};
	needing["bindings"] = {{"save", {{"export", "editor.save"}}}};
	const format::Composition composed = format::compose(appWith(needing));
	CHECK(composed.problems.empty());
	CHECK(composed.screen["actions"]["wordCount.adapted.save"] == json::array({{{"run", "save"}}}));
}

TEST_CASE("feature programs: steps its code writes as data reach no more than its code") {
	// A command's steps given as data, a key's and a part's actions: the app runs them later, as its own
	CHECK(refused("wordCount.x", R"((defcommand wordCount.x "X." :do [{:set "state.other" :value 1}] [] nil))",
	              "a feature's code may not run the step 'set' so"));
	CHECK(refused("wordCount.x", R"((defcommand wordCount.x "X." :do [{:run "save"}] [] nil))",
	              "a feature's code may not run the step 'run' so"));
	CHECK(refused("wordCount.x", R"((defcommand wordCount.x "X." :do [{:emit "saved"}] [] nil))",
	              "a feature's code may not run the step 'emit' so"));
	CHECK(refused("part.wordCount-p", R"((defpart wordCount-p :actions {:wordCount.y [{:toggle "state.other"}]}))",
	              "a feature's code may not run the step 'toggle' so"));
	// Its own state and commands, as data too
	CHECK(problems(appWith(record(withDefinition(
	          "wordCount.x",
	          R"((defcommand wordCount.x "X." :do [{:set "state.wordCount.on" :value true} {:run "wordCount.toggle"}] [] nil))"))))
	          .empty());
}

TEST_CASE("feature programs: hooks and advice join only what the app offers the feature's points") {
	const json hooked = withDefinition("add-hook after-save wordCount-flip", "(add-hook after-save wordCount-flip)");
	const format::Composition composed = format::compose(appWith(record(hooked)));
	INFO(problems(appWith(record(hooked))));
	CHECK(composed.problems.empty());
	REQUIRE(composed.extensions.size() == 1);
	CHECK(composed.extensions.front().feature == "wordCount");
	CHECK(composed.extensions.front().kind == "add-hook");
	CHECK(composed.extensions.front().target == "after-save");
	CHECK(refused("add-hook before-quit wordCount-flip", "(add-hook before-quit wordCount-flip)",
	              "joins the hook 'before-quit': no point the feature is placed in lists it"));
	json undeclared = appWith(record(hooked));
	undeclared["program"]["app.lisp"].erase("after-save");
	undeclared["program"]["app.lisp"]["x"] = "(def x 1)";
	CHECK(problems(undeclared).contains("the app declares no hook point 'after-save'"));
	// Advice on an action export the point receives
	const json advised = withDefinition("advice save after wordCount-flip", "(advice save :after wordCount-flip)");
	const format::Composition withAdvice = format::compose(appWith(record(advised)));
	INFO(problems(appWith(record(advised))));
	CHECK(withAdvice.problems.empty());
	REQUIRE(withAdvice.extensions.size() == 1);
	CHECK(withAdvice.extensions.front().target == "save");
	CHECK(refused("advice showTab after wordCount-flip", "(advice showTab :after wordCount-flip)",
	              "advises 'showTab': a feature advises only an action export"));
}

TEST_CASE("feature programs: two program features, one requiring the other, and a disabled one") {
	json document = appWith(record(programOf(wordCount()), 1));
	json other = record(programOf({{"state.lineCount", "(defstate lineCount {:on true})"},
	                               {"lineCount.show", R"((defcommand lineCount.show "S." [] (log "shown")))"}}),
	                    0);
	other["requires"]["features"] = {{"wordCount", "^1.0.0"}};
	document["features"]["lineCount"] = other;
	{
		const format::Composition composed = format::compose(document);
		CHECK(composed.problems.empty());
		CHECK(composed.screen["state"]["lineCount"] == json{{"on", true}});
		// The one required merges first, whatever the order
		std::vector<std::string> actions;
		for(const auto& [name, action] : composed.screen["actions"].items()) actions.push_back(name);
		CHECK(actions == std::vector<std::string>{"save", "wordCount.toggle", "lineCount.show"});
	}
	// A disabled feature builds nothing: its broken program says nothing
	document["features"]["lineCount"]["enabled"] = false;
	document["features"]["lineCount"]["requires"].erase("features");
	document["features"]["lineCount"]["provides"]["program"] = programOf({{"oops", "(defn oops [] (let [x] x))"}});
	CHECK(problems(document).empty());
}
