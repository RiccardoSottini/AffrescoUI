/// @file
/// @brief The revision that installs a feature from another app, and a proof's line for history and review.

#include "CompositionProofRevision.hpp"


#include <format>

namespace affresco::vcs {

json compositionRevision(const std::string& name, const json& record, std::string_view author) {
	const json proof = record.is_object() ? record.value("proof", json()) : json();
	if(!proof.is_object() || proof.value("passed", false) != true || proof.value("feature", "") != name)
		throw DocumentError(std::format("features.{}: only a passed proof of this feature can be installed", name));
	return {{"author", author},
	        {"message", std::format("Install {} from {} ({})", name, proof.value("source", "another app"),
	                                proofLine(record))},
	        {"set", {{"/features/" + name, record}}}};
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
	                   proof.value("passed", false) == true ? "passed" : "not passed", count("bindings"),
	                   count("scenarios"));
}

} // namespace affresco::vcs
