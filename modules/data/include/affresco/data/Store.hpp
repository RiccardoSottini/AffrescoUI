#pragma once

/// @file
/// @brief data::Store: an app's records, kept apart from its document in a SQLite database of their own
/// (`<data dir>/<screen name>.data.sqlite`, WAL mode), each write checked against the models, journaled with undo
/// and redo, and every model change applied as an atomic, archived migration.

#include "affresco/core/Signal.hpp"
#include "affresco/data/Migration.hpp"
#include "affresco/data/Model.hpp"
#include "affresco/data/Query.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::data {

/// @brief A stored record.
struct Record {
	std::string id;           ///< Its stable id.
	std::int64_t created = 0; ///< When it was stored (ms since 1970, UTC).
	std::int64_t updated = 0; ///< When it last changed.
	json fields;              ///< Its fields the model declares.

	/// @brief The record as screens read it: `{"id", "created", "updated", ...fields}`.
	/// @return The object.
	[[nodiscard]] json toJson() const;
};

/// @brief One entry of the journal: a write, who made it and when, and the record before and after.
struct JournalEntry {
	std::int64_t sequence = 0; ///< Its place in the journal.
	std::int64_t group = 0;    ///< The writes undone and redone together (one event's, a Batch's).
	std::int64_t time = 0;     ///< When (ms since 1970, UTC).
	std::string author;        ///< Who: the person ("app"), an agent's name, "seed", "import".
	std::string action;        ///< "insert", "update" or "delete".
	std::string collection;    ///< The collection.
	std::string id;            ///< The record.
	json before;               ///< The record before (null for an insert).
	json after;                ///< The record after (null for a delete).
	bool undone = false;       ///< Whether it is undone (redo brings it back).

	/// @brief As JSON.
	/// @return `{"sequence", "group", "time", "author", "action", "collection", "id", "before", "after", "undone"}`.
	[[nodiscard]] json toJson() const;
};

/// @brief What following a version of the models did, or would do, to one collection.
struct CollectionReport {
	std::string collection;            ///< The collection.
	std::string action;                ///< "create", "migrate", "revert", "check" or "none".
	int from = 0;                      ///< The version stored before.
	int to = 0;                        ///< The version after.
	std::size_t records = 0;           ///< Records looked at.
	std::size_t changed = 0;           ///< Records changed.
	std::size_t failed = 0;            ///< Records that don't fit (the change is refused if any).
	std::size_t archived = 0;          ///< Records whose former values were archived.
	std::size_t seeded = 0;            ///< Records a new collection started with (seed or import).
	bool destructive = false;          ///< It drops values (archived).
	bool restoredArchive = false;      ///< A revert put the archived records back instead of running an inverse.
	std::vector<std::string> failures; ///< The first failures: "id 17: duration: expected an integer".
};

/// @brief What following a version of the models did, or would do.
struct FollowReport {
	std::vector<CollectionReport> collections; ///< The collections that change.

	/// @brief Whether every record fits.
	/// @return true if none failed.
	[[nodiscard]] bool ok() const noexcept;
	/// @brief Records changed in all.
	/// @return The count.
	[[nodiscard]] std::size_t changed() const noexcept;
	/// @brief One line for a reviewer: "songs: 312 records migrate (3 to 4), 3 fail: id 17: ...".
	/// @return The text; "" if nothing changes.
	[[nodiscard]] std::string text() const;
	/// @brief As JSON: `[{"collection", "action", "from", "to", "records", "changed", "failed", ...}]`.
	/// @return The list.
	[[nodiscard]] json toJson() const;
};

/// @brief A record archived by a migration: its fields before it, restorable.
struct ArchivedRecord {
	std::int64_t migration = 0; ///< The migration that archived it.
	Record record;              ///< The record as it was.
	std::string reason;         ///< "migrate" (values changed or dropped) or "revert" (didn't fit the older model).
};

