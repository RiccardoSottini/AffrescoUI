/// @file
/// @brief data::Store: opening the database, records, queries and what agents see (Store.hpp).

#include "StoreImpl.hpp"
#include "Places.hpp"

#include <chrono>
#include <format>

namespace affresco::data {

namespace {

/// The tables. Every text that reaches SQLite is a constant like this one; values are always bound.
constexpr const char* schema = R"sql(
CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS collections(name TEXT PRIMARY KEY, version INTEGER NOT NULL, nextId INTEGER NOT NULL,
	created INTEGER NOT NULL, model TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS records(seq INTEGER PRIMARY KEY, collection TEXT NOT NULL, id TEXT NOT NULL,
	created INTEGER NOT NULL, updated INTEGER NOT NULL, body TEXT NOT NULL, UNIQUE(collection, id));
CREATE INDEX IF NOT EXISTS records_order ON records(collection, seq);
CREATE TABLE IF NOT EXISTS journal(seq INTEGER PRIMARY KEY, grp INTEGER NOT NULL, time INTEGER NOT NULL,
	author TEXT NOT NULL, action TEXT NOT NULL, collection TEXT NOT NULL, id TEXT NOT NULL, recordSeq INTEGER,
	version INTEGER NOT NULL, before TEXT, after TEXT, state INTEGER NOT NULL DEFAULT 0);
CREATE INDEX IF NOT EXISTS journal_group ON journal(grp);
CREATE TABLE IF NOT EXISTS migrations(seq INTEGER PRIMARY KEY, collection TEXT NOT NULL, fromVersion INTEGER NOT NULL,
	toVersion INTEGER NOT NULL, time INTEGER NOT NULL, author TEXT NOT NULL, kind TEXT NOT NULL,
	definition TEXT NOT NULL, modelBefore TEXT NOT NULL, reverted INTEGER NOT NULL DEFAULT 0);
CREATE TABLE IF NOT EXISTS archive(seq INTEGER PRIMARY KEY, migration INTEGER NOT NULL, collection TEXT NOT NULL,
	id TEXT NOT NULL, recordSeq INTEGER, created INTEGER NOT NULL, updated INTEGER NOT NULL, body TEXT NOT NULL,
	reason TEXT NOT NULL);
CREATE INDEX IF NOT EXISTS archive_lookup ON archive(migration, id);
INSERT OR IGNORE INTO meta(key, value) VALUES('format', '1');
)sql";

/// @brief Fails with an unknown record.
/// @param collection The collection.
/// @param id The record.
[[noreturn]] void notFound(const std::string& collection, const std::string& id) {
	throw DataError("notFound", collection + ": no record '" + id + "'");
}

/// @brief What a field's value is in SQL, appending its parameters: a column for the record's own fields, else
/// json_extract with the field's path bound.
/// @param field The field.
/// @param parameters The parameters so far.
/// @return The SQL.
std::string fieldSql(const std::string& field, json& parameters) {
	if(field == "id" || field == "created" || field == "updated") return field; // the column's constant name
	parameters.push_back("$." + field); // a name (letters, digits, _): the path is bound, never written into the SQL
	return std::format("json_extract(body, ?{})", parameters.size() + 1);
}

/// @brief A condition in SQL.
/// @param condition The condition.
/// @param model The collection's model.
/// @param parameters The parameters so far.
/// @return The SQL.
std::string conditionSql(const Condition& condition, const Model& model, json& parameters) {
	const auto parameter = [&](json value) {
		parameters.push_back(std::move(value));
		return std::format("?{}", parameters.size() + 1);
	};
	const bool list =
	    model.fields.contains(condition.field) && model.fields.at(condition.field).type == FieldType::List;
	if(condition.op == "contains" && list) {
		parameters.push_back("$." + condition.field);
		const std::string path = std::format("?{}", parameters.size() + 1);
		return "EXISTS (SELECT 1 FROM json_each(body, " + path + ") WHERE value = " + parameter(condition.value) + ")";
	}
	const std::string value = fieldSql(condition.field, parameters);
	if(condition.value.is_null() && (condition.op == "==" || condition.op == "!="))
		return value + (condition.op == "==" ? " IS NULL" : " IS NOT NULL");
	if(condition.op == "==") return value + " = " + parameter(condition.value);
	if(condition.op == "!=") return "(" + value + " IS NULL OR " + value + " != " + parameter(condition.value) + ")";
	if(condition.op == "in") return value + " IN (SELECT value FROM json_each(" + parameter(condition.value) + "))";
	if(condition.op == "contains") return "instr(" + value + ", " + parameter(condition.value) + ") > 0";
	if(condition.op == "startsWith") {
		const std::string start = parameter(condition.value);
		return "substr(" + value + ", 1, length(" + start + ")) = " + start;
	}
	for(const char* op : {"<", "<=", ">", ">="}) // one of these, written from the constant, never from the document
		if(condition.op == op) return value + " " + op + " " + parameter(condition.value);
	throw DataError("invalid", "unknown comparison");
}

/// @brief A filter in SQL.
/// @param filter The filter.
/// @param model The collection's model.
/// @param parameters The parameters so far.
/// @return The SQL ("1" for none).
std::string filterSql(const Filter& filter, const Model& model, json& parameters) {
	std::string sql;
	for(const Condition& condition : filter.all)
		sql += (sql.empty() ? "" : " AND ") + conditionSql(condition, model, parameters);
	for(const std::vector<Filter>& group : filter.any) {
		std::string either;
		for(const Filter& inner : group) either += (either.empty() ? "" : " OR ") + filterSql(inner, model, parameters);
		sql += (sql.empty() ? "(" : " AND (") + either + ")";
	}
	return sql.empty() ? "1" : "(" + sql + ")";
}

/// @brief Binds a query's parameters: the collection first, then the others.
/// @param statement The statement.
/// @param collection The collection.
/// @param parameters The others.
void bindAll(sql::Statement& statement, const std::string& collection, const json& parameters) {
	statement.bind(1, collection);
	for(std::size_t i = 0; i < parameters.size(); ++i) statement.bindValue(static_cast<int>(i + 2), parameters[i]);
}

/// @brief A field of a collection that may refer to records.
struct ReferringField {
	const std::string& collection; ///< The collection it is in.
	const std::string& name;       ///< The field.
	const Field& field;            ///< Its definition.
};

/// @brief The records whose field refers to a record.
/// @param impl The store.
/// @param referring The field.
/// @param id The record referred to.
/// @return Their ids (at most 1,000).
std::vector<std::string> referrers(Store::Impl& impl, const ReferringField& referring, const std::string& id) {
	const bool list = referring.field.type == FieldType::List;
	sql::Statement read(
	    impl.db, list ? "SELECT id FROM records WHERE collection = ?1 AND EXISTS (SELECT 1 FROM json_each(body, "
	                    "?2) WHERE value = ?3) LIMIT 1000"
		              : "SELECT id FROM records WHERE collection = ?1 AND json_extract(body, ?2) = ?3 LIMIT 1000");
	read.bind(1, referring.collection).bind(2, "$." + referring.name).bind(3, id);
	std::vector<std::string> ids;
	while(read.step()) ids.push_back(read.text(0));
	return ids;
}

/// @brief Lets the records whose field refers to a record about to be deleted let go of it: a list drops the id, a
/// single reference is cleared, if the field's "onDelete" is "nullify"; else the delete is refused.
/// @param impl The store.
/// @param referring The field.
/// @param collection The collection of the record deleted.
/// @param id The record deleted.
/// @param author Who deletes it.
/// @param where Its place, for errors.
/// @return Whether a record changed.
/// @throws DataError "reference" if one refers to it and may not let go.
bool releaseReference(Store::Impl& impl, const ReferringField& referring, const std::string& collection,
                      const std::string& id, const std::string& author, const std::string& where) {
	const bool list = referring.field.type == FieldType::List;
	const Field& target = list ? *referring.field.of : referring.field;
	if(target.type != FieldType::Reference || target.to != collection) return false;
	const std::vector<std::string> ids = referrers(impl, referring, id);
	if(ids.empty()) return false;
	const std::string& name = referring.collection;
	if(target.onDelete != "nullify" || (referring.field.required && !list))
		throw DataError("reference",
		                where + ": '" + name + "' record '" + ids.front() + "' refers to it (" + referring.name + ")");
	for(const std::string& referrer : ids) {
		Row other = impl.row(name, referrer).value(); // found by the query above, in this transaction
		const json before = rowJson(other);
		if(!list) other.body.erase(referring.name);
		else {
			json kept = json::array();
			for(const json& item : other.body[referring.name])
				if(item != id) kept.push_back(item);
			other.body[referring.name] = std::move(kept);
		}
		other.updated = impl.clock();
		impl.writeBody(name, other);
		impl.journal(author, "update", name, referrer, other.sequence, before, rowJson(other));
	}
	return true;
}

} // namespace

json stored(std::string_view text) {
	try {
		return parseUntrusted(text); // the file is the user's: it may have been changed or damaged since
	} catch(const std::exception&) {
		throw DataError("io", "data: the store holds a damaged value (its file was changed outside the app)");
	}
}

std::int64_t systemNow() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
	    .count();
}

