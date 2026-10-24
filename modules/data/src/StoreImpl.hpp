#pragma once

/// @file
/// @brief data::Store::Impl: the connection, the cached models and the pieces the store's files share: reading and
/// writing rows, the journal, checks of references and uniqueness.

#include "Sqlite.hpp"

#include "affresco/data/Store.hpp"

#include <set>

namespace affresco::data {

/// @brief A record as it lies in the database: its body holds every field it was given, declared or not.
struct Row {
	std::int64_t sequence = 0; ///< Its place in the order records were stored in.
	std::string id;            ///< Its id.
	std::int64_t created = 0;  ///< When it was stored.
	std::int64_t updated = 0;  ///< When it last changed.
	json body;                 ///< Its fields, the undeclared ones too.
};

struct Store::Impl {
	/// @brief Opens the database and makes its tables.
	/// @param path The file, or empty for memory.
	explicit Impl(std::filesystem::path path);

	sql::Database db;                    ///< The connection.
	std::filesystem::path file;          ///< See Store::file().
	Evaluator evaluate;                  ///< Migration and import expressions.
	std::function<std::int64_t()> clock; ///< Records' times.
	Models models;                       ///< The models followed last.
	json section;                        ///< Their section, as written.
	std::int64_t group = 0;              ///< The batch's group of writes, while one is open.
	int batches = 0;                     ///< Batches open.
	std::set<std::string> changed;       ///< Collections changed by the running follow().
	std::uint64_t changes = 0;           ///< See Store::changes().

	/// @brief Reads the models kept in the database (after opening, or after a copy).
	void loadModels();

	/// @brief The model of a collection the store holds.
	/// @param collection The collection.
	/// @param where Its place, for errors.
	/// @return The model.
	/// @throws DataError "model" if there is none.
	[[nodiscard]] const Model& modelOf(const std::string& collection, const std::string& where) const;

	/// @brief The version a collection is stored at.
	/// @param collection The collection.
	/// @return It, or std::nullopt if the store has never held it.
	[[nodiscard]] std::optional<int> storedVersion(const std::string& collection);

	/// @brief Reads a row.
	/// @param collection The collection.
	/// @param id The record.
	/// @return It, or std::nullopt.
	[[nodiscard]] std::optional<Row> row(const std::string& collection, const std::string& id);

	/// @brief A batch of rows in the order they were stored, from a place on.
	/// @param collection The collection.
	/// @param after Rows after this sequence.
	/// @param count At most this many.
	/// @return They.
	[[nodiscard]] std::vector<Row> rows(const std::string& collection, std::int64_t after, std::size_t count);

	/// @brief Writes a row's body and time.
	/// @param collection The collection.
	/// @param row The row.
	void writeBody(const std::string& collection, const Row& row);

	/// @brief Adds a row (at its sequence, if it has one).
	/// @param collection The collection.
	/// @param row The row; its sequence is set.
	void insertRow(const std::string& collection, Row& row);

	/// @brief Deletes a row.
	/// @param collection The collection.
	/// @param id Its id.
	void deleteRow(const std::string& collection, const std::string& id);

	/// @brief The number of rows of a collection.
	/// @param collection The collection.
	/// @return It.
	[[nodiscard]] std::size_t rowCount(const std::string& collection);

	/// @brief A record as screens read it: its declared fields only.
	/// @param model Its model.
	/// @param row The row.
	/// @return The record.
	[[nodiscard]] static Record recordOf(const Model& model, const Row& row);

	/// @brief Checks the references and unique fields of a record about to be stored.
	/// @param model The model.
	/// @param fields Its declared fields.
	/// @param id Its id (skipped by the unique check).
	/// @param where Its place.
	/// @throws DataError "reference" or "unique".
	void checkLinks(const Model& model, const json& fields, const std::string& id, const std::string& where);

	/// @brief Journals a write (in the open batch's group, or a group of its own), and drops what redo held.
	/// @param author Who.
	/// @param action "insert", "update" or "delete".
	/// @param collection The collection.
	/// @param id The record.
	/// @param sequence The row's sequence.
	/// @param before The row before (null for an insert).
	/// @param after The row after (null for a delete).
	void journal(const std::string& author, const char* action, const std::string& collection, const std::string& id,
	             std::int64_t sequence, const json& before, const json& after);

	/// @brief The next free generated id of a collection.
	/// @param collection The collection.
	/// @return It.
	[[nodiscard]] std::string nextId(const std::string& collection);

	/// @brief Applies every record of the models' changes: what follow() and plan() share.
	/// @param section The models.
	/// @param author Who.
	/// @param oldState For importFrom.
	/// @return The report; throws nothing for records that don't fit (the report says so).
	FollowReport apply(const json& section, const std::string& author, const json* oldState);
};

/// @brief A row's body and times as a record's JSON with its sequence, for the journal and the archive.
/// @param row The row.
/// @return `{"id", "created", "updated", ...body}`.
[[nodiscard]] json rowJson(const Row& row);

/// @brief A row from what rowJson() gave.
/// @param value The JSON.
/// @param sequence Its sequence.
/// @return The row.
[[nodiscard]] Row rowFrom(const json& value, std::int64_t sequence);

/// @brief The time now, from the system clock.
/// @return ms since 1970, UTC.
[[nodiscard]] std::int64_t systemNow();

} // namespace affresco::data
