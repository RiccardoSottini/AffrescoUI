/// @file
/// @brief The proof that a feature from another app fits a host: its checks run in order, each with its reasons, and
/// the scenarios run only once every other check passed.

#include "CompositionProof.hpp"
#include "AdapterMapping.hpp"
#include "Checker.hpp"
#include "Composition.hpp"
#include "ContributionPoints.hpp"
#include "Expression.hpp"
#include "ExpressionParser.hpp"
#include "FeatureManifest.hpp"
#include "FeatureSignatures.hpp"
#include "NodeKeys.hpp"

#include "affresco/format/Rules.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <ranges>

namespace affresco::format {

namespace {

using expect::fail;

/// The keys of a proof.
constexpr auto proofKeys =
    std::to_array<std::string_view>({"format", "version", "feature", "release", "source", "host", "base", "adapter",
	                                 "bindings", "checks", "scenarios", "passed", "unmet"});

/// The roots a feature's paths name that belong to someone: its own under its name, the host's otherwise.
constexpr auto ownedRoots = std::to_array<std::string_view>({"state", "computed"});

/// @brief Adds a check to a proof.
/// @param proof The proof.
/// @param name The check's name.
/// @param reasons Why it failed; empty if it passed. At most maxProofReasons are kept.
/// @return true if it passed.
bool addCheck(json& proof, const std::string& name, std::vector<std::string> reasons) {
	if(reasons.size() > maxProofReasons) {
		const std::size_t more = reasons.size() - maxProofReasons + 1;
		reasons.resize(maxProofReasons - 1);
		reasons.push_back(std::format("and {} more", more));
	}
	const bool passed = reasons.empty();
	proof["checks"].push_back({{"name", name}, {"passed", passed}, {"reasons", reasons}});
	if(!passed) proof["passed"] = false;
	return passed;
}

/// @brief Runs a check that throws: its reason is the error's text.
/// @param proof The proof.
/// @param name The check's name.
/// @param check The check.
/// @return true if it passed.
template <typename Check> bool addThrowingCheck(json& proof, const std::string& name, const Check& check) {
	try {
		check();
	} catch(const DocumentError& error) {
		return addCheck(proof, name, {error.what()});
	}
	return addCheck(proof, name, {});
}

/// @brief A member of an untrusted object that should be text, before anything checked it.
/// @param object The object, or any other value.
/// @param key The member's name.
/// @return The text, or "" if the object has no such member or it isn't text.
std::string textOf(const json& object, const char* key) {
	if(!object.is_object()) return "";
	const auto member = object.find(key);
	return member != object.end() && member->is_string() ? member->get<std::string>() : "";
}

/// @brief A proof with nothing checked yet.
/// @param inputs What it proves.
/// @return The proof.
json emptyProof(const ProofInputs& inputs) {
	const json& manifest = inputs.manifest;
	json adapter = nullptr;
	if(!inputs.adapter.is_null())
		adapter = {{"digest", digestOf(inputs.adapter.dump())}, {"note", textOf(inputs.adapter, "note")}};
	return {{"format", proofFormat},
	        {"version", 1},
	        {"feature", textOf(manifest, "name")},
	        {"release", textOf(manifest, "release")},
	        {"source", inputs.source},
	        {"host", inputs.hostName},
	        {"base", inputs.base},
	        {"adapter", adapter},
	        {"bindings", json::array()},
	        {"checks", json::array()},
	        {"scenarios", json::array()},
	        {"passed", true},
	        {"unmet", json::array()}};
}

/// @brief The place after the next order among a document's features.
/// @param document The document.
/// @return The highest order plus 10.
int nextOrder(const json& document) {
	int order = 0;
	if(const auto features = document.find("features"); features != document.end() && features->is_object())
		for(const auto& [name, record] : features->items())
			if(record.is_object() && record.contains("order") && record["order"].is_number_integer())
				order = std::max(order, record["order"].get<int>());
	return order + 10;
}

/// @brief The composition problems a document has that the host hasn't.
/// @param host The host.
/// @param document The host with the feature.
/// @return The new problems.
std::vector<std::string> compositionProblems(const json& host, const json& document) {
	const std::vector<std::string> before = compose(host).problems;
	std::vector<std::string> after = compose(document).problems;
	std::erase_if(after, [&](const std::string& problem) { return std::ranges::contains(before, problem); });
	return after;
}

/// @brief The checker's findings a document brings in over the host's.
/// @param host The host.
/// @param document The host with the feature.
/// @return Each finding's text.
std::vector<std::string> documentProblems(const json& host, const json& document) {
	const json state = host.value("state", json::object());
	const auto before = check(host, state);
	const auto after = check(document, state);
	if(!before || !after) return {"too large to check"};
	std::vector<std::string> lines;
	for(const Finding& finding : introduced(*before, *after)) lines.push_back(finding.text());
	return lines;
}

/// @brief Why a path a feature reads or writes reaches past what it is given.
/// @param name The feature.
/// @param names The path's names.
/// @param needs Its needs.
/// @return Why, or empty if it may.
std::string pathWhy(const std::string& name, const std::vector<std::string>& names, const json& needs) {
	if(names.empty()) return {};
	if(std::ranges::contains(ownedRoots, names.front()) && (names.size() < 2 || names[1] != name))
		return std::format("reads {}.{}, which isn't its own", names.front(), names.size() > 1 ? names[1] : "");
	if(names.front() == "app" && (names.size() < 2 || !needs.contains(names[1])))
		return std::format("reads app.{}, which it doesn't need", names.size() > 1 ? names[1] : "");
	return {};
}

/// @brief Splits a dotted path into its names.
/// @param path The path.
/// @return Its names.
std::vector<std::string> namesOf(std::string_view path) {
	std::vector<std::string> names;
	for(const auto part : path | std::views::split('.')) names.emplace_back(part.begin(), part.end());
	return names;
}

/// @brief Collects the paths an operator tree reads: the names each `$get` starts with.
/// @param tree The tree.
/// @param paths Receives each path's names.
/// @param depth How deep the walk is.
void getsIn(const json& tree, std::vector<std::vector<std::string>>& paths, int depth = 0) {
	if(depth > maxNesting || !tree.is_structured()) return;
	if(tree.is_object())
		if(const auto get = tree.find("$get"); get != tree.end() && get->is_array() && !get->empty()) {
			std::vector<std::string> names;
			for(const json& key : *get) {
				if(!key.is_string()) break;
				names.push_back(key.get<std::string>());
			}
			paths.push_back(std::move(names));
		}
	for(const json& item : tree) getsIn(item, paths, depth + 1);
}

/// @brief The isolation problems of one text: its expressions' paths, or the text itself when it is a bare path.
/// @param name The feature.
/// @param text The text.
/// @param needs Its needs.
/// @param place Its place.
/// @param problems Receives each problem.
void textProblems(const std::string& name, const std::string& text, const json& needs, const std::string& place,
                  std::vector<std::string>& problems) {
	std::vector<std::vector<std::string>> paths;
	const Placeholders found = placeholders(text);
	if(found.expressions.empty()) paths.push_back(namesOf(text));
	for(const std::string_view expression : found.expressions) try {
			getsIn(detail::parse(expression, place), paths);
		} catch(const DocumentError& error) {
			problems.push_back(error.what());
		}
	for(const auto& names : paths)
		if(std::string why = pathWhy(name, names, needs); !why.empty())
			problems.push_back(concatenate(place, ": ", why));
}

/// @brief Why a "run" target isn't the feature's own or a need.
/// @param name The feature.
/// @param target The target.
/// @param needs Its needs.
/// @return Why, or empty.
std::string runWhy(const std::string& name, const std::string& target, const json& needs) {
	if(target.starts_with(name + ".")) return {};
	if(target.starts_with("app.") && needs.contains(target.substr(4))) return {};
	return std::format("runs '{}', which is neither its own nor a need", target);
}

/// @brief Tells whether a key's text names an action to run: "run", an event handler ("onClick") or a key binding.
/// @param key The key.
/// @param place The place of the object holding it.
/// @return true if it does.
bool namesAction(const std::string& key, const std::string& place) {
	return key == "run" || place == "provides.keys" ||
	       (key.size() > 2 && key.starts_with("on") && key[2] >= 'A' && key[2] <= 'Z');
}

/// @brief Checks a text that names an action to run.
/// @param name The feature.
/// @param text The text.
/// @param needs Its needs.
/// @param place Its place.
/// @param problems Receives each problem.
void targetProblems(const std::string& name, const std::string& text, const json& needs, const std::string& place,
                    std::vector<std::string>& problems) {
	if(!placeholders(text).expressions.empty()) problems.push_back(place + ": a computed action");
	else if(std::string why = runWhy(name, text, needs); !why.empty()) problems.push_back(place + ": " + why);
}

/// @brief What an isolation walk checks against, and where its problems go.
struct IsolationWalk {
	const std::string& name;            ///< The feature.
	const json& needs;                  ///< Its needs.
	std::vector<std::string>& problems; ///< Receives each problem, at most maxProofReasons.
};

/// @brief Walks what a feature provides for isolation problems.
/// @param walk The feature, its needs and the problems.
/// @param value The value.
/// @param place Its place.
/// @param depth How deep the walk is.
void walkIsolation(const IsolationWalk& walk, const json& value, const std::string& place, int depth = 0) {
	if(depth > maxNesting || walk.problems.size() >= maxProofReasons) return;
	if(value.is_string()) return textProblems(walk.name, value.get<std::string>(), walk.needs, place, walk.problems);
	if(value.is_array())
		for(std::size_t index = 0; index < value.size(); ++index)
			walkIsolation(walk, value[index], std::format("{}[{}]", place, index), depth + 1);
	if(!value.is_object()) return;
	for(const auto& [key, item] : value.items()) {
		const std::string at = concatenate(place, ".", key);
		if(item.is_string() && namesAction(key, place))
			targetProblems(walk.name, item.get<std::string>(), walk.needs, at, walk.problems);
		else walkIsolation(walk, item, at, depth + 1);
	}
}

/// @brief Checks the manifest and the adapter, and binds the needs.
/// @param inputs What to prove.
/// @param proof The proof.
/// @param bound Receives the bindings.
/// @return true if all passed.
bool bindStep(const ProofInputs& inputs, json& proof, BoundNeeds& bound) {
	if(!addThrowingCheck(proof, "manifest", [&] { checkManifest(inputs.manifest, "feature"); })) return false;
	const bool adapterFine = addThrowingCheck(proof, "adapter", [&] {
		if(inputs.adapter.is_null()) return;
		checkAdapter(inputs.adapter);
		if(inputs.adapter.contains("feature") && inputs.adapter["feature"] != inputs.manifest["name"])
			fail("adapter.feature", "names another feature");
	});
	if(!adapterFine) return false;
	bound = bindNeeds(inputs.manifest.value("needs", json::object()), inputs.host, inputs.adapter);
	proof["bindings"] = bound.records;
	proof["unmet"] = bound.unmet;
	std::vector<std::string> reasons;
	for(const json& unmet : bound.unmet)
		reasons.push_back(std::format("{}: {}", unmet["need"].get<std::string>(), unmet["why"].get<std::string>()));
	return addCheck(proof, "needs", std::move(reasons));
}

/// @brief Runs the scenarios and records them.
/// @param proof The proof.
/// @param proven The document and manifest.
/// @param manifest The manifest.
/// @param runner The runner.
void scenarioStep(json& proof, const ProvenComposition& proven, const json& manifest, const ScenarioRunner& runner) {
	std::vector<std::string> reasons;
	std::vector<ScenarioOutcome> outcomes = runner ? runner(proven.document, manifest) : std::vector<ScenarioOutcome>{};
	if(outcomes.size() > maxProofScenarios) outcomes.resize(maxProofScenarios);
	for(const ScenarioOutcome& outcome : outcomes) {
		proof["scenarios"].push_back(
		    {{"file", outcome.file}, {"name", outcome.name}, {"passed", outcome.passed}, {"problem", outcome.problem}});
		if(!outcome.passed) reasons.push_back(std::format("{} ({}): {}", outcome.name, outcome.file, outcome.problem));
	}
	(void)addCheck(proof, "scenarios", std::move(reasons));
}

} // namespace

std::vector<std::string> isolationProblems(const std::string& name, const json& provides, const json& needs) {
	std::vector<std::string> problems;
	walkIsolation({name, needs, problems}, provides, "provides");
	return problems;
}

ProvenComposition proveComposition(const ProofInputs& inputs, const ScenarioRunner& runner) {
	ProvenComposition proven{.proof = emptyProof(inputs), .record = nullptr, .document = nullptr};
	json& proof = proven.proof;
	BoundNeeds bound;
	if(!bindStep(inputs, proof, bound)) return proven;
	const std::string name = inputs.manifest["name"].get<std::string>();
	proven.record = recordFor(inputs.manifest, inputs.source, inputs.digest, inputs.publisher, nextOrder(inputs.host));
	proven.record["bindings"] = bound.bindings;
	proven.document = inputs.host;
	bool passed = addCheck(proof, "composition", [&]() -> std::vector<std::string> {
		if(inputs.host.contains("features") && inputs.host["features"].contains(name))
			return {std::format("the host already has a feature '{}'", name)};
		proven.document["features"][name] = proven.record;
		return compositionProblems(inputs.host, proven.document);
	}());
	passed = addCheck(proof, "isolation",
	                  isolationProblems(name, inputs.manifest["provides"],
	                                    inputs.manifest.value("needs", json::object()))) &&
	         passed;
	passed =
	    passed && addThrowingCheck(proof, "permissions", [&] { Rules().checkChange(inputs.host, proven.document); });
	passed = passed && addCheck(proof, "document", documentProblems(inputs.host, proven.document));
	if(passed) scenarioStep(proof, proven, inputs.manifest, runner);
	proven.record["proof"] = proof;
	if(proof["passed"] == true) proven.document["features"][name] = proven.record;
	return proven;
}

std::vector<std::string> proofReasons(const json& proof) {
	std::vector<std::string> lines;
	if(!proof.is_object()) return {"not a proof"};
	lines.push_back(std::format("{} {} into {}: {}", proof.value("feature", ""), proof.value("release", ""),
	                            proof.value("host", ""), proof.value("passed", false) ? "proven" : "not proven"));
	for(const json& unmet : proof.value("unmet", json::array()))
		lines.push_back(std::format("unmet need {}: {}", unmet.value("need", ""), unmet.value("why", "")));
	for(const json& check : proof.value("checks", json::array()))
		if(!check.value("passed", true))
			for(const json& reason : check.value("reasons", json::array()))
				if(reason.is_string() && check.value("name", "") != "needs")
					lines.push_back(std::format("{}: {}", check.value("name", ""), reason.get<std::string>()));
	if(const std::size_t run = proof.value("scenarios", json::array()).size(); run > 0)
		lines.push_back(std::format("{} scenarios run", run));
	return lines;
}

void checkProofRecord(const json& proof, const std::string& place) {
	expect::object(proof, place);
	if(proof.dump().size() > maxProofBytes) fail(place, std::format("larger than {} bytes", maxProofBytes));
	for(const auto& [key, value] : proof.items())
		if(!std::ranges::contains(proofKeys, key)) fail(concatenate(place, ".", key), "unknown key");
	if(proof.value("format", "") != proofFormat) fail(place + ".format", std::format("expected \"{}\"", proofFormat));
	(void)expect::boolean(proof.value("passed", json()), place + ".passed");
	for(const char* part : {"bindings", "checks", "scenarios", "unmet"})
		if(proof.contains(part) && !proof[part].is_array()) fail(place + "." + part, "expected a list");
	if(proof.value("checks", json::array()).size() > 16) fail(place + ".checks", "too many checks");
	if(proof.value("scenarios", json::array()).size() > maxProofScenarios) fail(place + ".scenarios", "too many");
	if(proof.value("unmet", json::array()).size() > maxNeeds) fail(place + ".unmet", "too many");
	for(const char* text : {"feature", "release", "source", "host", "base"})
		if(proof.contains(text)) (void)expect::string(proof[text], place + "." + text);
}

void checkAdaptedParts(const json& record, const std::string& place) {
	if(const auto needs = record.find("needs"); needs != record.end()) checkNeeds(*needs, place + ".needs");
	if(const auto bindings = record.find("bindings"); bindings != record.end())
		checkBindings(*bindings, place + ".bindings");
	if(const auto proof = record.find("proof"); proof != record.end()) checkProofRecord(*proof, place + ".proof");
}

} // namespace affresco::format
