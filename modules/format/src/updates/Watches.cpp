/// @file
/// @brief format::Watches: the "watch" handlers.

#include "Watches.hpp"
#include "Engine.hpp"

#include <algorithm>

namespace affresco::format {

Watches::~Watches() {
	for(const auto& entry : entries_) engine_.tracker.remove(entry->id);
}

void Watches::follow(const json& section) {
	std::vector<std::unique_ptr<Entry>> next;
	for(const auto& [path, handler] : section.items()) {
		const auto kept =
		    std::ranges::find_if(entries_, [&](const auto& entry) { return entry && entry->path == path; });
		if(kept != entries_.end()) {
			next.push_back(std::move(*kept));
			continue;
		}
		next.push_back(std::make_unique<Entry>(path));
		next.back()->id = engine_.tracker.add(*next.back());
	}
	for(const auto& gone : entries_)
		if(gone) engine_.tracker.remove(gone->id);
	entries_ = std::move(next);
	section_ = section;
}

void Watches::look(Entry& entry, const json& handler, Panel& root) {
	ReadSet reads;
	json value;
	{
		const DocumentContext::ReadScope scope(engine_.document, &reads);
		value =
		    format::lookup(entry.path, engine_.document.context(*noVariables(), json::object()), "watch." + entry.path);
	}
	engine_.tracker.setReads(entry.id, std::move(reads));
	entry.due = false;
	if(entry.seen && entry.value != value)
		engine_.scheduler.defer(engine_.runner.later(handler, root,
		                                             {{"type", "change"}, {"value", value}, {"previous", entry.value}},
		                                             noVariables(), "watch." + entry.path));
	entry.value = std::move(value);
	entry.seen = true;
}

// "watch": a handler runs after the update in which its value changed. Not while handlers are suspended: the values
// seen then are never compared, so a document shown for a moment changes nothing. Only the paths whose reads the
// update reached are looked up again, unless it was full.
void Watches::watch(Panel& root) {
	const json& document = engine_.versions.document();
	const auto found = document.find("watch");
	if(found == document.end()) {
		if(!entries_.empty()) follow(json::object());
		return;
	}
	const json& section = object(*found, "watch");
	const bool full = engine_.reconciler.statistics().full;
	if((full || entries_.empty()) && section != section_) follow(section); // a revision's update is full
	for(const auto& entry : entries_)
		if(full || entry->due || !entry->seen) look(*entry, section[entry->path], root);
}

} // namespace affresco::format
