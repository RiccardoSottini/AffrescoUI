/// @file
/// @brief makeEditorPeer() (EditorLink.hpp): the protocol's side of a connection. Session operations are answered at
/// once; every other request joins the connection's queue and runs in order, one at a time. A request that needs the
/// app waits, with every request after it, until the person paired the connection (Pairing.hpp); the clients' own
/// text that needs no app (ClientForms.hpp) is answered without it.

#include "ClientForms.hpp"
#include "EditorLink.hpp"
#include "EditorOps.hpp"
#include "EditorSessions.hpp"
#include "Pairing.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace affresco::net {

namespace {

using bencode::Value;
using nrepl::textOf;

/// @brief What the person sees for a request that only reads the app's names.
constexpr std::string_view readingQuestion = "(nothing to run yet: it asked what this app defines)";
/// @brief What a waiting request is told when the person refuses the connection.
constexpr std::string_view refusedText = "the person refused this connection";
/// @brief The most requests one connection may have waiting.
constexpr std::size_t maxWaiting = 64;

/// @brief A request in the queue, and the session it named ("" for the connection's nameless one).
struct Waiting {
	Value request;         ///< The request.
	std::string session;   ///< Its session's id, or "".
	std::size_t bytes = 0; ///< The size of its text, for the queue's limit.
};

/// @brief One connection's protocol: its sessions, its queue and its pairing.
class ProtocolPeer final : public EditorPeer {
public:
	/// @brief An unpaired connection.
	/// @param link Where answers go. @param host The program and the question. @param options The limits.
	/// @param shared What the server's connections share. All four outlive the peer.
	ProtocolPeer(EditorLink& link, EditorHost& host, const EditorServerOptions& options, EditorShared& shared)
	    : link_(link), host_(host), options_(options), sessions_(options.maxSessions),
	      pairing_(host, options, shared, [this](const nrepl::PairingOutcome& outcome) { decided(outcome); }) {}

	/// @brief Takes one message. @param message It.
	void receive(const Value& message) override;
	/// @brief The connection went: nothing waiting runs, and its question is taken away.
	void closed() override {
		closed_ = true;
		queue_.clear();
		pairing_.cancel();
	}

private:
	/// @brief Answers clone, close, describe, ls-sessions and interrupt, which never wait.
	/// @param op The operation. @param request The request. @param session Its session's id, or "".
	/// @return true if it was one of them.
	bool answerAtOnce(std::string_view op, const Value& request, const std::string& session);
	/// @brief Answers `clone`. @param request It. @param session The session it names, or "".
	void clone(const Value& request, const std::string& session);
	/// @brief Answers `interrupt`. @param request It. @param session The session it names, or "".
	void interrupt(const Value& request, const std::string& session);
	/// @brief Queues a request, within the queue's limits. @param request It. @param session Its session, or "".
	void enqueue(const Value& request, std::string session);
	/// @brief Runs the queue in order, until it is empty or a request waits for the person.
	void drain();
	/// @brief Runs one request from the queue. @param waiting It.
	void run(const Waiting& waiting);
	/// @brief Answers `eval` or `load-file`. @param request It. @param session Its session.
	void evaluate(const Value& request, nrepl::EditorSession& session);
	/// @brief Answers a client's own text. @param request It. @param form What it asks. @param session Its session.
	void answerClientForm(const Value& request, const nrepl::ClientForm& form, nrepl::EditorSession& session);
	/// @brief Sends one value and `done`. @param request It. @param value The value, printed as Lisp.
	/// @param session Its session.
	void sendValue(const Value& request, std::string_view value, const nrepl::EditorSession& session);
	/// @brief Takes the end of the pairing question. @param outcome How it ended.
	void decided(const nrepl::PairingOutcome& outcome);
	/// @brief Ends every waiting request in error. @param why Its `err`. @param more A status word, or "".
	void failWaiting(std::string_view why, std::string_view more);
	/// @brief Tells whether the program has something to evaluate into. @return true if it has.
	[[nodiscard]] bool hasProgram() const { return host_.console != nullptr && host_.console->hasProgram(); }