json Record::toJson() const {
	json value = {{"id", id}, {"created", created}, {"updated", updated}};
	for(const auto& [name, field] : fields.items()) value[name] = field;
	return value;
}

json JournalEntry::toJson() const {
	return {{"sequence", sequence},     {"group", group}, {"time", time},     {"author", author}, {"action", action},
	        {"collection", collection}, {"id", id},       {"before", before}, {"after", after},   {"undone", undone}};
}

json rowJson(const Row& row) {
	json value = {{"id", row.id}, {"created", row.created}, {"updated", row.updated}};
	for(const auto& [name, field] : row.body.items()) value[name] = field;
	return value;
}

Row rowFrom(const json& value, std::int64_t sequence) {
	Row row{sequence, value.value("id", ""), value.value("created", std::int64_t{0}),
	        value.value("updated", std::int64_t{0}), json::object()};
	for(const auto& [name, field] : value.items())
		if(!isSystemField(name)) row.body[name] = field;
	return row;
}

/* Impl */

Store::Impl::Impl(std::filesystem::path path) : db(path), file(std::move(path)), clock(systemNow) {
	if(!file.empty()) db.run("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;");
	db.run("PRAGMA page_size=4096; PRAGMA max_page_count=262144;"); // 1 GiB: Limits::databaseBytes
	db.run(schema);
	loadModels();
}

