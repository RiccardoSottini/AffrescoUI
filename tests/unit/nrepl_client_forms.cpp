/// @file
/// @brief The text nREPL clients evaluate for themselves (modules/net/src/nrepl/ClientForms.hpp): each kind
/// recognised with and without leading comments and white space, answered without the program, and what must not be
/// recognised.

#ifndef __EMSCRIPTEN__ // a page can't listen for an editor: the protocol isn't built there

#include "nrepl/ClientForms.hpp" // modules/net/src
#include "nrepl_fakes.hpp"

#include <doctest/doctest.h>

#include <array>

using namespace nrepl_test;
using affresco::net::nrepl::ClientFormKind;
using affresco::net::nrepl::recogniseClientForm;

namespace {

/// @brief The same text, bare and behind leading comments and white space. @param text It. @return Both.
std::array<std::string, 3> spellings(std::string_view text) {
	return {std::string(text), "  \n\t" + std::string(text) + "  \n",
	        "; the client's own\n ;; again\n" + std::string(text) + " ; trailing"};
}

/// @brief An eval request. @param id Its id. @param code Its text. @return It.
Value evalRequest(std::string_view id, std::string_view code) {
	return dictionary({{"op", text("eval")}, {"id", text(id)}, {"code", text(code)}});
}

} // namespace

TEST_CASE("nrepl_client_forms: each kind is recognised, with and without leading comments and white space") {
	for(const std::string& spelled : spellings("*ns*"))
		CHECK(recogniseClientForm(spelled).kind == ClientFormKind::CurrentNamespace);
	for(const std::string& spelled : spellings("*2")) {
		CHECK(recogniseClientForm(spelled).kind == ClientFormKind::RecentValue);
		CHECK(recogniseClientForm(spelled).back == 2U);
	}
	for(const std::string& spelled : spellings("*e"))
		CHECK(recogniseClientForm(spelled).kind == ClientFormKind::LastError);
	for(const std::string& spelled : spellings("#?(:clj 'clj :cljs 'cljs :default 'unknown)"))
		CHECK(recogniseClientForm(spelled).kind == ClientFormKind::ReaderConditional);
	for(const std::string& spelled : spellings("(ns proof.app\n  (:require [clojure.string :as s]))")) {
		CHECK(recogniseClientForm(spelled).kind == ClientFormKind::SwitchNamespace);
		CHECK(recogniseClientForm(spelled).name == "proof.app");
	}
	for(const std::string& spelled : spellings("(in-ns 'other)")) CHECK(recogniseClientForm(spelled).name == "other");
	for(const char* form : {"(require 'clojure.repl)", "(use 'clojure.test)", "(clojure.core/require 'a.b)"})
		for(const std::string& spelled : spellings(form))
			CHECK(recogniseClientForm(spelled).kind == ClientFormKind::Require);
	for(const char* form : {"(doc add-item)", "(clojure.repl/doc add-item)"})
		for(const std::string& spelled : spellings(form)) {
			CHECK(recogniseClientForm(spelled).kind == ClientFormKind::Documentation);
			CHECK(recogniseClientForm(spelled).name == "add-item");
		}
	for(const char* form : {"(when-let [requires (resolve 'clojure.main/repl-requires)]\n"
	                        "  (clojure.core/apply clojure.core/require @requires))",
	                        "(create-ns 'conjure.internal)\n(intern 'conjure.internal 'initial-ns (symbol (str *ns*)))",
	                        "(require 'cider.nrepl.pprint)"})
		for(const std::string& spelled : spellings(form))
			CHECK(recogniseClientForm(spelled).kind == ClientFormKind::ClientSetup);
}

TEST_CASE("nrepl_client_forms: ten texts that are the program's, not a client's") {
	for(const char* text :
	    {"(defn ns-helper [] 1)", "(nsx foo)", "(required 1)", "(+ 1 2) (ns x)", "(clojure.string/upper-case \"a\")",
	     "*ns* *ns*", "(ns)", "\"(ns x)\"", "*4", "#?@(:clj [1])", "; only a comment\n", "(ns x", "(doc)"})
		CHECK_MESSAGE(recogniseClientForm(text).kind == ClientFormKind::None, text);
}

