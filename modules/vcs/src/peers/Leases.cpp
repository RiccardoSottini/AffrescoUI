/// @file
/// @brief vcs::Leases and vcs::LeasedRules: parts of a document held by one person for a while.

#include "affresco/vcs/Leases.hpp"

#include "permissions/Permissions.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace affresco::vcs {

namespace {

/// @brief Tells whether one JSON Pointer is the other or inside it, on segment boundaries ("/a" holds "/a/b", not
/// "/ab").
/// @param outer The pointer that may hold the other.
/// @param inner The other.
/// @return true if it does.
bool holds(std::string_view outer, std::string_view inner) noexcept {
	return inner.starts_with(outer) && (inner.size() == outer.size() || inner[outer.size()] == '/' || outer.empty());
}

/// @brief Finds where a lease's target is now.
/// @param rules Finds "#id" nodes.
/// @param document The document.
/// @param target "#id" or a JSON Pointer.
/// @return The pointer, or std::nullopt if it names nothing.
std::optional<std::string> resolve(const DocumentRules& rules, const json& document, std::string_view target) {
	if(target.starts_with('#')) return rules.findNode(document, target.substr(1));
	if(!target.starts_with('/') || !detail::locate(document, std::string(target))) return std::nullopt;
	return std::string(target);
}

/// @brief Why a lease request is malformed, before the document is looked at.
/// @param target "#id" or a JSON Pointer, without "*".
/// @param duration How long.
/// @return Why, or std::nullopt if it is well formed.
std::optional<std::string> malformed(std::string_view target, std::chrono::seconds duration) {
	if(target.empty() || target.size() > Leases::maxTargetBytes || target.contains('*'))
		return "a lease names one part: \"#id\" or a JSON Pointer, without \"*\"";
	if(duration.count() <= 0 || duration > Leases::maxDuration) return "a lease lasts 1 to 3600 seconds";
	return std::nullopt;
}

/// @brief The first lease someone else holds on a part overlapping a pointer (one inside the other).
/// @param leases The leases.
/// @param pointer The part asked for.
/// @param holder The person asking.
/// @return The lease, or nullptr if none.
const Lease* blocking(const std::vector<Lease>& leases, std::string_view pointer, std::string_view holder) {
	const auto found = std::ranges::find_if(leases, [&](const Lease& lease) {
		return lease.holder != holder && (holds(lease.pointer, pointer) || holds(pointer, lease.pointer));
	});
	return found == leases.end() ? nullptr : &*found;
}

/// @brief How many leases a person or a connection holds, together: a connection can't rotate names past the cap.
/// @param leases The leases.
/// @param holder The person.
/// @param peer The connection.
/// @return How many.
std::size_t heldBy(const std::vector<Lease>& leases, std::string_view holder, int peer) {
	return static_cast<std::size_t>(std::ranges::count_if(
	    leases, [&](const Lease& lease) { return lease.holder == holder || lease.peer == peer; }));
}

} // namespace

std::variant<Lease, std::string> Leases::acquire(const json& document, std::string_view target, std::string_view holder,
                                                 int peer, std::chrono::seconds duration, Clock::time_point now) {
	prune(now);
	if(std::optional<std::string> why = malformed(target, duration)) return std::move(*why);
	const std::optional<std::string> pointer = resolve(rules_, document, target);
	if(!pointer) return "nothing is at " + std::string(target);
	if(const Lease* other = blocking(leases_, *pointer, holder)) return other->holder + " holds " + other->target;
	const auto same = std::ranges::find_if(leases_, [&](const Lease& lease) {
		return lease.holder == holder && lease.target == target;
	});
	if(same != leases_.end()) {
		same->expires = now + duration;
		same->peer = peer;
		return *same;
	}
	if(heldBy(leases_, holder, peer) >= maxPerHolder)
		return std::string("one person or connection holds at most 16 leases");
	if(leases_.size() >= maxLeases) return std::string("this document has too many leases");
	leases_.push_back({std::string(target), *pointer, std::string(holder), peer, now + duration});
	return leases_.back();
}

bool Leases::release(std::string_view target, std::string_view holder) {
	return std::erase_if(leases_, [&](const Lease& lease) {
		       return lease.holder == holder && lease.target == target;
	       }) > 0;
}

std::size_t Leases::releasePeer(int peer) {
	return std::erase_if(leases_, [peer](const Lease& lease) { return lease.peer == peer; });
}

std::vector<Lease> Leases::active(Clock::time_point now) const {
	std::vector<Lease> running;
	std::ranges::copy_if(leases_, std::back_inserter(running), [now](const Lease& lease) { return lease.expires > now; });
	return running;
}

json Leases::toJson(Clock::time_point now) const {
	json list = json::array();
	for(const Lease& lease : active(now))
		list.push_back({{"target", lease.target},
		                {"holder", lease.holder},
		                {"seconds", std::chrono::ceil<std::chrono::seconds>(lease.expires - now).count()}});
	return list;
}

void Leases::prune(Clock::time_point now) {
	std::erase_if(leases_, [now](const Lease& lease) { return lease.expires <= now; });
}

LeasedRules::LeasedRules(const DocumentRules& base, const Leases& leases,
                         std::function<std::vector<std::string>()> people,
                         std::function<Leases::Clock::time_point()> clock)
    : base_(base), leases_(leases), people_(std::move(people)), clock_(std::move(clock)) {
	if(!clock_) clock_ = [] { return Leases::Clock::now(); };
}

// Deny rules are checked whatever else the document says, configured or not (Repository::evaluate())
Permissions LeasedRules::permissions(const json& document) const {
	Permissions permissions = base_.permissions(document);
	const std::vector<Lease> running = leases_.active(clock_());
	if(running.empty()) return permissions;
	const std::vector<std::string> people = people_ ? people_() : std::vector<std::string>{};
	for(const Lease& lease : running)
		for(const std::string& person : people)
			if(person != lease.holder)
				permissions.deny.push_back({person + "(" + lease.target + ")", person, lease.target});
	return permissions;
}

} // namespace affresco::vcs
