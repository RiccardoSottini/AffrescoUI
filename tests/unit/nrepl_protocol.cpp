/// @file
/// @brief The editor server's nREPL protocol (modules/net/src/nrepl): every operation's replies and statuses, the
/// sessions of a connection, and pairing: nothing of the app is read or evaluated before the person says yes.

#ifndef __EMSCRIPTEN__ // a page can't listen for an editor: the protocol isn't built there

#include "nrepl_fakes.hpp"

#include <doctest/doctest.h>

using namespace nrepl_test;

namespace {

/// @brief A request. @param op Its operation. @param id Its id. @return It, to add keys to.
Value::Dictionary request(std::string_view op, std::string_view id) {
	return Value::Dictionary{{"op", text(op)}, {"id", text(id)}};
}

/// @brief An eval request. @param id Its id. @param code Its text. @param session Its session, or "".
/// @return It.
Value evalRequest(std::string_view id, std::string_view code, std::string_view session = {}) {
	Value::Dictionary entries = request("eval", id);
	entries.emplace("code", text(code));
	if(!session.empty()) entries.emplace("session", text(session));
	return Value(std::move(entries));
}

/// @brief Checks that a request's replies end with exactly one `done`, in the last message, and echo its id.
/// @param replies Its replies.
void checkEnds(const std::vector<Value>& replies) {
	REQUIRE_FALSE(replies.empty());
	std::size_t dones = 0;
	for(const Value& reply : replies) {
		CHECK(reply.isDictionary());
		if(const Value* status = reply.find("status"); status != nullptr) CHECK(status->isList());
		if(hasStatus(reply, "done")) ++dones;
	}
	CHECK(dones == 1);
	CHECK(hasStatus(replies.back(), "done"));
}

/// @brief A connection the person paired, with a session.
struct Paired {
	FakeHost fake;               ///< The host.
	Connection connection{fake}; ///< The connection.
	std::string session;         ///< Its session.

	/// @brief Pairs it with an evaluation the person accepts.
	Paired() {
		session = connection.clone();
		connection.send(evalRequest("pair", "(+ 1 2)", session));
		fake.answer(true);
		fake.console.requests.clear();
	}
};

} // namespace

TEST_CASE("nrepl_protocol: clone makes a 32-hex-character session; close and ls-sessions") {
	FakeHost fake;
	Connection connection(fake);
	const std::vector<Value> cloned = connection.send(Value(request("clone", "1")));
	checkEnds(cloned);
	const std::string id = textOf(cloned.front(), "new-session");
	CHECK(id.size() == 32U);
	CHECK(id.find_first_not_of("0123456789abcdef") == std::string::npos);
	CHECK(textOf(cloned.front(), "id") == "1");
	CHECK(connection.clone() != id);

	const std::vector<Value> listed = connection.send(Value(request("ls-sessions", "2")));
	checkEnds(listed);
	REQUIRE(listed.front().find("sessions") != nullptr);
	CHECK(listed.front().find("sessions")->list().size() == 2U);

	Value::Dictionary close = request("close", "3");
	close.emplace("session", text(id));
	const std::vector<Value> closed = connection.send(Value(close));
	checkEnds(closed);
	CHECK(hasStatus(closed.front(), "session-closed"));
	CHECK(textOf(closed.front(), "session") == id);
	CHECK(connection.send(Value(request("ls-sessions", "4"))).front().find("sessions")->list().size() == 1U);
	CHECK(hasStatus(connection.send(evalRequest("5", "*ns*", id)).front(), "unknown-session"));
}

TEST_CASE("nrepl_protocol: clone stops at maxSessions") {
	FakeHost fake;
	fake.options.maxSessions = 2;
	Connection connection(fake);
	CHECK_FALSE(connection.clone().empty());
	CHECK_FALSE(connection.clone().empty());
	const std::vector<Value> third = connection.send(Value(request("clone", "x")));
	checkEnds(third);
	CHECK(statusOf(third.front()) == std::vector<std::string>{"error", "done"});
}

