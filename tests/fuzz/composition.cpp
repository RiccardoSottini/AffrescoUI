/// @file
/// @brief Fuzzes composition across apps (CompositionProof.hpp): a feature's manifest, its adapter and the host's
/// contribution points are untrusted input (SECURITY.md, "Installing across apps").
///
/// The input is `{ manifest, adapter, points, exports, program }`; the host is the JSON template with those points,
/// exports and program, and the feature's manifest may hold a program of its own (FeaturePrograms.hpp).
/// Besides not crashing, every proof must check as a proof record within its size, give at least one reason, and a
/// passed one must come with the composed document; the host's contributions must list and print.

#include "Fuzz.hpp"

#include "Composition.hpp"
#include "CompositionProof.hpp"
#include "ContributionPoints.hpp"
#include "FeatureManifest.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

using affresco::json;
namespace format = affresco::format;

/// @brief Silences the log.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point: proves the input's feature on the template, and lists the template's contributions.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const json input = affresco::fuzz::untrusted(affresco::fuzz::text(data, size));
	if(!input.is_object()) return 0;

	format::ProofInputs inputs;
	inputs.host = affresco::fuzz::jsonTemplate();
	inputs.host.erase("fonts");
	for(const char* part : {"points", "exports", "program"})
		if(input.contains(part)) inputs.host[part] = input[part];
	inputs.manifest = input.value("manifest", json());
	inputs.adapter = input.value("adapter", json());
	inputs.source = "app:fuzz";
	inputs.hostName = "app:sign-in";
	inputs.base = "0000000";
	const format::ScenarioRunner none = [](const json&, const json&) { return std::vector<format::ScenarioOutcome>(); };
	try {
		const format::ProvenComposition proven = format::proveComposition(inputs, none);
		format::checkProofRecord(proven.proof, "proof"); // throws, and the fuzzer reports it, if malformed
		if(proven.proof.dump().size() > format::maxProofBytes) affresco::fuzz::broken("a proof stays within its size");
		if(format::proofReasons(proven.proof).empty()) affresco::fuzz::broken("a proof gives its verdict");
		if(proven.proof.value("passed", false) == true && !proven.document.is_object())
			affresco::fuzz::broken("a passed proof comes with the composed document");
	} catch(const affresco::DocumentError&) {
		// A manifest too big to prove, refused before any check
	}
	try { // the record composed on its own, its program built
		format::checkManifest(inputs.manifest, "manifest");
		json document = inputs.host;
		document["features"][inputs.manifest["name"].get<std::string>()] =
		    format::recordFor(inputs.manifest, "fuzz", "", "", 0);
		const format::Composition composed = format::compose(document);
		for(const format::FeatureExtension& extension : composed.extensions)
			if(extension.target.empty()) affresco::fuzz::broken("an accepted extension names what it joins");
		// NOLINTNEXTLINE(bugprone-empty-catch): a manifest the format refuses is an answer, not a finding
	} catch(const affresco::DocumentError&) {
		// A manifest the format refuses
	}
	try {
		const json contributions = format::contributionsOf(inputs.host, json::array({format::partOf(inputs.manifest)}));
		static_cast<void>(format::contributionsText(contributions));
	} catch(const affresco::DocumentError&) {
		// Points, exports or a manifest the format refuses
	}
	return 0;
}
