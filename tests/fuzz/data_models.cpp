/// @file
/// @brief Fuzzes the data layer's parsers on untrusted documents (modules/data): the "models" section with its
/// migrations and seeds, queries, and CSV text.
///
/// The input is `{"models": {...}, "next": {...}, "query": {...}}` (any part may be missing or malformed) or, if it
/// isn't JSON, a CSV text. Everything is refused with DocumentError (DataError is one), or works: a store follows the
/// models, plans the next ones without changing (its records stay as they were), follows them or refuses them leaving
/// everything as it was, and runs the query; a CSV text parses within its limits.

#include "Fuzz.hpp"

#include <affresco/data/Changes.hpp>
#include <affresco/data/Csv.hpp>
#include <affresco/data/Store.hpp>

#include <cstddef>
#include <cstdint>

using affresco::json;

namespace {

/// @brief Stands in for the screen format's expressions (fuzzed on their own, `expression`): an expression gives the
/// value it converts, or fails when it says "fail", so migrations take both paths.
/// @param expression The expression.
/// @param variables Its variables.
/// @return Its value.
json evaluate(const std::string& expression, const json& variables) {
	if(expression.contains("fail")) throw affresco::DocumentError("fuzz: an expression that fails");
	return variables.value("value", variables.value("it", json()));
}

/// @brief Every record of every collection, for comparing before and after.
/// @param store The store.
/// @return {collection: [records]}.
json everything(const affresco::data::Store& store) {
	json all = json::object();
	for(const auto& [name, model] : store.models()) all[name] = store.run({{"from", name}}, "fuzz");
	return all;
}

/// @brief Runs the store on the input's parts.
/// @param input The input.
void runStore(const json& input) {
	affresco::data::Store store;
	store.setEvaluator(evaluate);
	store.setClock([] { return std::int64_t{1}; });
	try {
		(void)store.follow(input.value("models", json()), "fuzz");
	} catch(const affresco::DocumentError&) {
		return;
	}
	const json before = everything(store);
	if(input.contains("next")) {
		try {
			(void)affresco::data::compareModels(store.models(), affresco::data::parseModels(input["next"]));
			const affresco::data::FollowReport plan = store.plan(input["next"]);
			if(everything(store) != before) affresco::fuzz::broken("a plan changed the store");
			try {
				(void)store.follow(input["next"], "fuzz");
				if(!plan.ok()) affresco::fuzz::broken("a plan that fails followed");
			} catch(const affresco::DocumentError&) {
				if(everything(store) != before) affresco::fuzz::broken("a refused change changed the store");
			}
		} catch(const affresco::DocumentError&) { // NOLINT(bugprone-empty-catch): a malformed next version
		}
	}
	if(input.contains("query")) try {
			(void)store.run(input["query"], "fuzz");
		} catch(const affresco::DocumentError&) { // NOLINT(bugprone-empty-catch): a malformed query
		}
}

} // namespace

/// @brief Keeps the log quiet.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	const json input = affresco::fuzz::untrusted(text);
	if(input.is_object()) {
		runStore(input);
		return 0;
	}
	try {
		for(const auto& row : affresco::data::parseCsv(text))
			if(row.size() > affresco::data::Limits::csvColumns) affresco::fuzz::broken("a row past the columns' limit");
	} catch(const affresco::DocumentError&) { // NOLINT(bugprone-empty-catch): refused, as it should be
	}
	return 0;
}
