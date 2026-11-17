/// @file
/// @brief The revision that installs a feature from another app, and a proof's line for history and review.

#include "CompositionProofRevision.hpp"

#include "affresco/core/JsonDigest.hpp"

#include <format>

namespace affresco::vcs {

namespace {

/// @brief A record with its "installed" digest: its "requires", "provides" and "needs" as installed, the same digest
/// format::installedDigest() gives (the format isn't linked here; a unit test holds the two equal), so an edit made in
/// place is told when the feature is upgraded.
/// @param record The record.
/// @return It, with "installed".
json withInstalledDigest(json record) {
	json parts = json::object();
	for(const char* part : {"requires", "provides", "needs"}) parts[part] = record.value(part, json());
	record["installed"] = jsonDigest(parts);
	return record;
}

} // namespace

json compositionRevision(const std::string& name, const json& record, std::string_view author, const json& host) {
	const json proof = record.is_object() ? record.value("proof", json()) : json();
	if(!proof.is_object() || !proof.value("passed", false) || proof.value("feature", "") != name)
		throw DocumentError(std::format("features.{}: only a passed proof of this feature can be installed", name));
	const bool section = host.contains("features") && host["features"].is_object();
	json installed = withInstalledDigest(record);
	return {{"author", author},
	        {"message",
	         std::format("Install {} from {} ({})", name, proof.value("source", "another app"), proofLine(record))},
	        {"set", section ? json{{"/features/" + name, std::move(installed)}}
	                        : json{{"/features", json{{name, std::move(installed)}}}}}};
}

std::string proofLine(const json& record) {
	if(!record.is_object() || !record.contains("proof") || !record["proof"].is_object()) return {};
	const json& proof = record["proof"];
	const auto count = [&](const char* part) {
		const auto found = proof.find(part);
		return found != proof.end() && found->is_array() ? found->size() : 0;
	};
	return std::format("proof: {} {} from {}, {}, {} needs bound, {} scenarios", proof.value("feature", ""),
	                   proof.value("release", ""), proof.value("source", ""),
	                   proof.value("passed", false) ? "passed" : "not passed", count("bindings"), count("scenarios"));
}

} // namespace affresco::vcs