TEST_CASE("nrepl_protocol: describe lists the twelve operations as a dictionary, and no clojure version") {
	FakeHost fake;
	Connection connection(fake);
	const std::vector<Value> replies = connection.send(Value(request("describe", "d")));
	checkEnds(replies);
	const Value& reply = replies.front();
	REQUIRE(reply.find("ops") != nullptr);
	REQUIRE(reply.find("ops")->isDictionary());
	CHECK(reply.find("ops")->dictionary().size() == 12U);
	for(const char* op : {"clone", "close", "describe", "ls-sessions", "eval", "load-file", "interrupt", "completions",
	                      "complete", "lookup", "info", "eldoc"})
		CHECK(reply.find("ops")->find(op) != nullptr);
	const Value* versions = reply.find("versions");
	REQUIRE(versions != nullptr);
	CHECK(versions->find("clojure") == nullptr);
	REQUIRE(versions->find("affrescoui") != nullptr);
	CHECK(textOf(*versions->find("affrescoui"), "version-string") == "1.2.3");
	CHECK(reply.find("aux") != nullptr);
}

TEST_CASE("nrepl_protocol: an unknown operation and an unknown session") {
	FakeHost fake;
	Connection connection(fake);
	const std::vector<Value> unknown = connection.send(Value(request("stdin", "u")));
	checkEnds(unknown);
	CHECK(statusOf(unknown.front()) == std::vector<std::string>{"error", "unknown-op", "done"});
	CHECK(textOf(unknown.front(), "op") == "stdin");

	Connection other(fake);
	const std::string theirs = other.clone();
	const std::vector<Value> foreign = connection.send(evalRequest("f", "*ns*", theirs));
	checkEnds(foreign);
	CHECK(statusOf(foreign.front()) == std::vector<std::string>{"error", "unknown-session", "done"});
	CHECK(connection.send(Value(Value::List{})).size() == 2U); // not a dictionary: err, then error and done
}

TEST_CASE("nrepl_protocol: eval gives each value with ns, then done; an error gives err, ex, root-ex, eval-error") {
	Paired paired;
	paired.fake.console.answer = [](const affresco::ConsoleRequest&) {
		return affresco::ConsoleResult{.values = {"1", "2"}, .error = {}, .line = 0, .column = 0, .proposal = {}};
	};
	const std::vector<Value> values = paired.connection.send(evalRequest("e", "1 2", paired.session));
	checkEnds(values);
	REQUIRE(values.size() == 3U);
	CHECK(textOf(values[0], "value") == "1");
	CHECK(textOf(values[1], "value") == "2");
	CHECK(textOf(values[0], "ns") == "user");
	CHECK(textOf(values[0], "session") == paired.session);

	paired.fake.console.answer = [](const affresco::ConsoleRequest&) {
		return affresco::ConsoleResult{
		    .values = {}, .error = "no such name: frob", .line = 1, .column = 2, .proposal = {}};
	};
	const std::vector<Value> failed = paired.connection.send(evalRequest("x", "(frob)", paired.session));
	checkEnds(failed);
	REQUIRE(failed.size() == 3U);
	CHECK(textOf(failed[0], "err") == "no such name: frob (REPL:1:2)\n"); // its place in the buffer
	CHECK(textOf(failed[1], "ex") == "LispError");
	CHECK(textOf(failed[1], "root-ex") == "LispError");
	CHECK(hasStatus(failed[1], "eval-error"));
	CHECK(textOf(paired.connection.send(evalRequest("y", "*e", paired.session))[0], "value") ==
	      "\"no such name: frob\"");

	const std::vector<Value> empty = paired.connection.send(Value(request("eval", "n")));
	checkEnds(empty);
	CHECK(statusOf(empty.front()) == std::vector<std::string>{"error", "no-code", "done"});
}

