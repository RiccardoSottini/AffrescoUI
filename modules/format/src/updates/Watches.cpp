/// @file
/// @brief format::Watches: the "watch" handlers.

#include "Watches.hpp"
#include "Engine.hpp"

namespace affresco::format {

// "watch": a handler runs after the update in which its value changed. Not while handlers are suspended: the values
// seen then are never compared, so a document shown for a moment changes nothing.
void Watches::watch(Panel& root) {
	const json& document = engine_.versions.document();
	if(!document.contains("watch")) return;
	for(const auto& [path, handler] : object(document["watch"], "watch").items()) {
		const json value =
		    format::lookup(path, engine_.document.context(*noVariables(), json::object()), "watch." + path);
		if(watched_.contains(path) && watched_[path] != value)
			engine_.scheduler.defer(engine_.runner.later(
			    handler, root, {{"type", "change"}, {"value", value}, {"previous", watched_[path]}}, noVariables(),
			    "watch." + path));
		watched_[path] = value;
	}
}

} // namespace affresco::format
