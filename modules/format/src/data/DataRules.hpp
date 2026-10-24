#pragma once

/// @file
/// @brief The data layer as rules for version control: the "models" and "queries" sections checked, a change to the
/// models classified and refused without its migration (or when records don't fit it), the lines a person must
/// review, and the impact of a revision on each layer: view, logic, model, data.

#include "affresco/core/Json.hpp"

#include <string>
#include <vector>

namespace affresco::data {
class Store;
}

namespace affresco::format {

/// @brief Checks the "models" section: models, fields, migrations and seeds (every seed record fits its model).
/// @param value The section.
/// @param name "models".
/// @throws DocumentError Naming the place.
void checkModelsSection(const json& value, const std::string& name);

/// @brief Checks the "queries" section's shape: name -> {from, where, sort, limit, offset, count}.
/// @param value The section.
/// @param name "queries".
/// @throws DocumentError Naming the place.
void checkQueriesSection(const json& value, const std::string& name);

/// @brief Checks what the data sections say together: each query reads a declared collection and its fields, no
/// query has a collection's name, and no top-level "data" holds records (they live in the store).
/// @param document The document.
/// @throws DocumentError Naming the place.
void checkDataDocument(const json& document);

/// @brief Refuses a change to the models that needs a migration and doesn't bring it (a rise of the model's
/// version and a migration from the old version to the new), that rewrites a migration already applied, or whose
/// records don't fit (a dry run on a copy of the store, if there is one).
/// @param before The document before.
/// @param after The document after.
/// @param store The screen's store, or nullptr.
/// @throws DocumentError With the reason.
void checkModelChange(const json& before, const json& after, const data::Store* store);

/// @brief What a change to the data layer needs a person to review: a migration, a destructive change to the
/// models (the person's acceptance is the confirmation), and new or changed write steps.
/// @param before The document before.
/// @param after The document after.
/// @param store The screen's store, or nullptr (then no record counts).
/// @return The lines.
[[nodiscard]] std::vector<std::string> dataReviewLines(const json& before, const json& after, const data::Store* store);

/// @brief The impact of a change on each layer, computed from what it changes:
/// `{"view": nodes and look entries, "logic": actions, computed, rules, handlers and UI state, "model": [changes with
/// their class], "data": "none" | "migrates N records" | "destructive: ..."}`.
/// @param before The document before.
/// @param after The document after.
/// @param store The screen's store, or nullptr.
/// @return The impact.
[[nodiscard]] json dataImpact(const json& before, const json& after, const data::Store* store);

} // namespace affresco::format
