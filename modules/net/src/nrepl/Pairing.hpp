#pragma once

/// @file
/// @brief The pairing of one editor connection: the person's question (EditorHost::pair), asked for one connection at
/// a time (EditorShared::asking, the others wait their turn in EditorShared::waitingTurns), and the refusals the
/// connections of a server share (EditorShared::refusals, EditorServerOptions::maxRefusals). Main thread.

#include "EditorLink.hpp"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string>

namespace affresco::net::nrepl {

/// @brief How a pairing question ended.
struct PairingOutcome {
	/// @brief Its kinds.
	enum class Kind {
		Accepted, ///< The person said yes: the connection is paired until it closes.
		Refused,  ///< The person said no, or the server pairs no more (too many refusals).
		NotAsked  ///< The host could not ask now (EditorHost::pair gave a reason): nothing counted, still unpaired.
	};
	Kind kind = Kind::Refused; ///< How it ended.
	std::string reason;        ///< For NotAsked: the host's reason.
};

/// @brief One connection's pairing. Its owner (the connection's peer) outlives it, and decides what the outcome does.
class Pairing {
public:
	/// @brief Called once a question ends. It may close the connection and destroy this Pairing: Pairing touches
	/// nothing of itself after calling it.
	using Decided = std::function<void(const PairingOutcome& outcome)>;

	/// @brief An unpaired connection.
	/// @param host The pairing question; outlives this.
	/// @param options maxRefusals; outlive this.
	/// @param shared The question's turn and the refusals; outlive this.
	/// @param decided What an outcome does.
	Pairing(EditorHost& host, const EditorServerOptions& options, EditorShared& shared, Decided decided);
	/// @brief Takes a waiting question away (cancel()).
	~Pairing();
	Pairing(const Pairing&) = delete;
	Pairing& operator=(const Pairing&) = delete;
	Pairing(Pairing&&) = delete;
	Pairing& operator=(Pairing&&) = delete;

	/// @brief Tells whether the person accepted the connection. @return true if they did.
	[[nodiscard]] bool paired() const noexcept { return state_ == State::Paired; }
	/// @brief Tells whether a question is open or waits its turn. @return true if one does.
	[[nodiscard]] bool pending() const noexcept { return state_ == State::Asking || state_ == State::WaitingTurn; }
	/// @brief Tells whether the server pairs no more connections (EditorServerOptions::maxRefusals reached).
	/// @return true if it doesn't.
	[[nodiscard]] bool stopped() const noexcept { return shared_.refusals >= options_.maxRefusals; }

	/// @brief Asks the person, now or when the question is free; the outcome reaches `decided`. Refuses at once,
	/// without asking, once the server pairs no more. Nothing happens if a question is already pending or paired.
	/// @param text What the connection first wants to evaluate, to show.
	void ask(std::string text);
	/// @brief Takes the question away unanswered (the connection went, or its requests were interrupted): the host's
	/// question is cancelled, or the turn given up. Nothing is counted and `decided` isn't called.
	void cancel();

private:
	/// @brief Where the question is.
	enum class State {
		Unpaired,    ///< No question yet, or the last one wasn't asked.
		WaitingTurn, ///< Another connection's question is open: this one waits in EditorShared::waitingTurns.
		Asking,      ///< The host's question is open.
		Paired       ///< The person said yes.
	};

	/// @brief Asks the host now. @param text What to show.
	void askNow(std::string text);
	/// @brief Takes the person's answer to a question. @param question Which question. @param accepted Their answer.
	void answer(std::uint64_t question, bool accepted);
	/// @brief Ends the open question: frees it for the next connection that waits, then calls `decided`.
	/// @param outcome How it ended.
	void finish(PairingOutcome outcome);
	/// @brief Frees the question and gives it to the first connection waiting its turn.
	void freeTurn();

	EditorHost& host_;                   ///< The pairing question.
	const EditorServerOptions& options_; ///< maxRefusals.
	EditorShared& shared_;               ///< The question's turn and the refusals.
	Decided decided_;                    ///< What an outcome does.
	State state_ = State::Unpaired;      ///< Where the question is.
	std::uint64_t question_ = 0;         ///< The open question's number: a late answer to an older one is ignored.
	std::string waitingText_;            ///< While WaitingTurn: what to show once it is this connection's turn.
	std::list<std::function<void()>>::iterator turn_; ///< While WaitingTurn: this connection's entry.
	/// Alive while this Pairing is: the host's answer callback holds it weakly, and outlives it (shared, so a weak
	/// pointer can see it go).
	std::shared_ptr<char> alive_ = std::make_shared<char>();
};

} // namespace affresco::net::nrepl
