/// @file
/// @brief Unit tests for writing an app as a program (SCREEN_FORMAT.md, "Views" and "Writing an app as a program"):
/// views and their bindings, repeats and names of the running app, components, handlers, the sections as definitions,
/// the lowering of a binding's form to an expression, and `include`, with the attacks on it written as an attacker
/// would.

#include "HistoryFiles.hpp"

#include <affresco/affresco.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Program.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::TemporaryFolder;
namespace lisp = affresco::lisp;

namespace {

/// @brief What a program builds.
/// @param text The program.
/// @return The tree.
json built(const std::string& text) {
	lisp::Program program;
	program.addFile(text, "app.lisp");
	return program.build();
}

/// @brief What a binding's form builds: `(defscreen main (node :x ~form))`'s `x`.
/// @param form The form.
/// @param definitions Definitions it uses.
/// @return The value.
json binding(const std::string& form, const std::string& definitions = "") {
	return built(definitions + "\n(defscreen main (node :x ~" + form + "))")["root"]["x"];
}

/// @brief The message of the error a program's build gives.
/// @param text The program.
/// @return The message, or "" if it builds.
std::string buildError(const std::string& text) {
	try {
		static_cast<void>(built(text));
	} catch(const lisp::LispError& error) {
		return error.what();
	}
	return "";
}

/// @brief Writes a file.
/// @param path Where.
/// @param text What.
void write(const std::filesystem::path& path, const std::string& text) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream(path, std::ios::binary) << text;
}

} // namespace

TEST_CASE("lisp views: a view's templates hold bindings, with what it was given written in") {
	const json tree = built(R"lisp(
(defview choice "A layout." [view icon]
  `(node :id ~(str "layout-" view) :icon ~icon :selected ~(= state.view view) :label ~(t (str "view." view))))
(defscreen main (column ~(choice "board" "kanban") ~(choice "list" "rows")))
)lisp");
	const json& children = tree["root"]["children"];
	REQUIRE(children.size() == 2);
	CHECK(children[0] == json{{"id", "layout-board"},
	                          {"icon", "kanban"},
	                          {"selected", "{{(state.view == 'board')}}"},
	                          {"label", "{{t('view.board')}}"}});
	CHECK(children[1]["selected"] == "{{(state.view == 'list')}}");
	// A view's code outside a template runs at the build, which can't read the running app
	CHECK(buildError("(defview bad \"\" [] (if state.x `(a) `(b)))\n(defscreen main (column ~(bad)))")
	          .contains("nothing reads it while building"));
	// A view a template calls is worked out at the build, even when it reads the app inside
	CHECK(binding("(v)", "(defview v \"\" [] `(label :text ~state.x))") ==
	      json{{"type", "label"}, {"text", "{{state.x}}"}});
}

TEST_CASE("lisp views: repeat gives one node an item, its item a name of the running app") {
	const json tree = built(R"lisp(
(defn selectCard "Selects." [c i] (set "state.selected" {:c c :i i}))
(defview card-view "A card." [card i]
  `(node#card :text ~card.title :tag ~(:tag card) :hot ~(= (:tag card) "bug") :label ~(str "Card " card.title)
     :onClick ~(handler selectCard 0 i)))
(defscreen main (column ~(repeat [card computed.cards :index i :key card.id] (card-view card i))
                        ~(card-view {:title "Draft" :tag "bug"} 3)))
)lisp");
	const json& repeated = tree["root"]["children"][0];
	CHECK(repeated["id"] == "card");
	CHECK(repeated.begin().key() == "id"); // repeat comes after the type and the id
	CHECK(repeated["repeat"] ==
	      json{{"for", "card"}, {"in", "computed.cards"}, {"index", "i"}, {"key", "{{card.id}}"}});
	CHECK(repeated["text"] == "{{card.title}}");
	CHECK(repeated["tag"] == "{{card.tag}}");
	CHECK(repeated["hot"] == "{{(card.tag == 'bug')}}");
	CHECK(repeated["label"] == "Card {{card.title}}");
	CHECK(repeated["onClick"] == json{{"lisp", "selectCard"}, {"with", {0, "{{i}}"}}});
	// The same view given values: worked out at the build
	const json& plain = tree["root"]["children"][1];
	CHECK(plain["text"] == "Draft");
	CHECK(plain["hot"] == true);
	CHECK(plain["label"] == "Card Draft");
	CHECK(plain["onClick"] == json{{"lisp", "selectCard"}, {"with", {0, 3}}});
	// An expression for the list, and a quoted name of the running app
	CHECK(binding("(repeat [x (filter (fn [it] it.on) state.xs)] `(label :text ~x))")["repeat"]["in"] ==
	      "{{filter(state.xs, it.on)}}");
	CHECK(binding("(v 'menus.card)", "(defview v \"\" [items] `(node :items ~items))")["items"] == "{{menus.card}}");
	// repeat builds a view's nodes, and nothing else
	CHECK(buildError("(defn f \"\" [] (repeat [x state.xs] 1))\n(defscreen main (column))").contains("repeat builds"));
	CHECK(buildError("(defscreen main (column ~(repeat [x state.xs] 1)))").contains("gives one node"));
}

