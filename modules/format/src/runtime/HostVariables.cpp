/// @file
/// @brief format::HostVariables: the globals of expressions and the host's variables, built when read.

#include "HostVariables.hpp"
#include "Engine.hpp"

#include <algorithm>

namespace affresco::format {

namespace {

/// @brief The document's "window.menuBar", for expressions ("window.menuBar"): the drawn menu bar's default.
/// @param document The document.
/// @return Its list of {menu, title}, or [] if it has none.
json menuBarOf(const json& document) {
	const auto window = document.find("window");
	if(window == document.end() || !window->is_object()) return json::array();
	const auto menuBar = window->find("menuBar");
	return menuBar != window->end() && menuBar->is_array() ? *menuBar : json::array();
}

/// @brief The `window` global: the window's size, scale, colour scheme, ...
/// @param window The window, if there is one.
/// @param document The document.
/// @return The object; empty without a window.
json windowOf(const Window* window, const json& document) {
	if(!window) return json::object();
	return {{"width", window->width()},
	        {"height", window->height()},
	        {"scale", window->contentScale()},
	        {"colorScheme", window->colorScheme() == ColorScheme::Dark ? "dark" : "light"},
	        {"nativeMenuBar", window->nativeMenuBar()},
	        {"reducedMotion", window->reducedMotion()},
	        {"menuBar", menuBarOf(document)},
	        {"capsLock", window->capsLock()}};
}

/// @brief Where a global's name is in globalNames.
/// @param name The name.
/// @return Its index; globalNames.size() if it isn't one.
std::size_t globalIndex(std::string_view name) {
	std::size_t index = 0;
	while(index < globalNames.size() && globalNames[index] != name) ++index; // NOLINT(*-constant-array-index)
	return index;
}

/// @brief Whether a set of reads reads a place, or a place inside or around it.
/// @param reads The reads.
/// @param place The place.
/// @return true if one of its places overlaps it.
bool reads(const ReadSet& reads, std::string_view place) {
	return std::ranges::any_of(reads.paths, [place](const std::string& read) { return overlaps(read, place); });
}

} // namespace

json HostVariables::buildGlobal(std::size_t index) const {
	const VersionControl& versions = engine_.versions;
	const DocumentContext& context = engine_.document;
	switch(index) {
	case 0:
		return windowOf(engine_.window, versions.document());
	case 1:
		return toJson(versions.head());
	case 2: {
		json list = json::array();
		for(const ProposalInfo& proposal : versions.proposals()) list.push_back(toJson(proposal));
		return list;
	}
	case 3: {
		json log = json::array();
		for(const CommitInfo& commit : versions.log()) log.push_back(toJson(commit));
		return log;
	}
	case 4: {
		const auto previewing = versions.previewing();
		return previewing ? json(*previewing) : json();
	}
	case 5:
		return toString(versions.mode());
	case 6:
		return context.theme();
	case 7:
		return context.strings().locale();
	default:
		return context.screen();
	}
}

const json* HostVariables::global(std::string_view name) const {
	const std::size_t index = globalIndex(name);
	if(index == globalNames.size()) return nullptr;
	if(ReadSet* reading = engine_.document.reads())
		reading->addPath(std::string(places::globals) + "/" + std::string(name));
	std::optional<json>& value = globals_[index];
	if(!value) value = buildGlobal(index);
	return &*value;
}

void HostVariables::add(const std::string& name, std::function<json()> build) {
	Variable& variable = variables_[name];
	variable.place = std::string(places::variables) + "/" + name;
	variable.build = std::move(build);
	variable.stale = true;
}

const json* HostVariables::variable(std::string_view name) const {
	const auto found = variables_.find(name);
	if(found == variables_.end()) return nullptr;
	Variable& variable = found->second;
	ReadSet* reading = engine_.document.reads();
	if(reading) reading->addPath(variable.place);
	if(variable.stale) {
		if(std::ranges::find(building_, name) != building_.end()) return nullptr; // it reads itself
		building_.emplace_back(name);
		++engine_.reconciler.counters().variablesBuilt;
		ReadSet built;
		json value;
		try {
			const DocumentContext::ReadScope scope(engine_.document, &built);
			value = variable.build();
		} catch(...) {
			building_.pop_back();
			throw;
		}
		building_.pop_back();
		variable.value = std::move(value);
		variable.reads = std::move(built);
		variable.stale = false;
	}
	// One built from the clock or a panel's geometry is built again each update: what reads it reads always too
	if(reading && variable.reads.always) reading->addAlways(variable.reads.why);
	return &variable.value;
}

void HostVariables::makeStale(Variable& variable) const {
	if(variable.stale) return;
	variable.stale = true;
	engine_.document.markPlaceChanged(variable.place); // and, through placeMarked(), what read it
}

void HostVariables::placeMarked(std::string_view place) const {
	if(place.starts_with(places::globals) && place.size() > places::globals.size()) {
		const std::string_view name = place.substr(places::globals.size() + 1);
		if(const std::size_t index = globalIndex(name.substr(0, name.find('/'))); index < globalNames.size())
			globals_.at(index).reset();
	}
	for(auto& [name, variable] : variables_) {
		if(variable.place == place) variable.stale = true; // marked itself: its place is marked already
		else if(!variable.stale && reads(variable.reads, place)) makeStale(variable);
	}
}

void HostVariables::everythingChanged() const noexcept {
	for(std::optional<json>& value : globals_) value.reset();
	for(auto& [name, variable] : variables_) variable.stale = true;
}

void HostVariables::updateStarted() const noexcept {
	for(auto& [name, variable] : variables_)
		if(variable.reads.always) variable.stale = true;
}

bool HostVariables::built(std::string_view name) const {
	const auto found = variables_.find(name);
	return found != variables_.end() && !found->second.stale;
}

} // namespace affresco::format
