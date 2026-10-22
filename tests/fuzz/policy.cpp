/// @file
/// @brief Fuzzes budgets and data rules (VERSION_CONTROL.md, "Budgets" and "Data rules"): the Kanban template with the
/// input's "permissions" (when the format accepts them), judging the input's "revision" by an agent.
///
/// Besides not crashing, judging changes nothing and gives the same decision twice, and what an agent reads never
/// holds a path a data rule says it may never touch.

#include "Fuzz.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

using affresco::json;

namespace {

/// @brief Checks that an agent's readable view holds none of the paths its data rules say it may never touch.
/// @param repository The repository.
/// @param permissions The permissions it was loaded with.
void checkNeverRead(const affresco::vcs::Repository& repository, const json& permissions) {
	const std::optional<json> view = repository.readable("agent/fuzz");
	if(!view) return;
	for(const json& rule : permissions.value("data", json::array()))
		for(const json& path : rule.value("never", json::array())) {
			if(rule.value("author", "") != "agent" || path.get<std::string>().find('*') != std::string::npos) continue;
			try {
				if(view->contains(json::json_pointer(path.get<std::string>())) && path != "/")
					affresco::fuzz::broken("an agent never reads a path it may never touch");
			} catch(const json::exception&) {
			}
		}
}

} // namespace

/// @brief libFuzzer's entry point: one input, `{"permissions": {...}, "revision": {...}}`.
/// @param data The input's bytes.
/// @param size How many.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	json input;
	try {
		input = affresco::parseUntrusted(affresco::fuzz::text(data, size));
	} catch(const std::exception&) {
		return 0;
	}
	if(!input.is_object() || !input.contains("permissions")) return 0;
	affresco::fuzz::silenceLog();
	static const affresco::format::Rules rules;
	json document = affresco::fuzz::kanbanTemplate();
	document["permissions"] = input["permissions"];
	try {
		rules.validate(document);
	} catch(const std::exception&) {
		return 0; // the format refuses them: no repository holds them
	}
	affresco::vcs::Repository repository(rules, document);
	const std::string before = repository.document().dump();
	const affresco::vcs::Repository::Judgement first = repository.judge(input.value("revision", json()));
	const affresco::vcs::Repository::Judgement second = repository.judge(input.value("revision", json()));
	if(first.decision != second.decision || first.reason != second.reason)
		affresco::fuzz::broken("judging a revision twice gives the same decision");
	if(repository.document().dump() != before) affresco::fuzz::broken("judging a revision changes nothing");
	checkNeverRead(repository, input["permissions"]);
	return 0;
}