TEST_CASE("nrepl_protocol: eval builds the console's request from file, line, column, ns, and ignores the rest") {
	Paired paired;
	Value::Dictionary entries = request("eval", "e");
	entries.emplace("code", text("(f)"));
	entries.emplace("session", text(paired.session));
	entries.emplace("file", text("/home/someone/app/src/main.lisp"));
	entries.emplace("line", number(7));
	entries.emplace("column", number(3));
	entries.emplace("ns", text("app.main"));
	entries.emplace("nrepl.middleware.print/print", text("cider.nrepl.pprint/pr"));
	checkEnds(paired.connection.send(Value(entries)));
	REQUIRE(paired.fake.console.requests.size() == 1U);
	const affresco::ConsoleRequest& sent = paired.fake.console.requests.front();
	CHECK(sent.text == "(f)");
	CHECK(sent.file == "main.lisp");
	CHECK(sent.line == 7U);
	CHECK(sent.column == 3U);
	CHECK(sent.space == "app.main");
	CHECK_FALSE(sent.wholeFile);
	CHECK(paired.connection.send(evalRequest("n", "*ns*", paired.session))[0].find("value")->string() == "app.main");

	checkEnds(paired.connection.send(evalRequest("d", "(g)", paired.session)));
	CHECK(paired.fake.console.requests.back().line == 1U); // absent: 1
	CHECK(paired.fake.console.requests.back().column == 1U);
}

TEST_CASE("nrepl_protocol: load-file evaluates the whole file, from file-path") {
	Paired paired;
	Value::Dictionary entries = request("load-file", "l");
	entries.emplace("session", text(paired.session));
	entries.emplace("file", text("(defn a [] 1)\n(defn b [] 2)\n"));
	entries.emplace("file-path", text("/home/someone/app/src/tools.lisp"));
	entries.emplace("file-name", text("tools.lisp"));
	checkEnds(paired.connection.send(Value(entries)));
	REQUIRE(paired.fake.console.requests.size() == 1U);
	CHECK(paired.fake.console.requests.front().wholeFile);
	CHECK(paired.fake.console.requests.front().file == "tools.lisp");

	Value::Dictionary mentions = request("load-file", "m"); // a file is the program's, whatever it names
	mentions.emplace("file", text("; cider.nrepl\n(ns x)"));
	checkEnds(paired.connection.send(Value(mentions)));
	CHECK(paired.fake.console.requests.size() == 2U);
}

TEST_CASE("nrepl_protocol: a screen without a program answers with an error") {
	Paired paired;
	paired.fake.console.program = false;
	const std::vector<Value> replies = paired.connection.send(evalRequest("e", "(f)", paired.session));
	checkEnds(replies);
	CHECK(hasStatus(replies.back(), "error"));
	CHECK(paired.fake.console.requests.empty());
}

