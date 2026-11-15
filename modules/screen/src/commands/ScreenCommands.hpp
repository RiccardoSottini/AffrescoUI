#pragma once

/// @file
/// @brief detail::ScreenCommands: a screen's commands as tools for its peers (CommandService), over its runtime, its
/// run control and its version control.

#include "affresco/core/Commands.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/format/Runtime.hpp"
#include "pictures/OffscreenImages.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::detail {

/// @brief What a screen's peers ask about its commands: lists them as a peer may run them, runs one as the peer after
/// the run control's verdict (and, when it waits, after a decision), undoes a run, and passes a reviewer's decisions
/// to the run control.
///
/// A run is checked in a fixed order, and the first check that fails answers (SCREEN_FORMAT.md): runs switched off;
/// no window, a picture being drawn, a proposal previewed; no such command; its arguments; disabled; the run
/// control's verdict. Allow runs at once; Ask waits for a reviewer or the person; Person waits for the window's own
/// question. A run allowed later is checked again (from the window on), since the document may have changed while it
/// waited. Every run executes under its runner's name, never the person's.
///
/// Owned by Screen::Impl, after the runtime, the run control and the pictures, which it reads; each Session holds a
/// pointer to it while the host allows command runs (Screen::allowCommandRuns()). Its timer and the window's question
/// call back into it; both are cancelled in the destructor, and guarded by `alive_`. Main thread only.
class ScreenCommands final : public CommandService {
public:
	/// @brief The commands of a screen.
	/// @param runtime Its commands and their runs, and its window; outlives this object.
	/// @param control Who may run them, and the runs; outlives this object.
	/// @param versions Its document and its reviewers; outlives this object.
	/// @param images Its pictures, for a run's evidence; outlives this object.
	ScreenCommands(format::Runtime& runtime, RunControl& control, VersionControl& versions, OffscreenImages& images);
	/// @brief Cancels the expiry timer and the window's question, and stops listening to the run control.
	~ScreenCommands() override;

	ScreenCommands(const ScreenCommands&) = delete;
	ScreenCommands& operator=(const ScreenCommands&) = delete;
	ScreenCommands(ScreenCommands&&) = delete;
	ScreenCommands& operator=(ScreenCommands&&) = delete;

	/// @brief Main's commands with the run control's verdict for a peer, now: commands with typed arguments first.
	/// @param author The peer.
	/// @param filter Only commands whose name, title, category or description holds it, in any case; empty: all.
	/// @param all Also those it may never run.
	/// @return `{"type": "commands", "commands": [...]}`.
	[[nodiscard]] json list(std::string_view author, std::string_view filter, bool all) const override;
	/// @brief Runs a command for a peer, after the checks and the verdict (the class's order); a run that waits is
	/// answered again when it ran or was refused.
	/// @param author The runner.
	/// @param message `{"command", "arguments", "evidence", "scale"}`, checked by the session.
	/// @param answer Gives the answers.
	void run(std::string_view author, const json& message, Answer answer) override;
	/// @brief Undoes a run: only for its own channel or a reviewer.
	/// @param author The peer.
	/// @param message `{"run"}`.
	/// @return "undone" (`run`, `changed`) or "rejected".
	json undo(std::string_view author, const json& message) override;
	/// @brief Passes a decision on to the run control (allowRun, denyRun, revokeRun), under the reviewer's name.
	/// @param reviewer The peer, or "user" for the person.
	/// @param message The decision, checked by the session.
	/// @return "done" or "rejected".
	json decide(std::string_view reviewer, const json& message) override;
	/// @brief The runs and run grants: a reader whose reads are limited gets the runs without their arguments and
	/// errors, which can quote values it may not read.
	/// @param reader The peer.
	/// @return `{"runs", "runGrants"}`.
	[[nodiscard]] json pending(std::string_view reader) const override;

	/// @brief Lets runs start, or refuses them all ("this host runs no commands") and denies those that wait.
	/// @param allowed Whether runs may start.
	void setAllowed(bool allowed);

private:
	/// @brief A run that waits for a decision, and how to finish it.
	struct Waiting {
		int run = 0;                     ///< Its number.
		std::string author = {};         ///< The runner.
		std::string command = {};        ///< The command's name.
		json arguments = json::object(); ///< Its arguments, checked.
		std::string evidence = "state";  ///< "state", "picture" or "none".
		int scale = 1;                   ///< A picture's scale.
		Answer answer = {};              ///< Gives its last answer.
		std::uint64_t question = 0;      ///< The window's question, for a Person run while it is shown.
	};
	/// @brief What the checks of a run found.
	struct Checked {
		std::optional<std::string> refusal = std::nullopt; ///< Why it may not run, or std::nullopt.
		json entry = json::object();                       ///< The command, as commandList() gives it.
		CommandReach reach = {};                           ///< What it reaches.
		RunJudgement judgement = {};                       ///< The run control's verdict.
	};

	/// @brief The checks from the window on: steps 2 to 6 of the class's order.
	/// @param author The runner.
	/// @param name The command's name.
	/// @param arguments Its arguments, checked and filled in place.
	/// @return What they found.
	[[nodiscard]] Checked check(std::string_view author, const std::string& name, json& arguments) const;
	/// @brief Why the screen can't run anything now: no window, a picture being drawn, a proposal previewed.
	/// @return The reason, or std::nullopt.
	[[nodiscard]] std::optional<std::string> busy() const;
	/// @brief Begins a run that waits, answers "waiting", and asks the window's question for a Person run.
	/// @param waiting The run, without its number yet.
	/// @param checked What its checks found.
	void wait(Waiting waiting, const Checked& checked);
	/// @brief Asks the person in the window's own question whether a run may go ahead.
	/// @param waiting The run.
	/// @param checked What its checks found.
	/// @return The question's id, or 0 if the window is asking something else.
	std::uint64_t askPerson(const Waiting& waiting, const Checked& checked);
	/// @brief Runs a command as its runner, finishes the run, and answers "ran" or "rejected".
	/// @param run The run, begun.
	void execute(const Waiting& run);
	/// @brief Finishes a run that waited, after a decision: runs it if it is still allowed, else answers "rejected".
	/// @param decision The decision.
	void decided(const RunDecision& decision);
	/// @brief Starts the timer that expires runs that waited too long, if one waits and it isn't running.
	void armExpiry();

	format::Runtime& runtime_;     ///< The commands and their runs, and the window.
	RunControl& control_;          ///< Who may run them, and the runs.
	VersionControl& versions_;     ///< The document and its reviewers.
	OffscreenImages& images_;      ///< The pictures, for a run's evidence.
	bool allowed_ = true;          ///< See setAllowed().
	std::vector<Waiting> waiting_; ///< The runs that wait for a decision, oldest first.
	std::uint64_t expiry_ = 0;     ///< The window's timer for control_.expire(), while a run waits.
	Connection changed_ = 0;       ///< On control_.onChanged: forwarded as onChanged.
	Connection decisions_ = 0;     ///< On control_.onDecision: decided().
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< false once destroyed: guards the callbacks.
};

} // namespace affresco::detail