TEST_CASE("lisp views: defcomponent, handler, and the sections as definitions") {
	const json tree = built(R"lisp(
(defn moveCard "Moves." [from at to] nil)
(defcomponent chip "A chip." {:label "" :on false}
  `(label :text ~label :style ~(if on "chipOn" "chip")))
(defstate view board "Which layout." :type {:enum [board list]})
(defstate cards [])
(defcomputed open "Shown." (filter (fn [x] (not= x.id state.sel)) state.cards))
(defstyle caption "Small." (label :font small))
(defmenu card "The card's." {:edit {:run editCard}})
(deftokens colours "The colours." {:color.a {:light "#fff" :dark "#000"}})
(defpart presence "Who is here." :watch {:state.members buildTimeline} :persist [state.theme] :onLoad start)
(defpart more "More." :persist [state.view])
(defscreen main :persist [state.sizes]
  (column ~(repeat [c state.cols] `(node :onDrop ~(handler moveCard event.data.from (+ event.data.at 1) c)))))
)lisp");
	CHECK(tree["components"]["chip"] ==
	      json{{"note", "A chip."},
	           {"params", {{"label", ""}, {"on", false}}},
	           {"node", {{"type", "label"}, {"text", "{{label}}"}, {"style", "{{(on ? 'chipOn' : 'chip')}}"}}}});
	CHECK(tree["state"] == json{{"cards", json::array()}, {"view", "board"}});
	CHECK(tree["stateTypes"] == json{{"view", {{"enum", {"board", "list"}}}}});
	CHECK(tree["computed"]["open"] == "{{filter(state.cards, (it.id != state.sel))}}");
	CHECK(tree["styles"]["caption"] == json{{"type", "label"}, {"font", "small"}});
	CHECK(tree["menus"]["card"] == json{{"edit", {{"run", "editCard"}}}});
	CHECK(tree["tokens"]["color.a"] == json{{"light", "#fff"}, {"dark", "#000"}});
	CHECK(tree["watch"] == json{{"state.members", "buildTimeline"}});
	CHECK(tree["onLoad"] == "start");
	CHECK(tree["persist"].size() == 3); // main's, then the parts': lists join
	CHECK(tree["persist"][0] == "state.sizes");
	CHECK(tree["root"]["children"][0]["onDrop"] ==
	      json{{"lisp", "moveCard"}, {"with", {"{{event.data.from}}", "{{(event.data.at + 1)}}", "{{c}}"}}});
	// Addresses that meet no function's name; a member given twice is an error naming it
	lisp::Program program;
	program.addFile("(defstate card 1)\n(defn card \"\" [] 1)\n(defscreen main (column))", "app.lisp");
	CHECK(program.entries()[0].name == "state.card");
	CHECK(buildError("(deftokens a \"\" {:x 1})\n(deftokens b \"\" {:x 2})\n(defscreen main (column))")
	          .contains("given twice"));
	CHECK(buildError("(defpart a \"\" :onLoad x)\n(defpart b \"\" :onLoad y)\n(defscreen main (column))")
	          .contains("given twice"));
	CHECK(buildError("(defstate x 1 \"Doc.\" :default 2)\n(defscreen main (column))").contains("only option is :type"));
}

