#pragma once

/// @file
/// @brief An app's commands as tools for peers: what a command reaches, how a run is judged and recorded, and the
/// RunControl and CommandService interfaces the modules meet through.
///
/// A command is an entry of a document's `actions` that describes itself (in Lisp, a `defcommand`); plain actions are
/// never tools. A run is one execution of a command that a peer asked for. It always executes under the runner's name
/// (the author its channel gives it, e.g. "agent/claude-code"), never the person's, even after the person said yes:
/// a run is never authority. Runs are numbered from 1, per screen and per start of the host.
///
/// The modules: vcs::Runs implements RunControl (who may run what, the runs that wait, the log); format::Runtime
/// lists, checks and runs commands under a runner's authority; the screen's ScreenCommands implements CommandService
/// over both, for channel::Session. Everything here is used on the main thread only.

#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/core/VersionControl.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// The runs a RunControl keeps and the `runs` variable lists, the newest ones (waiting or done).
inline constexpr std::size_t maxKeptRuns = 64;
/// The runs whose changes are kept for undo, the newest ones.
inline constexpr std::size_t maxUndoableRuns = 32;
/// Runs that may wait for a decision at once: RunControl::begin refuses more.
inline constexpr std::size_t maxWaitingRuns = 16;
/// How long a run waits for a decision before RunControl::expire refuses it, in milliseconds.
inline constexpr std::int64_t runWaitMilliseconds = 120'000;

/// @brief What a command can reach, worked out from its steps (format::Runtime::commandReach()): what decides whether
/// a peer may run it at all, and whether the person must say yes each time.
struct CommandReach {
	/// It reaches a control or review step, a native or unknown action, or a `run` whose name is computed: no peer
	/// may run it.
	bool control = false;
	/// The first such thing it reaches, for the reason (e.g. "accept", or "a computed run"); empty when control is
	/// false.
	std::string controlStep = {};
	/// The service uses it reaches that write or run, as lines for the person, e.g. "invoke files.write (writes
	/// files in 'workspace')": the person says yes to each run, in the window's own question.
	std::vector<std::string> personUses = {};
	bool writesRecords = false; ///< It writes records (insert, update, delete, undoData, redoData, importCsv).
};

/// @brief What would happen if a runner ran a command now (RunControl::judge()).
enum class RunVerdict {
	Allow,  ///< It runs at once.
	Ask,    ///< It waits for a reviewer or the person (allowRun, denyRun).
	Person, ///< It waits for the person's yes in the window's own question, each time, never remembered.
	Deny,   ///< It never runs.
};

/// @brief A RunControl's verdict on a run, and why.
struct RunJudgement {
	RunVerdict verdict = RunVerdict::Deny; ///< The verdict.
	std::string reason = {}; ///< Why, for the runner and the person (e.g. "plan mode: commands don't run").
};

/// @brief Where a run is.
enum class RunState {
	Waiting, ///< Waiting for a decision, or running now.
	Ran,     ///< It ran; RunRecord::errors may still hold what its steps reported.
	Failed,  ///< It was refused while it ran, and rolled back.
	Denied,  ///< A rule, a reviewer or the person said no.
	Expired, ///< Nobody decided in time (runWaitMilliseconds).
	Undone,  ///< It ran, and was undone since.
};

/// @brief How much of what a run changed undo can put back.
enum class Undoable {
	None,    ///< Nothing (it changed nothing, or what it changed is no longer kept).
	Partial, ///< Its state and records, but it also reached outside them (a sound played, a file a service wrote).
	State,   ///< All it changed: state paths and records.
};

