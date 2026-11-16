#pragma once

/// @file
/// @brief format::ComputedValues: the document's "computed" values, evaluated when first read after a state change
/// and cached with what they read.

#include "Reads.hpp"

#include "affresco/core/Json.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief `computed.<name>`: each value evaluated when first read after the state changed, then cached with what it
/// read, so what reads it reads that too.
///
/// Invariants: the cache belongs to one DocumentContext::stateVersion(), and is dropped only between evaluations (a
/// value read in place stays valid until the evaluation reading it ends); a value being evaluated that reads itself,
/// or a chain deeper than maxComputedDepth, is an error. Callers: the document context's lookups, for expressions.
/// Main thread only.
class ComputedValues {
public:
	/// @brief The computed values of an engine's document.
	/// @param engine The runtime; outlives this object.
	explicit ComputedValues(Engine& engine) noexcept : engine_(engine) {}

	/// @brief `computed.<name>`: evaluated when first read after a state change, then cached.
	/// @param name The computed value.
	/// @return Its value.
	/// @throws DocumentError If there is no such value, it reads itself, or it is invalid.
	[[nodiscard]] json computed(std::string_view name);
	/// @brief `computed.<name>` in place: as computed(), without a copy.
	/// @param name The computed value.
	/// @return Its value, valid until the state next changes and the evaluation reading it has ended.
	/// @throws DocumentError As computed().
	[[nodiscard]] const json& value(std::string_view name);

private:
	Engine& engine_; ///< The runtime.
	/// The values read since the state last changed: a map, so a value read in place keeps its address.
	std::map<std::string, json, std::less<>> values_;
	std::uint64_t version_ = ~0ull;                     ///< The state version values_ belong to.
	std::map<std::string, ReadSet, std::less<>> reads_; ///< What each value read.
	std::vector<std::string> computing_;                ///< The values being evaluated (finds circles).
};

} // namespace affresco::format
