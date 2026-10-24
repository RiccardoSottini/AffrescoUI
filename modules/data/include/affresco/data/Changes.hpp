#pragma once

/// @file
/// @brief data::compareModels: what a change to the models does to the records already stored, classified like a
/// schema registry's compatibility check: compatible, transforming (needs a migration) or destructive (needs the
/// person's confirmation).

#include "affresco/data/Migration.hpp"
#include "affresco/data/Model.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco::data {

/// @brief What a model change does to the stored records.
enum class ChangeClass {
	Compatible,   ///< Every stored record still fits: a new optional field, a new collection, a widened enum.
	Transforming, ///< Records must change to fit (rename, retype, a newly required field): it needs a migration.
	Destructive   ///< Values are taken away or may no longer fit (a drop, a removed field, a narrowed type).
};

/// @brief One change to the models.
struct ModelChange {
	std::string collection;                     ///< The collection.
	std::string field;                          ///< The field, or "" for the whole collection.
	std::string text;                           ///< As a reviewer reads it: "songs: adds the optional field 'year'".
	ChangeClass kind = ChangeClass::Compatible; ///< Its class.
};

/// @brief The changes between two versions of the models.
struct ModelDiff {
	std::vector<ModelChange> changes; ///< Each change, collection by collection.

	/// @brief The worst class of all the changes.
	/// @return It; Compatible if there are none.
	[[nodiscard]] ChangeClass worst() const noexcept;
	/// @brief The worst class of one collection's changes.
	/// @param collection The collection.
	/// @return It; Compatible if it has none.
	[[nodiscard]] ChangeClass worst(std::string_view collection) const noexcept;
	/// @brief As JSON, for the impact of a revision: `[{"collection", "field", "change", "class"}]`.
	/// @return The list.
	[[nodiscard]] json toJson() const;
};

/// @brief The name of a class: "compatible", "transforming" or "destructive".
/// @param kind The class.
/// @return The name.
[[nodiscard]] std::string_view className(ChangeClass kind) noexcept;

/// @brief Compares two versions of the models. The migrations the newer one adds explain some changes: a field a
/// migration renames, merges or splits away is transformed, not removed; one it drops is destructive.
/// @param before The models before.
/// @param after The models after.
/// @return The changes; none if they are the same (titles and notes aside).
/// @throws DocumentError If a migration of @p after is malformed.
[[nodiscard]] ModelDiff compareModels(const Models& before, const Models& after);

/// @brief The migrations of a collection that take it from one version to another, in order.
/// @param model The model, at the later version.
/// @param from The version stored.
/// @param where Its place, for errors.
/// @return The migrations, or an empty list if none starts at @p from (a compatible bump needs none).
/// @throws DocumentError If they are malformed.
[[nodiscard]] std::vector<Migration> migrationsFrom(const Model& model, int from, const std::string& where);

} // namespace affresco::data