/// @brief One run, waiting or done: an entry of the `runs` variable and of a `review` message.
struct RunRecord {
	int id = 0;                           ///< Its number, from 1.
	std::string author = {};              ///< The runner, e.g. "agent/claude-code".
	std::string command = {};             ///< The command's name.
	std::string title = {};               ///< The command's title, for the person.
	json arguments = json::object();      ///< The arguments it was asked with, defaults filled in.
	RunState state = RunState::Waiting;   ///< Where it is.
	std::string reason = {};              ///< Why it waits, or why it was denied, failed or expired.
	std::int64_t time = 0;                ///< When it was asked for, in milliseconds since 1970 (UTC).
	std::size_t changes = 0;              ///< How many state paths and records it changed.
	std::vector<std::string> errors = {}; ///< What its steps reported while it ran.
	Undoable undoable = Undoable::None;   ///< What undo can put back.
	bool personOnly = false; ///< Only the person may allow it (RunVerdict::Person): no reviewer's yes counts.
};

/// @brief The name of a verdict.
/// @param verdict The verdict.
/// @return "allow", "ask", "person" or "deny".
inline std::string toString(RunVerdict verdict) {
	switch(verdict) {
	case RunVerdict::Allow:
		return "allow";
	case RunVerdict::Ask:
		return "ask";
	case RunVerdict::Person:
		return "person";
	case RunVerdict::Deny:
		break;
	}
	return "deny";
}

/// @brief The name of a run's state.
/// @param state The state.
/// @return "waiting", "ran", "failed", "denied", "expired" or "undone".
inline std::string toString(RunState state) {
	switch(state) {
	case RunState::Waiting:
		return "waiting";
	case RunState::Ran:
		return "ran";
	case RunState::Failed:
		return "failed";
	case RunState::Denied:
		return "denied";
	case RunState::Expired:
		return "expired";
	case RunState::Undone:
		break;
	}
	return "undone";
}

/// @brief The name of what undo can put back.
/// @param undoable It.
/// @return "none", "partial" or "state".
inline std::string toString(Undoable undoable) {
	return undoable == Undoable::State ? "state" : undoable == Undoable::Partial ? "partial" : "none";
}

/// @brief Converts a run to its JSON: an entry of the `runs` variable and of a `review` message's `runs`.
/// @param r The run.
/// @return `{"id", "author", "command", "title", "arguments", "state", "reason", "time", "changes", "errors",
/// "undoable", "personOnly"}`.
inline json toJson(const RunRecord& r) {
	return {{"id", r.id},
	        {"author", r.author},
	        {"command", r.command},
	        {"title", r.title},
	        {"arguments", r.arguments},
	        {"state", toString(r.state)},
	        {"reason", r.reason},
	        {"time", r.time},
	        {"changes", r.changes},
	        {"errors", r.errors},
	        {"undoable", toString(r.undoable)},
	        {"personOnly", r.personOnly}};
}

/// @brief A decision on a waiting run (RunControl::onDecision).
struct RunDecision {
	int run = 0;               ///< The run.
	bool allowed = false;      ///< true: allowed, so it runs now; false: denied or expired.
	std::string reviewer = {}; ///< Who decided ("user" for the person); empty when it expired.
	std::string reason = {};   ///< Why it was denied or expired; empty when allowed.
};

/// @brief What to run: a command, under a runner's authority (format::Runtime::runCommandAs()).
struct CommandRun {
	int run = 0;                     ///< The run's number (RunControl::begin()).
	std::string author = {};         ///< The runner: every step and record write is judged as this author's.
	std::string command = {};        ///< The command's name.
	json arguments = json::object(); ///< Its arguments, checked and with their defaults filled in.
};

/// @brief What happened when a command ran, or was undone (format::Runtime::runCommandAs(), undoCommandRun()). The
/// command function's own return value is never part of it.
struct CommandOutcome {
	bool started = false;                 ///< Its steps began to run.
	bool failed = false;                  ///< It was refused while it ran, and rolled back whole.
	std::vector<std::string> errors = {}; ///< What its steps reported.
	/// What it changed in the live state, as a JSON Patch whose paths start with "/state", at most 64 operations and
	/// 16 KiB; past that, only the paths, and `truncated`.
	json changed = json::array();
	std::size_t changes = 0;       ///< How many state paths and records it changed (what a limited reader is told).
	bool truncated = false;        ///< `changed` holds only the paths.
	json records = json::object(); ///< The records it wrote, as counts per collection: never a record.
	Undoable undoable = Undoable::None; ///< What undo can put back.
	/// Set when it never started, or was rolled back: why. A run that ran keeps this empty, even with errors.
	std::optional<std::string> refusal = std::nullopt;
};