TEST_CASE("nrepl_protocol: completions, complete, lookup, info and eldoc") {
	Paired paired;
	paired.fake.console.known = {
	    {.name = "add-item", .kind = "defn", .doc = "Adds.", .file = "main.lisp", .line = 4, .arguments = {"[a b]"}},
	    {.name = "add-ons", .kind = "def", .doc = "", .file = "", .line = 0, .arguments = {}},
	    {.name = "and", .kind = "macro", .doc = "", .file = "", .line = 0, .arguments = {"[& forms]"}}};
	Value::Dictionary completions = request("completions", "c");
	completions.emplace("prefix", text("a"));
	const std::vector<Value> listed = paired.connection.send(Value(completions));
	checkEnds(listed);
	const Value::List& candidates = listed.front().find("completions")->list();
	REQUIRE(candidates.size() == 3U);
	CHECK(textOf(candidates[0], "candidate") == "add-item");
	CHECK(textOf(candidates[0], "type") == "function");
	CHECK(textOf(candidates[1], "type") == "var");
	CHECK(textOf(candidates[2], "type") == "macro");
	Value::Dictionary complete = request("complete", "k");
	complete.emplace("symbol", text("add-"));
	CHECK(paired.connection.send(Value(complete)).front().find("completions")->list().size() == 2U);

	Value::Dictionary lookup = request("lookup", "l");
	lookup.emplace("sym", text("add-item"));
	const std::vector<Value> looked = paired.connection.send(Value(lookup));
	checkEnds(looked);
	const Value* info = looked.front().find("info");
	REQUIRE(info != nullptr);
	CHECK(textOf(*info, "name") == "add-item");
	CHECK(textOf(*info, "doc") == "Adds.");
	CHECK(textOf(*info, "arglists-str") == "[a b]");
	CHECK(textOf(*info, "file") == "/app/main.lisp");
	CHECK(info->find("line")->integer() == 4);
	lookup["sym"] = text("add-ons");
	CHECK(paired.connection.send(Value(lookup)).front().find("info")->find("file") == nullptr); // no file, no line
	lookup["sym"] = text("nothing");
	const std::vector<Value> missing = paired.connection.send(Value(lookup));
	CHECK(missing.front().find("info")->dictionary().empty());
	CHECK(statusOf(missing.front()) == std::vector<std::string>{"done", "no-info"});

	Value::Dictionary infoRequest = request("info", "i");
	infoRequest.emplace("symbol", text("add-item"));
	const std::vector<Value> flat = paired.connection.send(Value(infoRequest));
	checkEnds(flat);
	CHECK(textOf(flat.front(), "name") == "add-item");
	CHECK(textOf(flat.front(), "ns") == "user");
	infoRequest["symbol"] = text("nothing");
	CHECK(statusOf(paired.connection.send(Value(infoRequest)).front()) == std::vector<std::string>{"done", "no-info"});

	Value::Dictionary eldoc = request("eldoc", "o");
	eldoc.emplace("sym", text("add-item"));
	const std::vector<Value> described = paired.connection.send(Value(eldoc));
	checkEnds(described);
	CHECK(textOf(described.front(), "type") == "function");
	CHECK(textOf(described.front(), "docstring") == "Adds.");
	const Value::List& lists = described.front().find("eldoc")->list();
	REQUIRE(lists.size() == 1U);
	REQUIRE(lists[0].list().size() == 2U);
	CHECK(lists[0].list()[1].string() == "b");
	eldoc["sym"] = text("add-ons");
	CHECK(textOf(paired.connection.send(Value(eldoc)).front(), "type") == "variable");
	eldoc["sym"] = text("nothing");
	CHECK(statusOf(paired.connection.send(Value(eldoc)).front()) == std::vector<std::string>{"done", "no-eldoc"});
}

TEST_CASE("nrepl_protocol: completions stop at 200") {
	Paired paired;
	for(int index = 0; index < 300; ++index)
		paired.fake.console.known.push_back({.name = "n" + std::to_string(1000 + index),
		                                     .kind = "def",
		                                     .doc = {},
		                                     .file = {},
		                                     .line = 0,
		                                     .arguments = {}});
	Value::Dictionary completions = request("completions", "c");
	completions.emplace("prefix", text("n"));
	CHECK(paired.connection.send(Value(completions)).front().find("completions")->list().size() == 200U);
}

TEST_CASE("nrepl_protocol: two sessions keep their values and namespaces apart; a request without one has its own") {
	Paired paired;
	const std::string other = paired.connection.clone();
	checkEnds(paired.connection.send(evalRequest("1", "10", paired.session)));
	checkEnds(paired.connection.send(evalRequest("2", "20", other)));
	checkEnds(paired.connection.send(evalRequest("3", "(ns elsewhere)", other)));
	CHECK(textOf(paired.connection.send(evalRequest("4", "*1", paired.session))[0], "value") == "10");
	CHECK(textOf(paired.connection.send(evalRequest("5", "*1", other))[0], "value") == "20");
	CHECK(textOf(paired.connection.send(evalRequest("6", "*ns*", paired.session))[0], "value") == "user");
	CHECK(textOf(paired.connection.send(evalRequest("7", "*ns*", other))[0], "value") == "elsewhere");
	CHECK(textOf(paired.connection.send(evalRequest("8", "*1"))[0], "value") == "nil");
	checkEnds(paired.connection.send(evalRequest("9", "30")));
	CHECK(textOf(paired.connection.send(evalRequest("10", "*1"))[0], "value") == "30");
	CHECK(paired.connection.send(evalRequest("11", "*1"))[0].find("session") == nullptr);
}

