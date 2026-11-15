/// @file
/// @brief Pairing (Pairing.hpp): the person's question for one connection, its turn among the connections, and the
/// refusals they share.

#include "Pairing.hpp"

#include "affresco/core/Log.hpp"

#include <utility>

namespace affresco::net::nrepl {

Pairing::Pairing(EditorHost& host, const EditorServerOptions& options, EditorShared& shared, Decided decided)
    : host_(host), options_(options), shared_(shared), decided_(std::move(decided)), turn_(shared.waitingTurns.end()) {}

// NOLINTNEXTLINE(bugprone-exception-escape): only running out of memory throws (the next question): it ends the program
Pairing::~Pairing() { cancel(); }

void Pairing::ask(std::string text) {
	if(state_ != State::Unpaired) return;
	if(stopped()) {
		finish({.kind = PairingOutcome::Kind::Refused, .reason = {}});
		return;
	}
	if(shared_.asking) {
		state_ = State::WaitingTurn;
		waitingText_ = std::move(text);
		turn_ = shared_.waitingTurns.insert(shared_.waitingTurns.end(), [this] {
			// Its entry is erased before this runs (freeTurn), and on cancel(), so `this` is alive here
			turn_ = shared_.waitingTurns.end();
			state_ = State::Unpaired;
			if(stopped()) finish({.kind = PairingOutcome::Kind::Refused, .reason = {}});
			else askNow(std::exchange(waitingText_, {}));
		});
		return;
	}
	askNow(std::move(text));
}

void Pairing::askNow(std::string text) {
	if(!host_.pair) {
		finish({.kind = PairingOutcome::Kind::NotAsked, .reason = "this host can't ask the person to pair an editor"});
		return;
	}
	state_ = State::Asking;
	shared_.asking = true;
	const std::uint64_t question = ++question_;
	const std::weak_ptr<char> alive = alive_;
	auto why = host_.pair(std::move(text), [this, alive, question](bool accepted) {
		if(!alive.expired()) answer(question, accepted);
	});
	// The host may have answered inside pair() already, and that outcome may have closed the connection
	if(alive.expired() || state_ != State::Asking || question_ != question || !why) return;
	finish({.kind = PairingOutcome::Kind::NotAsked, .reason = std::move(*why)});
}

void Pairing::answer(std::uint64_t question, bool accepted) {
	if(state_ != State::Asking || question != question_) return; // cancelled, or answered already
	if(accepted) {
		finish({.kind = PairingOutcome::Kind::Accepted, .reason = {}});
		return;
	}
	++shared_.refusals;
	if(shared_.refusals == options_.maxRefusals)
		log(LogLevel::Warning, "AffrescoUI editor server: the person refused " + std::to_string(shared_.refusals) +
		                           " connections; it pairs no more until the host restarts");
	finish({.kind = PairingOutcome::Kind::Refused, .reason = {}});
}

void Pairing::finish(const PairingOutcome& outcome) {
	const bool asked = state_ == State::Asking;
	state_ = outcome.kind == PairingOutcome::Kind::Accepted ? State::Paired : State::Unpaired;
	const Decided decided = decided_; // a copy: calling it may destroy this
	if(asked) freeTurn();
	decided(outcome);
}

void Pairing::freeTurn() {
	shared_.asking = false;
	if(shared_.waitingTurns.empty()) return;
	const std::function<void()> next = std::move(shared_.waitingTurns.front());
	shared_.waitingTurns.pop_front();
	next();
}

void Pairing::cancel() {
	if(state_ == State::WaitingTurn) {
		shared_.waitingTurns.erase(turn_);
		turn_ = shared_.waitingTurns.end();
		waitingText_.clear();
	} else if(state_ == State::Asking) {
		++question_; // a late answer is ignored
		if(host_.cancelPairing) host_.cancelPairing();
		state_ = State::Unpaired;
		freeTurn();
		return;
	}
	if(state_ != State::Paired) state_ = State::Unpaired;
}

} // namespace affresco::net::nrepl