/// @brief A screen's records: collections of typed records in one SQLite database, apart from the document.
///
/// The store follows the document's "models" (follow()): it creates collections (seeded once), migrates their
/// records when a model's version rises, and undoes a migration when it falls, all atomically. Every write is
/// validated against the model, so a bad record never gets in, and journaled. A field the models no longer declare
/// keeps its values in the records, unread, so a view or model edit never erases what was stored.
/// Invariants: one thread (the main thread); every SQL text is a constant of this module, values are bound.
class Store {
public:
	/// @brief Opens a store.
	/// @param file The database file (created if missing, WAL mode); empty for an in-memory store.
	/// @throws DataError "io" if it can't be opened.
	explicit Store(std::filesystem::path file = {});
	~Store();
	Store(const Store&) = delete;
	Store& operator=(const Store&) = delete;
	Store(Store&&) = delete;
	Store& operator=(Store&&) = delete;

	/// @brief Where the database is.
	/// @return The file, or empty for an in-memory store.
	[[nodiscard]] const std::filesystem::path& file() const noexcept;

	/// @brief Sets how migration and import expressions are evaluated (format::evaluateExpression).
	/// @param evaluate The evaluator.
	void setEvaluator(Evaluator evaluate);

	/// @brief Sets the clock records' times come from (the system's by default).
	/// @param clock Gives ms since 1970, UTC.
	void setClock(std::function<std::int64_t()> clock);

	/// @brief The models followed last.
	/// @return They.
	[[nodiscard]] const Models& models() const noexcept;

	/* Following the document's models */

	/// @brief Brings the store to a version of the models: creates new collections (with their seed, or their
	/// importFrom records from @p oldState), migrates or reverts the collections whose version changed, and checks
	/// every record against changed models. All at once, or nothing.
	/// @param section The document's "models" section (null: none).
	/// @param author Who changed the models, for the journal and the migrations.
	/// @param oldState A state an earlier version of the app kept on disk, for importFrom (nullptr: none).
	/// @return What it did.
	/// @throws DocumentError If the section is malformed; DataError "migration" naming the records that don't fit.
	FollowReport follow(const json& section, const std::string& author, const json* oldState = nullptr);

	/// @brief What follow() would do, on a copy of the store: nothing here changes.
	/// @param section The models.
	/// @param oldState As for follow().
	/// @return The report; ok() is false if records don't fit.
	/// @throws DocumentError If the section is malformed.
	[[nodiscard]] FollowReport plan(const json& section, const json* oldState = nullptr) const;

	/// @brief A copy of the store in memory (records, journal, archive and models), e.g. for a preview to change.
	/// @return The copy, with the same evaluator and clock.
	[[nodiscard]] std::unique_ptr<Store> copy() const;

	/// @brief Whether a version of the models differs from the one followed last.
	/// @param section The models.
	/// @return true if it does.
	[[nodiscard]] bool differs(const json& section) const;

	/* Records */

	/// @brief Stores a new record.
	/// @param collection The collection.
	/// @param fields Its fields; a given "id" is kept if it is free.
	/// @param author Who.
	/// @return The record stored.
	/// @throws DataError "model", "invalid", "unique", "reference" or "limit"; nothing changes then.
	Record insert(const std::string& collection, const json& fields, const std::string& author);
	/// @brief Changes some fields of a record (a null removes an optional field).
	/// @param collection The collection.
	/// @param id The record.
	/// @param changes The fields to change.
	/// @param author Who.
	/// @return The record after.
	/// @throws DataError "notFound" and as insert().
	Record update(const std::string& collection, const std::string& id, const json& changes, const std::string& author);
	/// @brief Deletes a record. References to it are refused or nullified, as their fields say.
	/// @param collection The collection.
	/// @param id The record.
	/// @param author Who.
	/// @throws DataError "notFound" or "reference".
	void remove(const std::string& collection, const std::string& id, const std::string& author);
	/// @brief Reads a record.
	/// @param collection The collection.
	/// @param id The record.
	/// @return It, or std::nullopt.
	[[nodiscard]] std::optional<Record> get(const std::string& collection, const std::string& id) const;
	/// @brief Reads the records a query selects.
	/// @param query The query.
	/// @return Them, at most query.limit.
	/// @throws DataError "model" if its collection isn't one.
	[[nodiscard]] std::vector<Record> query(const Query& query) const;
	/// @brief Counts the records a query selects (its limit and offset aside).
	/// @param query The query.
	/// @return The count.
	[[nodiscard]] std::size_t count(const Query& query) const;
	/// @brief Runs a query written as JSON (Query.hpp): its records as JSON, or its count.
	/// @param query The query.
	/// @param where Its place, for errors.
	/// @return A list of records, or a number for a count.
	/// @throws DataError If it is malformed.
	[[nodiscard]] json run(const json& query, const std::string& where) const;
	/// @brief The number of records of a collection.
	/// @param collection The collection.
	/// @return The count.
	[[nodiscard]] std::size_t size(const std::string& collection) const;

