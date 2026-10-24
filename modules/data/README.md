# data: models, records and migrations

`data` keeps an app's records apart from its document: the **data** layer of the four (view, logic, model, data;
SCREEN_FORMAT.md, "Data: models and records"). A `Store` holds collections of records in a SQLite database of its own
(`<data folder>/<screen name>.data.sqlite`, WAL mode; in memory for a screen from text), follows the document's
`models` (the **model** layer: collections, typed fields, references, a version), checks every write against them,
journals every write with undo and redo, runs queries, and applies each model change as a migration: dry-run on a
copy, archived, atomic, reversible. It knows nothing of screens, panels or expressions: the format module evaluates
migrations' and imports' expressions for it (`Evaluator`). Target: `affrescoui::data` (uses `core` and SQLite 3.54.1,
the amalgamation compiled in, `cmake/AffrescoUISqlite.cmake`).

## Public headers (`include/affresco/data/`)

| Header | Offers |
|---|---|
| `Model.hpp` | `parseModels` (the `models` section: fields of type string, number, integer, boolean, date, dateTime, enum, reference, list, object and map; required, default, unique, bounds; seed, importFrom), `validateRecord`, `validateValue`, `schemaOf` (a model as typed-state JSON Schema, what the checker reads), `isName`, `isRecordId`, `DataError` (with a `code`), `Limits` |
| `Changes.hpp` | `compareModels`: what a model change does to the stored records, `ChangeClass` compatible, transforming or destructive, each change as a line; `migrationsFrom` |
| `Migration.hpp` | `parseMigration`, `migrationsOf`; the steps rename, convert, fill, drop, split and merge, `applyStep`, `inverseOf` (a drop's inverse restores from the archive), `seedFields` (a seed written for the first version, migrated), `describe` |
| `Query.hpp` | `parseQuery`: `from`, `where` (field comparisons `==`, `!=`, `<`, `<=`, `>`, `>=`, `in`, `contains`, `startsWith`, and `any` groups), `sort`, `limit`, `offset`, `count` |
| `Store.hpp` | `Store`: `follow` (the models of a version: create and seed, migrate, revert, check), `plan` (the same on a copy), `copy`, `insert`, `update`, `remove`, `get`, `query`, `count`, `run`, `Batch` (one step of undo), `undo`, `redo`, `journal`, `archived`, `restoreArchived`, `outline` (versions and counts, never a record), `onChange`, `changes`; `LiveQuery`, which tells when its results change |
| `Csv.hpp` | `parseCsv` (RFC 4180) and `importCsv`: rows into a collection, columns by header or mapping, cells converted to the fields' types, failing rows reported, all or none if asked |

## Invariants

- **No SQL from documents.** Every statement's text is a constant of `src/` (or assembled from constants by the query
  builder: operators come from a fixed list); collection names, field paths and values are always bound parameters.
  The schema is fixed (`records`, `journal`, `migrations`, `archive`, `collections`, `meta`): a model never creates a
  table or a column, so no document text is ever an identifier. Records are JSON bodies, read with `json_extract`.
- **A bad record never gets in.** Each write is checked against its model (types, required, enum values, bounds,
  unknown fields), its references must exist and its unique fields be unique, inside one transaction.
- **Model edits never erase values.** A field a model stops declaring keeps its values in the records' bodies,
  unread, and they come back if it does; only a migration's `drop` takes values away, and it archives them first.
- **Migrations are all or nothing.** `follow` runs in one transaction: every record a migration changes is archived
  (its body before), migrated, checked against the new model; one that doesn't fit refuses the whole change and the
  store is as it was. `plan` runs the same on an in-memory copy (`sqlite3_backup`), so a dry run is the real code path.
- **Reverts follow the history.** The store records each migration (its steps and the model before). When a model's
  version falls (a commit reverted), its migrations past that version are undone, newest first: by their inverse when
  every step has one and the records then fit the older model, else by putting the archived records back; records
  written since that the older model can't hold are archived, never lost.
- **Undo is per group and per model version.** A `Batch` (the format module opens one per event) makes one undo step;
  undo refuses a write made under a model version that has changed since.
- **Bounded:** `Limits` (and SECURITY.md, "Limits"): 256 collections, 256 fields, 8 levels, a record of 1 MiB,
  1,000,000 records a collection, 10,000 results a query, 64 conditions, 256 migrations and 64 steps, a database of
  1 GiB (`max_page_count`), 100,000 journal entries (the oldest go), a CSV of 16 MiB, 100,000 rows and 256 columns.
  SQLite runs with extension loading compiled out, no triggers or views, `SQLITE_DBCONFIG_DEFENSIVE`, no attached
  databases.

## Threads

One thread: the store and its signals belong to the thread that made it (the main thread for a screen). SQLite is
built thread-safe, one connection per thread (`SQLITE_THREADSAFE=2`), single-threaded in the browser build.

## Errors

Writes and queries throw `DataError` with a `code` (`invalid`, `unique`, `reference`, `notFound`, `model`, `limit`,
`migration`, `io`) and a message naming the place: `notes[17].duration: expected an integer, got 1.5`. A malformed
`models` section throws `DocumentError` naming its place (`models.notes.fields.kind.values: ...`). A refused change
changes nothing.

## Files

```
include/affresco/data/  Model.hpp, Changes.hpp, Migration.hpp, Query.hpp, Store.hpp, Csv.hpp
src/
  Sqlite.hpp / .cpp     the RAII layer over SQLite: connection, statements, transactions (savepoints), copies
  Model.cpp             reading models, checking records
  Changes.cpp           classifying model changes
  Migration.cpp         reading and applying migrations, inverses, seeds
  Query.cpp             reading queries
  StoreImpl.hpp         the store's private parts: rows, links, the journal
  Store.cpp             opening the database, records, the query builder, outlines, live queries
  StoreJournal.cpp      the journal, undo and redo, the archive
  StoreFollow.cpp       following models: create and seed or import, migrate, revert, check; plan
  Csv.cpp               CSV import
CMakeLists.txt          the affrescoui_data target, with SQLite's amalgamation
```

## Recipes

**Keep an app's records on disk.** `Screen::fromFile` does it (`setDataFile`, `<data folder>/<name>.data.sqlite`);
a host with its own folder calls `screen.setDataFile(path)` after `setStateFile`. Tests and pictures use a screen
from text, whose store is in memory, so they are the same on any machine.

**Read and write from native code.** `screen.data()` is the store: `insert`, `update`, `remove`, `run({"from":
"notes", "where": {...}})`, `LiveQuery` for a native view that follows a query.

**Change a model.** A revision of `/models/<collection>`: compatible changes as they are; a transforming one with the
version raised and its migration added; the dry run and the review say what it does to the records.
