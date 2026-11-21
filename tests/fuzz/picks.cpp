/// @file
/// @brief Fuzzes what the person pointed at, as the in-process agent reads it (modules/intelligence/src/Picks.hpp):
/// any JSON as a request's "picks", through picksOf() and pickBlock() against a small program and a small JSON screen.
///
/// Besides not crashing, the records must stay within their bounds (at most 4, each with a "node" text) and the block
/// within its size: a header line and, for each of the 4 picks, at most maxPickBlock (12 KiB) bytes.

#include "Fuzz.hpp"
#include "Picks.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

using affresco::json;

namespace {

/// The most bytes of the block: its header, then 4 picks of at most 12 KiB each and a line break.
constexpr std::size_t mostBlock = 64 + affresco::intelligence::maxPicks * (affresco::intelligence::maxPickBlock + 1);

/// @brief A small document with a program, built nodes and a state that a pick must never bring.
/// @return It.
json document() {
	return json::parse(R"json({
		"root": { "id": "page", "children": [
			{ "id": "cards", "repeat": { "for": "card", "in": "state.cards" },
			  "children": [{ "id": "cardTitle", "type": "label", "text": "{{card.title}}" }] }] },
		"components": { "badge": { "node": { "id": "badgeText", "type": "label" } } },
		"program": { "columns.lisp": {
			"card-title": "(defview card-title [card]\n  `(label#cardTitle :text ~card.title))",
			"page": "(defscreen page (column#page ~(card-title)))" } },
		"state": { "secret": "never in a block" }
	})json");
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const json input = affresco::fuzz::untrusted(affresco::fuzz::text(data, size));
	if(input.is_discarded()) return 0;
	static const json screen = document();

	const json picks = affresco::intelligence::picksOf(input);
	if(!picks.is_array() || picks.size() > affresco::intelligence::maxPicks)
		affresco::fuzz::broken("at most 4 picks are kept");
	for(const json& pick : picks)
		if(!pick.is_object() || !pick.contains("node") || !pick["node"].is_string())
			affresco::fuzz::broken("every pick kept has a node text");

	const std::string block = affresco::intelligence::pickBlock(picks, screen);
	if(block.size() > mostBlock) affresco::fuzz::broken("the block stays within 4 picks of 12 KiB");
	const auto said = [&](const json& value) { return value.dump(-1, ' ', false, json::error_handler_t::replace); };
	if(block.contains("never in a block") && !said(input).contains("never in a block"))
		affresco::fuzz::broken("a pick never brings the state");
	(void)affresco::intelligence::screenSummary(screen, 1, input.is_array());
	return 0;
}