TEST_CASE("lisp views: defdefaults yields to any other definition, and to the document's own value") {
	const std::string program = R"lisp(
(defdefaults addon "An add-on's colours and key." :tokens {:addon.accent "#00f" :addon.text "#000"} :keys {"mod+a" open})
(deftokens app "The app's." {:addon.accent "#f00"})
(defscreen main (column))
)lisp";
	const json tree = built(program);
	CHECK(tree["tokens"] == json{{"addon.accent", "#f00"}, {"addon.text", "#000"}});
	CHECK(tree["keys"]["mod+a"] == "open");
	// A document's own value replaces a default; one a definition gives must still be the same
	json source = lisp::sourceDocument(program, "app.lisp");
	source["tokens"] = {{"addon.text", "#111"}};
	source["keys"] = {{"mod+a", "close"}};
	const json document = lisp::buildDocument(source);
	CHECK(document["tokens"]["addon.text"] == "#111");
	CHECK(document["keys"]["mod+a"] == "close");
	source["tokens"] = {{"addon.accent", "#0f0"}};
	CHECK_THROWS_WITH_AS(static_cast<void>(lisp::buildDocument(source)), doctest::Contains("written in the document"),
	                     lisp::LispError);
}

TEST_CASE("lisp views: a binding's form is the expression machine's, at its speed") {
	CHECK(binding("(count state.xs)") == "{{length(state.xs)}}");
	CHECK(binding("(get-in computed.columns [state.c :cards 0])") == "{{computed.columns[state.c].cards[0]}}");
	CHECK(binding("(get state.m \"a b\")") == "{{state.m['a b']}}");
	CHECK(binding("(:title (first state.xs))") == "{{state.xs[0].title}}");
	CHECK(binding("(or-else state.x 0)") == "{{(state.x ?? 0)}}");
	CHECK(binding("(nil? state.x)") == "{{(state.x == null)}}");
	CHECK(binding("(cond (= state.k \"a\") 1 (= state.k \"b\") 2 :else 3)") ==
	      "{{((state.k == 'a') ? 1 : ((state.k == 'b') ? 2 : 3))}}");
	CHECK(binding("(str \"a \" state.n \" b\")") == "a {{state.n}} b");
	CHECK(binding("(t (str \"tag.\" state.tag))") == "{{t(('tag.' + state.tag))}}");
	CHECK(binding("(map-indexed (fn [i x] (+ i x.n)) state.xs)") == "{{map(state.xs, (index + it.n))}}");
	CHECK(binding("(remove (fn [x] x.done) state.xs)") == "{{filter(state.xs, !(it.done))}}");
	CHECK(binding("(sort-by (fn [x] x.n) > state.xs)") == "{{sort(state.xs, it.n, 'desc')}}");
	CHECK(binding("(reduce (fn [total x] (+ total x.n)) 0 state.xs)") == "{{reduce(state.xs, (acc + it.n), 0)}}");
	CHECK(binding("(sum state.xs (fn [it] it.n))") == "{{sum(state.xs, it.n)}}");
	CHECK(binding("(join \", \" state.names)") == "{{join(state.names, ', ')}}");
	CHECK(binding("(- state.n)") == "{{-(state.n)}}");
	CHECK(binding("(* state.r 0.000001)") == "{{(state.r * 0.000001)}}"); // an expression reads no exponent
	// A macro writes the form, which is then an expression: once a call, its gensyms the same
	CHECK(binding("(ordered state.c)", "(defmacro ordered \"\" [c] `(filter (fn [it] (= it.c ~c)) state.xs))") ==
	      "{{filter(state.xs, (it.c == state.c))}}");
	// A function inside another can't use the outer one's item: both are `it`
	CHECK(buildError("(defscreen main (node :x ~(filter (fn [a] (some (fn [b] (= a b)) state.ys)) state.xs)))")
	          .contains("outer one's"));
	// A word naming a function of the program, uncalled, is a name of the running app: a shared file's `at`
	CHECK(binding("at", "(defn at \"An app's own.\" [] 1)") == "{{at}}");
	CHECK(binding("(+ at 1)", "(defn at \"An app's own.\" [] 1)") == "{{(at + 1)}}");
}

