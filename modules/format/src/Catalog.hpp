#pragma once

/// @file
/// @brief format::catalog(): what the format offers, for agents, built from the runtime's registries.

#include "affresco/core/Json.hpp"

namespace affresco::format {

struct Engine;

/// @brief What the format offers (SCREEN_FORMAT.md, "Catalog"), built from the registries: the node types and their
/// properties (TypeRegistry), the events, the steps and control steps (StepRegistry), the operators, the variables,
/// the sections (SectionRegistry), and what the application registered (types, native actions and calls).
/// @param engine The runtime.
/// @return The catalog.
[[nodiscard]] json catalog(const Engine& engine);

} // namespace affresco::format
