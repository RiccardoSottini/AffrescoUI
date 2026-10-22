/// @file
/// @brief The parts of vcs::Repository that several agents at once add (VERSION_CONTROL.md, "Roles" and "Leases"):
/// who is connected, the parts they lease, and the roles a reviewer gives.

#include "affresco/vcs/Repository.hpp"

#include "permissions/Permissions.hpp"

#include <algorithm>

namespace affresco::vcs {

namespace {

/// @brief The key a lease probe adds to an object, to see whether the author may change it at all.
constexpr const char* probeKey = "affrescoLeaseProbe";

/// @brief Sets a pointer back when it goes out of scope (Repository::submitFrom()).
class Restore {
public:
	/// @brief Sets a pointer for a while.
	/// @param slot The pointer.
	/// @param value Its value meanwhile.
	Restore(const void*& slot, const void* value) noexcept : slot_(slot), saved_(slot) { slot_ = value; }
	/// @brief Sets it back.
	~Restore() { slot_ = saved_; }
	Restore(const Restore&) = delete;            ///< Not copied.
	Restore& operator=(const Restore&) = delete; ///< Not copied.

private:
	const void*& slot_; ///< The pointer.
	const void* saved_; ///< Its value before.
};

} // namespace

const Repository::Peer* Repository::peerOn(const void* connection) const {
	const auto found = std::ranges::find(peers_, connection, &Peer::connection);
	return found == peers_.end() ? nullptr : &*found;
}

void Repository::join(const void* connection, std::string_view author) {
	if(peerOn(connection) || peers_.size() >= maxPeers) return;
	peers_.push_back({connection, nextPeer_++, std::string(author)});
	onReviewChanged();
}

void Repository::leave(const void* connection) {
	const Peer* peer = peerOn(connection);
	if(!peer) return;
	leases_.releasePeer(peer->id);
	std::erase_if(peers_, [&](const Peer& each) { return each.connection == connection; });
	onReviewChanged();
}

RevisionResult Repository::submitFrom(const void* connection, const json& revision) {
	if(const auto found = std::ranges::find(peers_, connection, &Peer::connection); found != peers_.end())
		if(const std::string author = revision.is_object() ? revision.value("author", std::string()) : "";
		   !author.empty() && author.size() <= Roles::maxNameBytes)
			found->author = author;
	const Restore restore(submitting_, peerOn(connection) ? connection : nullptr);
	return submit(revision);
}

std::optional<std::string> Repository::leaseBeyondRules(std::string_view target, const std::string& author) {
	const json before = detail::view(document_, state_);
	const std::optional<std::string> pointer = target.starts_with('#') ? rules_.findNode(before, target.substr(1))
	                                           : target.starts_with('/') && detail::locate(before, std::string(target))
	                                               ? std::optional(std::string(target))
	                                               : std::nullopt;
	if(!pointer) return "nothing is at " + std::string(target);
	json after = before;
	json& part = after.at(json::json_pointer(*pointer));
	if(part.is_object()) part[probeKey] = true;
	else part = json::array({part});
	if(const Verdict verdict = evaluate(before, after, author, true); verdict.kind == Verdict::Kind::Deny)
		return "a lease can't reach beyond its holder's rules: " + verdict.reason;
	return std::nullopt;
}

std::optional<std::string> Repository::lease(const void* connection, std::string_view holder, std::string_view target,
                                             int seconds) {
	const Peer* peer = peerOn(connection);
	if(!peer) return std::string("only a connected peer leases");
	if(holder.empty() || holder.size() > Roles::maxNameBytes) return std::string("a lease names its holder");
	for(const Lease& each : leases_.active(Leases::Clock::now()))
		if(each.holder == holder && each.peer != peer->id)
			return "another connection holds leases as '" + std::string(holder) + "'";
	if(const auto beyond = leaseBeyondRules(target, std::string(holder))) return beyond;
	auto result = leases_.acquire(detail::view(document_, state_), target, holder, peer->id,
	                              std::chrono::seconds(std::clamp(seconds, 1, 3600)), Leases::Clock::now());
	if(const std::string* refused = std::get_if<std::string>(&result)) return *refused;
	onReviewChanged();
	return std::nullopt;
}

bool Repository::release(const void* connection, std::string_view target) {
	const Peer* peer = peerOn(connection);
	if(!peer) return false;
	for(const Lease& each : leases_.active(Leases::Clock::now()))
		if(each.peer == peer->id && each.target == target && leases_.release(target, each.holder)) {
			onReviewChanged();
			return true;
		}
	return false;
}

std::optional<std::string> Repository::endLease(std::string_view target, std::string_view by) {
	if(!mayReview(by)) return std::string("only a reviewer ends someone's lease");
	for(const Lease& each : leases_.active(Leases::Clock::now()))
		if(each.target == target && leases_.release(target, each.holder)) {
			onReviewChanged();
			return std::nullopt;
		}
	return "no lease on " + std::string(target);
}

std::optional<std::string> Repository::setRole(std::string_view author, std::string_view role, std::string_view by) {
	if(!mayReview(by)) return std::string("only a reviewer or the host sets a role");
	if(detail::channelOf(author) == detail::channelOf(by)) return std::string("nobody sets their own role");
	if(auto refused = roles_.set(author, role)) return refused;
	onReviewChanged();
	return std::nullopt;
}

json Repository::coordination() const {
	json peers = json::array();
	for(const Peer& peer : peers_) {
		json leases = json::array();
		for(const Lease& each : leases_.active(Leases::Clock::now()))
			if(each.peer == peer.id) leases.push_back(each.target);
		peers.push_back({{"author", peer.author}, {"role", roles_.of(peer.author)}, {"leases", leases}});
	}
	return {{"peers", peers}, {"leases", leases_.toJson(Leases::Clock::now())}, {"roles", roles_.toJson()}};
}

std::optional<std::string> Repository::leaseDenial(const json& before, const json& after,
                                                   const std::string& author) const {
	const Peer* submitter = submitting_ ? peerOn(submitting_) : nullptr;
	for(const Lease& each : leases_.active(Leases::Clock::now())) {
		const bool own = submitter ? each.peer == submitter->id : each.holder == author;
		if(!own && detail::changes(rules_, before, after, each.target))
			return "'" + each.holder + "' holds a lease on " + each.target + "; wait for it to end";
	}
	return std::nullopt;
}

} // namespace affresco::vcs