TEST_CASE("nrepl_protocol: an unpaired connection reaches nothing of the app") {
	FakeHost fake;
	Connection connection(fake);
	const std::string session = connection.clone();
	CHECK(connection.send(evalRequest("e", "(launch)", session)).empty());
	Value::Dictionary completions = request("completions", "c");
	completions.emplace("prefix", text("a"));
	CHECK(connection.send(Value(completions)).empty());
	Value::Dictionary lookup = request("lookup", "l");
	lookup.emplace("sym", text("a"));
	CHECK(connection.send(Value(lookup)).empty());
	CHECK(fake.console.requests.empty());
	CHECK(fake.console.lookups == 0U);
	REQUIRE(fake.questions.size() == 1U); // one question for all three
	CHECK(fake.questions.front() == "(launch)");
	CHECK(fake.shared.asking);
	// the clients' own text that needs no app is answered before pairing, and asks nothing
	Connection fresh(fake);
	CHECK(textOf(fresh.send(evalRequest("n", "*ns*"))[0], "value") == "user");
	CHECK(fake.questions.size() == 1U);
}

TEST_CASE("nrepl_protocol: a completion asks with a sentence, not code") {
	FakeHost fake;
	Connection connection(fake);
	Value::Dictionary completions = request("completions", "c");
	completions.emplace("prefix", text("a"));
	connection.send(Value(completions));
	REQUIRE(fake.questions.size() == 1U);
	CHECK(fake.questions.front() == "(nothing to run yet: it asked what this app defines)");
}

TEST_CASE("nrepl_protocol: yes runs what waited, in order, and later requests run at once") {
	FakeHost fake;
	Connection connection(fake);
	connection.send(evalRequest("1", "(first)"));
	connection.send(evalRequest("2", "*ns*"));
	connection.send(evalRequest("3", "(second)"));
	CHECK(connection.link.sent.empty());
	fake.answer(true);
	REQUIRE(fake.console.requests.size() == 2U);
	CHECK(fake.console.requests[0].text == "(first)");
	CHECK(fake.console.requests[1].text == "(second)");
	checkEnds(connection.repliesTo("1"));
	checkEnds(connection.repliesTo("2"));
	checkEnds(connection.repliesTo("3"));
	CHECK(textOf(connection.link.sent.front(), "id") == "1");
	CHECK_FALSE(fake.shared.asking);
	checkEnds(connection.send(evalRequest("4", "(third)")));
	CHECK(fake.questions.size() == 1U);
}

TEST_CASE("nrepl_protocol: no ends what waited, closes the connection and counts a refusal") {
	FakeHost fake;
	Connection connection(fake);
	connection.send(evalRequest("1", "(first)"));
	connection.send(evalRequest("2", "(second)"));
	fake.answer(false);
	CHECK(connection.link.closed);
	CHECK(fake.console.requests.empty());
	CHECK(fake.shared.refusals == 1U);
	for(const char* id : {"1", "2"}) {
		const std::vector<Value> replies = connection.repliesTo(id);
		checkEnds(replies);
		CHECK(textOf(replies.front(), "err") == "the person refused this connection\n");
		CHECK(statusOf(replies.back()) == std::vector<std::string>{"error", "not-paired", "done"});
	}
	CHECK(connection.send(evalRequest("3", "(again)")).empty()); // closed: nothing more is answered
}

