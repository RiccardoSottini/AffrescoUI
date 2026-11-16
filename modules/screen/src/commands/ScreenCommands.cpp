/// @file
/// @brief detail::ScreenCommands, and affresco::Screen's commands as tools.

#include "commands/ScreenCommands.hpp"
#include "Png.hpp"
#include "ScreenImpl.hpp"
#include "checker/CheckerShapes.hpp" // modules/format/src: the checker's "did you mean"
#include "pictures/ProposalEvidence.hpp"

#include "affresco/core/Assets.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ranges>
#include <string>
#include <utility>

namespace affresco {

namespace detail {

namespace {

/// How often the expiry timer looks for runs that waited too long, in milliseconds, while one waits.
constexpr double expiryCheckMilliseconds = 1000.0;

/// @brief A "rejected" answer.
/// @param error Why.
/// @param run The run it is about, or 0.
/// @return `{"type": "rejected", "error", "run"?}`.
json rejected(const std::string& error, int run = 0) {
	json answer = {{"type", "rejected"}, {"error", error}};
	if(run > 0) answer["run"] = run;
	return answer;
}

/// @brief The channel of an author: what comes before its '/'.
/// @param author E.g. "agent/claude-code".
/// @return E.g. "agent".
std::string_view channelOf(std::string_view author) { return author.substr(0, author.find('/')); }

/// @brief Milliseconds since 1970 (UTC), as the run control counts them.
/// @return Now.
std::int64_t systemNow() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/// @brief Tells whether a text holds another, in any case (ASCII).
/// @param text The text.
/// @param part What to find, in lower case.
/// @return Whether it is there.
bool holds(const json& text, std::string_view part) {
	if(!text.is_string()) return false;
	std::string lower = text.get<std::string>();
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower.find(part) != std::string::npos;
}

/// @brief Tells whether a command matches a getCommands filter.
/// @param entry The command.
/// @param filter The filter, in lower case.
/// @return Whether its name, title, category or description holds it.
bool matches(const json& entry, std::string_view filter) {
	return std::ranges::any_of(std::array{"name", "title", "category", "description"},
	                           [&](const char* key) { return holds(entry.value(key, json()), filter); });
}

/// @brief Tells whether a command declares typed arguments: a map of names to types.
/// @param entry The command.
/// @return Whether it does.
bool typed(const json& entry) {
	const auto arguments = entry.find("arguments");
	return arguments != entry.end() && arguments->is_object() && !arguments->empty();
}

/// @brief Joins what a command reaches for the person: "a", "a and b", "a, b and c".
/// @param uses The lines.
/// @return The text.
std::string joined(const std::vector<std::string>& uses) {
	std::string text;
	for(std::size_t i = 0; i < uses.size(); ++i) {
		if(i > 0) text += i + 1U == uses.size() ? " and " : ", ";
		text += uses[i];
	}
	return text;
}

/// @brief The title a command shows the person.
/// @param entry The command.
/// @param name Its name.
/// @return Its title, or its name without one.
std::string titleOf(const json& entry, const std::string& name) {
	const json& title = entry.value("title", json());
	return title.is_string() && !title.get_ref<const std::string&>().empty() ? title.get<std::string>() : name;
}

/// @brief Adds a picture after a run to its answer: what changed on screen and how it looks now.
/// @param images The pictures.
/// @param before The screen before the run.
/// @param scale Its scale.
/// @param answer The "ran" answer, changed in place.
void addPicture(OffscreenImages& images, const RgbImage& before, int scale, json& answer) {
	RgbImage after;
	if(const std::optional<std::string> error = images.drawShown(scale, after)) {
		answer["pictureError"] = *error; // it ran: the answer says so, without its picture
		return;
	}
	answer["region"] = changedRegion(before, after);
	const std::string png = encodePng(after.width, after.height, after.rgb);
	answer["picture"] = encodeBase64(png);
	answer["width"] = after.width;
	answer["height"] = after.height;
	answer["scale"] = scale;
}

} // namespace

ScreenCommands::ScreenCommands(format::Runtime& runtime, RunControl& control, VersionControl& versions,
                               OffscreenImages& images)
    : runtime_(runtime), control_(control), versions_(versions), images_(images) {
	changed_ = control_.onChanged.connect([this] { onChanged(); });
	decisions_ = control_.onDecision.connect([this](const RunDecision& decision) { decided(decision); });
}

ScreenCommands::~ScreenCommands() {
	*alive_ = false;
	if(Window* window = runtime_.window()) {
		if(expiry_) window->cancelTimer(expiry_);
		for(const Waiting& waiting : waiting_)
			if(waiting.question) window->dismissConfirmation(waiting.question);
	}
	control_.onChanged.disconnect(changed_);
	control_.onDecision.disconnect(decisions_);
}

json ScreenCommands::list(std::string_view author, std::string_view filter, bool all) const {
	std::string lower(filter);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	json typedFirst = json::array(), rest = json::array();
	for(json entry : runtime_.commandList()) {
		if(!entry.is_object() || !entry.value("name", json()).is_string()) continue;
		if(!lower.empty() && !matches(entry, lower)) continue;
		const auto name = entry["name"].get<std::string>();
		const RunJudgement judgement = control_.judge(author, name, runtime_.commandReach(name));
		if(!all && judgement.verdict == RunVerdict::Deny) continue;
		entry["run"] = toString(judgement.verdict);
		if(judgement.verdict != RunVerdict::Allow) entry["reason"] = judgement.reason;
		(typed(entry) ? typedFirst : rest).push_back(std::move(entry));
	}
	for(json& entry : rest) typedFirst.push_back(std::move(entry));
	return {{"type", "commands"}, {"commands", std::move(typedFirst)}};
}

std::optional<std::string> ScreenCommands::busy() const {
	if(!runtime_.window()) return "the screen has no window yet";
	if(images_.rendering()) return "a picture of the screen is being drawn: try again";
	if(versions_.previewing()) return "a proposal is being previewed: commands run on main only";
	return std::nullopt;
}

ScreenCommands::Checked ScreenCommands::check(std::string_view author, const std::string& name, json& arguments) const {
	Checked checked;
	if((checked.refusal = busy())) return checked;
	const std::optional<json> entry = runtime_.command(name);
	if(!entry) {
		std::vector<std::string> names;
		for(const json& command : runtime_.commandList())
			if(command.is_object() && command.value("name", json()).is_string()) names.push_back(command["name"]);
		const std::string hint = format::checker::hintFor(name, names, "", "");
		checked.refusal = "no command '" + name + "'" + (hint.empty() ? "" : "; " + hint);
		return checked;
	}
	checked.entry = *entry;
	if((checked.refusal = runtime_.checkCommandArguments(name, arguments))) return checked;
	if(!checked.entry.value("enabled", true)) {
		checked.refusal = "'" + name + "' is disabled now";
		return checked;
	}
	checked.reach = runtime_.commandReach(name);
	checked.judgement = control_.judge(author, name, checked.reach);
	if(checked.judgement.verdict == RunVerdict::Deny) checked.refusal = checked.judgement.reason;
	return checked;
}

void ScreenCommands::run(std::string_view author, const json& message, Answer answer) {
	if(!answer) answer = [](const json&) {};
	if(!allowed_) return answer(rejected("this host runs no commands"));
	Waiting run{0,
	            std::string(author),
	            message.value("command", ""),
	            message.value("arguments", json::object()),
	            message.value("evidence", "state"),
	            message.value("scale", 1),
	            std::move(answer)};
	if(!run.arguments.is_object()) return run.answer(rejected("runCommand.arguments: expected an object"));
	const Checked checked = check(author, run.command, run.arguments);
	log(LogLevel::Info, "AffrescoUI: '" + run.author + "' asks to run '" + run.command +
	                        "': " + (checked.refusal ? std::string("refused") : toString(checked.judgement.verdict)));
	if(checked.refusal) return run.answer(rejected(*checked.refusal));
	if(checked.judgement.verdict != RunVerdict::Allow) return wait(std::move(run), checked);

	const RunControl::Begun begun =
	    control_.begin(author, run.command, run.arguments, titleOf(checked.entry, run.command),
		               checked.judgement.verdict, checked.judgement.reason);
	if(begun.run == 0) return run.answer(rejected(begun.error));
	run.run = begun.run;
	execute(run);
}

void ScreenCommands::wait(Waiting waiting, const Checked& checked) {
	const RunControl::Begun begun =
	    control_.begin(waiting.author, waiting.command, waiting.arguments, titleOf(checked.entry, waiting.command),
		               checked.judgement.verdict, checked.judgement.reason);
	if(begun.run == 0) return waiting.answer(rejected(begun.error));
	waiting.run = begun.run;
	if(checked.judgement.verdict == RunVerdict::Person) {
		waiting.question = askPerson(waiting, checked);
		if(waiting.question == 0) {
			// Not kept in waiting_ yet: the denial below answers nobody, and the runner hears why here
			(void)control_.deny(waiting.run, "the person is being asked something else");
			return waiting.answer(rejected("the person is being asked something else", waiting.run));
		}
	}
	const json answer = {{"type", "waiting"},
	                     {"run", waiting.run},
	                     {"reason", checked.judgement.reason},
	                     {"expires", runWaitMilliseconds / 1000}};
	const Answer give = waiting.answer;
	waiting_.push_back(std::move(waiting));
	armExpiry();
	give(answer);
}

std::uint64_t ScreenCommands::askPerson(const Waiting& waiting, const Checked& checked) {
	Window* window = runtime_.window();
	if(!window) return 0;
	const std::string uses = joined(checked.reach.personUses);
	const Confirmation question{"Run “" + titleOf(checked.entry, waiting.command) + "”?",
	                            waiting.author + " asks to run it." + (uses.empty() ? "" : " It " + uses + "."),
	                            waiting.arguments.dump(2, ' ', false, json::error_handler_t::replace), "Run",
	                            "Don't run"};
	return window->confirm(question, [this, alive = std::weak_ptr<bool>(alive_), run = waiting.run](bool accepted) {
		if(alive.expired()) return;
		const auto it = std::ranges::find(waiting_, run, &Waiting::run);
		if(it != waiting_.end()) it->question = 0; // answered: nothing to dismiss
		// Only the person's own click or key answers the window's question: their yes, never remembered
		if(accepted) (void)control_.allow(run, "user", false, "");
		else (void)control_.deny(run, "the person said no");
	});
}

void ScreenCommands::decided(const RunDecision& decision) {
	const auto it = std::ranges::find(waiting_, decision.run, &Waiting::run);
	if(it == waiting_.end()) return; // not one of ours, or answered already
	Waiting waiting = std::move(*it);
	waiting_.erase(it);
	if(waiting.question)
		if(Window* window = runtime_.window()) window->dismissConfirmation(waiting.question);
	if(!decision.allowed) {
		log(LogLevel::Info,
		    "AffrescoUI: run " + std::to_string(waiting.run) + " of '" + waiting.command + "' was not allowed");
		return waiting.answer(rejected(decision.reason.empty() ? "denied" : decision.reason, waiting.run));
	}
	// The document, the mode or the rules may have changed while it waited
	std::optional<std::string> refusal =
	    allowed_ ? check(waiting.author, waiting.command, waiting.arguments).refusal : "this host runs no commands";
	if(refusal) {
		control_.finish(waiting.run, RunState::Failed, *refusal, 0, {}, Undoable::None);
		return waiting.answer(rejected(*refusal, waiting.run));
	}
	execute(waiting);
}

void ScreenCommands::execute(const Waiting& run) {
	RgbImage before;
	if(run.evidence == "picture")
		if(const std::optional<std::string> error = images_.drawShown(run.scale, before)) {
			control_.finish(run.run, RunState::Failed, *error, 0, {}, Undoable::None);
			return run.answer(rejected(*error, run.run));
		}
	CommandOutcome outcome = runtime_.runCommandAs({run.run, run.author, run.command, run.arguments});
	if(outcome.refusal) {
		control_.finish(run.run, RunState::Failed, *outcome.refusal, 0, outcome.errors, Undoable::None);
		return run.answer(rejected(*outcome.refusal, run.run));
	}
	control_.finish(run.run, RunState::Ran, "", outcome.changes, outcome.errors, outcome.undoable);
	json answer = {{"type", "ran"},
	               {"run", run.run},
	               {"command", run.command},
	               {"changes", outcome.changes},
	               {"truncated", outcome.truncated},
	               {"records", std::move(outcome.records)},
	               {"errors", std::move(outcome.errors)},
	               {"failed", outcome.failed},
	               {"undoable", toString(outcome.undoable)}};
	if(run.evidence != "none") answer["changed"] = std::move(outcome.changed);
	if(run.evidence == "picture") addPicture(images_, before, run.scale, answer);
	run.answer(answer);
}

void ScreenCommands::armExpiry() {
	Window* window = runtime_.window();
	if(expiry_ || waiting_.empty() || !window) return;
	expiry_ = window->setTimer(expiryCheckMilliseconds, [this, alive = std::weak_ptr<bool>(alive_)] {
		if(alive.expired()) return;
		expiry_ = 0;
		if(const Window* shown = runtime_.window()) control_.expire(now(*shown)); // refusals come back to decided()
		armExpiry();
	});
}

std::int64_t ScreenCommands::now(const Window& window) {
	const std::int64_t system = systemNow();
	if(!epoch_) epoch_.emplace(system, window.clock().seconds());
	const double moved = (window.clock().seconds() - epoch_->second) * 1000.0;
	return std::max(system, epoch_->first + static_cast<std::int64_t>(moved));
}

json ScreenCommands::undo(std::string_view author, const json& message) {
	const json& number = message.value("run", json());
	if(!number.is_number_integer()) return rejected("undoRun.run: expected a run's number");
	const int run = number.get<int>();
	const std::vector<RunRecord> runs = control_.runs();
	const auto record = std::ranges::find(runs, run, &RunRecord::id);
	if(record == runs.end()) return rejected("no run " + std::to_string(run));
	if(channelOf(record->author) != channelOf(author) && !versions_.mayReview(channelOf(author)))
		return rejected("permission denied: run " + std::to_string(run) + " is another channel's");
	if(record->state != RunState::Ran)
		return rejected("run " + std::to_string(run) + " can't be undone: it is " + toString(record->state));
	CommandOutcome outcome = runtime_.undoCommandRun(run);
	if(outcome.refusal) return rejected(*outcome.refusal, run);
	const std::vector<RunRecord> after = control_.runs(); // the runtime may have finished it already
	const auto still = std::ranges::find(after, run, &RunRecord::id);
	if(still != after.end() && still->state == RunState::Ran)
		control_.finish(run, RunState::Undone, "", outcome.changes, outcome.errors, outcome.undoable);
	return {{"type", "undone"}, {"run", run}, {"changed", std::move(outcome.changed)}, {"changes", outcome.changes}};
}

json ScreenCommands::decide(std::string_view reviewer, const json& message) {
	const std::string type = message.value("type", "");
	if(type == "revokeRun") {
		const std::string rule = message.value("rule", "");
		return control_.revoke(rule) ? json{{"type", "done"}} : rejected("no run grant '" + rule + "'");
	}
	const json& number = message.value("run", json());
	if(!number.is_number_integer()) return rejected(type + ".run: expected a run's number");
	const int run = number.get<int>();
	if(type == "denyRun") {
		const std::optional<std::string> refused = control_.deny(run, message.value("reason", ""));
		return refused ? rejected(*refused, run) : json{{"type", "done"}, {"run", run}};
	}
	// The run control refuses these too; a run's authority must never come from its own channel, so it is held here
	// as well, where the reviewer's name is known
	const std::vector<RunRecord> runs = control_.runs();
	if(const auto record = std::ranges::find(runs, run, &RunRecord::id); record != runs.end()) {
		if(reviewer != "user" && record->personOnly) return rejected("only the person may allow this run", run);
		if(reviewer != "user" && channelOf(record->author) == channelOf(reviewer))
			return rejected("a run is never allowed from its own channel", run);
	}
	const std::optional<std::string> refused =
	    control_.allow(run, reviewer, message.value("always", false), message.value("for", ""));
	return refused ? rejected(*refused, run) : json{{"type", "done"}, {"run", run}};
}

json ScreenCommands::pending(std::string_view reader) const {
	const bool limited = versions_.readable(reader).has_value();
	json runs = json::array(), grants = json::array();
	for(const RunRecord& run : control_.runs()) {
		json entry = toJson(run);
		if(limited) { // what a run was asked with, and what its steps said, can quote values the reader may not read
			entry.erase("arguments");
			entry.erase("errors");
		}
		runs.push_back(std::move(entry));
	}
	for(const Grant& grant : control_.grants()) grants.push_back(toJson(grant));
	return {{"runs", std::move(runs)}, {"runGrants", std::move(grants)}};
}

void ScreenCommands::setAllowed(bool allowed) {
	allowed_ = allowed;
	if(allowed) return;
	std::vector<int> runs;
	for(const Waiting& waiting : waiting_) runs.push_back(waiting.run);
	for(const int run : runs) (void)control_.deny(run, "this host runs no commands");
}

} // namespace detail

void Screen::allowCommandRuns(bool allow) {
	impl_->commands.setAllowed(allow); // denies the runs that wait while their peers still hear it
	impl_->commandsAllowed = allow;
	for(const auto& peer : impl_->peers) peer->session->setCommands(allow ? &impl_->commands : nullptr);
}

bool Screen::allowRun(int run, bool always, std::string& error) {
	const json answer =
	    impl_->commands.decide("user", {{"type", "allowRun"}, {"run", run}, {"always", always}, {"for", ""}});
	if(answer.value("type", "") == "done") return true;
	error = answer.value("error", "");
	return false;
}

bool Screen::denyRun(int run) {
	return impl_->commands.decide("user", {{"type", "denyRun"}, {"run", run}, {"reason", "the person said no"}})
	           .value("type", "") == "done";
}

json Screen::commandsJson() const {
	json commands = impl_->commands.pending("user");
	commands["commands"] = impl_->commands.list("user", "", true)["commands"];
	return commands;
}

} // namespace affresco
