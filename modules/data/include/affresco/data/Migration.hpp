#pragma once

/// @file
/// @brief data::Migration: a declarative, versioned change to a collection's records, as a model's "migrations"
/// lists them (`{"from": 3, "to": 4, "steps": [{"rename": "artist", "to": "by"}]}`), applied record by record, and
/// its inverse where one exists.

#include "affresco/data/Model.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace affresco::data {

/// @brief Evaluates an expression of the screen format (`{{value * 1000}}`) with some variables. The data module has
/// no expression language of its own: the format gives one (format::evaluateExpression).
using Evaluator = std::function<json(const std::string& expression, const json& variables)>;

/// @brief One step of a migration.
struct MigrationStep {
	/// @brief What it does to each record.
	enum class Kind {
		Rename,  ///< `{"rename": "artist", "to": "by"}`: the value moves to another field.
		Convert, ///< `{"convert": "duration", "with": "{{value * 1000}}", "inverse": "{{value / 1000}}"}`.
		Fill,    ///< `{"fill": "rating", "with": 0}`: a value where the field has none (a newly required field).
		Drop,    ///< `{"drop": "legacy"}`: the value is removed (destructive: archived, restorable).
		Split,   ///< `{"split": "name", "into": ["first", "last"], "with": [...], "inverse": "{{...}}"}`.
		Merge,   ///< `{"merge": ["first", "last"], "into": "name", "with": "{{...}}", "inverse": [...]}`.
		Restore  ///< A drop's inverse: the value comes back from the archive (never written in a document).
	};
	Kind kind = Kind::Rename;        ///< What it does.
	std::vector<std::string> fields; ///< The fields it reads: one, or a merge's several.
	std::vector<std::string> into;   ///< The fields it writes: a rename's or a merge's one, a split's several.
	json with;                       ///< The value or expression(s) of convert, fill, split and merge.
	json inverse;                    ///< The expression(s) that undo convert, split or merge (null: none).
};

/// @brief A migration of one collection from one model version to the next.
struct Migration {
	std::string collection;           ///< The collection.
	int from = 0;                     ///< The model version it starts from.
	int to = 0;                       ///< The version it ends at.
	std::vector<MigrationStep> steps; ///< Its steps, in order.
	json written;                     ///< As the document writes it.
};

/// @brief Reads one migration.
/// @param definition `{"from", "to", "steps": [...], "note"?}`.
/// @param collection Its collection.
/// @param where Its place, for errors: "models.songs.migrations[0]".
/// @return The migration.
/// @throws DocumentError If it is malformed.
[[nodiscard]] Migration parseMigration(const json& definition, const std::string& collection, const std::string& where);

/// @brief Reads a model's migrations and checks that they chain: each starts where one before it ends or later, and
/// none ends past the model's version.
/// @param model The model.
/// @param where Its place: "models.songs".
/// @return The migrations, oldest first.
/// @throws DocumentError If one is malformed or they overlap.
[[nodiscard]] std::vector<Migration> migrationsOf(const Model& model, const std::string& where);

/// @brief Applies one step to a record's fields.
/// @param step The step.
/// @param fields The fields (no id, created or updated), changed in place.
/// @param evaluate Evaluates the expressions; may be empty if the step has none.
/// @param archived For a Restore step: the record's fields before the migration it undoes (nullptr: none).
/// @throws DataError "migration" if an expression fails or there is no evaluator for one.
void applyStep(const MigrationStep& step, json& fields, const Evaluator& evaluate, const json* archived = nullptr);

/// @brief The steps that undo a migration, last step first.
/// @param migration The migration.
/// @return The steps, or std::nullopt if a step has no inverse (a convert, split or merge without "inverse"): then
/// undoing it restores the records archived before it ran.
[[nodiscard]] std::optional<std::vector<MigrationStep>> inverseOf(const Migration& migration);

/// @brief Whether a migration takes values away (a drop): it needs the person's confirmation.
/// @param migration The migration.
/// @return true if it drops a field.
[[nodiscard]] bool dropsData(const Migration& migration) noexcept;

/// @brief A step as a reviewer reads it: "renames artist to by".
/// @param step The step.
/// @return The line.
[[nodiscard]] std::string describe(const MigrationStep& step);

} // namespace affresco::data
