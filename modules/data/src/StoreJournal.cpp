/// @file
/// @brief data::Store: the journal of writes, undo and redo, and the archive (Store.hpp).

#include "StoreImpl.hpp"

#include <format>

namespace affresco::data {

namespace {

/// @brief Parses a column holding JSON, or null.
/// @param statement The statement on a row.
/// @param column The column.
/// @return The value.
json jsonColumn(const sql::Statement& statement, int column) {
	return statement.isNull(column) ? json() : json::parse(statement.text(column));
}

/// @brief One journal entry of the current row of a statement over the journal.
/// @param read The statement: seq, grp, time, author, action, collection, id, before, after, state.
/// @return The entry.
JournalEntry entryOf(const sql::Statement& read) {
	return {read.integer(0), read.integer(1), read.integer(2),     read.text(3),        read.text(4),
	        read.text(5),    read.text(6),    jsonColumn(read, 7), jsonColumn(read, 8), read.integer(9) == 1};
}

/// @brief The entries of a group, with their record's sequence and model version.
struct GroupEntry {
	JournalEntry entry;        ///< The entry.
	std::int64_t sequence = 0; ///< The record's sequence.
	int version = 0;           ///< The model version it was written at.
};

/// @brief Reads a group's entries, in journal order.
/// @param db The connection.
/// @param group The group.
/// @return They.
std::vector<GroupEntry> groupEntries(sql::Database& db, std::int64_t group) {
	sql::Statement read(db, "SELECT seq, grp, time, author, action, collection, id, before, after, state, recordSeq, "
	                        "version FROM journal WHERE grp = ?1 ORDER BY seq");
	read.bind(1, group);
	std::vector<GroupEntry> entries;
	while(read.step()) entries.push_back({entryOf(read), read.integer(10), static_cast<int>(read.integer(11))});
	return entries;
}

/// @brief The group undo would undo, or redo redo.
/// @param db The connection.
/// @param undo Undo (the newest done group) or redo (the oldest undone one).
/// @return It, or std::nullopt.
std::optional<std::int64_t> nextGroup(sql::Database& db, bool undo) {
	sql::Statement read(db, undo ? "SELECT MAX(grp) FROM journal WHERE state = 0"
	                             : "SELECT MIN(grp) FROM journal WHERE state = 1");
	if(!read.step() || read.isNull(0)) return std::nullopt;
	return read.integer(0);
}

} // namespace

void Store::Impl::journal(const std::string& author, const char* action, const std::string& collection,
                          const std::string& id, std::int64_t sequence, const json& before, const json& after) {
	std::int64_t entryGroup = batches > 0 ? group : 0;
	if(entryGroup == 0) { // a group of its own, or the batch's first write
		sql::Statement last(db, "SELECT COALESCE(MAX(grp), 0) + 1 FROM journal");
		entryGroup = last.step() ? last.integer(0) : 1;
		last.reset();
		if(batches > 0) group = entryGroup;
		db.run("UPDATE journal SET state = 2 WHERE state = 1"); // a new write: what redo held is gone
	}
	sql::Statement write(db, "INSERT INTO journal(grp, time, author, action, collection, id, recordSeq, version, "
	                         "before, after) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");
	const auto model = models.find(collection);
	write.bind(1, entryGroup).bind(2, clock()).bind(3, author).bind(4, std::string_view(action)).bind(5, collection);
	write.bind(6, id).bind(7, sequence).bind(8, std::int64_t{model == models.end() ? 0 : model->second.version});
	if(before.is_null()) write.bindNull(9);
	else write.bind(9, before.dump());
	if(after.is_null()) write.bindNull(10);
	else write.bind(10, after.dump());
	write.run();
	// Bounded: the oldest entries go first
	sql::Statement trim(db, "DELETE FROM journal WHERE seq <= (SELECT MAX(seq) FROM journal) - ?1");
	trim.bind(1, static_cast<std::int64_t>(Limits::journalEntries)).run();
}

Store::Batch::Batch(Store& store) : store_(store), outermost_(store.impl_->batches == 0) {
	++store_.impl_->batches;
	if(outermost_) store_.impl_->group = 0; // the first write takes a new group
}

Store::Batch::~Batch() {
	if(--store_.impl_->batches == 0) store_.impl_->group = 0;
}

bool Store::canUndo() const { return nextGroup(impl_->db, true).has_value(); }

bool Store::canRedo() const { return nextGroup(impl_->db, false).has_value(); }

bool Store::undo() {
	Impl& impl = *impl_;
	const std::optional<std::int64_t> group = nextGroup(impl.db, true);
	if(!group) return false;
	std::vector<GroupEntry> entries = groupEntries(impl.db, *group);
	std::set<std::string> touched;
	sql::Transaction transaction(impl.db);
	for(auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const JournalEntry& entry = it->entry;
		const auto model = impl.models.find(entry.collection);
		if(model == impl.models.end() || model->second.version != it->version)
			throw DataError("model", entry.collection + ": its model changed since this write: it can't be undone");
		if(entry.action == "insert") impl.deleteRow(entry.collection, entry.id);
		else if(entry.action == "delete") {
			Row row = rowFrom(entry.before, it->sequence);
			impl.insertRow(entry.collection, row);
		} else {
			impl.writeBody(entry.collection, rowFrom(entry.before, it->sequence));
		}
		touched.insert(entry.collection);
	}
	sql::Statement mark(impl.db, "UPDATE journal SET state = 1 WHERE grp = ?1");
	mark.bind(1, *group).run();
	transaction.commit();
	for(const std::string& collection : touched) notify(collection);
	return true;
}

bool Store::redo() {
	Impl& impl = *impl_;
	const std::optional<std::int64_t> group = nextGroup(impl.db, false);
	if(!group) return false;
	const std::vector<GroupEntry> entries = groupEntries(impl.db, *group);
	std::set<std::string> touched;
	sql::Transaction transaction(impl.db);
	for(const GroupEntry& it : entries) {
		const JournalEntry& entry = it.entry;
		const auto model = impl.models.find(entry.collection);
		if(model == impl.models.end() || model->second.version != it.version)
			throw DataError("model", entry.collection + ": its model changed since this write: it can't be redone");
		if(entry.action == "delete") impl.deleteRow(entry.collection, entry.id);
		else if(entry.action == "insert") {
			Row row = rowFrom(entry.after, it.sequence);
			impl.insertRow(entry.collection, row);
		} else {
			impl.writeBody(entry.collection, rowFrom(entry.after, it.sequence));
		}
		touched.insert(entry.collection);
	}
	sql::Statement mark(impl.db, "UPDATE journal SET state = 0 WHERE grp = ?1");
	mark.bind(1, *group).run();
	transaction.commit();
	for(const std::string& collection : touched) notify(collection);
	return true;
}

std::vector<JournalEntry> Store::journal(std::size_t limit, const std::string& collection) const {
	sql::Statement read(impl_->db, "SELECT seq, grp, time, author, action, collection, id, before, after, state FROM "
	                               "journal WHERE (?1 = '' OR collection = ?1) ORDER BY seq DESC LIMIT ?2");
	read.bind(1, collection).bind(2, static_cast<std::int64_t>(std::min<std::size_t>(limit, Limits::journalEntries)));
	std::vector<JournalEntry> entries;
	while(read.step()) entries.push_back(entryOf(read));
	return entries;
}

std::vector<ArchivedRecord> Store::archived(const std::string& collection, std::size_t limit) const {
	sql::Statement read(impl_->db, "SELECT migration, id, created, updated, body, reason FROM archive WHERE "
	                               "collection = ?1 ORDER BY seq DESC LIMIT ?2");
	read.bind(1, collection).bind(2, static_cast<std::int64_t>(std::min<std::size_t>(limit, Limits::queryResults)));
	std::vector<ArchivedRecord> records;
	while(read.step())
		records.push_back({read.integer(0),
		                   {read.text(1), read.integer(2), read.integer(3), json::parse(read.text(4))},
		                   read.text(5)});
	return records;
}

Record Store::restoreArchived(const std::string& collection, std::int64_t migration, const std::string& id,
                              const std::string& author) {
	sql::Statement read(impl_->db, "SELECT body FROM archive WHERE collection = ?1 AND migration = ?2 AND id = ?3");
	read.bind(1, collection).bind(2, migration).bind(3, id);
	if(!read.step())
		throw DataError("notFound",
		                std::format("{}: no record '{}' archived by migration {}", collection, id, migration));
	json fields = json::parse(read.text(0));
	read.reset();
	const Model& model = impl_->modelOf(collection, collection);
	json declared = json::object();
	for(const std::string& name : model.order)
		if(fields.contains(name)) declared[name] = fields[name];
	if(!impl_->row(collection, id)) declared["id"] = id;
	return insert(collection, declared, author);
}

} // namespace affresco::data
