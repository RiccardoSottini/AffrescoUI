/// @file
/// @brief format::CommandRuns: runs a command as its runner, rolls a refused run back, and undoes a run.

#include "CommandRuns.hpp"

#include "Engine.hpp"
#include "Contributions.hpp"

#include <affresco/data/Store.hpp>

#include <utility>

namespace affresco::format {

namespace {

/// @brief Keeps a flag set while it lives.
class Raised {
public:
	/// @brief Sets the flag.
	/// @param flag The flag; outlives this object.
	explicit Raised(bool& flag) noexcept : flag_(flag) { flag_ = true; }
	Raised(const Raised&) = delete;
	Raised& operator=(const Raised&) = delete;
	Raised(Raised&&) = delete;
	Raised& operator=(Raised&&) = delete;
	/// @brief Clears the flag.
	~Raised() { flag_ = false; }

private:
	bool& flag_; ///< The flag.
};

/// @brief Watches the steps for one that reaches outside the state and the records while it lives
/// (Steps::watchOutside).
class OutsideWatch {
public:
	/// @brief Starts the watch.
	/// @param runner The steps.
	/// @param seen Set when such a step runs; outlives this object.
	OutsideWatch(Steps& runner, bool& seen) noexcept : runner_(runner) { runner_.watchOutside(&seen); }
	OutsideWatch(const OutsideWatch&) = delete;
	OutsideWatch& operator=(const OutsideWatch&) = delete;
	OutsideWatch(OutsideWatch&&) = delete;
	OutsideWatch& operator=(OutsideWatch&&) = delete;
	/// @brief Ends the watch.
	~OutsideWatch() { runner_.watchOutside(nullptr); }

private:
	Steps& runner_; ///< The steps.
};

/// @brief Collects the errors the runtime reports while it lives, within CommandRuns' limits.
class ErrorCollector {
public:
	/// @brief Starts collecting.
	/// @param engine The runtime; outlives this object.
	/// @param errors Where they go; outlives this object.
	ErrorCollector(Engine& engine, std::vector<std::string>& errors)
	    : engine_(engine), connection_(engine.owner.onError.connect([&errors](const std::string& error) {
		      if(errors.size() >= CommandRuns::maxErrors) return;
		      std::string kept = error;
		      if(kept.size() > CommandRuns::maxErrorBytes) {
			      std::size_t end = CommandRuns::maxErrorBytes;
			      while(end > 0 && (static_cast<unsigned char>(kept[end]) & 0xC0U) == 0x80U) --end; // whole characters
			      kept.resize(end);
		      }
		      errors.push_back(std::move(kept));
	      })) {}
	ErrorCollector(const ErrorCollector&) = delete;
	ErrorCollector& operator=(const ErrorCollector&) = delete;
	ErrorCollector(ErrorCollector&&) = delete;
	ErrorCollector& operator=(ErrorCollector&&) = delete;
	/// @brief Stops collecting.
	~ErrorCollector() { engine_.owner.onError.disconnect(connection_); }

private:
	Engine& engine_;        ///< The runtime.
	Connection connection_; ///< On its onError.
};

} // namespace

std::optional<std::string> CommandRuns::startRefusal(const CommandRun& run) const {
	if(run.author.empty() || isPersonsAuthor(run.author))
		return "a run executes under its runner's name, never the person's: \"" + run.author + "\" can't run commands";
	if(!engine_.window) return std::string("this screen has no window");
	if(running_) return std::string("a command is running already: runs run one at a time");
	if(engine_.versions.previewing()) return std::string("a proposal is previewed: commands run when it ends");
	const json& document = engine_.versions.document();
	const auto actions = document.find("actions");
	if(actions == document.end() || !actions->is_object() || !actions->contains(run.command) ||
	   !isCommand((*actions)[run.command]) || !(*actions)[run.command].contains("do"))
		return "'" + run.command + "' is not a command of this screen";
	return std::nullopt;
}

std::optional<std::string> CommandRuns::rollBack(const json& before, const std::vector<std::int64_t>& groups) {
	engine_.versions.state() = before;
	engine_.document.markStateChanged();
	engine_.reconciler.requestStateUpdate();
	data::Store& store = engine_.data.store();
	try {
		for(std::size_t at = 0; at < groups.size(); ++at) static_cast<void>(store.undo());
	} catch(const data::DataError& error) {
		return std::string("its records couldn't all be put back: ") + error.what();
	}
	return std::nullopt;
}

void CommandRuns::describe(const json& patch, CommandOutcome& outcome) {
	json changed = json::array();
	for(json operation : patch) {
		operation["path"] = "/state" + operation["path"].get<std::string>();
		changed.push_back(std::move(operation));
	}
	if(changed.size() <= maxChangedOperations && changed.dump().size() <= maxChangedBytes) {
		outcome.changed = std::move(changed);
		return;
	}
	outcome.truncated = true; // the paths, as many as fit: `changes` still counts them all
	outcome.changed = json::array();
	std::size_t bytes = 2;
	for(const json& operation : changed) {
		json path = {{"op", operation["op"]}, {"path", operation["path"]}};
		bytes += path.dump().size() + 1U;
		if(bytes > maxChangedBytes) break;
		outcome.changed.push_back(std::move(path));
	}
}

CommandOutcome CommandRuns::run(const CommandRun& run) {
	CommandOutcome outcome;
	if(std::optional<std::string> why = startRefusal(run)) {
		outcome.refusal = std::move(why);
		return outcome;
	}
	const Raised running(running_);
	outcome.started = true;
	const data::Store& store = engine_.data.store();
	engine_.data.endGroup(); // the run's record writes are groups of their own, for its undo
	const std::int64_t journalBefore = newestSequence(store);
	const json before = engine_.versions.state();
	const json arguments = run.arguments.is_object() ? run.arguments : json::object();
	const json event = {{"type", "command"}, {"run", run.run}, {"author", run.author}};
	std::optional<std::string> ended;
	bool outside = false;
	{
		// Every step it reaches, and what those schedule, is judged as the runner's; its record writes are under the
		// runner's name (DataLayer::author())
		const StepAuthority runner{{run.author}};
		const AuthorityScope authority(engine_, &runner);
		const ErrorCollector collector(engine_, outcome.errors);
		const OutsideWatch watch(engine_.runner, outside);
		ended = engine_.runner.attempt(json(run.command), engine_.window->root(), event,
		                               Scope{nullptr, nullptr, arguments, {}, {}}, "commands." + run.command);
		if(ended) engine_.report(*ended); // logged, and among its errors
	}
	engine_.data.endGroup();
	const RecordWrites writes = recordWritesSince(store, journalBefore);
	const json& state = engine_.versions.state();
	std::optional<std::string> refusal = std::move(ended);
	if(!refusal && engine_.runControl) refusal = engine_.runControl->stateDenial(run.author, before, state);
	if(refusal) { // refused while it ran: nothing of it stays
		if(std::optional<std::string> partly = rollBack(before, writes.groups)) outcome.errors.push_back(*partly);
		outcome.failed = true;
		outcome.refusal = std::move(refusal);
		return outcome;
	}
	const json patch = json::diff(before, state);
	describe(patch, outcome);
	std::vector<RunUndo::Place> places = RunUndo::placesOf(before, state, patch);
	outcome.records = writes.counts;
	outcome.changes = places.size() + writes.count;
	outcome.undoable = outcome.changes == 0 ? Undoable::None : outside ? Undoable::Partial : Undoable::State;
	undo_.keep({run.run, std::move(places), writes.groups, outcome.changes, outcome.undoable});
	if(const RunUndo::Kept* kept = undo_.find(run.run); kept && kept->tooLarge) outcome.undoable = Undoable::None;
	return outcome;
}

CommandOutcome CommandRuns::undo(int run) {
	CommandOutcome outcome;
	const RunUndo::Kept* kept = undo_.find(run);
	const std::size_t changes = kept ? kept->changes : 0;
	if(std::optional<std::string> why = undo_.undo(run, engine_.versions.state(), engine_.data.store())) {
		outcome.refusal = std::move(why);
		return outcome;
	}
	engine_.document.markStateChanged();
	engine_.reconciler.requestStateUpdate();
	outcome.changes = changes;
	if(engine_.runControl) engine_.runControl->finish(run, RunState::Undone, "", changes, {}, Undoable::None);
	return outcome;
}

} // namespace affresco::format