void Store::Impl::loadModels() {
	sql::Statement read(db, "SELECT value FROM meta WHERE key = 'models'");
	section = read.step() ? stored(read.text(0)) : json::object();
	read.reset();
	models = parseModels(section);
}

const Model& Store::Impl::modelOf(const std::string& collection, const std::string& where) const {
	const auto it = models.find(collection);
	if(it == models.end()) throw DataError("model", where + ": no collection '" + collection + R"(' in "models")");
	return it->second;
}

std::optional<int> Store::Impl::storedVersion(const std::string& collection) {
	sql::Statement read(db, "SELECT version FROM collections WHERE name = ?1");
	read.bind(1, collection);
	if(!read.step()) return std::nullopt;
	return static_cast<int>(read.integer(0));
}

std::optional<Row> Store::Impl::row(const std::string& collection, const std::string& id) {
	sql::Statement read(db, "SELECT seq, id, created, updated, body FROM records WHERE collection = ?1 AND id = ?2");
	read.bind(1, collection).bind(2, id);
	if(!read.step()) return std::nullopt;
	return Row{read.integer(0), read.text(1), read.integer(2), read.integer(3), stored(read.text(4))};
}

std::vector<Row> Store::Impl::rows(const std::string& collection, std::int64_t after, std::size_t count) {
	sql::Statement read(db, "SELECT seq, id, created, updated, body FROM records WHERE collection = ?1 AND seq > ?2 "
	                        "ORDER BY seq LIMIT ?3");
	read.bind(1, collection).bind(2, after).bind(3, static_cast<std::int64_t>(count));
	std::vector<Row> result;
	while(read.step())
		result.push_back({read.integer(0), read.text(1), read.integer(2), read.integer(3), stored(read.text(4))});
	return result;
}