	/* The journal */

	/// @brief Groups the writes made while it lives into one step of undo and redo.
	class Batch {
	public:
		/// @brief Starts grouping (nested batches join the outermost).
		/// @param store The store; outlives the batch.
		explicit Batch(Store& store);
		~Batch();
		Batch(const Batch&) = delete;
		Batch& operator=(const Batch&) = delete;
		Batch(Batch&&) = delete;
		Batch& operator=(Batch&&) = delete;

	private:
		Store& store_;   ///< The store.
		bool outermost_; ///< Whether this one started the group.
	};

	/// @brief Whether there is a write to undo.
	/// @return true if there is.
	[[nodiscard]] bool canUndo() const;
	/// @brief Whether there is an undone write to redo.
	/// @return true if there is.
	[[nodiscard]] bool canRedo() const;
	/// @brief Undoes the last group of writes.
	/// @return Whether there was one.
	/// @throws DataError "model" if the collection's model changed since (undo stops at a migration).
	bool undo();
	/// @brief Redoes the last undone group.
	/// @return Whether there was one.
	/// @throws DataError As undo().
	bool redo();
	/// @brief The journal, newest first.
	/// @param limit At most this many entries.
	/// @param collection Only this collection's ("" for all).
	/// @return The entries.
	[[nodiscard]] std::vector<JournalEntry> journal(std::size_t limit, const std::string& collection = "") const;

	/* The archive */

	/// @brief The records a collection's migrations archived, newest first.
	/// @param collection The collection.
	/// @param limit At most this many.
	/// @return They.
	[[nodiscard]] std::vector<ArchivedRecord> archived(const std::string& collection, std::size_t limit = 1000) const;
	/// @brief Puts an archived record back as a new record of its collection, checked against the model now.
	/// @param collection The collection.
	/// @param migration The migration that archived it.
	/// @param id Its id.
	/// @param author Who.
	/// @return The record stored (a new id if its own is taken).
	/// @throws DataError "notFound", or as insert() if it doesn't fit the model now.
	Record restoreArchived(const std::string& collection, std::int64_t migration, const std::string& id,
	                       const std::string& author);

	/// @brief What agents see of the store: each collection's model version, its fields' types and its count of
	/// records, never a record.
	/// @return `{"songs": {"version": 3, "count": 312, "fields": {...}}}`.
	[[nodiscard]] json outline() const;

	Signal<const std::string&> onChange; ///< A collection's records changed (its name).

	/// @brief Counts the store's changes (writes, undo, redo, models followed), so what is worked out from it can be
	/// kept until it changes.
	/// @return The count.
	[[nodiscard]] std::uint64_t changes() const noexcept;

	struct Impl; ///< The connection and statements (StoreImpl.hpp).

private:
	/// @brief Counts a change and tells onChange.
	/// @param collection The collection that changed.
	void notify(const std::string& collection);

	std::unique_ptr<Impl> impl_; ///< See Impl.
};

/// @brief A query that tells when its results change: it runs again after each change of its collection and
/// emits onResults only if they differ.
class LiveQuery {
public:
	/// @brief Starts watching.
	/// @param store The store; outlives the live query.
	/// @param query The query, as JSON (Query.hpp).
	/// @throws DataError If it is malformed.
	LiveQuery(Store& store, json query);
	~LiveQuery();
	LiveQuery(const LiveQuery&) = delete;
	LiveQuery& operator=(const LiveQuery&) = delete;
	LiveQuery(LiveQuery&&) = delete;
	LiveQuery& operator=(LiveQuery&&) = delete;

	/// @brief The latest results.
	/// @return A list of records, or a count.
	[[nodiscard]] const json& results() const noexcept { return results_; }

	Signal<const json&> onResults; ///< The results changed.

private:
	Store& store_;          ///< The store.
	json query_;            ///< The query.
	std::string from_;      ///< Its collection.
	json results_;          ///< See results().
	Connection connection_; ///< To the store's onChange.
};

} // namespace affresco::data
