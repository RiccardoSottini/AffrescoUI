#pragma once

/// @file
/// @brief vcs::Runs: who may run which of a document's commands as a peer, the runs that wait for a decision, the log
/// of runs and the run grants (core::RunControl).

#include "affresco/core/Commands.hpp"
#include "affresco/core/DocumentRules.hpp"
#include "affresco/core/VersionControl.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vcs {

/// @brief The RunControl of a screen: judges runs by the document's `permissions.run` rules, the mode and the run
/// grants, keeps the runs that wait and the newest done, and the grants an "always allow" adds.
///
/// Not built yet (f4b fills it in): every run is denied with "commands can't be run yet", nothing is kept and nothing
/// waits. Constructed by the screen after its repository; reads the document through the version control and its
/// rules through the document rules, both of which outlive it. Main thread only.
class Runs final : public RunControl {
public:
	/// @brief Sets the run control up for a document.
	/// @param versions The document, its mode and its reviewers; outlives this.
	/// @param rules How the document's permissions are read; outlives this.
	Runs(VersionControl& versions, const DocumentRules& rules) noexcept : versions_(versions), rules_(rules) {}

	/// @brief Not built yet (f4b): denies every run.
	/// @param author The runner.
	/// @param command The command's name.
	/// @param reach What the command reaches.
	/// @return Deny, "commands can't be run yet".
	[[nodiscard]] RunJudgement judge(std::string_view author, std::string_view command,
	                                 const CommandReach& reach) const override;
	/// @brief Not built yet (f4b): begins no run.
	/// @param author The runner.
	/// @param command The command's name.
	/// @param arguments Its arguments.
	/// @param title Its title.
	/// @param verdict What judge() said.
	/// @param reason Why.
	/// @return No run, "commands can't be run yet".
	Begun begin(std::string_view author, std::string_view command, const json& arguments, std::string_view title,
	            RunVerdict verdict, std::string_view reason) override;
	/// @brief Not built yet (f4b): no run waits.
	/// @param run The run.
	/// @param reviewer Who allows it.
	/// @param always Also grant it.
	/// @param duration How long the grant lasts.
	/// @return "no run <run> waits".
	std::optional<std::string> allow(int run, std::string_view reviewer, bool always,
	                                 std::string_view duration) override;
	/// @brief Not built yet (f4b): no run waits.
	/// @param run The run.
	/// @param reason Why.
	/// @return "no run <run> waits".
	std::optional<std::string> deny(int run, std::string_view reason) override;
	/// @brief Not built yet (f4b): no run is kept, so there is nothing to record.
	/// @param run The run.
	/// @param state How it ended.
	/// @param reason Why it failed.
	/// @param changes How many changes it made.
	/// @param errors What its steps reported.
	/// @param undoable What undo can put back.
	void finish(int run, RunState state, std::string_view reason, std::size_t changes, std::vector<std::string> errors,
	            Undoable undoable) override;
	/// @brief Not built yet (f4b): no run is kept.
	/// @return An empty list.
	[[nodiscard]] std::vector<RunRecord> runs() const override;
	/// @brief Not built yet (f4b): no grant is kept.
	/// @return An empty list.
	[[nodiscard]] std::vector<Grant> grants() const override;
	/// @brief Not built yet (f4b): no grant is kept.
	/// @param rule The grant's rule.
	/// @return false.
	bool revoke(std::string_view rule) override;
	/// @brief Not built yet (f4b): nothing waits, nothing expires.
	/// @param now Milliseconds since 1970.
	void expire(std::int64_t now) override;
	/// @brief Not built yet (f4b): no run changes the state, so nothing is judged.
	/// @param author The runner.
	/// @param stateBefore The state before.
	/// @param stateAfter The state after.
	/// @return "commands can't be run yet".
	[[nodiscard]] std::optional<std::string> stateDenial(std::string_view author, const json& stateBefore,
	                                                     const json& stateAfter) const override;

private:
	[[maybe_unused]] VersionControl& versions_;   ///< The document, its mode and reviewers (f4b reads them).
	[[maybe_unused]] const DocumentRules& rules_; ///< Reads `permissions.run` (f4b).
};

} // namespace affresco::vcs