void Store::Impl::writeBody(const std::string& collection, const Row& row) {
	sql::Statement write(db, "UPDATE records SET body = ?1, updated = ?2 WHERE collection = ?3 AND id = ?4");
	write.bind(1, row.body.dump()).bind(2, row.updated).bind(3, collection).bind(4, row.id).run();
}

void Store::Impl::insertRow(const std::string& collection, Row& row) {
	if(rowCount(collection) >= Limits::records)
		throw DataError("limit", std::format("{}: at most {} records", collection, Limits::records));
	sql::Statement write(db, "INSERT INTO records(seq, collection, id, created, updated, body) VALUES(?1, ?2, ?3, ?4, "
	                         "?5, ?6)");
	if(row.sequence > 0) write.bind(1, row.sequence);
	else write.bindNull(1);
	write.bind(2, collection).bind(3, row.id).bind(4, row.created).bind(5, row.updated).bind(6, row.body.dump()).run();
	row.sequence = db.lastRow();
}

void Store::Impl::deleteRow(const std::string& collection, const std::string& id) {
	sql::Statement write(db, "DELETE FROM records WHERE collection = ?1 AND id = ?2");
	write.bind(1, collection).bind(2, id).run();
}

std::size_t Store::Impl::rowCount(const std::string& collection) {
	sql::Statement read(db, "SELECT COUNT(*) FROM records WHERE collection = ?1");
	read.bind(1, collection);
	return read.step() ? static_cast<std::size_t>(read.integer(0)) : 0;
}

Record Store::Impl::recordOf(const Model& model, const Row& row) {
	Record record{row.id, row.created, row.updated, json::object()};
	for(const std::string& name : model.order)
		if(const auto it = row.body.find(name); it != row.body.end()) record.fields[name] = *it;
	return record;
}

void Store::Impl::checkLinks(const Model& model, const json& fields, const std::string& id, const std::string& where) {
	for(const auto& [name, field] : model.fields) {
		const auto value = fields.find(name);
		if(value == fields.end()) continue;
		const Field* target = &field;
		const bool list = field.type == FieldType::List;
		if(list) target = field.of.get();
		if(target->type == FieldType::Reference) {
			const json ids = list ? *value : json::array({*value});
			for(const json& referred : ids)
				if(!row(target->to, referred.get<std::string>()))
					throw DataError("reference", std::format("{}: no record '{}' in '{}'", placeOf(where, name),
					                                         referred.get<std::string>(), target->to));
		}
		if(field.unique) {
			sql::Statement same(db, "SELECT id FROM records WHERE collection = ?1 AND json_extract(body, ?2) = ?3 AND "
			                        "id != ?4 LIMIT 1");
			same.bind(1, model.name).bind(2, "$." + name).bindValue(3, *value).bind(4, id);
			if(same.step())
				throw DataError("unique", std::format("{}: '{}' already has {}", placeOf(where, name), same.text(0),
				                                      value->dump()));
		}
	}
}

std::string Store::Impl::nextId(const std::string& collection) {
	sql::Statement read(db, "SELECT nextId FROM collections WHERE name = ?1");
	read.bind(1, collection);
	std::int64_t next = read.step() ? read.integer(0) : 1;
	read.reset();
	while(row(collection, std::to_string(next))) ++next;
	sql::Statement write(db, "UPDATE collections SET nextId = ?1 WHERE name = ?2");
	write.bind(1, next + 1).bind(2, collection).run();
	return std::to_string(next);
}

/* Store */

Store::Store(std::filesystem::path file) : impl_(std::make_unique<Impl>(std::move(file))) {}

Store::~Store() = default;

const std::filesystem::path& Store::file() const noexcept { return impl_->file; }

void Store::setEvaluator(Evaluator evaluate) { impl_->evaluate = std::move(evaluate); }

void Store::setClock(std::function<std::int64_t()> clock) { impl_->clock = clock ? std::move(clock) : systemNow; }

const Models& Store::models() const noexcept { return impl_->models; }

std::uint64_t Store::changes() const noexcept { return impl_->changes; }

void Store::notify(const std::string& collection) {
	++impl_->changes;
	onChange.emit(collection);
}

bool Store::differs(const json& section) const {
	return (section.is_null() ? json::object() : section) != impl_->section;
}

