#pragma once

/// @file
/// @brief What the nREPL protocol's tests share: a link in memory, a console that records what reaches it, a host
/// whose pairing question the test answers, and helpers to build requests and read replies. No socket, no window.

#include "nrepl/EditorLink.hpp" // modules/net/src

#include "affresco/core/ProgramConsole.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nrepl_test {

using affresco::net::bencode::Value;

/// @brief A byte string. @param text It. @return The value.
inline Value text(std::string_view text) { return Value(std::string(text)); }
/// @brief An integer. @param number It. @return The value.
inline Value number(std::int64_t number) { return Value(number); }
/// @brief A dictionary. @param entries Its entries. @return The value.
inline Value dictionary(std::initializer_list<std::pair<const std::string, Value>> entries) {
	return Value(Value::Dictionary(entries));
}

/// @brief A reply's status words. @param reply It. @return Them; none when it has no status list.
inline std::vector<std::string> statusOf(const Value& reply) {
	std::vector<std::string> words;
	const Value* status = reply.find("status");
	if(status == nullptr || !status->isList()) return words;
	for(const Value& word : status->list())
		if(word.isString()) words.push_back(word.string());
	return words;
}

/// @brief Tells whether a reply's status holds a word. @param reply It. @param word The word. @return true if so.
inline bool hasStatus(const Value& reply, std::string_view word) {
	const std::vector<std::string> words = statusOf(reply);
	return std::ranges::find(words, word) != words.end();
}

/// @brief A reply's text value. @param reply It. @param key The key. @return The text, or "" when absent.
inline std::string textOf(const Value& reply, std::string_view key) {
	const Value* found = reply.find(key);
	return found != nullptr && found->isString() ? found->string() : std::string();
}

/// @brief A link that keeps what is sent.
class MemoryLink final : public affresco::net::EditorLink {
public:
	std::vector<Value> sent; ///< Every message sent, in order.
	bool closed = false;     ///< close() was called.

	/// @brief Keeps a message. @param message It.
	void send(const Value& message) override { sent.push_back(message); }
	/// @brief Notes the close. @param why Why.
	void close(std::string_view why) override {
		static_cast<void>(why);
		closed = true;
	}
};

/// @brief A console that records what reaches it.
class FakeConsole final : public affresco::ProgramConsole {
public:
	bool program = true;                            ///< What hasProgram() says.
	std::vector<affresco::ConsoleRequest> requests; ///< Every evaluate() call.
	std::size_t lookups = 0;                        ///< Calls of names() and describe().
	std::vector<affresco::ConsoleName> known;       ///< The names it knows, sorted.
	/// What evaluate() gives; by default each request's text as its one value.
	std::function<affresco::ConsoleResult(const affresco::ConsoleRequest&)> answer;

	/// @brief Has it a program. @return program.
	[[nodiscard]] bool hasProgram() const override { return program; }
	/// @brief Records and answers. @param request It. @return The answer.
	[[nodiscard]] affresco::ConsoleResult evaluate(const affresco::ConsoleRequest& request) override {
		requests.push_back(request);
		if(answer) return answer(request);
		return {.values = {request.text}, .error = {}, .line = 0, .column = 0, .proposal = {}};
	}
	/// @brief The known names with a prefix. @param prefix It. @param most The most. @return Them.
	[[nodiscard]] std::vector<affresco::ConsoleName> names(std::string_view prefix, std::size_t most) override {
		++lookups;
		std::vector<affresco::ConsoleName> found;
		for(const auto& name : known)
			if(name.name.starts_with(prefix) && found.size() < most) found.push_back(name);
		return found;
	}
	/// @brief One known name. @param name It. @return It, or nothing.
	[[nodiscard]] std::optional<affresco::ConsoleName> describe(std::string_view name) override {
		++lookups;
		for(const auto& entry : known)
			if(entry.name == name) return entry;
		return std::nullopt;
	}
	/// @brief No files. @return None.
	[[nodiscard]] std::vector<std::string> files() const override { return {}; }
	/// @brief Everything shows. @param text It. @return Nothing.
	[[nodiscard]] std::optional<std::string> unshowable(std::string_view text) const override {
		static_cast<void>(text);
		return std::nullopt;
	}
};

/// @brief A host whose questions the test answers, and the server's shared state and options.
struct FakeHost {
	FakeConsole console;                            ///< The program.
	std::vector<std::string> questions;             ///< The text of every question asked.
	std::vector<std::function<void(bool)>> answers; ///< Each question's answer, for the test to call.
	std::size_t cancels = 0;                        ///< cancelPairing() calls.
	std::optional<std::string> cannotAsk;           ///< When set, pair() gives it as its reason.
	affresco::net::EditorHost host;                 ///< What the peers get.
	affresco::net::EditorServerOptions options;     ///< The limits.
	affresco::net::EditorShared shared;             ///< What the connections share.

	/// @brief Wires the host to this.
	FakeHost() {
		options.version = "1.2.3";
		host.console = &console;
		host.pair = [this](std::string first, std::function<void(bool)> answered) -> std::optional<std::string> {
			if(cannotAsk) return cannotAsk;
			questions.push_back(std::move(first));
			answers.push_back(std::move(answered));
			return std::nullopt;
		};
		host.cancelPairing = [this] { ++cancels; };
		host.programFile = [](const std::string& path) {
			const std::size_t slash = path.rfind('/');
			return path.ends_with(".lisp") ? path.substr(slash == std::string::npos ? 0 : slash + 1) : std::string();
		};
		host.pathOf = [](const std::string& file) { return "/app/" + file; };
	}
	FakeHost(const FakeHost&) = delete;
	FakeHost& operator=(const FakeHost&) = delete;
	FakeHost(FakeHost&&) = delete;
	FakeHost& operator=(FakeHost&&) = delete;
	~FakeHost() = default;

	/// @brief Answers the last question asked. @param accepted The answer.
	void answer(bool accepted) {
		const std::function<void(bool)> answered = answers.back();
		answered(accepted);
	}
};

/// @brief One connection to a FakeHost.
struct Connection {
	MemoryLink link;                                 ///< Its answers.
	std::unique_ptr<affresco::net::EditorPeer> peer; ///< Its protocol.

	/// @brief Connects. @param fake The host.
	explicit Connection(FakeHost& fake)
	    : peer(affresco::net::makeEditorPeer(link, fake.host, fake.options, fake.shared)) {}
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;
	Connection(Connection&&) = delete;
	Connection& operator=(Connection&&) = delete;
	/// @brief Closes it, as the wire does.
	~Connection() {
		if(peer) peer->closed();
	}

	/// @brief Sends a request. @param request It. @return The messages it was answered with at once.
	std::vector<Value> send(const Value& request) {
		const std::size_t before = link.sent.size();
		peer->receive(request);
		return {link.sent.begin() + static_cast<std::ptrdiff_t>(before), link.sent.end()};
	}
	/// @brief Clones a session. @return Its id.
	std::string clone() {
		const std::vector<Value> replies = send(dictionary({{"op", text("clone")}, {"id", text("c")}}));
		return replies.empty() ? std::string() : nrepl_test::textOf(replies.front(), "new-session");
	}
	/// @brief The messages sent for one request id. @param id It. @return Them, in order.
	[[nodiscard]] std::vector<Value> repliesTo(std::string_view id) const {
		std::vector<Value> found;
		for(const Value& message : link.sent)
			if(nrepl_test::textOf(message, "id") == id) found.push_back(message);
		return found;
	}
};

} // namespace nrepl_test
