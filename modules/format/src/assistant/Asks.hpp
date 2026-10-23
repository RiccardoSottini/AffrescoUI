#pragma once

/// @file
/// @brief format::Asks: the requests a screen's own person sends its agents ("ask" steps), and their replies.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <string_view>

namespace affresco::format {

struct Engine;

/// @brief The requests the screen sent to the peers connected to it, each with its replies and the proposals its
/// replier made since: the "requests" variable.
///
/// A request goes out after the event (Runtime::onAsk), from the author "user"; a reply comes back through
/// Runtime::receiveReply() and runs the action "ask:reply". A reply is never an approval: while its action runs, the
/// steps that decide for the person (accept, reject, restore, revert, undo) are refused (deciding()). Invariants: at
/// most maxRequests requests (the oldest finished one goes first), maxReplies replies each, texts at most maxText
/// bytes. Callers: the "ask" step, Runtime::receiveReply(), the "requests" variable, the review steps. Main thread
/// only.
class Asks {
public:
	static constexpr std::size_t maxRequests = 64;          ///< Requests kept; the oldest finished one goes first.
	static constexpr std::size_t maxReplies = 256;          ///< Replies kept per request; the oldest goes first.
	static constexpr std::size_t maxAsk = 16384;            ///< The longest request, in bytes.
	static constexpr std::size_t maxText = 65536;           ///< A reply's text and error are cut to this many bytes.
	static constexpr std::size_t maxFrom = 256;             ///< A replier's name is cut to this many bytes.
	static constexpr std::string_view action = "ask:reply"; ///< The action a reply runs, if the screen has it.

	/// @brief A ledger for an engine.
	/// @param engine The runtime; outlives this object.
	explicit Asks(Engine& engine) : engine_(engine) {}

	Asks(const Asks&) = delete;
	Asks& operator=(const Asks&) = delete;
	Asks(Asks&&) = delete;
	Asks& operator=(Asks&&) = delete;
	~Asks() = default;

	/// @brief Records a request and sends it to the peers after the event (Runtime::onAsk).
	/// @param text What the person asks.
	/// @param where The step, for errors.
	/// @return The request's id, unique across runs ("ask-<milliseconds>-<n>").
	/// @throws DocumentError If the text is empty or longer than maxAsk.
	std::string send(const std::string& text, const std::string& where);

	/// @brief Takes a reply to one of the screen's requests: records it, finds the proposals its replier made since
	/// the request, and runs "ask:reply" with `{"request", "from", "text", "done", "error", "proposals", "time"}`.
	/// @param reply `{"request": id, "from"?, "text"?, "done"? (true), "error"?}`; fields of the wrong type are
	/// ignored. One not done, with no text and no error, only marks the request "working".
	/// @return false if it answers no request of the screen's (it is for another peer).
	bool receive(const json& reply);

	/// @brief The requests, oldest first: `[{"id", "text", "time", "state", "replies", "proposals"}]`. "state" is
	/// "sent", "working" (a reply that isn't done), "answered" or "failed"; times are milliseconds since 1970.
	/// @return The "requests" variable.
	[[nodiscard]] json list() const;

	/// @brief Whether a reply's action is running: the steps that decide for the person are refused.
	/// @return It.
	[[nodiscard]] bool deciding() const noexcept { return replying_; }

private:
	/// @brief A request and what came back.
	struct Request {
		std::string id;                      ///< Its id.
		std::string text;                    ///< What the person asked.
		std::int64_t time = 0;               ///< When, in milliseconds since 1970.
		std::string state = "sent";          ///< See list().
		json replies = json::array();        ///< `[{"from", "text", "time", "done", "error"?}]`, oldest first.
		json proposals = json::array();      ///< The ids of the proposals its repliers made since.
		std::map<int, std::string> heads;    ///< The open proposals when it was sent: id to head.
		std::map<int, std::string> reported; ///< The proposals reported for it: id to the head reported.
	};

	/// @brief Finds the proposals a replier opened or pushed to since a request, not reported for it yet.
	/// @param request The request.
	/// @param from The replier: proposals by an author of the same channel count (the part before '/').
	/// @return Their ids.
	json newProposals(Request& request, std::string_view from) const;

	/// @brief Makes room for one more request: drops the oldest finished one, or the oldest.
	void makeRoom();

	Engine& engine_;               ///< The runtime.
	std::deque<Request> requests_; ///< See list().
	std::uint64_t sent_ = 0;       ///< Requests sent in this run, for their ids.
	bool replying_ = false;        ///< See deciding().
};

} // namespace affresco::format
