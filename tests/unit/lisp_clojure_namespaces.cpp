/// @file
/// @brief Unit tests for a program file's `(ns name (:require ...))` form (modules/lisp/reference/namespaces.md): each
/// clause and its errors, places kept, aliases of one file only, required files loaded once and only from the app's
/// folder, the form kept from revisions, and the command line's ns.

#include "../../modules/lisp/src/Machine.hpp"
#include "../../modules/lisp/src/Namespaces.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Json.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>
#include <affresco/notation/words/Namespaces.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>

namespace lisp = affresco::lisp;
using affresco::json;

namespace {

/// The files of an app, by path from its folder.
using Files = std::map<std::string, std::string>;

/// @brief Writes an app's files into a fresh folder of the system's temporary folder, with an app.json.
/// @param name The folder's name.
/// @param files The files.
/// @return The folder.
std::filesystem::path writeApp(const std::string& name, const Files& files) {
	const std::filesystem::path root = std::filesystem::temp_directory_path() / ("affrescoui-namespaces-" + name);
	std::filesystem::remove_all(root);
	const std::filesystem::path app = root / "app";
	Files all = files;
	all.try_emplace(
	    "app.json",
	    R"({ "format": "affrescoui-app", "version": 1, "id": "ns", "name": "Ns", "entry": "index.screen.lisp" })");
	for(const auto& [path, text] : all) {
		std::filesystem::create_directories((app / path).parent_path());
		std::ofstream(app / path) << text;
	}
	return app;
}

/// @brief An app's program, as the library loads it, built.
/// @param name A name for its folder.
/// @param files Its files; index.screen.lisp is the entry.
/// @return The tree its program builds.
json built(const std::string& name, const Files& files) {
	affresco::Screen screen = affresco::Screen::fromApp(writeApp(name, files), {});
	return lisp::programOf(screen.versions().sourceDocument().at("program")).build();
}

/// @brief Why an app doesn't open or build.
/// @param name A name for its folder.
/// @param files Its files.
/// @return The error, or "it built".
std::string refusal(const std::string& name, const Files& files) {
	try {
		static_cast<void>(built(name, files));
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return "it built";
}

/// @brief A one-screen program whose label shows a form's value.
/// @param head What comes before the screen: an ns form, definitions.
/// @param form The form.
/// @return The text.
std::string screenShowing(const std::string& head, const std::string& form) {
	return head + "\n(defscreen main (label#shown :text ~" + form + "))\n";
}

/// @brief The error a program file gives when it is read and built, alone.
/// @param text Its text.
/// @return The error, or "it built".
std::string fileError(std::string_view text) {
	try {
		static_cast<void>(lisp::buildDocument(lisp::readDocument(text, "index.screen.lisp")));
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return "it built";
}

/// @brief What a text evaluates to at the command line, in a new interpreter.
/// @param text The text.
/// @return The value printed, or the error's message.
std::string atCommandLine(std::string_view text) {
	lisp::Interpreter interpreter;
	try {
		return interpreter.print(interpreter.eval(text, "repl", "repl"));
	} catch(const lisp::LispError& error) {
		return error.message();
	}
}

/// The fixture app's three files (tests/unit/fixtures/namespaces), as a test writes them.
const Files board = {
    {"index.screen.lisp",
	 screenShowing("(ns tally.index (:require [clojure.string :as s] [tally.parts.board :as board]))",
	               "(s/upper-case (board/title))")},
    {"parts/board.lisp", "(ns tally.parts.board (:require [tally.parts.cell :as cell]))\n"
	                     "(defn title [] (cell/named \"board\"))"},
    {"parts/cell.lisp", "(ns tally.parts.cell)\n(defn named [n] (str \"the \" n))"},
};

} // namespace

TEST_CASE("lisp namespaces: a file's ns form names it, and its aliases reach the files it requires") {
	CHECK(built("board", board)["root"]["text"] == "THE BOARD");
	// A docstring, a plain name, a string path and :refer of a program's file
	Files files = board;
	files["index.screen.lisp"] = screenShowing(
	    "(ns tally.index \"The window.\" (:require clojure.string [\"parts/board.lisp\" :as b :refer [title]]))",
	    "(clojure.string/upper-case (str (title) (b/title)))");
	CHECK(built("forms", files)["root"]["text"] == "THE BOARDTHE BOARD");
	// The language's namespaces need no file, and :refer takes a word that is the same function plain
	CHECK(built("language", {{"index.screen.lisp",
	                          screenShowing("(ns app (:require [clojure.string :as s :refer [join]] [clojure.set]))",
	                                        "(join \"-\" [(s/lower-case \"A\") \"b\"])")}})["root"]["text"] == "a-b");
	// A file with nothing but its ns form is a program's file too
	CHECK(lisp::isProgram("(ns app.empty)"));
}

TEST_CASE("lisp namespaces: what the form refuses, naming the place") {
	const auto refused = [](const std::string& form, const std::string& message) {
		const std::string error = fileError(screenShowing(form, "1"));
		CHECK_MESSAGE(error.contains(message), error);
	};
	refused("(ns app (:use clojure.string))", "ns takes (:require ...) only");
	refused("(ns app (:require [clojure.string :refer :all]))", "name what you refer");
	refused("(ns app (:require [clojure.string :refer [shout]]))", "\"shout\" isn't a function of clojure.string");
	refused("(ns app (:require [clojure.string :as]))", ":as is missing its value");
	refused("(ns app (:require [clojure.string :rename {}]))", "takes :as alias and :refer");
	refused("(ns app (:require 12))", "(:require ...) takes namespaces");
	refused("(ns \"app\")", "ns takes the file's namespace first");
	refused("(ns clojure.string)", "clojure.string is the language's: name the file otherwise");
	// Once, and first: anywhere else it would be an entry of the program, which a revision could change
	refused("(defn f [] 1)\n(ns app)", "ns is read with the file an app loads from its folder: no entry of a program");
	refused("(ns app)\n(ns other)", "ns is read with the file an app loads from its folder");
	refused("(defn ns [] 1)", "\"ns\" is a file's namespace form, and can't be defined");
	// A :refer of the language's namespace takes only a word that already means the same function
	lisp::Interpreter interpreter;
	lisp::Machine& machine = interpreter.machine();
	machine.defineLanguageValue("clojure.string/shout", machine.languageGlobal("upper-case").value);
	machine.defineLanguageValue("shout", machine.languageGlobal("lower-case").value);
	try {
		static_cast<void>(interpreter.eval("(ns app (:require [clojure.string :refer [shout]]))", "repl", "repl"));
		FAIL("it was taken");
	} catch(const lisp::LispError& error) {
		CHECK(error.message() == "\"shout\" isn't a plain name here: write str/shout");
	}
}

TEST_CASE("lisp namespaces: taking the form out of the text keeps every other form's line and column") {
	const std::string text =
	    ";;; a file\n\n(ns app\n  (:require [clojure.string :as s]))\n\n\n\n\n(defn f [] (+ 1 ))))\n";
	try {
		static_cast<void>(lisp::readDocument(text, "index.screen.lisp"));
		FAIL("it read");
	} catch(const lisp::LispError& error) {
		CHECK(error.line() == 9);
		CHECK(error.column() == 19);
	}
	// The form is kept with the file's entries, first, with the comments above it; the rest is blanked in place
	const json document = lisp::readDocument(";; head\n(ns app) ; the app\n(defn f [] 1)\n", "index.screen.lisp");
	const json& entries = document["program"]["index.screen.lisp"];
	CHECK(entries.begin().key() == lisp::namespaceEntry);
	CHECK(entries[std::string(lisp::namespaceEntry)] == ";; head\n(ns app) ; the app");
	CHECK(entries["f"] == "(defn f [] 1)");
}

TEST_CASE("lisp namespaces: an alias is its file's; a qualified name is checked against the file it names") {
	Files files = board;
	files["parts/board.lisp"] = "(ns tally.parts.board (:require [tally.parts.cell :as cell]))\n"
	                            "(defn title [] (s/upper-case \"x\"))"; // s is index.screen.lisp's
	CHECK(refusal("alias", files).contains("no namespace \"s\" here"));
	files = board;
	files["index.screen.lisp"] =
	    screenShowing("(ns tally.index (:require [tally.parts.board :as board]))", "(board/named \"x\")");
	CHECK(refusal("qualified", files).contains("board/named: \"named\" isn't defined in parts/board.lisp"));
	files["index.screen.lisp"] =
	    screenShowing("(ns tally.index (:require [tally.parts.board :refer [named]]))", "(named \"x\")");
	CHECK(refusal("refer", files).contains(":refer named: \"named\" isn't defined in parts/board.lisp"));
	// A name is still one global: a file that doesn't require board reaches title by its plain name
	files["index.screen.lisp"] = screenShowing("(ns tally.index (:require tally.parts.board))", "(named \"x\")");
	CHECK(built("flat", files)["root"]["text"] == "the x");
	// A file required by a dotted name declares that name
	files = board;
	files["parts/cell.lisp"] = "(ns tally.parts.other)\n(defn named [n] n)";
	CHECK(refusal("declares", files).contains("tally.parts.cell is required, and parts/cell.lisp doesn't declare it"));
}

TEST_CASE("lisp namespaces: a dotted name is a path from the app's folder") {
	CHECK(lisp::namespacePath("flow.parts.board", "flow.index") == "parts/board.lisp");
	CHECK(lisp::namespacePath("flow.board", "flow.index") == "board.lisp");
	CHECK(lisp::namespacePath("board", "flow.index") == "board.lisp");
	CHECK(lisp::namespacePath("flow.parts.board", "flow.parts.cell") == "../parts/board.lisp");
	CHECK(lisp::namespacePath("flow.side-bar", "flow") == "side-bar.lisp");
}

TEST_CASE("lisp namespaces: a file two files require is read once; two that require each other, both once") {
	Files files = board;
	files["index.screen.lisp"] =
	    screenShowing("(ns tally.index (:require [tally.parts.board :as board] [tally.parts.cell :as cell]))",
		              "(cell/named (board/title))");
	CHECK(built("twice", files)["root"]["text"] == "the the board");
	files["parts/cell.lisp"] = "(ns tally.parts.cell (:require tally.parts.board))\n(defn named [n] (str \"the \" n))";
	CHECK(built("cycle", files)["root"]["text"] == "the the board");
	// Its entries are the program's once, under one name whichever file requires it
	affresco::Screen screen = affresco::Screen::fromApp(writeApp("once", files), {});
	const json& program = screen.versions().sourceDocument().at("program");
	CHECK(program.contains("parts/board.lisp"));
	CHECK(program.contains("parts/cell.lisp"));
	CHECK(program.size() == 3);
}

TEST_CASE("lisp namespaces: a required file is read from the app's folder only, by the loader's own check") {
	Files files = {{"index.screen.lisp", screenShowing("(ns app (:require [\"../x.lisp\" :as x]))", "1")}};
	CHECK(refusal("outside", files).contains("is outside the app's folder"));
	// A dotted name whose file says it is deeper than it is leaves the folder too
	files = {{"index.screen.lisp", screenShowing("(ns app.a.b.c (:require app.x))", "1")}};
	CHECK(refusal("dotted", files).contains("is outside the app's folder"));
}

TEST_CASE("lisp namespaces: a revision can't add, change or remove a file's ns form") {
	const json before = lisp::readDocument(screenShowing("(ns app (:require [clojure.string :as s]))", "1"), "i.lisp");
	const affresco::format::Rules rules;
	const auto refused = [&](const json& after) {
		try {
			rules.checkChange(before, after);
			return std::string("it was taken");
		} catch(const affresco::DocumentError& error) {
			return std::string(error.what());
		}
	};
	const std::string entry(lisp::namespaceEntry);
	json after = before;
	after["program"]["i.lisp"][entry] = "(ns app (:require [\"../../secret.lisp\"]))";
	CHECK(refused(after).contains("a revision can't add, change or remove one"));
	after = before;
	after["program"]["i.lisp"].erase(entry);
	CHECK(refused(after).contains("a revision can't add, change or remove one"));
	after = before;
	after["program"]["other.lisp"] = {{entry, "(ns other (:require x))"}};
	CHECK(refused(after).contains("a revision can't add, change or remove one"));
	// Nor as a definition's text, nor at the command line's evaluate
	CHECK_THROWS_WITH_AS(lisp::Interpreter().evaluate("(ns app)", "i.lisp", "app"),
	                     doctest::Contains("no entry of a program, so a revision can't add one"), lisp::LispError);
}

TEST_CASE("lisp namespaces: at the command line, ns gives the forms after it aliases, and loads no file") {
	CHECK(atCommandLine("(ns a (:require [clojure.string :as s]))\n(s/upper-case \"a\")") == "\"A\"");
	CHECK(atCommandLine("(ns a (:require [clojure.string :refer [upper-case]]))\n(upper-case \"a\")") == "\"A\"");
	CHECK(atCommandLine("(ns a (:require [app.board :as b]))") == "a file is required by the app that loads it");
	CHECK(atCommandLine("(ns a (:require [\"board.lisp\"]))") == "a file is required by the app that loads it");
	CHECK(atCommandLine("(s/upper-case \"a\")").contains("no namespace \"s\" here"));
	// ns is the language's word, as the notation lists it
	CHECK(std::ranges::find(affresco::notation::namespacesWords, "ns") != affresco::notation::namespacesWords.end());
}

TEST_CASE("lisp namespaces: the fixture app builds, through its aliases") {
	const std::filesystem::path folder = std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY).parent_path() / "tests" /
	                                     "unit" / "fixtures" / "namespaces";
	affresco::Screen screen = affresco::Screen::fromFile(folder / "index.screen.lisp");
	const json tree = lisp::programOf(screen.versions().sourceDocument().at("program")).build();
	CHECK(tree["root"]["children"][0]["text"] == "THE BOARD");
	CHECK(tree["root"]["children"][1]["children"][1]["text"] == "cell b");
}
