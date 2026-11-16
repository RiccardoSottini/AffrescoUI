#pragma once

/// @file
/// @brief vcs::Runs: who may run which of a document's commands as a peer, the runs that wait for a decision, the log
/// of runs and the run grants (core::RunControl).

#include "affresco/core/Commands.hpp"
#include "affresco/core/DocumentRules.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/vcs/Roles.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vcs {

/// @brief The RunControl of a screen: judges runs by the document's `permissions.run` rules, the mode and the run
/// grants, keeps the runs that wait and the newest done, and the grants an "always allow" adds
/// (VERSION_CONTROL.md, "Running commands").
///
/// Invariants: run numbers start at 1 and only grow; at most maxWaitingRuns runs are Waiting at once, and at most
/// maxWaitingRunsPerChannel of one channel; the log keeps the newest maxKeptRuns runs and never drops a waiting one;
/// grants live in memory only and never outlast the host. Nothing here runs a command.
///
/// Constructed by the screen after its repository; reads the document through the version control and its rules
/// through the document rules, both of which outlive it. Main thread only.
class Runs final : public RunControl {
public:
	/// @brief Gives the time, in milliseconds since 1970 (UTC): when a run begins and whether a grant still holds.
	using Clock = std::function<std::int64_t()>;

	/// Runs of one channel that may wait for a decision at once: begin() refuses more.
	static constexpr std::size_t maxWaitingRunsPerChannel = 4;
	/// The most JSON text a run's arguments keep in the log; a larger value is shown as tooLargeToShow.
	static constexpr std::size_t maxArgumentBytes = 4096;
	/// What a run's argument too large to keep is replaced by.
	static constexpr std::string_view tooLargeToShow = "(too large to show)";

	/// @brief Sets the run control up for a document.
	/// @param versions The document, its mode and its reviewers; outlives this.
	/// @param rules How the document's permissions are read; outlives this.
	/// @param clock The time; empty: the system's clock. Tests pass their own, so that nothing sleeps.
	Runs(VersionControl& versions, const DocumentRules& rules, Clock clock = {});

	/// @brief What a run started now would come to (the table in VERSION_CONTROL.md, "Running commands"): deny rules;
	/// a control reach; plan mode; service uses only the person allows; a reviewer; ask rules; allow rules and grants;
	/// no `permissions` at all; else ask.
	/// @param author The runner.
	/// @param command The command's name.
	/// @param reach What the command reaches.
	/// @return The verdict, and a reason that names the rule or the step (empty for Allow).
	[[nodiscard]] RunJudgement judge(std::string_view author, std::string_view command,
	                                 const CommandReach& reach) const override;
	/// @brief Numbers a run and logs it, Waiting until a decision or finish().
	/// @param author The runner.
	/// @param command The command's name.
	/// @param arguments Its arguments; kept cut to maxArgumentBytes.
	/// @param title Its title.
	/// @param verdict What judge() said: Ask and Person wait for a decision, Allow is run by the caller.
	/// @param reason Why.
	/// @return The run's number, or 0 and "too many runs are waiting" past maxWaitingRuns (or
	/// maxWaitingRunsPerChannel for the runner's channel), or a refusal for a Deny verdict.
	Begun begin(std::string_view author, std::string_view command, const json& arguments, std::string_view title,
	            RunVerdict verdict, std::string_view reason) override;
	/// @brief Allows a run that waits for a decision, and with `always` grants its channel that command.
	/// @param run The run.
	/// @param reviewer "user" for the person, else a reviewer of the document, never on the runner's channel.
	/// @param always Also grant it (refused for a personOnly run).
	/// @param duration How long the grant lasts ("30m", "8h", "7d"), capped by `permissions.review.grantsLast`.
	/// @return Why it was refused, or std::nullopt.
	std::optional<std::string> allow(int run, std::string_view reviewer, bool always,
	                                 std::string_view duration) override;
	/// @brief Denies a run that waits for a decision; it is logged Denied.
	/// @param run The run.
	/// @param reason Why; empty: "denied by a reviewer".
	/// @return Why it was refused (no such run waits), or std::nullopt.
	std::optional<std::string> deny(int run, std::string_view reason) override;
	/// @brief Records how a run ended, or that it was undone. A run that isn't kept is ignored.
	/// @param run The run.
	/// @param state Ran, Failed or Undone.
	/// @param reason Why it failed.
	/// @param changes How many changes it made.
	/// @param errors What its steps reported.
	/// @param undoable What undo can put back.
	void finish(int run, RunState state, std::string_view reason, std::size_t changes, std::vector<std::string> errors,
	            Undoable undoable) override;
	/// @brief The runs kept, oldest first.
	/// @return At most maxKeptRuns runs.
	[[nodiscard]] std::vector<RunRecord> runs() const override;
	/// @brief The run grants still in force, oldest first.
	/// @return The grants, their rules like "agent(run:move-card)".
	[[nodiscard]] std::vector<Grant> grants() const override;
	/// @brief Withdraws a run grant.
	/// @param rule The grant's rule.
	/// @return false if there is no such grant.
	bool revoke(std::string_view rule) override;
	/// @brief Refuses the runs that waited for a decision longer than runWaitMilliseconds, and drops grants that
	/// expired.
	/// @param now Milliseconds since 1970 (UTC).
	void expire(std::int64_t now) override;
	/// @brief Judges a run's state changes by effect, with the runner's data rules (a `role:` rule covers no runner).
	/// @param author The runner.
	/// @param stateBefore The state before.
	/// @param stateAfter The state after.
	/// @return Why the change is refused, or std::nullopt (always without data rules).
	[[nodiscard]] std::optional<std::string> stateDenial(std::string_view author, const json& stateBefore,
	                                                     const json& stateAfter) const override;

private:
	/// @brief A kept run, and whether it still waits for someone to decide (a run allowed or judged Allow is Waiting
	/// while it runs, but no longer waits for a decision).
	struct Entry {
		RunRecord record = {};    ///< The run.
		bool undecided = false;   ///< It waits for allow(), deny() or expire().
	};
	/// @brief A run grant, with its expiry in milliseconds for comparing against the clock.
	struct RunGrant {
		Grant grant = {};           ///< As grants() gives it.
		std::int64_t expiresAt = 0; ///< When it stops allowing; 0: until revoked or the host quits.
	};

