/// @file
/// @brief format::ComputedValues: the "computed" values, each kept until a place it read changes (ComputedValues.hpp).

#include "ComputedValues.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <span>

namespace affresco::format {

namespace {

/// @brief Whether a value's reads meet one of some places.
/// @param reads The value's reads.
/// @param paths The places.
/// @return true if one of them overlaps one it read.
bool meets(const ReadSet& reads, std::span<const std::string> paths) noexcept {
	return std::ranges::any_of(paths, [&reads](const std::string& written) {
		return std::ranges::any_of(reads.paths,
		                           [&written](const std::string& read) { return overlaps(read, written); });
	});
}

} // namespace

std::string computedPlace(std::string_view name) {
	std::string place(places::computed);
	appendKey(place, name);
	return place;
}

ComputedValues::~ComputedValues() { drop(); }

json ComputedValues::computed(std::string_view name) { return value(name); }

bool ComputedValues::current(const Entry& entry) const noexcept {
	const DocumentContext& context = engine_.document;
	if(entry.version != context.stateVersion()) return false;
	// What reads always (a built variable, a panel's geometry, the clock) holds within the update that evaluated it
	return !entry.reads.always || (entry.built == context.variablesEpoch() && entry.epoch == epoch_);
}

ComputedValues::Entry& ComputedValues::entryOf(std::string_view name) {
	if(const auto found = values_.find(name); found != values_.end()) return found->second;
	if(!settleId_) {
		settleId_ = engine_.tracker.add(settle_);
		ReadSet always;
		always.addAlways(AlwaysReason::Behaviour); // it looks at every update's changes
		engine_.tracker.setReads(settleId_, std::move(always));
	}
	const std::string key(name);
	Entry& entry = values_.try_emplace(key, *this, key).first->second; // a map: its address stays
	entry.id = engine_.tracker.add(entry);
	return entry;
}

const json& ComputedValues::value(std::string_view name) {
	const DocumentContext& context = engine_.document;
	const json& document = engine_.versions.document();
	const auto definitions = document.find("computed");
	if(definitions == document.end() || !definitions->contains(std::string(name)))
		fail("computed", "no computed value '" + std::string(name) + "'");
	Entry& entry = entryOf(name);
	ReadSet* const reading = context.reads();
	if(reading) reading->addPath(computedPlace(name)); // its place, not its reads
	// Between updates a move of the state may reach any value: evaluated again. A value read in place by an evaluation
	// still running isn't replaced: an evaluation spans no move of the state, so it read this value current already
	if(!entry.known || entry.stale || !current(entry)) evaluate(entry, (*definitions)[std::string(name)]);
	// What no change announces (a built variable) may move it with no update reaching it: its reader reads always too
	if(reading && entry.reads.always) reading->addAlways(entry.reads.why);
	return entry.value;
}

void ComputedValues::evaluate(Entry& entry, const json& definition) {
	const DocumentContext& context = engine_.document;
	const std::string& key = entry.name_;
	if(std::ranges::find(computing_, key) != computing_.end())
		fail("computed." + key, "computed values refer to each other in a circle");
	if(computing_.size() >= maxComputedDepth)
		fail("computed." + key, "computed values read each other too deeply (more than 32 levels)");
	computing_.push_back(key);
	++engine_.reconciler.counters().computedEvaluated;
	json result;
	ReadSet read;
	try {
		const DocumentContext::ReadScope scope(context, &read);
		const json none = json::object();
		result = format::evaluate(definition, context.context(*noVariables(), none), "computed." + key);
	} catch(...) {
		computing_.pop_back();
		throw;
	}
	computing_.pop_back();
	const bool differs = entry.valued && result != entry.value;   // after a drop, readers may still show the old value
	if(!entry.valued || differs) entry.value = std::move(result); // equal: the old storage, which may be read in place
	entry.valued = true;
	engine_.tracker.setReads(entry.id, read);
	entry.reads = std::move(read);
	entry.known = true;
	entry.stale = false;
	entry.version = context.stateVersion();
	entry.built = context.variablesEpoch();
	entry.appearance = context.appearance();
	entry.epoch = epoch_;
	if(differs) engine_.document.markPlaceChanged(computedPlace(key));
}

void ComputedValues::makeStale(Entry& entry) {
	if(entry.stale) return;
	entry.stale = true;
	const std::string place = computedPlace(entry.name_);
	for(auto& [name, other] : values_)
		if(other.known && !other.stale &&
		   std::ranges::any_of(other.reads.paths, [&](const std::string& read) { return overlaps(read, place); }))
			makeStale(other);
}

void ComputedValues::placeChanged(std::string_view place) {
	const std::string changed(place);
	for(auto& [name, entry] : values_)
		if(entry.known && !entry.stale && meets(entry.reads, std::span(&changed, 1))) makeStale(entry);
}

void ComputedValues::settle(const StateChanges& changes) {
	++epoch_;
	swept_ = changes.paths.size();
	const std::uint64_t now = engine_.document.stateVersion();
	const std::uint64_t built = engine_.document.variablesEpoch();
	const std::uint64_t appearance = engine_.document.appearance();
	for(auto& [name, entry] : values_) {
		// A new appearance (a locale, a theme) changes what t() and the colours give, though no place it read changed
		if(entry.known && (changes.all || entry.appearance != appearance)) { // dropped, whatever it read
			entry.known = false;
			entry.stale = false;
			// evaluated when next read, maybe late in the walk: what read it before is evaluated again now
			if(!changes.all) engine_.document.markPlaceChanged(computedPlace(name));
			continue;
		}
		if(!entry.known || entry.stale) continue;
		if(meets(entry.reads, changes.paths) || (entry.reads.always && !current(entry))) {
			makeStale(entry);
		}
	}
	for(auto& [name, entry] : values_)
		if(entry.known && !entry.stale) { // untouched: fresh at the state as it is now
			entry.version = now;
			entry.built = built;
		}
}

void ComputedValues::sweepNew(const StateChanges& changes) {
	if(changes.all || swept_ >= changes.paths.size()) return;
	const std::span<const std::string> fresh = std::span(changes.paths).subspan(swept_);
	swept_ = changes.paths.size();
	// A value evaluated in this update read the values it reads as they are now (stale ones are evaluated when read)
	std::vector<std::string> outside;
	for(const std::string& path : fresh)
		if(!contains(places::computed, path, false)) outside.push_back(path);
	for(auto& [name, entry] : values_) {
		if(!entry.known || entry.stale) continue;
		if(entry.epoch == epoch_ ? meets(entry.reads, outside) : meets(entry.reads, fresh)) makeStale(entry);
	}
}

void ComputedValues::updateEntry(Entry& entry, const StateChanges& changes) {
	sweepNew(changes); // the places marked since the settling, by values or queries evaluated before it
	if(!entry.known || !entry.stale) return;
	const json& document = engine_.versions.document();
	const auto definitions = document.find("computed");
	if(definitions == document.end() || !definitions->contains(entry.name_)) return; // gone: a revision drops it
	evaluate(entry, (*definitions)[entry.name_]); // read before: its readers learn now whether it differs
}

void ComputedValues::documentChanged() {
	for(auto& [name, entry] : values_) { // kept, not erased: the tracker may be reaching them
		entry.known = false;
		entry.stale = false;
		entry.reads.clear();
		engine_.tracker.setReads(entry.id, {});
	}
}

std::optional<std::string> ComputedValues::checkKept() {
	const DocumentContext& context = engine_.document;
	const json& document = engine_.versions.document();
	const auto definitions = document.find("computed");
	if(definitions == document.end()) return std::nullopt;
	for(auto& [name, entry] : values_) {
		if(!entry.known || entry.stale || !current(entry) || !definitions->contains(name)) continue;
		// A panel's geometry and the clock move within an update (the layout follows the walk): not comparable here
		if(entry.reads.why == AlwaysReason::Geometry || entry.reads.why == AlwaysReason::Clock) continue;
		computing_.push_back(name);
		json again;
		try {
			const DocumentContext::ReadScope scope(context, nullptr);
			const json none = json::object();
			again = format::evaluate((*definitions)[name], context.context(*noVariables(), none), "computed." + name);
		} catch(const DocumentError& error) {
			computing_.pop_back();
			return "computed." + name + " kept, but evaluating it again fails: " + error.what();
		}
		computing_.pop_back();
		if(again != entry.value) return "computed." + name + " kept a value that differs from evaluating it again";
	}
	return std::nullopt;
}

void ComputedValues::drop() noexcept {
	for(auto& [name, entry] : values_) engine_.tracker.remove(entry.id);
	engine_.tracker.remove(settleId_);
	settleId_ = 0;
}

} // namespace affresco::format
