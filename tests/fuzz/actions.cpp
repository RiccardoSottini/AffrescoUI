/// @file
/// @brief Fuzzes the steps an action runs, as a document or a revision may write them, on the Kanban template's
/// state and actions: loops, nested actions, expressions, state writes.
///
/// Besides not crashing, every event must stop within its work budget (SECURITY.md, "Limits"): the run's -timeout
/// catches an action that doesn't, and the state an event leaves must stay within what its budget lets it write.

#include "Fuzz.hpp"

#include "Expression.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Panel.hpp>
#include <affresco/vcs/Repository.hpp>

#include <cstddef>
#include <cstdint>

using affresco::json;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const json steps = json::parse(affresco::fuzz::text(data, size), nullptr, false);
	if(steps.is_discarded()) return 0;

	// The template without its fonts, which are read from files next to it: a runtime with no window needs none
	json document = affresco::fuzz::kanbanTemplate();
	document.erase("fonts");
	document["actions"]["fuzz"] = steps;
	static const affresco::format::Rules rules;
	try {
		affresco::vcs::Repository repository(rules, document);
		affresco::format::Runtime runtime(repository, {});
		affresco::Panel sender;
		const std::size_t before = repository.state().dump().size();
		runtime.runAction("fuzz", sender, json::object()); // errors are reported, never thrown
		if(repository.state().dump().size() > before + 2 * affresco::format::bytesPerEvent)
			affresco::fuzz::broken("an event writes no more into the state than its budget lets it");
	} catch(const affresco::DocumentError&) {
		return 0; // a document the rules refuse: never runs
	}
	return 0;
}