TEST_CASE("lisp views: an app written as a program names its files with include") {
	const TemporaryFolder folder;
	const std::filesystem::path app = folder.path() / "app";
	write(app / "app.lisp", "{:format affrescoui-app :version 1 :id views-test :name \"Views\" :entry main.lisp}");
	write(app / "main.lisp",
	      ";;; main.lisp --- the root\n(include \"parts/hello.lisp\")\n\n;; Its screen.\n"
	      "(defscreen main :window {:title \"T\" :width 300 :height 200}\n  (column#root ~(hello)))\n");
	write(app / "parts" / "hello.lisp",
	      "(defview hello \"Hi.\" [] `(label#hi :text ~(str \"n=\" state.n)))\n(defstate n 3)\n");
	const affresco::Screen screen = affresco::Screen::fromApp(app, {}, affresco::Screen::History::InMemory);
	const json document = json::parse(screen.documentJson());
	CHECK(document["root"]["children"][0]["text"] == "n={{state.n}}");
	CHECK(document["state"]["n"] == 3);
	CHECK(document["program"].contains("main.lisp"));
	CHECK(document["program"].contains("parts/hello.lisp"));
	CHECK_FALSE(document["program"]["main.lisp"].contains("include")); // never an entry
	// The form is taken out with its places kept: a mistake after it names its own line
	const json read = lisp::readDocument("(include \"a.lisp\"\n  \"b.lisp\")\n(defn f \"\" [] 1)", "x.lisp");
	CHECK(read["include"] == json{"a.lisp", "b.lisp"});
	try {
		static_cast<void>(
		    lisp::readDocument("(include \"a.lisp\")\n\n(defn f \"\" [] 1)\n(defn g \"\" [] (", "x.lisp"));
		FAIL("an unfinished form is an error");
	} catch(const affresco::DocumentError& error) {
		CHECK(std::string(error.what()).contains("line 4"));
	}
}

TEST_CASE("lisp views attack: a revision can't make a program read a file") {
	const TemporaryFolder folder;
	const std::filesystem::path app = folder.path() / "app";
	write(folder.path() / "secret.lisp", "(defstate secret \"the secret\")\n");
	write(app / "app.lisp", "{:format affrescoui-app :version 1 :id views-attack :name \"Attack\" :entry main.lisp}");
	write(app / "main.lisp",
	      "(defscreen main :permissions {:allow [studio agent]} (column#root (label#a :text \"a\")))\n");
	affresco::Screen screen = affresco::Screen::fromApp(app, {}, affresco::Screen::History::InMemory);
	// As a new entry, under any name, and as a definition's text: refused, the screen as it was
	for(const json& set : {json{{"/program/main.lisp/include", "(include \"../secret.lisp\")"}},
	                       json{{"/program/main.lisp/main", "(include \"../secret.lisp\")"}},
	                       json{{"/program/main.lisp/x", "(do (include \"../secret.lisp\"))"}}}) {
		CAPTURE(set.dump());
		const json revision = {{"author", "agent/x"}, {"message", "read"}, {"add", set}};
		CHECK(screen.submitRevision(revision.dump()).status != RevisionStatus::Applied);
		CHECK_FALSE(json::parse(screen.documentJson())["state"].contains("secret"));
	}
	// The document's own include can't be given either
	const json include = {{"author", "agent/x"}, {"message", "read"}, {"add", {{"/include", {"../secret.lisp"}}}}};
	CHECK(screen.submitRevision(include.dump()).status != RevisionStatus::Applied);
	CHECK_FALSE(json::parse(screen.documentJson())["state"].contains("secret"));
	// And a file the app loads can't name one outside its folder
	write(app / "main.lisp", "(include \"../secret.lisp\")\n(defscreen main (column#root))\n");
	CHECK_THROWS_WITH_AS(static_cast<void>(affresco::Screen::fromApp(app, {}, affresco::Screen::History::InMemory)),
	                     doctest::Contains("outside the app's folder"), affresco::DocumentError);
}

TEST_CASE("lisp views: what only builds is judged by what it builds, and a view counts as code") {
	const std::string program = R"lisp(
(defstate n 1)
(defview hello "Hi." [] `(label#hi :text "hi"))
(defscreen main :permissions {:allow [studio agent] :ask ["agent(/actions)"] :reviewers [studio]} (column#root ~(hello)))
)lisp";
	affresco::Screen screen = affresco::Screen::fromString(lisp::sourceDocument(program, "app.lisp").dump());
	screen.versions().setMode(affresco::Mode::AcceptEdits);
	// A comment on a state's definition changes nothing built: applied, as a JSON section's no-op would be
	const json state = {
	    {"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/state.n", ";; One.\n(defstate n 1)"}}}};
	CHECK(screen.submitRevision(state.dump()).status == RevisionStatus::Applied);
	// A view is a function: the rule on /actions covers it, as it covers a defn
	const json view = {
	    {"author", "agent/x"},
	    {"message", "m"},
	    {"set", {{"/program/app.lisp/hello", ";; Hi.\n(defview hello \"Hi.\" [] `(label#hi :text \"hi\"))"}}}};
	CHECK(screen.submitRevision(view.dump()).status != RevisionStatus::Applied);
}