TEST_CASE("nrepl_protocol: three refusals stop the asking, for every connection") {
	FakeHost fake;
	for(int round = 0; round < 3; ++round) {
		Connection connection(fake);
		connection.send(evalRequest("e", "(f)"));
		fake.answer(false);
	}
	CHECK(fake.shared.refusals == 3U);
	CHECK(fake.questions.size() == 3U);
	Connection late(fake);
	const std::vector<Value> replies = late.send(evalRequest("e", "(f)"));
	checkEnds(replies);
	CHECK(hasStatus(replies.back(), "not-paired"));
	CHECK(late.link.closed);
	CHECK(fake.questions.size() == 3U);
	CHECK(fake.console.requests.empty());
}

TEST_CASE("nrepl_protocol: a host that can't ask ends what waited with its reason, and counts nothing") {
	FakeHost fake;
	fake.cannotAsk = "no window to ask in";
	Connection connection(fake);
	const std::vector<Value> replies = connection.send(evalRequest("e", "(f)"));
	checkEnds(replies);
	CHECK(textOf(replies.front(), "err") == "no window to ask in\n");
	CHECK(statusOf(replies.back()) == std::vector<std::string>{"error", "done"});
	CHECK_FALSE(connection.link.closed);
	CHECK(fake.shared.refusals == 0U);
	CHECK_FALSE(fake.shared.asking);
	fake.cannotAsk.reset();
	connection.send(evalRequest("g", "(g)"));
	CHECK(fake.questions.size() == 1U); // it asks again
}

TEST_CASE("nrepl_protocol: one question at a time; the next connection waits its turn") {
	FakeHost fake;
	Connection first(fake);
	Connection second(fake);
	first.send(evalRequest("a", "(a)"));
	second.send(evalRequest("b", "(b)"));
	REQUIRE(fake.questions.size() == 1U);
	fake.answer(true);
	REQUIRE(fake.questions.size() == 2U); // the second asks once the first is answered
	CHECK(fake.questions.back() == "(b)");
	fake.answer(true);
	CHECK(fake.console.requests.size() == 2U);
}

TEST_CASE("nrepl_protocol: a connection that goes takes its question away, or gives up its turn") {
	FakeHost fake;
	auto first = std::make_unique<Connection>(fake);
	auto second = std::make_unique<Connection>(fake);
	first->send(evalRequest("a", "(a)"));
	second->send(evalRequest("b", "(b)"));
	second.reset(); // waiting its turn: no question to cancel
	CHECK(fake.cancels == 0U);
	first.reset();
	CHECK(fake.cancels == 1U);
	CHECK_FALSE(fake.shared.asking);
	CHECK(fake.questions.size() == 1U);
	CHECK(fake.shared.refusals == 0U);
	fake.answers.front()(true); // a late answer to a cancelled question does nothing
	CHECK(fake.console.requests.empty());
}

TEST_CASE("nrepl_protocol: a waiting request counts whole against the queue's limit, keys nothing reads too") {
	FakeHost fake;
	fake.options.maxQueuedBytes = std::size_t{1} << 20;
	Connection connection(fake);
	connection.send(evalRequest("first", "(f)")); // waits for the person, who doesn't answer
	REQUIRE(fake.questions.size() == 1U);
	const std::string padding(std::size_t{400} << 10, 'x');
	for(const std::string_view id : {"2", "3", "4"}) {
		Value::Dictionary padded = request("eval", id);
		padded.emplace("code", text("(g)"));
		padded.emplace("padding", text(padding));
		connection.send(Value(std::move(padded)));
	}
	CHECK(connection.repliesTo("2").empty());
	CHECK(connection.repliesTo("3").empty());
	const std::vector<Value> refused = connection.repliesTo("4");
	REQUIRE_FALSE(refused.empty());
	CHECK(textOf(refused.front(), "err").starts_with("too many requests wait"));
	checkEnds(refused);
	fake.answer(true);
	CHECK(fake.console.requests.size() == 3U);
}

