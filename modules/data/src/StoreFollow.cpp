/// @file
/// @brief data::Store: following the document's models: new collections (seeded or imported once), migrations,
/// reverts and checks, all at once or not at all; and plan(), the same on a copy (Store.hpp).

#include "StoreImpl.hpp"

#include "affresco/data/Changes.hpp"
#include "Places.hpp"

#include <format>
#include <map>

namespace affresco::data {

namespace {

/// Records read and changed at a time.
constexpr std::size_t batchSize = 1000;

/// @brief The steps of a chain of migrations, in order.
/// @param chain The migrations.
/// @return Their steps.
std::vector<MigrationStep> stepsOf(const std::vector<Migration>& chain) {
	std::vector<MigrationStep> steps;
	for(const Migration& migration : chain) steps.insert(steps.end(), migration.steps.begin(), migration.steps.end());
	return steps;
}

/// @brief Records a failure, the first ones by name.
/// @param report The collection's report.
/// @param id The record.
/// @param error What is wrong.
void failure(CollectionReport& report, const std::string& id, const std::string& error) {
	++report.failed;
	if(report.failures.size() < Limits::failuresReported) report.failures.push_back("id " + id + ": " + error);
}

/// @brief The message of a refused record without the store's place prefix.
/// @param error The error.
/// @return Its text.
std::string messageOf(const std::exception& error) { return error.what(); }

/// @brief A value at a state path, "state.habits.list", in a state object.
/// @param state The state.
/// @param path The path.
/// @return The value, or nullptr.
const json* atStatePath(const json& state, const std::string& path) {
	const json* at = &state;
	std::size_t start = 6; // after "state."
	while(at && start <= path.size()) {
		const std::size_t dot = std::min(path.find('.', start), path.size());
		const std::string key = path.substr(start, dot - start);
		if(!at->is_object() || !at->contains(key)) return nullptr;
		at = &(*at)[key];
		start = dot + 1;
	}
	return at;
}

/// @brief What following needs while it runs: the connection's impl, the report and the unique values seen.
struct Follower {
	Store::Impl& impl;         ///< The store.
	const std::string& author; ///< Who.

	/// @brief The model a collection was last stored with, if the store has one.
	/// @param name The collection.
	/// @return The model, or std::nullopt.
	std::optional<Model> storedModel(const std::string& name) {
		sql::Statement read(impl.db, "SELECT name, model FROM collections");
		json known = json::object();
		while(read.step()) known[read.text(0)] = stored(read.text(1));
		read.reset();
		if(!known.contains(name)) return std::nullopt;
		try {
			Models models = parseModels(known, "stored models");
			return models.at(name);
		} catch(const DocumentError&) {
			// a reference to a collection the store never held: read it alone, without its references' check
			json alone = known[name];
			for(auto& [field, definition] : alone["fields"].items())
				if(definition.value("type", "") == "reference") definition["to"] = name;
			return parseModels(json{{name, alone}}, "stored models").at(name);
		}
	}

	/// @brief Writes the collection's version and model.
	/// @param model The model.
	/// @param created Whether it is new.
	void storeModel(const Model& model, bool created) {
		if(created) {
			sql::Statement write(impl.db, "INSERT INTO collections(name, version, nextId, created, model) "
			                              "VALUES(?1, ?2, 1, ?3, ?4)");
			write.bind(1, model.name).bind(2, std::int64_t{model.version}).bind(3, impl.clock());
			write.bind(4, model.written.dump()).run();
			return;
		}
		sql::Statement write(impl.db, "UPDATE collections SET version = ?1, model = ?2 WHERE name = ?3");
		write.bind(1, std::int64_t{model.version}).bind(2, model.written.dump()).bind(3, model.name).run();
	}

