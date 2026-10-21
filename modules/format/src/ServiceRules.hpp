#pragma once

/// @file
/// @brief What version control checks of the service steps a revision adds (RFC 0002): uses of declared services and
/// revisable methods only, and a line for the reviewer when one writes or runs.

#include "affresco/core/Json.hpp"
#include "affresco/core/Services.hpp"

#include <string>
#include <vector>

namespace affresco::format {

/// @brief Refuses a revision that adds a use the screen doesn't allow: a computed or malformed name, a service the
/// document doesn't declare, or (when the services are known) a method the service doesn't have or keeps from
/// revisions.
/// @param before The document before the revision.
/// @param after The document after it.
/// @param services The screen's services, or nullptr if none are set.
/// @throws DocumentError If a use it adds isn't allowed.
void checkServiceUses(const json& before, const json& after, const services::Services* services);

/// @brief One line for each use a revision adds of a method that writes or runs, for the reviewer: e.g. "uses
/// files.write (writes: writes a text file under the root)". A method the services don't know counts as one.
/// @param before The document before the revision.
/// @param after The document after it.
/// @param services The screen's services, or nullptr if none are set.
/// @return The lines, sorted.
[[nodiscard]] std::vector<std::string> serviceReviewLines(const json& before, const json& after,
                                                          const services::Services* services);

} // namespace affresco::format