TEST_CASE("nrepl_protocol: an interrupt cancels a queued evaluation and the question that waits") {
	FakeHost fake;
	Connection connection(fake);
	const std::string session = connection.clone();
	connection.send(evalRequest("e", "(f)", session));
	Value::Dictionary interrupt = request("interrupt", "i");
	interrupt.emplace("session", text(session));
	const std::vector<Value> replies = connection.send(Value(interrupt));
	REQUIRE(replies.size() == 2U);
	CHECK(textOf(replies[0], "id") == "e");
	CHECK(statusOf(replies[0]) == std::vector<std::string>{"interrupted", "done"});
	CHECK(textOf(replies[1], "id") == "i");
	CHECK(statusOf(replies[1]) == std::vector<std::string>{"done"});
	CHECK(fake.cancels == 1U);
	CHECK_FALSE(fake.shared.asking);
	CHECK(fake.shared.refusals == 0U);
	CHECK(fake.console.requests.empty());

	const std::vector<Value> idle = connection.send(Value(interrupt));
	REQUIRE(idle.size() == 1U);
	CHECK(statusOf(idle[0]) == std::vector<std::string>{"session-idle", "done"});
}

TEST_CASE("nrepl_protocol: interrupting the request the question shows asks again, with the next one's text") {
	FakeHost fake;
	Connection connection(fake);
	connection.send(evalRequest("shown", "(shown)"));
	connection.send(evalRequest("next", "(unseen)"));
	REQUIRE(fake.questions == std::vector<std::string>{"(shown)"});
	Value::Dictionary interrupt = request("interrupt", "i");
	interrupt.emplace("interrupt-id", text("shown"));
	connection.send(Value(std::move(interrupt)));
	CHECK(fake.cancels == 1U);
	REQUIRE(fake.questions.size() == 2U);
	CHECK(fake.questions.back() == "(unseen)"); // a yes runs only what the person saw
	fake.answer(true);
	REQUIRE(fake.console.requests.size() == 1U);
	CHECK(fake.console.requests[0].text == "(unseen)");
	checkEnds(connection.repliesTo("next"));
}

TEST_CASE("nrepl_protocol: interrupting a request the question doesn't show leaves the question as it is") {
	FakeHost fake;
	Connection connection(fake);
	connection.send(evalRequest("shown", "(shown)"));
	connection.send(evalRequest("later", "(later)"));
	Value::Dictionary interrupt = request("interrupt", "i");
	interrupt.emplace("interrupt-id", text("later"));
	connection.send(Value(std::move(interrupt)));
	CHECK(fake.cancels == 0U);
	CHECK(fake.questions == std::vector<std::string>{"(shown)"});
	fake.answer(true);
	REQUIRE(fake.console.requests.size() == 1U);
	CHECK(fake.console.requests[0].text == "(shown)");
}

TEST_CASE("nrepl_protocol: an interrupt with an interrupt-id takes only that request") {
	FakeHost fake;
	Connection connection(fake);
	connection.send(evalRequest("1", "(one)"));
	connection.send(evalRequest("2", "(two)"));
	Value::Dictionary interrupt = request("interrupt", "i");
	interrupt.emplace("interrupt-id", text("2"));
	CHECK(statusOf(connection.send(Value(interrupt)).back()) == std::vector<std::string>{"done"});
	CHECK(fake.cancels == 0U); // (one) still needs the question
	fake.answer(true);
	REQUIRE(fake.console.requests.size() == 1U);
	CHECK(fake.console.requests.front().text == "(one)");
	checkEnds(connection.repliesTo("2"));
}

TEST_CASE("nrepl_protocol: every reply echoes the request's id and session") {
	Paired paired;
	for(const Value& message : paired.connection.send(evalRequest("z", "(f)", paired.session))) {
		CHECK(textOf(message, "id") == "z");
		CHECK(textOf(message, "session") == paired.session);
	}
}

#endif
