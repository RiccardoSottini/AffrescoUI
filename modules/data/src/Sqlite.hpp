#pragma once

/// @file
/// @brief data::sql: a thin RAII layer over SQLite: a connection, prepared statements with bound parameters, and
/// transactions. Every statement's text is a constant of the data module; documents only ever give bound values.

#include "affresco/core/Json.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace affresco::data::sql {

/// @brief A database connection.
class Database {
public:
	/// @brief Opens (or creates) a database file, or an in-memory one.
	/// @param file The file; empty for an in-memory database.
	/// @throws DataError "io" if it can't be opened.
	explicit Database(const std::filesystem::path& file);
	~Database();
	Database(const Database&) = delete;
	Database& operator=(const Database&) = delete;
	Database(Database&&) = delete;
	Database& operator=(Database&&) = delete;

	/// @brief Runs statements that take no parameters (the schema, pragmas).
	/// @param text The statements: constants of this module only.
	/// @throws DataError "io" on failure.
	void run(const char* text);

	/// @brief The connection.
	/// @return It.
	[[nodiscard]] sqlite3* handle() const noexcept { return db_; }

	/// @brief The rowid of the last insert.
	/// @return It.
	[[nodiscard]] std::int64_t lastRow() const noexcept;

	/// @brief Rows changed by the last statement.
	/// @return The count.
	[[nodiscard]] int changes() const noexcept;

	/// @brief The last error, for messages.
	/// @return The text.
	[[nodiscard]] std::string error() const;

private:
	sqlite3* db_ = nullptr; ///< The connection.
};

/// @brief Copies a whole database into another one (SQLite's online backup), replacing what it held.
/// @param from The database copied.
/// @param to The copy.
/// @throws DataError "io" on failure.
void copy(const Database& from, const Database& to);

/// @brief A prepared statement, reset after each use.
class Statement {
public:
	/// @brief Prepares a statement.
	/// @param database The connection; outlives the statement.
	/// @param text The statement: assembled only from this module's constants, values bound with bind().
	/// @throws DataError "io" if it doesn't prepare.
	Statement(Database& database, std::string_view text);
	~Statement();
	Statement(const Statement&) = delete;
	Statement& operator=(const Statement&) = delete;
	Statement(Statement&&) = delete;
	Statement& operator=(Statement&&) = delete;

	/// @brief Binds a text.
	/// @param index The parameter, from 1.
	/// @param value The text.
	/// @return This statement.
	Statement& bind(int index, std::string_view value);
	/// @brief Binds an integer.
	/// @param index The parameter, from 1.
	/// @param value The integer.
	/// @return This statement.
	Statement& bind(int index, std::int64_t value);
	/// @brief Binds a JSON scalar as SQLite's JSON functions compare it: text, integer, real, 1 or 0, or NULL.
	/// @param index The parameter, from 1.
	/// @param value The value.
	/// @return This statement.
	Statement& bindValue(int index, const json& value);
	/// @brief Binds NULL.
	/// @param index The parameter, from 1.
	/// @return This statement.
	Statement& bindNull(int index);

	/// @brief Steps to the next row.
	/// @return true if there is one, false when done.
	/// @throws DataError "io" (or "limit" when the database is full) on failure.
	bool step();
	/// @brief Runs it to the end, ignoring rows.
	void run();
	/// @brief Resets it for another use, keeping nothing bound.
	void reset() noexcept;

	/// @brief A column of the current row, as text.
	/// @param column From 0.
	/// @return The text ("" for NULL).
	[[nodiscard]] std::string text(int column) const;
	/// @brief A column of the current row, as an integer.
	/// @param column From 0.
	/// @return The integer.
	[[nodiscard]] std::int64_t integer(int column) const noexcept;
	/// @brief Whether a column of the current row is NULL.
	/// @param column From 0.
	/// @return true if it is.
	[[nodiscard]] bool isNull(int column) const noexcept;

private:
	Database& database_;                ///< The connection.
	sqlite3_stmt* statement_ = nullptr; ///< The statement.
};

/// @brief A transaction: committed by commit(), rolled back if it goes out of scope first. Nested ones are savepoints.
class Transaction {
public:
	/// @brief Begins one.
	/// @param database The connection; outlives it.
	explicit Transaction(Database& database);
	~Transaction();
	Transaction(const Transaction&) = delete;
	Transaction& operator=(const Transaction&) = delete;
	Transaction(Transaction&&) = delete;
	Transaction& operator=(Transaction&&) = delete;

	/// @brief Commits it.
	/// @throws DataError "io" on failure.
	void commit();

private:
	Database& database_; ///< The connection.
	bool done_ = false;  ///< Committed.
};

} // namespace affresco::data::sql