/// @brief Who may run which command, and the runs that wait for a decision or are done: the rules in
/// `permissions.run`, the mode and the run grants, applied to runs. Implemented by vcs::Runs.
///
/// It decides and records; it never runs anything. Its callers: the screen's CommandService (judge, begin, finish,
/// expire, stateDenial), the review steps allowRun and denyRun (format), and the `runs` and `runGrants` variables.
/// Main thread only.
class RunControl {
public:
	virtual ~RunControl() = default;

	/// @brief What a run started now would come to: the rules, the mode and the grants, applied to what the command
	/// reaches (deny rules; a control reach; plan mode; a service use that writes or runs; a reviewer; ask rules;
	/// allow rules and grants; no `permissions` at all; else ask).
	/// @param author The runner.
	/// @param command The command's name.
	/// @param reach What the command reaches (format::Runtime::commandReach()).
	/// @return The verdict, and why.
	[[nodiscard]] virtual RunJudgement judge(std::string_view author, std::string_view command,
	                                         const CommandReach& reach) const = 0;

	/// @brief What begin() gives: the run's number, or why it was refused.
	struct Begun {
		int run = 0;            ///< The run's number, from 1; 0 when refused.
		std::string error = {}; ///< Why it was refused (too many runs wait: maxWaitingRuns); empty when begun.
	};
	/// @brief Records a run that starts now: Waiting until finish(), allow() or deny(). A run judged Ask or Person
	/// waits for a decision (onDecision tells it); one judged Allow is run by the caller, which then calls finish().
	/// onChanged fires.
	/// @param author The runner.
	/// @param command The command's name.
	/// @param arguments Its arguments, checked.
	/// @param title The command's title, for the person.
	/// @param verdict What judge() said (never Deny: a denied run is never begun).
	/// @param reason What judge() said, why.
	/// @return The run's number, or why it was refused.
	virtual Begun begin(std::string_view author, std::string_view command, const json& arguments,
	                    std::string_view title, RunVerdict verdict, std::string_view reason) = 0;

	/// @brief Allows a waiting run: onDecision fires (allowed), then onChanged. With `always`, a grant also lets
	/// that channel run that command without asking (a rule like "agent(run:move-card)"), for `duration`.
	/// @param run The run.
	/// @param reviewer Who allows it: "user" for the person (the allowRun step, the window's question), else a
	/// reviewer's full name; never the runner's own channel, and only the person for a run that is personOnly.
	/// @param always Also grant it, so runs like it no longer ask (never for a personOnly run).
	/// @param duration How long the grant lasts ("30m", "8h", "7d"); empty: as the review policy says.
	/// @return Why it was refused, or std::nullopt.
	virtual std::optional<std::string> allow(int run, std::string_view reviewer, bool always,
	                                         std::string_view duration) = 0;
	/// @brief Denies a waiting run: onDecision fires (denied), then onChanged.
	/// @param run The run.
	/// @param reason Why, for the runner; may be empty.
	/// @return Why it was refused (no such run waits), or std::nullopt.
	virtual std::optional<std::string> deny(int run, std::string_view reason) = 0;
	/// @brief Records how a run that started ended, or that it was undone: onChanged fires.
	/// @param run The run.
	/// @param state Ran, Failed or Undone.
	/// @param reason Why it failed; may be empty.
	/// @param changes How many state paths and records it changed.
	/// @param errors What its steps reported.
	/// @param undoable What undo can put back.
	virtual void finish(int run, RunState state, std::string_view reason, std::size_t changes,
	                    std::vector<std::string> errors, Undoable undoable) = 0;

