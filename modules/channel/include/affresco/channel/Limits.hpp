#pragma once

/// @file
/// @brief What one peer may send: the largest message, and how many messages and revisions in a given time, through
/// token buckets.

#include <algorithm>
#include <chrono>
#include <cstddef>

namespace affresco::channel {

/// @brief The limits a Session holds a peer to. The defaults leave room for any real client (an agent sends a few
/// messages a second, the Studio a few more while a person clicks), and stop one that floods the app.
struct PeerLimits {
	/// The largest message a peer may send. A larger one is refused without being parsed (the channel itself
	/// closes past maxMessageBytes, 64 MiB). A whole screen is tens of KiB; 8 MiB leaves room for large revisions.
	std::size_t maxMessageBytes = std::size_t{8} << 20;
	double messagesPerSecond = 100.0; ///< Messages of any kind, on average.
	double messageBurst = 200.0;      ///< Messages at once, after a quiet time.
	double revisionsPerMinute = 120.0; ///< Changes (revision, restore, revert, undo), which rebuild the screen.
	double revisionBurst = 30.0;       ///< Changes at once, after a quiet time.
};

/// @brief A token bucket: holds up to `burst` tokens, gains `perSecond` a second, and each event takes one.
///
/// Times are passed in, so tests can drive it without waiting. Not thread-safe: a Session uses its buckets on the
/// main thread only.
class TokenBucket {
public:
	using Clock = std::chrono::steady_clock; ///< The clock the times come from.

	/// @brief Starts full.
	/// @param perSecond Tokens gained a second (more than 0).
	/// @param burst The most tokens it holds (at least 1).
	/// @param now The time it starts.
	TokenBucket(double perSecond, double burst, Clock::time_point now) noexcept
	    : perSecond_(perSecond), burst_(std::max(burst, 1.0)), tokens_(burst_), last_(now) {}

	/// @brief Takes a token for an event, if there is one.
	/// @param now The time of the event (not earlier than the last one's; an earlier one adds nothing).
	/// @return true if the event may go ahead, false if it is over the limit.
	[[nodiscard]] bool take(Clock::time_point now) noexcept {
		if(now > last_) {
			const std::chrono::duration<double> elapsed = now - last_;
			tokens_ = std::min(burst_, tokens_ + elapsed.count() * perSecond_);
			last_ = now;
		}
		if(tokens_ < 1.0) return false;
		tokens_ -= 1.0;
		return true;
	}

private:
	double perSecond_;       ///< Tokens gained a second.
	double burst_;           ///< The most tokens held.
	double tokens_;          ///< Held now.
	Clock::time_point last_; ///< When tokens_ was last brought up to date.
};

} // namespace affresco::channel