	/// @brief Records a migration (or a revert) for later reverts and the archive.
	/// @param name The collection.
	/// @param from The version before.
	/// @param to The version after.
	/// @param kind "migrate" or "revert".
	/// @param definition The migrations run.
	/// @param before The model before.
	/// @return Its sequence.
	std::int64_t recordMigration(const std::string& name, int from, int to, const char* kind, const json& definition,
	                             const json& before) {
		sql::Statement write(impl.db, "INSERT INTO migrations(collection, fromVersion, toVersion, time, author, kind, "
		                              "definition, modelBefore) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
		write.bind(1, name).bind(2, std::int64_t{from}).bind(3, std::int64_t{to}).bind(4, impl.clock());
		write.bind(5, author).bind(6, std::string_view(kind)).bind(7, definition.dump()).bind(8, before.dump()).run();
		return impl.db.lastRow();
	}

	/// @brief Archives a record's former values.
	/// @param migration The migration archiving it.
	/// @param name The collection.
	/// @param row The row as it was.
	/// @param reason "migrate" or "revert".
	void archive(std::int64_t migration, const std::string& name, const Row& row, const char* reason) {
		sql::Statement write(impl.db, "INSERT INTO archive(migration, collection, id, recordSeq, created, updated, "
		                              "body, reason) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
		write.bind(1, migration).bind(2, name).bind(3, row.id).bind(4, row.sequence).bind(5, row.created);
		write.bind(6, row.updated).bind(7, row.body.dump()).bind(8, std::string_view(reason)).run();
	}

	/// @brief A record's archived values from a migration.
	/// @param migration The migration.
	/// @param id The record.
	/// @return Its body then, or std::nullopt.
	std::optional<json> archivedBody(std::int64_t migration, const std::string& id) {
		sql::Statement read(impl.db, "SELECT body FROM archive WHERE migration = ?1 AND id = ?2 LIMIT 1");
		read.bind(1, migration).bind(2, id);
		if(!read.step()) return std::nullopt;
		return stored(read.text(0));
	}

	/// @brief A row's body made to fit a model: the declared fields checked (defaults added), the others kept unread.
	/// @param model The model.
	/// @param body The body.
	/// @param where The record's place.
	/// @param unique The unique values seen so far, by field (dumped value -> id).
	/// @param id The record.
	/// @return The body.
	/// @throws DataError If it doesn't fit.
	static json fitted(const Model& model, const json& body, const std::string& where,
	                   std::map<std::string, std::map<std::string, std::string>>& unique, const std::string& id) {
		json declared = json::object();
		for(const auto& [name, value] : body.items())
			if(model.fields.contains(name)) declared[name] = value;
		const json checked = validateRecord(model, declared, where);
		for(const auto& [name, field] : model.fields)
			if(field.unique && checked.contains(name)) {
				const auto [seen, fresh] = unique[name].emplace(checked[name].dump(), id);
				if(!fresh)
					throw DataError("unique", std::format("{}: '{}' has it too", placeOf(where, name), seen->second));
			}
		json result = body;
		for(const std::string& name : model.order) result.erase(name);
		for(const auto& [name, value] : checked.items()) result[name] = value;
		return result;
	}

	/// @brief Runs steps over every record of a collection and checks each against a model, writing what changes.
	/// @param model The model the records must fit after.
	/// @param steps The steps (none: only the check).
	/// @param migration The migration archiving the former values (0: nothing archived).
	/// @param restoreFrom For Restore steps: the migration whose archive holds the values.
	/// @param report The collection's report.
	void transform(const Model& model, const std::vector<MigrationStep>& steps, std::int64_t migration,
	               std::int64_t restoreFrom, CollectionReport& report) {
		std::map<std::string, std::map<std::string, std::string>> unique;
		for(std::int64_t after = 0;;) {
			const std::vector<Row> rows = impl.rows(model.name, after, batchSize);
			if(rows.empty()) break;
			for(const Row& row : rows) {
				after = row.sequence;
				++report.records;
				try {
					json body = row.body;
					std::optional<json> archived;
					for(const MigrationStep& step : steps) {
						if(step.kind == MigrationStep::Kind::Restore && !archived)
							archived = archivedBody(restoreFrom, row.id).value_or(json::object());
						applyStep(step, body, impl.evaluate, archived ? &*archived : nullptr);
					}
					body = fitted(model, body, model.name + "[" + row.id + "]", unique, row.id);
					if(body == row.body) continue;
					++report.changed;
					if(migration) {
						archive(migration, model.name, row, "migrate");
						++report.archived;
					}
					Row next = row;
					next.body = std::move(body);
					impl.writeBody(model.name, next);
				} catch(const DocumentError& error) {
					failure(report, row.id, messageOf(error));
				}
			}
		}
	}

	/// @brief The id a first record keeps: its own, if it is one and free, else the next generated one.
	/// @param model The model.
	/// @param fields The record's fields.
	/// @return The id.
	std::string firstId(const Model& model, const json& fields) {
		const json given = fields.value("id", json());
		std::string id = given.is_string()           ? given.get<std::string>()
		                 : given.is_number_integer() ? std::to_string(given.get<std::int64_t>())
		                                             : std::string();
		if(!isRecordId(id) || impl.row(model.name, id)) id = impl.nextId(model.name);
		return id;
	}

	/// @brief One of a new collection's first records, stored: an item of an old state mapped by importFrom's
	/// expression, or a seed record (written for the first version, migrated).
	/// @param model The model.
	/// @param item The item.
	/// @param index Its position.
	/// @param importing Whether it comes from an old state.
	/// @param where Its place.
	/// @param unique The unique values seen so far.
	void storeFirst(const Model& model, const json& item, std::size_t index, bool importing, const std::string& where,
	                std::map<std::string, std::map<std::string, std::string>>& unique) {
		json fields = item;
		const std::string mapping = importing && model.importFrom ? model.importFrom->expression : std::string();
		if(!mapping.empty()) {
			if(!impl.evaluate) throw DataError("migration", "importFrom needs the screen format's expressions");
			fields = impl.evaluate(mapping, {{"it", item}, {"index", index}});
		}
		if(!importing) fields = seedFields(model, fields, impl.evaluate, where); // written for version 1
		if(!fields.is_object()) throw DataError("invalid", where + ": expected a record");
		const std::string id = firstId(model, fields);
		json body = fitted(model, fields, where, unique, id);
		for(const auto& [name, value] : fields.items()) // only declared fields come in
			if(!model.fields.contains(name)) body.erase(name);
		const std::int64_t now = impl.clock();
		Row row{0, id, now, now, std::move(body)};
		impl.insertRow(model.name, row);
	}

	/// @brief Brings a new collection's first records: its importFrom ones from an old state, else its seed.
	/// @param model The model.
	/// @param oldState The old state, or nullptr.
	/// @param report The collection's report.
	void populate(const Model& model, const json* oldState, CollectionReport& report) {
		const json* imported = oldState && model.importFrom ? atStatePath(*oldState, model.importFrom->path) : nullptr;
		const bool importing = imported && imported->is_array();
		const json& source = importing ? *imported : model.seed;
		std::map<std::string, std::map<std::string, std::string>> unique;
		for(std::size_t i = 0; i < source.size() && i < Limits::seedRecords; ++i) try {
				storeFirst(model, source[i], i, importing,
				           std::format("{}.{}[{}]", model.name, importing ? "importFrom" : "seed", i), unique);
				++report.seeded;
			} catch(const DocumentError& error) {
				if(!importing) failure(report, std::to_string(i), messageOf(error));
				else if(report.failures.size() < Limits::failuresReported) // it stays in the old file's copy
					report.failures.push_back("not imported: " + messageOf(error));
			}
		if(importing) report.action = "import";
	}

	/// @brief Undoes the migrations that took a collection past a version, newest first: their inverse where every
	/// step has one and the records then fit, else the archive they made.
	/// @param model The model it goes back to.
	/// @param from The version stored.
	/// @param report The collection's report.
	void revert(const Model& model, int from, CollectionReport& report) {
		sql::Statement read(impl.db, "SELECT seq, definition FROM migrations WHERE collection = ?1 AND kind = "
		                             "'migrate' AND reverted = 0 AND toVersion > ?2 AND toVersion <= ?3 ORDER BY seq "
		                             "DESC");
		read.bind(1, model.name).bind(2, std::int64_t{model.version}).bind(3, std::int64_t{from});
		std::vector<std::pair<std::int64_t, json>> undone;
		while(read.step()) undone.emplace_back(read.integer(0), stored(read.text(1)));
		read.reset();
		const std::int64_t revertSeq =
		    recordMigration(model.name, from, model.version, "revert", json::array(), model.written);
		for(const auto& [sequence, definition] : undone) {
			sql::Statement mark(impl.db, "UPDATE migrations SET reverted = 1 WHERE seq = ?1");
			mark.bind(1, sequence).run();
		}
		// The inverse of each, newest first
		bool invertible = true;
		std::vector<std::pair<std::int64_t, std::vector<MigrationStep>>> inverses;
		for(const auto& [sequence, definition] : undone) {
			Migration chain;
			for(const json& written : definition) {
				const Migration one = parseMigration(written, model.name, "migrations");
				chain.steps.insert(chain.steps.end(), one.steps.begin(), one.steps.end());
			}
			std::optional<std::vector<MigrationStep>> inverse = inverseOf(chain);
			if(!inverse) invertible = false;
			else inverses.emplace_back(sequence, std::move(*inverse));
		}
		if(invertible) {
			CollectionReport trial = report;
			sql::Transaction attempt(impl.db);
			for(const auto& [sequence, steps] : inverses) transform(model, steps, 0, sequence, trial);
			if(trial.failed == 0) {
				attempt.commit();
				report = std::move(trial);
				return;
			}
		}
		// The archive of the oldest one undone: the records as they were then
		restoreArchive(model, undone.empty() ? 0 : undone.back().first, revertSeq, report);
	}

	/// @brief Puts back the records a migration archived, and archives the records that don't fit the model.
	/// @param model The model.
	/// @param migration The migration whose archive is restored (0: none).
	/// @param revertSeq The revert, which archives what doesn't fit.
	/// @param report The collection's report.
	void restoreArchive(const Model& model, std::int64_t migration, std::int64_t revertSeq, CollectionReport& report) {
		report.restoredArchive = true;
		std::map<std::string, std::map<std::string, std::string>> unique;
		for(std::int64_t after = 0;;) {
			const std::vector<Row> rows = impl.rows(model.name, after, batchSize);
			if(rows.empty()) break;
			for(const Row& row : rows) {
				after = row.sequence;
				++report.records;
				Row next = row;
				if(const std::optional<json> archived = migration ? archivedBody(migration, row.id) : std::nullopt)
					next.body = *archived;
				try {
					next.body = fitted(model, next.body, model.name + "[" + row.id + "]", unique, row.id);
					if(next.body == row.body) continue;
					++report.changed;
					impl.writeBody(model.name, next);
				} catch(const DocumentError&) { // made after the migration, for the newer model: kept, restorable
					archive(revertSeq, model.name, row, "revert");
					impl.deleteRow(model.name, row.id);
					++report.archived;
				}
			}
		}
	}

	/// @brief Follows one collection's model.
	/// @param model The model now.
	/// @param oldState For importFrom.
	/// @return Its report.
	CollectionReport follow(const Model& model, const json* oldState) {
		CollectionReport report;
		report.collection = model.name;
		report.action = "none";
		report.from = report.to = model.version;
		const std::optional<int> stored = impl.storedVersion(model.name);
		if(!stored) {
			report.action = "create";
			report.from = 0;
			storeModel(model, true);
			populate(model, oldState, report);
			impl.changed.insert(model.name);
			return report;
		}
		report.from = *stored;
		const std::optional<Model> before = storedModel(model.name);
		const json beforeWritten = before ? before->written : json::object();
		if(model.version > *stored) {
			const std::vector<Migration> chain = migrationsFrom(model, *stored, "models." + model.name);
			const std::vector<MigrationStep> steps = stepsOf(chain);
			json definition = json::array();
			for(const Migration& migration : chain) definition.push_back(migration.written);
			report.action = steps.empty() ? "check" : "migrate";
			report.destructive = std::ranges::any_of(chain, dropsData);
			const std::int64_t sequence = steps.empty() ? 0
			                                            : recordMigration(model.name, *stored, model.version, "migrate",
			                                                              definition, beforeWritten);
			transform(model, steps, sequence, 0, report);
		} else if(model.version < *stored) {
			report.action = "revert";
			revert(model, *stored, report);
		} else if(beforeWritten.value("fields", json()) != model.written.value("fields", json())) {
			report.action = "check";
			transform(model, {}, 0, 0, report);
		}
		if(report.action != "none") {
			storeModel(model, false);
			if(report.changed || report.archived) impl.changed.insert(model.name);
		}
		return report;
	}
};

} // namespace

FollowReport Store::Impl::apply(const json& next, const std::string& author, const json* oldState) {
	const json written = next.is_null() ? json::object() : next;
	const Models parsed = parseModels(written);
	for(const auto& [name, model] : parsed) (void)migrationsOf(model, "models." + name); // malformed ones throw
	const Models previous = std::exchange(models, parsed); // writes and checks read the new models
	FollowReport report;
	try {
		Follower follower{*this, author};
		for(const auto& [name, model] : parsed) {
			CollectionReport collection = follower.follow(model, oldState);
			if(collection.action != "none") report.collections.push_back(std::move(collection));
		}
		// Seeds and imports may refer to each other: their references are checked once all are in
		for(CollectionReport& collection : report.collections) {
			if(collection.action != "create" && collection.action != "import") continue;
			const Model& model = parsed.at(collection.collection);
			for(const Row& row : rows(model.name, 0, Limits::seedRecords)) try {
					checkLinks(model, recordOf(model, row).fields, row.id, model.name + "[" + row.id + "]");
				} catch(const DataError& error) {
					failure(collection, row.id, error.what());
				}
		}
	} catch(...) {
		models = previous;
		throw;
	}
	sql::Statement write(db, "INSERT OR REPLACE INTO meta(key, value) VALUES('models', ?1)");
	write.bind(1, written.dump()).run();
	section = written;
	return report;
}

FollowReport Store::follow(const json& section, const std::string& author, const json* oldState) {
	Impl& impl = *impl_;
	const Models previous = impl.models;
	const json previousSection = impl.section;
	impl.changed.clear();
	FollowReport report;
	{
		sql::Transaction transaction(impl.db);
		report = impl.apply(section, author, oldState);
		if(!report.ok()) {
			impl.models = previous;
			impl.section = previousSection;
			throw DataError("migration",
			                "data: the change to the models is refused, some records don't fit: " + report.text());
		}
		transaction.commit();
	}
	++impl.changes; // even when no record changed: the models did
	for(const std::string& collection : std::exchange(impl.changed, {})) notify(collection);
	return report;
}

std::unique_ptr<Store> Store::copy() const {
	auto copy = std::make_unique<Store>();
	sql::copy(impl_->db, copy->impl_->db);
	copy->impl_->loadModels();
	copy->impl_->evaluate = impl_->evaluate;
	copy->impl_->clock = impl_->clock;
	return copy;
}

FollowReport Store::plan(const json& section, const json* oldState) const {
	const std::unique_ptr<Store> copied = copy();
	const sql::Transaction transaction(copied->impl_->db); // never committed: the copy is dropped anyway
	return copied->impl_->apply(section, "plan", oldState);
}

} // namespace affresco::data
