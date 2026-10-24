/// @file
/// @brief The RAII layer over SQLite (Sqlite.hpp).

#include "Sqlite.hpp"

#include "affresco/data/Model.hpp"

#include <sqlite3.h>

#include <limits>

namespace affresco::data::sql {

namespace {

/// @brief Fails with the connection's error.
/// @param db The connection.
/// @param what What was being done.
[[noreturn]] void failed(sqlite3* db, const std::string& what) {
	const int code = db ? sqlite3_errcode(db) : SQLITE_ERROR;
	const std::string message = db ? sqlite3_errmsg(db) : "no database";
	if(code == SQLITE_FULL) throw DataError("limit", "data: the store is full (at most 1 GiB)");
	if(code == SQLITE_CONSTRAINT) throw DataError("unique", "data: " + message);
	throw DataError("io", "data: " + what + ": " + message);
}

} // namespace

Database::Database(const std::filesystem::path& file) {
	// SQLite takes UTF-8 file names everywhere (on Windows too, where path::string() is the ANSI code page's)
	const std::u8string utf8 = file.u8string();
	const std::string name = file.empty() ? std::string(":memory:") : std::string(utf8.begin(), utf8.end());
	const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE;
	if(sqlite3_open_v2(name.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
		const std::string message = db_ ? sqlite3_errmsg(db_) : "out of memory";
		sqlite3_close(db_);
		db_ = nullptr;
		throw DataError("io", "data: could not open " + name + ": " + message);
	}
	sqlite3_extended_result_codes(db_, 0);
	// Untrusted values never become SQL, but a bound value can still be large: keep every one within the record limit
	sqlite3_limit(db_, SQLITE_LIMIT_LENGTH, static_cast<int>(Limits::recordBytes * 4));
	sqlite3_limit(db_, SQLITE_LIMIT_SQL_LENGTH, 1 << 20);
	sqlite3_limit(db_, SQLITE_LIMIT_ATTACHED, 0);
	sqlite3_limit(db_, SQLITE_LIMIT_TRIGGER_DEPTH, 0);
	sqlite3_db_config(db_, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr); // no writes to the schema's own tables
	sqlite3_db_config(db_, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr);
	sqlite3_db_config(db_, SQLITE_DBCONFIG_ENABLE_TRIGGER, 0, nullptr);
	sqlite3_db_config(db_, SQLITE_DBCONFIG_ENABLE_VIEW, 0, nullptr);
	sqlite3_busy_timeout(db_, 2000);
}

Database::~Database() { sqlite3_close(db_); }

void Database::run(const char* text) {
	char* message = nullptr;
	if(sqlite3_exec(db_, text, nullptr, nullptr, &message) != SQLITE_OK) {
		const std::string error = message ? message : "error";
		sqlite3_free(message);
		if(sqlite3_errcode(db_) == SQLITE_FULL) throw DataError("limit", "data: the store is full (at most 1 GiB)");
		throw DataError("io", "data: " + error);
	}
}

std::int64_t Database::lastRow() const noexcept { return sqlite3_last_insert_rowid(db_); }

int Database::changes() const noexcept { return sqlite3_changes(db_); }

std::string Database::error() const { return sqlite3_errmsg(db_); }

void copy(const Database& from, const Database& to) {
	sqlite3_backup* backup = sqlite3_backup_init(to.handle(), "main", from.handle(), "main");
	if(!backup) failed(to.handle(), "copy");
	const int result = sqlite3_backup_step(backup, -1);
	sqlite3_backup_finish(backup);
	if(result != SQLITE_DONE) failed(to.handle(), "copy");
}

Statement::Statement(Database& database, std::string_view text) : database_(database) {
	if(text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) failed(database.handle(), "prepare");
	if(sqlite3_prepare_v3(database.handle(), text.data(), static_cast<int>(text.size()), SQLITE_PREPARE_PERSISTENT,
	                      &statement_, nullptr) != SQLITE_OK)
		failed(database.handle(), "prepare");
}

Statement::~Statement() { sqlite3_finalize(statement_); }

Statement& Statement::bind(int index, std::string_view value) {
	if(sqlite3_bind_text64(statement_, index, value.data(), value.size(), SQLITE_TRANSIENT, SQLITE_UTF8) != SQLITE_OK)
		failed(database_.handle(), "bind");
	return *this;
}

Statement& Statement::bind(int index, std::int64_t value) {
	if(sqlite3_bind_int64(statement_, index, value) != SQLITE_OK) failed(database_.handle(), "bind");
	return *this;
}

Statement& Statement::bindNull(int index) {
	if(sqlite3_bind_null(statement_, index) != SQLITE_OK) failed(database_.handle(), "bind");
	return *this;
}

Statement& Statement::bindValue(int index, const json& value) {
	if(value.is_string()) return bind(index, value.get_ref<const std::string&>());
	if(value.is_boolean()) return bind(index, std::int64_t{value.get<bool>() ? 1 : 0});
	if(value.is_number_integer()) return bind(index, value.get<std::int64_t>());
	if(value.is_number()) {
		if(sqlite3_bind_double(statement_, index, value.get<double>()) != SQLITE_OK) failed(database_.handle(), "bind");
		return *this;
	}
	if(value.is_null()) return bindNull(index);
	return bind(index, value.dump()); // a list, for "in": read with json_each
}

bool Statement::step() {
	const int result = sqlite3_step(statement_);
	if(result == SQLITE_ROW) return true;
	if(result == SQLITE_DONE) return false;
	const int code = sqlite3_errcode(database_.handle());
	const std::string message = sqlite3_errmsg(database_.handle());
	reset();
	if(code == SQLITE_FULL) throw DataError("limit", "data: the store is full (at most 1 GiB)");
	if(code == SQLITE_CONSTRAINT) throw DataError("unique", "data: " + message);
	throw DataError("io", "data: " + message);
}

void Statement::run() {
	while(step()) {
	}
	reset();
}

void Statement::reset() noexcept {
	sqlite3_reset(statement_);
	sqlite3_clear_bindings(statement_);
}

std::string Statement::text(int column) const {
	const unsigned char* value = sqlite3_column_text(statement_, column);
	if(!value) return {};
	return {reinterpret_cast<const char*>(value), static_cast<std::size_t>(sqlite3_column_bytes(statement_, column))};
}

std::int64_t Statement::integer(int column) const noexcept { return sqlite3_column_int64(statement_, column); }

bool Statement::isNull(int column) const noexcept { return sqlite3_column_type(statement_, column) == SQLITE_NULL; }

Transaction::Transaction(Database& database) : database_(database) { database_.run("SAVEPOINT affresco"); }

Transaction::~Transaction() {
	if(done_) return;
	try {
		database_.run("ROLLBACK TO affresco; RELEASE affresco");
	} catch(...) { // NOLINT(bugprone-empty-catch): a destructor reports nothing; the connection rolls back on close
	}
}

void Transaction::commit() {
	database_.run("RELEASE affresco");
	done_ = true;
}

} // namespace affresco::data::sql