	EditorLink& link_;                   ///< Where answers go.
	EditorHost& host_;                   ///< The program and the question.
	const EditorServerOptions& options_; ///< The limits.
	nrepl::EditorSessions sessions_;     ///< The connection's sessions.
	std::deque<Waiting> queue_;          ///< The requests waiting to run, in order.
	std::size_t queuedBytes_ = 0;        ///< The size of their texts.
	nrepl::Pairing pairing_;             ///< The person's question.
	bool closed_ = false;                ///< The connection went, or is closing.
	bool running_ = false;               ///< drain() is running: a nested call leaves the work to it.
	/// Alive while the peer is: an outcome may close the connection, and the wire may destroy the peer inside that
	/// call (shared, so a weak pointer can see it go).
	std::shared_ptr<char> alive_ = std::make_shared<char>();
};

/// @brief Tells whether a request needs the app, and so a paired connection. @param request It. @return true if so.
bool needsApp(const Value& request) {
	const std::string_view op = textOf(request, "op");
	if(op == "eval") {
		const std::string_view code = textOf(request, "code");
		return !code.empty() && nrepl::needsProgram(nrepl::recogniseClientForm(code).kind);
	}
	if(op == "load-file") return !textOf(request, "file").empty();
	if(op == "completions" || op == "complete") return true;
	return !textOf(request, "sym", "symbol").empty(); // lookup, info, eldoc
}

/// @brief What the person is shown for a request: its text, or a sentence for a request that only reads names.
/// @param request It. @return The text.
std::string questionFor(const Value& request) {
	const std::string_view op = textOf(request, "op");
	if(op == "eval") return std::string(textOf(request, "code"));
	if(op == "load-file") return std::string(textOf(request, "file"));
	return std::string(readingQuestion);
}

/// @brief A request's line or column. @param request It. @param key `line` or `column`. @return It, 1 when absent.
std::size_t positionOf(const Value& request, std::string_view key) {
	const Value* found = request.find(key);
	if(found == nullptr || !found->isInteger() || found->integer() < 1) return 1;
	return static_cast<std::size_t>(found->integer());
}

void ProtocolPeer::receive(const Value& message) {
	if(closed_) return;
	if(!message.isDictionary()) {
		nrepl::sendFailure(link_, message, "a request is a dictionary", "");
		return;
	}
	std::string session;
	if(const Value* named = message.find("session"); named != nullptr) {
		if(!named->isString() || sessions_.find(named->string()) == nullptr) {
			nrepl::sendStatus(link_, message, {"error", "unknown-session", "done"});
			return;
		}
		session = named->string();
	}
	const std::string_view op = textOf(message, "op");
	if(answerAtOnce(op, message, session)) return;
	if(std::ranges::find(nrepl::operationNames(), op) == nrepl::operationNames().end()) {
		Value::Dictionary reply = nrepl::replyTo(message);
		if(const Value* asked = message.find("op"); asked != nullptr) reply.emplace("op", *asked);
		reply.emplace("status", nrepl::statusOf({"error", "unknown-op", "done"}));
		link_.send(Value(std::move(reply)));
		return;
	}
	enqueue(message, std::move(session));
	drain();
}

bool ProtocolPeer::answerAtOnce(std::string_view op, const Value& request, const std::string& session) {
	if(op == "clone") {
		clone(request, session);
	} else if(op == "close") {
		if(!session.empty()) static_cast<void>(sessions_.close(session));
		nrepl::sendStatus(link_, request, {"done", "session-closed"});
	} else if(op == "describe") {
		nrepl::answerDescribe(link_, request, options_.version);
	} else if(op == "ls-sessions") {
		Value::List ids;
		for(std::string& id : sessions_.ids()) ids.emplace_back(std::move(id));
		Value::Dictionary reply = nrepl::replyTo(request);
		reply.emplace("sessions", Value(std::move(ids)));
		reply.emplace("status", nrepl::statusOf({"done"}));
		link_.send(Value(std::move(reply)));
	} else if(op == "interrupt") {
		interrupt(request, session);
	} else {
		return false;
	}
	return true;
}

void ProtocolPeer::clone(const Value& request, const std::string& session) {
	const nrepl::EditorSession* from = session.empty() ? nullptr : sessions_.find(session);
	const std::optional<std::string> id = sessions_.clone(from);
	if(!id) {
		nrepl::sendStatus(link_, request, {"error", "done"});
		return;
	}
	Value::Dictionary reply = nrepl::replyTo(request);
	reply.emplace("new-session", Value(*id));
	reply.emplace("status", nrepl::statusOf({"done"}));
	link_.send(Value(std::move(reply)));
}

void ProtocolPeer::interrupt(const Value& request, const std::string& session) {
	const std::string_view only = textOf(request, "interrupt-id");
	std::size_t interrupted = 0;
	for(auto waiting = queue_.begin(); waiting != queue_.end();) {
		if(waiting->session != session || (!only.empty() && textOf(waiting->request, "id") != only)) {
			++waiting;
			continue;
		}
		nrepl::sendStatus(link_, waiting->request, {"interrupted", "done"});
		queuedBytes_ -= waiting->bytes;
		waiting = queue_.erase(waiting);
		++interrupted;
	}
	// The question was for what waited: once nothing waiting needs the app, it is taken away unanswered
	if(interrupted > 0 && pairing_.pending() &&
	   std::ranges::none_of(queue_, [](const Waiting& waiting) { return needsApp(waiting.request); }))
		pairing_.cancel();
	if(interrupted > 0) nrepl::sendStatus(link_, request, {"done"});
	else nrepl::sendStatus(link_, request, {"session-idle", "done"});
	drain(); // what waited behind a cancelled question may need no app
}

void ProtocolPeer::enqueue(const Value& request, std::string session) {
	const std::size_t bytes = textOf(request, "code").size() + textOf(request, "file").size();
	if(queue_.size() >= maxWaiting || queuedBytes_ + bytes > options_.maxQueuedBytes) {
		nrepl::sendFailure(link_, request, "too many requests wait on this connection", "");
		return;
	}
	queuedBytes_ += bytes;
	queue_.push_back({.request = request, .session = std::move(session), .bytes = bytes});
}

void ProtocolPeer::drain() {
	if(running_) return;
	running_ = true;
	const std::weak_ptr<char> alive = alive_;
	while(!closed_ && !queue_.empty()) {
		if(needsApp(queue_.front().request) && !pairing_.paired()) {
			if(pairing_.pending()) break;
			running_ = false; // the outcome may run the queue itself, or close the connection
			pairing_.ask(questionFor(queue_.front().request));
			if(alive.expired() || !pairing_.paired()) return;
			running_ = true;
			continue;
		}
		const Waiting waiting = std::move(queue_.front());
		queue_.pop_front();
		queuedBytes_ -= waiting.bytes;
		run(waiting);
	}
	running_ = false;
}

void ProtocolPeer::run(const Waiting& waiting) {
	nrepl::EditorSession* session = waiting.session.empty() ? &sessions_.nameless() : sessions_.find(waiting.session);
	if(session == nullptr) { // closed while the request waited
		nrepl::sendStatus(link_, waiting.request, {"error", "unknown-session", "done"});
		return;
	}
	const std::string_view op = textOf(waiting.request, "op");
	if(op == "eval" || op == "load-file") {
		evaluate(waiting.request, *session);
	} else if(op == "completions" || op == "complete") {
		if(hasProgram()) {
			nrepl::answerCompletions(link_, waiting.request, *host_.console);
		} else {
			Value::Dictionary reply = nrepl::replyTo(waiting.request);
			reply.emplace("completions", Value(Value::List{}));
			reply.emplace("status", nrepl::statusOf({"done"}));
			link_.send(Value(std::move(reply)));
		}
	} else {
		nrepl::answerLookup(link_, waiting.request, hasProgram() ? host_.console : nullptr, host_, session->space);
	}
}

void ProtocolPeer::evaluate(const Value& request, nrepl::EditorSession& session) {
	const bool wholeFile = textOf(request, "op") == "load-file";
	const std::string_view code = textOf(request, wholeFile ? "file" : "code");
	if(code.empty()) {
		nrepl::sendStatus(link_, request, {"error", "no-code", "done"});
		return;
	}
	if(const std::string_view space = textOf(request, "ns"); !space.empty()) session.space = space;
	if(!wholeFile) { // a client's own text comes as `eval`; a loaded file is the program's, whatever it mentions
		const nrepl::ClientForm form = nrepl::recogniseClientForm(code);
		if(form.kind != nrepl::ClientFormKind::None) {
			answerClientForm(request, form, session);
			return;
		}
	}
	if(!hasProgram()) {
		nrepl::sendFailure(link_, request, "this screen has no program to evaluate into", "");
		return;
	}
	const std::string_view editorPath = wholeFile ? textOf(request, "file-path") : textOf(request, "file");
	const ConsoleRequest console{.text = std::string(code),
	                             .file = host_.programFile ? host_.programFile(std::string(editorPath)) : std::string(),
	                             .space = session.space,
	                             .origin = "editor",
	                             .line = positionOf(request, "line"),
	                             .column = positionOf(request, "column"),
	                             .wholeFile = wholeFile};
	const ConsoleResult result = host_.console->evaluate(console);
	nrepl::answerEvaluation(link_, request, result, session);
}

void ProtocolPeer::answerClientForm(const Value& request, const nrepl::ClientForm& form,
                                    nrepl::EditorSession& session) {
	using Kind = nrepl::ClientFormKind;
	switch(form.kind) {
	case Kind::CurrentNamespace:
		sendValue(request, session.space, session);
		return;
	case Kind::RecentValue: {
		const std::string* value = session.recentValue(form.back);
		sendValue(request, value != nullptr ? std::string_view(*value) : "nil", session);
		return;
	}
	case Kind::LastError:
		sendValue(request, session.lastError ? nrepl::lispString(*session.lastError) : "nil", session);
		return;
	case Kind::ReaderConditional:
		sendValue(request, "unknown", session);
		return;
	case Kind::SwitchNamespace:
		session.space = form.name;
		break;
	case Kind::Documentation:
		if(const auto name = hasProgram() ? host_.console->describe(form.name) : std::nullopt; name) {
			std::string text = "-------------------------\n" + name->name + "\n";
			for(const std::string& list : name->arguments) text += list + "\n";
			text += "  " + name->doc + "\n";
			Value::Dictionary out = nrepl::replyTo(request);
			out.emplace("out", Value(std::move(text)));
			link_.send(Value(std::move(out)));
		}
		break;
	case Kind::Require:
	case Kind::ClientSetup:
	case Kind::None:
		break;
	}
	sendValue(request, "nil", session);
}

void ProtocolPeer::sendValue(const Value& request, std::string_view value, const nrepl::EditorSession& session) {
	Value::Dictionary message = nrepl::replyTo(request);
	message.emplace("value", Value(std::string(value)));
	message.emplace("ns", Value(session.space));
	link_.send(Value(std::move(message)));
	nrepl::sendStatus(link_, request, {"done"});
}

void ProtocolPeer::decided(const nrepl::PairingOutcome& outcome) {
	using Kind = nrepl::PairingOutcome::Kind;
	if(closed_) return;
	if(outcome.kind == Kind::Accepted) {
		drain();
	} else if(outcome.kind == Kind::NotAsked) {
		failWaiting(outcome.reason, "");
	} else {
		failWaiting(refusedText, "not-paired");
		closed_ = true;
		link_.close("the person refused the connection"); // last: the wire may destroy this peer inside it
	}
}

void ProtocolPeer::failWaiting(std::string_view why, std::string_view more) {
	const std::deque<Waiting> waiting = std::exchange(queue_, {});
	queuedBytes_ = 0;
	for(const Waiting& request : waiting) nrepl::sendFailure(link_, request.request, why, more);
}

} // namespace

std::unique_ptr<EditorPeer> makeEditorPeer(EditorLink& link, EditorHost& host, const EditorServerOptions& options,
                                           EditorShared& shared) {
	return std::make_unique<ProtocolPeer>(link, host, options, shared);
}

} // namespace affresco::net