	/// @brief The runs kept, waiting or done, oldest first (the newest maxKeptRuns).
	[[nodiscard]] virtual std::vector<RunRecord> runs() const = 0;
	/// @brief The run grants ("always" on allow()), oldest first; their rules like "agent(run:move-card)".
	[[nodiscard]] virtual std::vector<Grant> grants() const = 0;
	/// @brief Withdraws a run grant: onChanged fires.
	/// @param rule The grant's rule.
	/// @return false if there is no such grant.
	virtual bool revoke(std::string_view rule) = 0;
	/// @brief Refuses the runs that waited longer than runWaitMilliseconds, and drops grants that expired: for each
	/// run, onDecision fires (not allowed, no reviewer); onChanged fires once if anything changed.
	/// @param now Milliseconds since 1970 (UTC).
	virtual void expire(std::int64_t now) = 0;
	/// @brief Judges what a run did to the live state, by effect: the runner's data rules on what changed.
	/// @param author The runner.
	/// @param stateBefore The state before the run.
	/// @param stateAfter The state after it.
	/// @return Why the change is refused (the run is then rolled back whole), or std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> stateDenial(std::string_view author, const json& stateBefore,
	                                                             const json& stateAfter) const = 0;

	Signal<const RunDecision&> onDecision; ///< A waiting run was allowed, denied or expired.
	Signal<> onChanged;                    ///< The runs or the run grants changed.
};

/// @brief What a peer's session asks about commands: list them, run one, undo a run, decide on a waiting run, and
/// what every peer's `review` message carries. Implemented by the screen's ScreenCommands; called by
/// channel::Session. Main thread only.
///
/// Each answer is a whole protocol message (SCREEN_FORMAT.md, "Live screens") without its "id" and "revision", which
/// the session adds: "commands", "ran", "waiting", "undone", "done" or "rejected" (`{"type": "rejected", "error"}`).
class CommandService {
public:
	virtual ~CommandService() = default;

	/// @brief Gives an answer to a run: once at once, and once more after a wait.
	using Answer = std::function<void(const json& answer)>;

	/// @brief The commands of main's document as this peer may run them now: `getCommands`.
	/// @param author The peer's author (its channel, then the name it gives itself).
	/// @param filter Only commands whose name or title contains it; empty: all.
	/// @param all Also the commands the peer may never run (`run` of "deny").
	/// @return `{"type": "commands", "commands": [{name, title, category, description, arguments, enabled, run,
	/// reason?, key?}]}`, or a "rejected".
	[[nodiscard]] virtual json list(std::string_view author, std::string_view filter, bool all) const = 0;
	/// @brief Runs a command for a peer: `runCommand`. `answer` is called once now ("ran", "waiting" or
	/// "rejected") and, for a run that waited, once more when it ran or was refused ("ran" or "rejected", with
	/// `run`). The caller makes `answer` safe to call after it went away (a Session guards it with a token it drops in
	/// its destructor).
	/// @param author The runner.
	/// @param message The `runCommand` message: `command`, `arguments`?, `evidence`?, `scale`?.
	/// @param answer Gives the answers.
	virtual void run(std::string_view author, const json& message, Answer answer) = 0;
	/// @brief Undoes a run of this peer's own: `undoRun`.
	/// @param author The peer's author.
	/// @param message The `undoRun` message: `run`.
	/// @return "undone" or "rejected".
	virtual json undo(std::string_view author, const json& message) = 0;
	/// @brief A reviewer's decision on runs: `allowRun` (`run`, `always`?, `for`?), `denyRun` (`run`, `reason`?)
	/// or `revokeRun` (`rule`). Never from the runner's own channel, and a personOnly run never from a peer.
	/// @param reviewer The peer's author.
	/// @param message The message.
	/// @return "done" or "rejected".
	virtual json decide(std::string_view reviewer, const json& message) = 0;
	/// @brief What a `review` message and a `getDocument` answer carry about runs, as a reader may see them.
	/// @param reader The peer's author.
	/// @return `{"runs": [...], "runGrants": [...]}` (toJson(RunRecord), toJson(Grant)).
	[[nodiscard]] virtual json pending(std::string_view reader) const = 0;

	Signal<> onChanged; ///< What pending() gives changed: every session sends its `review` again.
};

} // namespace affresco
