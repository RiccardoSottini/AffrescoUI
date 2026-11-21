/// @file
/// @brief vcs::Runs, the grants: what an "always allow" adds ("agent(run:move-card)"), how long it lasts, and
/// withdrawing it. Grants live in memory and end when the host quits.

#include "affresco/vcs/Runs.hpp"

#include "affresco/core/Portable.hpp"
#include "permissions/Permissions.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <ctime>
#include <iterator>
#include <limits>

namespace affresco::vcs {

namespace {

/// The longest a grant may last: a year, as for an accept's "for".
constexpr long long maxGrantSeconds = 366LL * 24 * 3600;

/// @brief Reads a duration as an accept's "for" is read: a whole number and a unit, "30m", "8h" or "7d".
/// @param text The duration.
/// @return Seconds, or std::nullopt if it isn't one or is longer than maxGrantSeconds.
std::optional<long long> secondsOf(std::string_view text) {
	if(text.size() < 2) return std::nullopt;
	long long count = 0;
	const std::string_view digits = text.substr(0, text.size() - 1);
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), count);
	if(error != std::errc() || end != digits.data() + digits.size() || count < 1) return std::nullopt;
	const char unit = text.back();
	const long long scale = unit == 'm' ? 60 : unit == 'h' ? 3600 : unit == 'd' ? 86400 : 0;
	if(scale == 0 || count > maxGrantSeconds / scale) return std::nullopt;
	return count * scale;
}

/// @brief A time as grants show it.
/// @param milliseconds Milliseconds since 1970 (UTC).
/// @return UTC, ISO 8601, e.g. "2026-11-15T20:00:00Z".
std::string isoTime(std::int64_t milliseconds) {
	const std::tm time = utcTime(static_cast<std::time_t>(milliseconds / 1000));
	std::array<char, 32> buffer{};
	if(std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &time) == 0) return {};
	return buffer.data();
}

/// @brief The rule of the grant that lets a runner's channel run a command.
/// @param author The runner.
/// @param command The command's name.
/// @return e.g. "agent(run:move-card)".
std::string grantRule(std::string_view author, std::string_view command) {
	return detail::channelOf(author) + "(run:" + std::string(command) + ")";
}

} // namespace

std::optional<std::string> Runs::grantSeconds(std::string_view duration, std::optional<long long>& seconds) const {
	const std::optional<long long> asked = secondsOf(duration);
	if(!duration.empty() && !asked)
		return "'" + std::string(duration.substr(0, 40)) +
		       "' is not a duration for grants: expected minutes, hours or days, e.g. \"30m\", \"8h\", \"7d\" (at "
		       "most a year)";
	// A grantsLast past a year (the schema takes up to 999999d) caps grants at a year, never leaves them unlimited
	const std::string& last = permissions().review.grantsLast;
	const std::optional<long long> longest =
	    last.empty() ? std::nullopt : std::optional<long long>(secondsOf(last).value_or(maxGrantSeconds));
	seconds = std::nullopt;
	if(asked || longest)
		seconds = std::min(asked.value_or(std::numeric_limits<long long>::max()),
		                   longest.value_or(std::numeric_limits<long long>::max()));
	return std::nullopt;
}

void Runs::addGrant(const RunRecord& record, std::string_view reviewer, std::optional<long long> seconds) {
	const std::int64_t now = clock_();
	RunGrant added;
	added.grant.rule = grantRule(record.author, record.command);
	added.grant.grantedBy = reviewer;
	added.grant.time = isoTime(now);
	if(seconds) {
		added.expiresAt = now + *seconds * 1000;
		added.grant.expires = isoTime(added.expiresAt);
	}
	// One grant per rule: allowing again renews it
	std::erase_if(grants_, [&](const RunGrant& g) { return g.grant.rule == added.grant.rule; });
	grants_.push_back(std::move(added));
}

bool Runs::granted(std::string_view author, std::string_view command) const {
	const std::string rule = grantRule(author, command);
	const std::int64_t now = clock_();
	return std::ranges::any_of(grants_, [&](const RunGrant& g) {
		return g.grant.rule == rule && (g.expiresAt == 0 || g.expiresAt > now);
	});
}

std::vector<Grant> Runs::grants() const {
	const std::int64_t now = clock_();
	std::vector<Grant> active;
	for(const RunGrant& g : grants_)
		if(g.expiresAt == 0 || g.expiresAt > now) active.push_back(g.grant);
	return active;
}

bool Runs::revoke(std::string_view rule) {
	if(std::erase_if(grants_, [&](const RunGrant& g) { return g.grant.rule == rule; }) == 0) return false;
	onChanged();
	return true;
}

bool Runs::dropExpiredGrants(std::int64_t now) {
	return std::erase_if(grants_, [&](const RunGrant& g) { return g.expiresAt != 0 && g.expiresAt <= now; }) > 0;
}

} // namespace affresco::vcs