	/// @brief The document's permissions, read again only when the head commit changed.
	/// @return The permissions.
	[[nodiscard]] const Permissions& permissions() const;
	/// @brief Tells whether a grant lets an author's channel run a command now.
	/// @param author The runner.
	/// @param command The command's name.
	/// @return true if a grant that hasn't expired covers it.
	[[nodiscard]] bool granted(std::string_view author, std::string_view command) const;
	/// @brief Adds (or renews) the grant an "always allow" gives.
	/// @param record The run allowed.
	/// @param reviewer Who allowed it.
	/// @param seconds How long it lasts; std::nullopt: until revoked.
	void addGrant(const RunRecord& record, std::string_view reviewer, std::optional<long long> seconds);
	/// @brief How long a grant asked for lasts, capped by `permissions.review.grantsLast`.
	/// @param duration As asked ("8h"); empty: as the review policy says.
	/// @param seconds Set to how long it lasts, std::nullopt for until revoked.
	/// @return Why the duration is refused, or std::nullopt.
	[[nodiscard]] std::optional<std::string> grantSeconds(std::string_view duration,
	                                                      std::optional<long long>& seconds) const;
	/// @brief Why a reviewer may not allow a run, if they may not.
	/// @param entry The run.
	/// @param reviewer Who allows it.
	/// @param always Whether a grant is asked too.
	/// @return The refusal, or std::nullopt.
	[[nodiscard]] std::optional<std::string> allowDenial(const Entry& entry, std::string_view reviewer,
	                                                     bool always) const;
	/// @brief The kept run with a number that waits for a decision.
	/// @param run The run.
	/// @return It, or nullptr.
	[[nodiscard]] Entry* undecided(int run);
	/// @brief Ends a run that waited for a decision with a refusal: logs it, then emits onDecision.
	/// @param entry The run.
	/// @param state Denied or Expired.
	/// @param reviewer Who decided; empty when it expired.
	/// @param reason Why.
	void refuse(Entry& entry, RunState state, std::string_view reviewer, std::string_view reason);
	/// @brief Drops the oldest finished runs while the log holds more than maxKeptRuns.
	void trimLog();
	/// @brief Drops grants that expired by a time.
	/// @param now Milliseconds since 1970.
	/// @return true if any was dropped.
	bool dropExpiredGrants(std::int64_t now);

	VersionControl& versions_;   ///< The document, its mode and reviewers; outlives this.
	const DocumentRules& rules_; ///< Reads the permissions; outlives this.
	Clock clock_;                ///< The time.
	Roles roles_;                ///< No roles: a `role:` data rule covers no runner.
	std::vector<Entry> log_;     ///< The runs kept, oldest first.
	std::vector<RunGrant> grants_; ///< The run grants, oldest first.
	int nextRun_ = 1;            ///< The number the next run gets.
	mutable std::optional<Permissions> permissions_; ///< The permissions as last read.
	mutable std::string permissionsHead_;            ///< The head commit they were read at.
};

} // namespace affresco::vcs