Record Store::insert(const std::string& collection, const json& fields, const std::string& author) {
	Impl& impl = *impl_;
	const std::string where = collection + "[new]";
	const Model& model = impl.modelOf(collection, where);
	if(!fields.is_object()) throw DataError("invalid", where + ": expected a record: an object of fields");
	json declared = validateRecord(model, fields, where);
	sql::Transaction transaction(impl.db);
	std::string id;
	if(const auto given = fields.find("id"); given != fields.end() && !given->is_null()) {
		if(!given->is_string() || !isRecordId(given->get<std::string>()))
			throw DataError("invalid", where + ".id: expected letters, digits, - and _ (at most 64)");
		id = given->get<std::string>();
		if(impl.row(collection, id)) throw DataError("unique", collection + ": a record '" + id + "' is there already");
	} else {
		id = impl.nextId(collection);
	}
	impl.checkLinks(model, declared, id, collection + "[" + id + "]");
	const std::int64_t now = impl.clock();
	Row row{0, id, now, now, std::move(declared)};
	impl.insertRow(collection, row);
	impl.journal(author, "insert", collection, id, row.sequence, nullptr, rowJson(row));
	transaction.commit();
	notify(collection);
	return Impl::recordOf(model, row);
}

Record Store::update(const std::string& collection, const std::string& id, const json& changes,
                     const std::string& author) {
	Impl& impl = *impl_;
	const std::string where = collection + "[" + id + "]";
	const Model& model = impl.modelOf(collection, where);
	if(!changes.is_object()) throw DataError("invalid", where + ": expected the fields to change");
	std::optional<Row> row = impl.row(collection, id);
	if(!row) notFound(collection, id);
	const json before = rowJson(*row);
	json merged = Impl::recordOf(model, *row).fields;
	for(const auto& [name, value] : changes.items()) {
		if(isSystemField(name)) throw DataError("invalid", std::format("{}: the store gives it", placeOf(where, name)));
		if(value.is_null()) merged.erase(name);
		else merged[name] = value;
	}
	const json declared = validateRecord(model, merged, where);
	sql::Transaction transaction(impl.db);
	impl.checkLinks(model, declared, id, where);
	for(const std::string& name : model.order) row->body.erase(name); // the undeclared ones stay, unread
	for(const auto& [name, value] : declared.items()) row->body[name] = value;
	row->updated = impl.clock();
	impl.writeBody(collection, *row);
	impl.journal(author, "update", collection, id, row->sequence, before, rowJson(*row));
	transaction.commit();
	notify(collection);
	return Impl::recordOf(model, *row);
}

void Store::remove(const std::string& collection, const std::string& id, const std::string& author) {
	Impl& impl = *impl_;
	const std::string where = collection + "[" + id + "]";
	(void)impl.modelOf(collection, where);
	const std::optional<Row> row = impl.row(collection, id);
	if(!row) notFound(collection, id);
	sql::Transaction transaction(impl.db);
	std::set<std::string> touched = {collection};
	for(const auto& [name, model] : impl.models) // what refers to it lets go of it, or refuses
		for(const auto& [fieldName, field] : model.fields)
			if(releaseReference(impl, {name, fieldName, field}, collection, id, author, where)) touched.insert(name);
	impl.deleteRow(collection, id);
	impl.journal(author, "delete", collection, id, row->sequence, rowJson(*row), nullptr);
	transaction.commit();
	for(const std::string& name : touched) notify(name);
}

std::optional<Record> Store::get(const std::string& collection, const std::string& id) const {
	const auto model = impl_->models.find(collection);
	if(model == impl_->models.end()) return std::nullopt;
	const std::optional<Row> row = impl_->row(collection, id);
	if(!row) return std::nullopt;
	return Impl::recordOf(model->second, *row);
}

