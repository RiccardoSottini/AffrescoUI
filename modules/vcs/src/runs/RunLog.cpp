/// @file
/// @brief vcs::Runs, the runs: numbering a run, the runs that wait for a decision and their limits, allowing, denying
/// and expiring them, and the log of the newest runs.

#include "affresco/vcs/Runs.hpp"

#include "permissions/Permissions.hpp"

#include <algorithm>
#include <utility>

namespace affresco::vcs {

namespace {

/// @brief The size of a value as JSON text.
/// @param value The value.
/// @return Its bytes.
std::size_t textSize(const json& value) { return value.dump().size(); }

/// @brief A run's arguments as the log keeps them: at most Runs::maxArgumentBytes of JSON text, the largest values
/// shown as Runs::tooLargeToShow until they fit.
/// @param arguments The arguments.
/// @return Them, cut.
json keptArguments(const json& arguments) {
	if(textSize(arguments) <= Runs::maxArgumentBytes) return arguments;
	const json marker = std::string(Runs::tooLargeToShow);
	if(!arguments.is_object()) return json::object();
	json kept = arguments;
	while(textSize(kept) > Runs::maxArgumentBytes) {
		auto largest = kept.end();
		std::size_t largestSize = textSize(marker);
		for(auto it = kept.begin(); it != kept.end(); ++it)
			if(const std::size_t size = textSize(it.value()); size > largestSize) {
				largest = it;
				largestSize = size;
			}
		// Only names are left, too many of them: keep none rather than a partial picture
		if(largest == kept.end()) return json::object();
		largest.value() = marker;
	}
	return kept;
}

/// @brief Tells whether a kept run is Waiting (for a decision, or while it runs).
/// @param state Its state.
/// @return true if it is.
bool waiting(RunState state) { return state == RunState::Waiting; }

} // namespace

RunControl::Begun Runs::begin(std::string_view author, std::string_view command, const json& arguments,
                              std::string_view title, RunVerdict verdict, std::string_view reason) {
	if(verdict == RunVerdict::Deny) return {0, "a denied run is never begun: " + std::string(reason)};
	const std::string channel = detail::channelOf(author);
	const auto waitingRuns = std::ranges::count_if(log_, [](const Entry& e) { return waiting(e.record.state); });
	const auto ofChannel = std::ranges::count_if(
	    log_, [&](const Entry& e) { return waiting(e.record.state) && detail::channelOf(e.record.author) == channel; });
	if(std::cmp_greater_equal(waitingRuns, maxWaitingRuns) ||
	   std::cmp_greater_equal(ofChannel, maxWaitingRunsPerChannel))
		return {0, "too many runs are waiting"};
	RunRecord record;
	record.id = nextRun_++;
	record.author = author;
	record.command = command;
	record.title = title;
	record.arguments = keptArguments(arguments);
	record.reason = reason;
	record.time = clock_();
	record.personOnly = verdict == RunVerdict::Person;
	log_.push_back({std::move(record), verdict != RunVerdict::Allow});
	trimLog();
	onChanged();
	return {log_.back().record.id, {}};
}

Runs::Entry* Runs::undecided(int run) {
	const auto found = std::ranges::find_if(log_, [&](const Entry& e) { return e.record.id == run && e.undecided; });
	return found == log_.end() ? nullptr : &*found;
}

std::optional<std::string> Runs::allowDenial(const Entry& entry, std::string_view reviewer, bool always) const {
	const bool person = reviewer == "user";
	if(!person && !versions_.mayReview(reviewer)) return "'" + std::string(reviewer) + "' may not review runs";
	if(!person && detail::channelOf(reviewer) == detail::channelOf(entry.record.author))
		return std::string("a reviewer can't allow a run from its own channel");
	if(entry.record.personOnly && !person)
		return "run " + std::to_string(entry.record.id) + " only the person may allow";
	if(entry.record.personOnly && always)
		return std::string("a run that reaches outside is asked each time: allow it without \"always\"");
	return std::nullopt;
}

std::optional<std::string> Runs::allow(int run, std::string_view reviewer, bool always, std::string_view duration) {
	Entry* entry = undecided(run);
	if(!entry) return "no run " + std::to_string(run) + " waits";
	if(std::optional<std::string> denial = allowDenial(*entry, reviewer, always)) return denial;
	std::optional<long long> seconds;
	if(always)
		if(std::optional<std::string> refusal = grantSeconds(duration, seconds)) return refusal;
	entry->undecided = false;
	entry->record.reason.clear();
	if(always) addGrant(entry->record, reviewer, seconds);
	onDecision(RunDecision{run, true, std::string(reviewer), {}});
	onChanged();
	return std::nullopt;
}

void Runs::refuse(Entry& entry, RunState state, std::string_view reviewer, std::string_view reason) {
	entry.undecided = false;
	entry.record.state = state;
	entry.record.reason = reason;
	onDecision(RunDecision{entry.record.id, false, std::string(reviewer), std::string(reason)});
}

std::optional<std::string> Runs::deny(int run, std::string_view reason) {
	Entry* entry = undecided(run);
	if(!entry) return "no run " + std::to_string(run) + " waits";
	refuse(*entry, RunState::Denied, {}, reason.empty() ? std::string_view("denied by a reviewer") : reason);
	trimLog();
	onChanged();
	return std::nullopt;
}

void Runs::finish(int run, RunState state, std::string_view reason, std::size_t changes,
                  std::vector<std::string> errors, Undoable undoable) {
	const auto found = std::ranges::find_if(log_, [&](const Entry& e) { return e.record.id == run; });
	if(found == log_.end()) return;
	found->undecided = false;
	found->record.state = state;
	found->record.reason = reason;
	found->record.changes = changes;
	found->record.errors = std::move(errors);
	found->record.undoable = undoable;
	trimLog();
	onChanged();
}

std::vector<RunRecord> Runs::runs() const {
	std::vector<RunRecord> records;
	records.reserve(log_.size());
	for(const Entry& entry : log_) records.push_back(entry.record);
	return records;
}

void Runs::expire(std::int64_t now) {
	bool changed = dropExpiredGrants(now);
	// onDecision may begin or finish runs, which changes the log: find the runs first, then refuse them one by one
	std::vector<int> late;
	for(const Entry& entry : log_)
		if(entry.undecided && now - entry.record.time > runWaitMilliseconds) late.push_back(entry.record.id);
	for(const int run : late)
		if(Entry* entry = undecided(run)) {
			refuse(*entry, RunState::Expired, {}, "nobody answered in 120 s");
			changed = true;
		}
	if(!changed) return;
	trimLog();
	onChanged();
}

void Runs::trimLog() {
	// A waiting run is never dropped: begin() bounds how many there are, so the log stays bounded too
	while(log_.size() > maxKeptRuns) {
		const auto oldest = std::ranges::find_if(log_, [](const Entry& e) { return !waiting(e.record.state); });
		if(oldest == log_.end()) return;
		log_.erase(oldest);
	}
}

} // namespace affresco::vcs