TEST_CASE("nrepl_client_forms: an accepted limit: a text that only quotes a client's namespace is answered nil") {
	// The scanner reads names, not strings' meaning: "conjure.internal" inside a string still reads as a client's
	// own setup. Nothing runs, and the answer is nil (modules/net/README.md says so too).
	CHECK(recogniseClientForm("(println \"conjure.internal\")").kind == ClientFormKind::ClientSetup);
}

TEST_CASE("nrepl_client_forms: answered before pairing, without the program, from the session") {
	FakeHost fake;
	Connection connection(fake);
	const std::string session = connection.clone();
	const auto value = [&](std::string_view code) {
		Value::Dictionary entries{
		    {"op", text("eval")}, {"id", text("v")}, {"code", text(code)}, {"session", text(session)}};
		const std::vector<Value> replies = connection.send(Value(std::move(entries)));
		REQUIRE(replies.size() == 2U);
		CHECK(statusOf(replies.back()) == std::vector<std::string>{"done"});
		return textOf(replies.front(), "value");
	};
	CHECK(value("*ns*") == "user");
	CHECK(value("*1") == "nil");
	CHECK(value("*e") == "nil");
	CHECK(value("#?(:clj 'clj :default 'unknown)") == "unknown");
	CHECK(value("(ns proof.forms)") == "nil");
	CHECK(value("*ns*") == "proof.forms");
	CHECK(value("; back\n(in-ns 'user)") == "nil");
	CHECK(value("*ns*") == "user");
	CHECK(value("(require '[clojure.string :as s])") == "nil");
	CHECK(value("(create-ns 'conjure.internal)") == "nil");
	CHECK(fake.questions.empty());
	CHECK(fake.console.requests.empty());
	CHECK(fake.console.lookups == 0U);
}

TEST_CASE("nrepl_client_forms: doc needs the app: asked, then answered as out and nil") {
	FakeHost fake;
	fake.console.known = {
	    {.name = "add-item", .kind = "defn", .doc = "Adds an item.", .file = "", .line = 0, .arguments = {"[item]"}}};
	Connection connection(fake);
	CHECK(connection.send(evalRequest("d", "(clojure.repl/doc add-item)")).empty());
	CHECK(fake.console.lookups == 0U);
	REQUIRE(fake.questions.size() == 1U);
	fake.answer(true);
	const std::vector<Value> replies = connection.repliesTo("d");
	REQUIRE(replies.size() == 3U);
	CHECK(textOf(replies[0], "out") == "-------------------------\nadd-item\n[item]\n  Adds an item.\n");
	CHECK(textOf(replies[1], "value") == "nil");
	CHECK(statusOf(replies[2]) == std::vector<std::string>{"done"});
	CHECK(fake.console.requests.empty());
}

TEST_CASE("nrepl_client_forms: what isn't recognised goes to the console and fails there") {
	FakeHost fake;
	fake.console.answer = [](const affresco::ConsoleRequest&) {
		return affresco::ConsoleResult{
		    .values = {}, .error = "no such name: clojure.string/upper-case", .line = 0, .column = 0, .proposal = {}};
	};
	Connection connection(fake);
	connection.send(evalRequest("u", "(clojure.string/upper-case \"a\")"));
	fake.answer(true);
	REQUIRE(fake.console.requests.size() == 1U);
	const std::vector<Value> replies = connection.repliesTo("u");
	REQUIRE_FALSE(replies.empty());
	CHECK(textOf(replies.front(), "err") == "no such name: clojure.string/upper-case\n");
	CHECK(textOf(connection.send(evalRequest("e", "*e")).front(), "value") ==
	      "\"no such name: clojure.string/upper-case\"");
}

#endif