std::vector<Record> Store::query(const Query& query) const {
	const Model& model = impl_->modelOf(query.from, "query");
	json parameters = json::array();
	std::string text = "SELECT seq, id, created, updated, body FROM records WHERE collection = ?1 AND " +
	                   filterSql(query.where, model, parameters) + " ORDER BY ";
	for(const SortKey& key : query.sort)
		text += fieldSql(key.field, parameters) + (key.descending ? " DESC, " : " ASC, ");
	parameters.push_back(static_cast<std::int64_t>(std::min(query.limit, Limits::queryResults)));
	parameters.push_back(static_cast<std::int64_t>(std::min<std::size_t>(query.offset, 1u << 30)));
	text += std::format("seq LIMIT ?{} OFFSET ?{}", parameters.size(), parameters.size() + 1);
	sql::Statement read(impl_->db, text);
	bindAll(read, query.from, parameters);
	std::vector<Record> records;
	while(read.step())
		records.push_back(Impl::recordOf(
		    model, {read.integer(0), read.text(1), read.integer(2), read.integer(3), stored(read.text(4))}));
	return records;
}

std::size_t Store::count(const Query& query) const {
	const Model& model = impl_->modelOf(query.from, "query");
	json parameters = json::array();
	const std::string text =
	    "SELECT COUNT(*) FROM records WHERE collection = ?1 AND " + filterSql(query.where, model, parameters);
	sql::Statement read(impl_->db, text);
	bindAll(read, query.from, parameters);
	return read.step() ? static_cast<std::size_t>(read.integer(0)) : 0;
}

json Store::run(const json& query, const std::string& where) const {
	const Query parsed = parseQuery(query, impl_->models, where);
	if(parsed.count) return count(parsed);
	json records = json::array();
	for(const Record& record : this->query(parsed)) records.push_back(record.toJson());
	return records;
}

std::size_t Store::size(const std::string& collection) const { return impl_->rowCount(collection); }

json Store::outline() const {
	json outline = json::object();
	for(const auto& [name, model] : impl_->models) {
		json fields = json::object();
		for(const std::string& field : model.order) fields[field] = model.fields.at(field).written;
		outline[name] = {{"version", model.version}, {"count", impl_->rowCount(name)}, {"fields", fields}};
	}
	return outline;
}

/* LiveQuery */

LiveQuery::LiveQuery(Store& store, json query) : store_(store), query_(std::move(query)) {
	results_ = store_.run(query_, "query");
	from_ = query_["from"].get<std::string>();
	connection_ = store_.onChange.connect([this](const std::string& collection) {
		if(collection != from_) return;
		json now;
		try {
			now = store_.run(query_, "query");
		} catch(const DataError&) {
			now = json::array(); // its collection is gone from the models
		}
		if(now == results_) return;
		results_ = std::move(now);
		onResults.emit(results_);
	});
}

LiveQuery::~LiveQuery() { store_.onChange.disconnect(connection_); }

/* Reports */

bool FollowReport::ok() const noexcept {
	return std::ranges::all_of(collections, [](const CollectionReport& c) { return c.failed == 0; });
}

std::size_t FollowReport::changed() const noexcept {
	std::size_t total = 0;
	for(const CollectionReport& c : collections) total += c.changed;
	return total;
}

std::string FollowReport::text() const {
	std::string text;
	for(const CollectionReport& c : collections) {
		if(c.action == "none") continue;
		std::string line = c.collection + ": ";
		if(c.action == "create") line += std::format("a new collection with {} records", c.seeded);
		else if(c.action == "check") line += std::format("{} records checked against the new model", c.records);
		else
			line += std::format("{} of {} records {} ({} to {})", c.changed, c.records,
			                    c.action == "revert" ? "revert" : "migrate", c.from, c.to);
		if(c.archived) line += std::format(", {} archived", c.archived);
		if(c.restoredArchive) line += ", the archive restored";
		if(c.failed) {
			line += std::format(", {} fail", c.failed);
			for(std::size_t i = 0; i < c.failures.size() && i < 3; ++i) line += (i ? "; " : ": ") + c.failures[i];
		}
		text += (text.empty() ? "" : "; ") + line;
	}
	return text;
}

json FollowReport::toJson() const {
	json list = json::array();
	for(const CollectionReport& c : collections)
		list.push_back({{"collection", c.collection},
		                {"action", c.action},
		                {"from", c.from},
		                {"to", c.to},
		                {"records", c.records},
		                {"changed", c.changed},
		                {"failed", c.failed},
		                {"archived", c.archived},
		                {"seeded", c.seeded},
		                {"destructive", c.destructive},
		                {"failures", c.failures}});
	return list;
}

} // namespace affresco::data
