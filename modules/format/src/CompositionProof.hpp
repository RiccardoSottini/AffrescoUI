#pragma once

/// @file
/// @brief The proof that a feature from another app fits a host: each need bound (directly or through an adapter),
/// each check (manifest, adapter, needs, composition, isolation, document, permissions, scenarios), the scenarios run
/// and the base commit. The install revision carries it at /features/<name>/proof; a failed proof names each unmet
/// need and why.
///
/// Proofs are untrusted input when read back: checkProofRecord() bounds them.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

inline constexpr std::string_view proofFormat = "affrescoui-proof"; ///< A proof's "format".
inline constexpr std::size_t maxProofBytes = 256u << 10;            ///< A proof, as JSON: 256 KiB.
inline constexpr std::size_t maxProofReasons = 16;                  ///< Reasons kept per check.
inline constexpr std::size_t maxProofScenarios = 256;               ///< Scenarios kept in a proof.

/// @brief One scenario run on the composed document.
struct ScenarioOutcome {
	std::string file;    ///< Where it came from: "feature" or the host's scenarios file.
	std::string name;    ///< Its name.
	bool passed = false; ///< Whether it passed.
	std::string problem; ///< Why it failed, or empty.
};

/// @brief Runs the feature's and the host's scenarios on a composed document (the caller has the runtime).
using ScenarioRunner = std::function<std::vector<ScenarioOutcome>(const json& document, const json& manifest)>;

/// @brief What a proof is made from.
struct ProofInputs {
	json host;             ///< The host document, as stored (raw).
	json manifest;         ///< The feature's manifest, unchecked.
	json adapter;          ///< The adapter, unchecked, or null.
	std::string source;    ///< Where the feature came from: "app:sinopia".
	std::string hostName;  ///< The host app's name.
	std::string base;      ///< The commit the host document is at.
	std::string digest;    ///< The manifest's digest, or empty.
	std::string publisher; ///< Its publisher if signed (5.5's trust applies unchanged), or empty.
};

/// @brief A proof, with the record and the document it proves.
struct ProvenComposition {
	json proof;    ///< The proof record.
	json record;   ///< The feature's record, bindings and proof included, or null if the manifest is malformed.
	json document; ///< The host document with the record installed, or null.
};

/// @brief Proves a composition: checks the manifest and the adapter, binds every need, composes, checks isolation,
/// the composed document and the permissions, then runs the scenarios (only if all else passed).
/// @param inputs What to prove.
/// @param runner Runs the scenarios.
/// @return The proof (passed or not), the record and the document.
[[nodiscard]] ProvenComposition proveComposition(const ProofInputs& inputs, const ScenarioRunner& runner);

/// @brief A proof's reasons, one line each: its verdict, each unmet need, each failed check and scenario.
/// @param proof The proof.
/// @return The lines.
[[nodiscard]] std::vector<std::string> proofReasons(const json& proof);

/// @brief Checks a proof record's shape and size.
/// @param proof The proof.
/// @param place Its place.
/// @throws DocumentError If it is malformed or too big.
void checkProofRecord(const json& proof, const std::string& place);

/// @brief Checks the parts of a feature record composition across apps adds: "needs", "bindings" and "proof".
/// @param record The record.
/// @param place Its place.
/// @throws DocumentError If one is malformed.
void checkAdaptedParts(const json& record, const std::string& place);

/// @brief Why a feature from another app reaches past what it is given: state, computed values, actions or components
/// of the host (it may use only its own, `app.<need>` and built-ins).
/// @param name The feature.
/// @param provides What it provides.
/// @param needs What it needs.
/// @return Each reason, at most maxProofReasons.
[[nodiscard]] std::vector<std::string> isolationProblems(const std::string& name, const json& provides,
                                                         const json& needs);

} // namespace affresco::format
