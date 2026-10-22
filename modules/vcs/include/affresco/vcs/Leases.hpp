#pragma once

/// @file
/// @brief vcs::Leases and vcs::LeasedRules: a person holds a part of a document for a while, and nobody else may
/// change it until the lease ends (VERSION_CONTROL.md, "Leases").

#include "affresco/core/DocumentRules.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace affresco::vcs {

/// @brief One lease: a part of the document, its holder, and when it ends.
struct Lease {
	std::string target;  ///< As asked: "#id" or a JSON Pointer.
	std::string pointer; ///< Where the target was when it was leased (for overlaps).
	std::string holder;  ///< The person holding it, e.g. "ada" (the trusted part of an author's name).
	int peer = 0;        ///< The connection that asked, so its leases end with it.
	std::chrono::steady_clock::time_point expires; ///< When it ends.
};

/// @brief The leases on one document. Leases on overlapping parts (one inside the other) can't be held by two
/// people at once; the same holder renews its own. Bounded: maxLeases in all, maxPerHolder each, maxDuration long.
///
/// Not thread-safe: kept on the thread that changes the document.
class Leases {
public:
	using Clock = std::chrono::steady_clock;          ///< The clock the times come from.
	static constexpr std::size_t maxLeases = 256;       ///< Leases on one document.
	static constexpr std::size_t maxPerHolder = 16;     ///< Leases one person holds.
	static constexpr std::chrono::seconds maxDuration{3600}; ///< The longest lease.
	static constexpr std::size_t maxTargetBytes = 256;  ///< The longest target.

	/// @brief Leases on a document of some format.
	/// @param rules Finds "#id" nodes; must outlive the leases.
	explicit Leases(const DocumentRules& rules) noexcept : rules_(rules) {}

	/// @brief Takes or renews a lease.
	/// @param document The document as it is (with its live state as "state"), where the target is looked up.
	/// @param target "#id" or a JSON Pointer, without "*".
	/// @param holder The person asking.
	/// @param peer The connection asking.
	/// @param duration How long, at most maxDuration.
	/// @param now The time now.
	/// @return The lease, or why it was refused.
	[[nodiscard]] std::variant<Lease, std::string> acquire(const json& document, std::string_view target,
	                                                       std::string_view holder, int peer,
	                                                       std::chrono::seconds duration, Clock::time_point now);
	/// @brief Ends a holder's lease on a target.
	/// @param target The target, as leased.
	/// @param holder The holder.
	/// @return false if it held none there.
	bool release(std::string_view target, std::string_view holder);
	/// @brief Ends every lease a connection took (it went away).
	/// @param peer The connection.
	/// @return How many ended.
	std::size_t releasePeer(int peer);
	/// @brief The leases still running.
	/// @param now The time now.
	/// @return Them, oldest first.
	[[nodiscard]] std::vector<Lease> active(Clock::time_point now) const;
	/// @brief The leases still running, as JSON: `[{target, holder, seconds}]`, seconds being what is left.
	/// @param now The time now.
	/// @return The list.
	[[nodiscard]] json toJson(Clock::time_point now) const;

private:
	/// @brief Forgets the leases that ended.
	/// @param now The time now.
	void prune(Clock::time_point now);

	const DocumentRules& rules_; ///< Finds "#id" nodes.
	std::vector<Lease> leases_;  ///< Oldest first; some may have ended (prune()).
};

/// @brief The rules of a document, plus a deny rule for each lease and each person but its holder: a leased part is
/// judged like any rule, by the change's effect, and nothing about the document or its history changes.
class LeasedRules final : public DocumentRules {
public:
	/// @brief Decorates rules.
	/// @param base The format's rules; must outlive these.
	/// @param leases The leases; must outlive these.
	/// @param people Everyone who may connect (the names a lease denies); may change, read on each check.
	/// @param clock The time now (steady_clock::now by default).
	LeasedRules(const DocumentRules& base, const Leases& leases, std::function<std::vector<std::string>()> people,
	            std::function<Leases::Clock::time_point()> clock = {});

	/// @brief As the base rules.
	void validate(const json& document) const override { base_.validate(document); }
	/// @brief As the base rules.
	[[nodiscard]] std::optional<std::string> findNode(const json& document, std::string_view id) const override {
		return base_.findNode(document, id);
	}
	/// @brief The base rules' permissions, with `person(target)` denied for every running lease and every person
	/// other than its holder.
	/// @param document The document.
	/// @return The rules.
	[[nodiscard]] Permissions permissions(const json& document) const override;
	/// @brief As the base rules.
	void checkChange(const json& before, const json& after) const override { base_.checkChange(before, after); }
	/// @brief As the base rules.
	void checkRevision(const json& patch) const override { base_.checkRevision(patch); }
	/// @brief As the base rules.
	[[nodiscard]] std::vector<std::string> reviewLines(const json& before, const json& after) const override {
		return base_.reviewLines(before, after);
	}
	/// @brief As the base rules.
	void nameNodes(const json& document, std::string_view pointer, bool replaces, json& value) const override {
		base_.nameNodes(document, pointer, replaces, value);
	}
	/// @brief As the base rules.
	[[nodiscard]] std::string pathHint(const json& document, std::string_view path) const override {
		return base_.pathHint(document, path);
	}

private:
	const DocumentRules& base_;                          ///< The format's rules.
	const Leases& leases_;                               ///< The leases.
	std::function<std::vector<std::string>()> people_;   ///< See the constructor.
	std::function<Leases::Clock::time_point()> clock_;   ///< See the constructor.
};

} // namespace affresco::vcs
