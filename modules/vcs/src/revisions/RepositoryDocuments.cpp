/// @file
/// @brief vcs::Repository's documents: rebuilding a commit's document from the nearest snapshot and the deltas after
/// it, blame, which follows a node or a path back through the deltas to the commit that last changed it (Trail.hpp),
/// the live state's changes a commit records for its undo, and the revision that restores a commit.

#include "affresco/vcs/Repository.hpp"

#include "revisions/Deltas.hpp"
#include "revisions/Trail.hpp"
#include "permissions/Permissions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

#include <algorithm>
#include <map>
#include <array>
#include <cctype>
#include <ranges>

namespace affresco::vcs {

using expect::fail;

namespace {

/// @brief Tells whether an operation of a revision's patch reaches a path of the live state.
/// @param op The operation, as submitted.
/// @param target The path's segments ("state", ...).
/// @return true if its path or its "from" is the path, inside it, or holds it.
bool reaches(const json& op, const std::vector<std::string>& target) {
	return std::ranges::any_of(std::array{"path", "from"}, [&](const char* key) {
		const auto at = op.find(key);
		if(at == op.end() || !at->is_string() || !at->get_ref<const std::string&>().starts_with("/state")) return false;
		const std::vector<std::string> segments = detail::segmentsOf(at->get_ref<const std::string&>());
		return detail::startsWith(segments, target) || detail::startsWith(target, segments);
	});
}

/// @brief Tells whether a revision's patch can change the live state: a path or a "from" at "/state" or inside it, or
/// the whole document ("").
/// @param patch The patch, as stored.
/// @return true if it can.
bool touchesState(const json& patch) {
	return std::ranges::any_of(patch, [](const json& op) {
		return std::ranges::any_of(std::array{"path", "from"}, [&](const char* key) {
			const auto at = op.find(key);
			if(at == op.end() || !at->is_string()) return false;
			const std::string& path = at->get_ref<const std::string&>();
			return path.empty() || path == "/state" || path.starts_with("/state/");
		});
	});
}

/// @brief The part of the live state an operation of a delta changes: its path, or the list that holds it, since
/// adding or removing an item moves the ones after it.
/// @param state The live state after the delta.
/// @param path The operation's path, in the live state.
/// @return The part's path.
std::string partOf(const json& state, const std::string& path) {
	const std::string holder = path.substr(0, path.rfind('/'));
	const json* list = path.empty() ? nullptr : detail::locate(state, holder);
	return list && list->is_array() ? holder : path;
}

/// The prefix of a feature's files among a program's (lisp::effectiveProgram(), which the history doesn't link).
constexpr std::string_view featureFiles = "features/";

/// @brief The program a document runs, as lisp::effectiveProgram() gives it: its own files, and each enabled feature's
/// program under `features/<name>/<file>`; nothing of a record of the wrong shape.
/// @param document The document.
/// @return `{file: {name: text}}`; null with no `program`.
json runningProgram(const json& document) {
	const auto own = document.find("program");
	if(own == document.end()) return nullptr;
	json program = *own;
	const auto records = document.find("features");
	if(!program.is_object() || records == document.end() || !records->is_object()) return program;
	for(const auto& [name, record] : records->items()) {
		if(!record.is_object() || record.value("enabled", json()) != true || name.contains('/')) continue;
		const auto provides = record.find("provides");
		if(provides == record.end() || !provides->is_object() || !provides->contains("program")) continue;
		for(const auto& [file, entries] : (*provides)["program"].items())
			if(entries.is_object()) {
				std::string named(featureFiles);
				named += name;
				named += '/';
				named += file;
				program[named] = entries;
			}
	}
	return program;
}

/// @brief The feature a file of a running program belongs to.
/// @param file The file.
/// @return Its name; empty for the document's own files.
std::string featureOf(std::string_view file) {
	if(!file.starts_with(featureFiles)) return {};
	const std::string_view rest = file.substr(featureFiles.size());
	const std::size_t slash = rest.find('/');
	return slash == std::string_view::npos ? std::string() : std::string(rest.substr(0, slash));
}

/// @brief What a value holds under a key, if it is an object that has it.
/// @param at The value, or nullptr.
/// @param key The key.
/// @return It, or nullptr.
const json* below(const json* at, const char* key) {
	if(!at || !at->is_object()) return nullptr;
	const auto found = at->find(key);
	return found == at->end() ? nullptr : &*found;
}

/// @brief The program of a feature's record in a document.
/// @param document The document.
/// @param feature The feature.
/// @return `{file: {name: text}}`, or nullptr.
const json* featureProgram(const json& document, const std::string& feature) {
	return below(below(below(below(&document, "features"), feature.c_str()), "provides"), "program");
}

/// @brief Goes through the definitions some files hold.
/// @tparam Each What to call: (file, name, text).
/// @param prefix What goes before each file's name.
/// @param files `{file: {name: text}}`.
/// @param each What to call.
template <class Each> void eachEntryOf(const std::string& prefix, const json& files, const Each& each) {
	for(const auto& [file, entries] : files.items())
		if(entries.is_object())
			for(const auto& [name, text] : entries.items()) each(prefix + file, name, text);
}

/// @brief Tells whether a path of the "features" section may hold a feature's program, or part of it.
/// @param path The path's segments, the first "features".
/// @return true for the section, a record, its `provides`, its program, a file and a definition of it.
bool reachesFeatureProgram(const std::vector<std::string>& path) {
	return path.size() <= 6 && (path.size() < 3 || path[2] == "provides") && (path.size() < 4 || path[3] == "program");
}

/// @brief The definitions a value written at a path of the "features" section holds: what a feature's program has.
/// @tparam Each What to call: (file, name, text), the file as runningProgram() names it.
/// @param path The path's segments, the first "features".
/// @param value What was written there.
/// @param each What to call.
template <class Each> void eachFeatureWritten(const std::vector<std::string>& path, const json& value, const Each& each) {
	if(path.size() == 1) { // the whole section
		if(value.is_object())
			for(const auto& [name, record] : value.items()) eachFeatureWritten({"features", name}, record, each);
		return;
	}
	if(!reachesFeatureProgram(path)) return;
	std::string prefix(featureFiles);
	prefix += path[1];
	prefix += '/';
	if(path.size() == 6) return each(prefix + path[4], path[5], value);
	const json* files = &value; // what the path holds, brought down to the program's files
	if(path.size() <= 2) files = below(files, "provides");
	if(path.size() <= 3) files = below(files, "program");
	if(!files || !files->is_object()) return;
	if(path.size() == 5) eachEntryOf(prefix, json{{path[4], *files}}, each);
	else eachEntryOf(prefix, *files, each);
}

/// @brief A definition's text in a document.
/// @param document The document.
/// @param file The file.
/// @param name Its address.
/// @return It, or nullptr.
const json* entryIn(const json& document, const std::string& file, const std::string& name) {
	const auto section = document.find("program");
	if(section == document.end() || !section->is_object()) return nullptr;
	if(const std::string feature = featureOf(file); !feature.empty()) { // a feature's: in its record
		const json* entries = below(featureProgram(document, feature),
		                            file.substr(featureFiles.size() + feature.size() + 1).c_str());
		const json* text = entries ? below(entries, name.c_str()) : nullptr;
		return text && text->is_string() ? text : nullptr;
	}
	const auto entries = section->find(file);
	if(entries == section->end() || !entries->is_object()) return nullptr;
	const auto text = entries->find(name);
	return text == entries->end() ? nullptr : &*text;
}

/// @brief The definitions a value written at a path of the document's own program holds.
/// @tparam Each What to call: (file, name, text).
/// @param path The path's segments: empty for the whole document, else the first "program".
/// @param value What was written there.
/// @param each What to call.
template <class Each> void eachOwnWritten(const std::vector<std::string>& path, const json& value, const Each& each) {
	if(path.size() == 3) return each(path[1], path[2], value);
	const json* files = path.empty() ? below(&value, "program") : &value;
	if(!files || !files->is_object()) return;
	if(path.size() == 2) eachEntryOf("", json{{path[1], *files}}, each);
	else eachEntryOf("", *files, each);
}

/// @brief Goes through the definitions an operation of a commit's delta writes, reaching them from any level above.
/// @tparam Each What to call: (file, name, text).
/// @param op The operation.
/// @param each What to call.
template <class Each> void eachWritten(const json& op, const Each& each) {
	const auto value = op.find("value");
	if(value == op.end()) return;
	const std::vector<std::string> path = detail::segmentsOf(op.value("path", ""));
	if(!path.empty() && path[0] == "features") return eachFeatureWritten(path, *value, each);
	if(path.empty())
		if(const json* features = below(&*value, "features")) eachFeatureWritten({"features"}, *features, each);
	if(path.empty() || (path[0] == "program" && path.size() <= 3)) eachOwnWritten(path, *value, each);
}

/// @brief The entries a person's commit is credited with, whose author is whoever first wrote that text.
class FirstWriters {
public:
	/// @brief Takes the entries to answer for.
	/// @param program The program on main's head; it must outlive this.
	/// @param credited `{file: {name: author}}`, changed where it is settled; it must outlive this.
	FirstWriters(const json& program, json& credited) : program_(program) {
		for(auto& [file, entries] : credited.items())
			for(auto& [name, author] : entries.items()) {
				const auto& who = author.get_ref<const std::string&>();
				if(who == "user" || who.starts_with("user/")) wanted_.emplace(std::pair(file, name), &author);
			}
	}
	/// @brief Tells whether every entry has its answer. @return Whether it has.
	[[nodiscard]] bool done() const noexcept { return wanted_.empty(); }
	/// @brief Takes a writing of an entry, the earliest first: if it is the text the entry holds now, that is its
	/// author.
	/// @param file The file.
	/// @param name The entry's address.
	/// @param text What was written.
	/// @param author Who wrote it.
	void settle(const std::string& file, const std::string& name, const json& text, const std::string& author) {
		const auto found = wanted_.find(std::pair(file, name));
		if(found == wanted_.end() || text != program_[file][name]) return;
		*found->second = author;
		wanted_.erase(found);
	}
	/// @brief Ends the walk: an entry with no writing it could name is the person's commit's.
	void finish() {
		for(const auto& [entry, author] : wanted_) *author = "user";
	}

private:
	const json& program_;                                         ///< See the constructor.
	std::map<std::pair<std::string, std::string>, json*> wanted_; ///< What is left to answer for.
};

/// @brief Who last wrote each definition of a document's program, credited a commit at a time, newest first.
class EntryAuthors {
public:
	/// @brief Starts with no entry credited.
	/// @param program The program the document runs (runningProgram()); it must outlive this.
	explicit EntryAuthors(const json& program) {
		if(!program.is_object()) return;
		section_ = &program;
		for(const auto& [file, entries] : section_->items())
			if(entries.is_object()) missing_ += entries.size();
	}
	/// @brief Credits what an operation's path reaches: an entry, a file or the whole program; or a feature's
	/// record, its program, a file or an entry of it.
	/// @param path The path's segments.
	/// @param author The commit's author.
	void creditPath(const std::vector<std::string>& path, const std::string& author) {
		if(path.empty()) return creditAll(author);
		if(path[0] == "features") return creditFeature(path, author);
		if(path[0] != "program") return;
		if(path.size() >= 3) credit(path[1], path[2], author);
		else if(path.size() == 2) creditFile(path[1], author);
		else creditOwn(author);
	}
	/// @brief Credits every entry not credited yet.
	/// @param author The author.
	void creditAll(const std::string& author) {
		if(!section_) return;
		for(const auto& [file, entries] : section_->items()) creditFile(file, author);
	}
	/// @brief Tells whether some entry has no author yet.
	/// @return Whether one has none.
	[[nodiscard]] bool missing() const noexcept { return missing_ > 0; }
	/// @brief The authors.
	/// @return `{file: {name: author}}`.
	[[nodiscard]] json take() { return std::move(authors_); }

private:
	/// @brief Credits one entry, if it is there and has no author yet.
	/// @param file Its file.
	/// @param name Its address.
	/// @param author The author.
	void credit(const std::string& file, const std::string& name, const std::string& author) {
		if(!section_) return;
		const auto entries = section_->find(file);
		if(entries == section_->end() || !entries->is_object() || !entries->contains(name)) return;
		json& ofFile = authors_[file];
		if(ofFile.contains(name)) return;
		ofFile[name] = author;
		--missing_;
	}
	/// @brief Credits what a path in the "features" section reaches of a feature's program.
	/// @param path The path's segments, the first "features".
	/// @param author The author.
	void creditFeature(const std::vector<std::string>& path, const std::string& author) {
		if((path.size() >= 3 && path[2] != "provides") || (path.size() >= 4 && path[3] != "program") || !section_)
			return;
		const std::string prefix = path.size() >= 2 ? std::string(featureFiles) + path[1] + "/" : std::string(featureFiles);
		if(path.size() >= 6) return credit(prefix + path[4], path[5], author);
		if(path.size() == 5) return creditFile(prefix + path[4], author);
		for(const auto& [file, entries] : section_->items())
			if(file.starts_with(prefix)) creditFile(file, author);
	}
	/// @brief Credits the entries of the document's own files that have no author yet.
	/// @param author The author.
	void creditOwn(const std::string& author) {
		if(!section_) return;
		for(const auto& [file, entries] : section_->items())
			if(featureOf(file).empty()) creditFile(file, author);
	}
	/// @brief Credits a file's entries that have no author yet.
	/// @param file The file.
	/// @param author The author.
	void creditFile(const std::string& file, const std::string& author) {
		const auto entries = section_ ? section_->find(file) : json::const_iterator();
		if(!section_ || entries == section_->end() || !entries->is_object()) return;
		for(const auto& [name, text] : entries->items()) credit(file, name, author);
	}

	const json* section_ = nullptr; ///< The program, if it has one.
	json authors_ = json::object(); ///< See take().
	std::size_t missing_ = 0;       ///< How many entries have no author.
};

/// @brief Credits a feature's entries that the app's file or the person is credited with to the feature: its code has
/// a revision's authority, whoever installed it.
/// @param credited `{file: {name: author}}`, changed.
void creditFeatures(json& credited) {
	for(auto& [file, entries] : credited.items()) {
		const std::string feature = featureOf(file);
		if(feature.empty()) continue;
		for(auto& [name, author] : entries.items()) {
			const auto& who = author.get_ref<const std::string&>();
			if(who == "file" || who == "user" || who.starts_with("user/")) author = "feature:" + feature;
		}
	}
}

} // namespace

// Only revisions that reach the live state can change it: what actions change between commits isn't a commit's
void Repository::recordStateChange(Commit& commit, const json& before, const json& after) {
	if(!touchesState(commit.patch)) return;
	const json change = detail::delta(before, after);
	if(change.empty()) return;
	commit.stateUndo = json::array();
	for(json op : detail::delta(after, before)) {
		op["path"] = "/state" + op["path"].get<std::string>();
		commit.stateUndo.push_back(std::move(op));
	}
	commit.stateAfter = json::array();
	for(const json& op : change) {
		const std::string part = partOf(after, op["path"].get<std::string>());
		if(std::ranges::any_of(commit.stateAfter, [&](const json& kept) { return kept["path"] == "/state" + part; }))
			continue;
		json kept = {{"path", "/state" + part}};
		if(const json* value = detail::locate(after, part)) kept["value"] = *value;
		commit.stateAfter.push_back(std::move(kept));
	}
}

json Repository::documentOf(const Commit& commit) const {
	if(commit.id == main_.back()) return mainDocument();
	if(commit.snapshot) return *commit.snapshot;
	return rebuild(objects_, commit);
}

json Repository::rebuild(const std::unordered_map<std::string, Commit>& objects, const Commit& commit) {
	std::vector<const Commit*> chain;
	const Commit* base = &commit;
	for(; !base->snapshot; base = &objects.at(base->parents.front())) chain.push_back(base);

	json document = *base->snapshot;
	for(const Commit* next : chain | std::views::reverse) {
		try {
			detail::applyDelta(document, next->delta, "delta");
		} catch(const json::exception& error) {
			fail("commit " + next->id,
			     "its delta doesn't apply (" + detail::reasonOf(error) + "): the history is damaged");
		}
	}
	// Deltas read from disk aren't covered by the ids: the document they give must be the one the commit names
	if(!chain.empty() && detail::sha256(document.dump()) != commit.tree)
		fail("commit " + commit.id, "its deltas don't give the document its id names: the history is damaged");
	return document;
}

json Repository::deltaOf(const Commit& commit) const {
	if(!commit.delta.is_null()) return commit.delta;
	// A commit from a history written before deltas keeps its whole document, as its parent does
	return detail::delta(documentOf(objects_.at(commit.parents.front())), documentOf(commit));
}

// The live state isn't in the documents: the commits whose patches reached the path changed it
CommitInfo Repository::blameState(const std::string& pointer) const {
	const std::vector<std::string> path = detail::segmentsOf(pointer);
	for(const std::string& id : main_ | std::views::reverse) {
		const Commit& commit = objects_.at(id);
		if(std::ranges::any_of(commit.patch, [&](const json& op) { return op.is_object() && reaches(op, path); }))
			return info(commit);
	}
	return info(objects_.at(main_.front())); // as the screen file has it
}

CommitInfo Repository::blame(std::string_view target) const {
	const std::string text(target);
	if(text.empty() || (text.front() != '#' && text.front() != '/'))
		fail("blame", R"(expected a node ("#board", "#board/text") or a path ("/colors/accent", "/state/cards"))");

	if(text == "/state" || text.starts_with("/state/")) return blameState(text);

	const json& head = mainDocument();
	const std::string pointer = resolvePath(head, text, "blame");
	if(!detail::locate(head, pointer)) fail("blame", "nothing at '" + text + "' on main's head" + hintFor(head, text));
	detail::Trail trail(head, pointer);
	for(std::size_t n = main_.size() - 1; n > 0; --n) {
		const Commit& commit = objects_.at(main_[n]);
		const json worked = commit.delta.is_null() ? deltaOf(commit) : json(); // only for a history without deltas
		if(trail.changedBy(commit.delta.is_null() ? worked : commit.delta)) return info(commit);
	}
	return info(objects_.at(main_.front())); // unchanged since the screen file
}

json Repository::restoreRevision(const json& revision, std::string_view author) const {
	const Commit& target = find(revision);
	json patch = json::diff(mainDocument(), documentOf(target));
	if(patch.empty()) fail("restore", "nothing to restore: the screen already looks like " + describe(target));
	return {{"author", std::string(author)}, {"message", "Restore " + describe(target)}, {"patch", std::move(patch)}};
}

json Repository::diff(const json& from, const json& to) const {
	return json::diff(documentOf(find(from)), documentOf(find(to)));
}

json Repository::impact(const json& from, const json& to) const {
	json builtFrom, builtTo;
	const json before = documentOf(find(from)), after = documentOf(find(to));
	return rules_.impact(judged(before, builtFrom), judged(after, builtTo));
}

// Newest first: the first commit met that changed an entry, or held it whole, is who last wrote it
json Repository::programAuthors() const {
	const json program = runningProgram(mainDocument());
	EntryAuthors authors(program);
	for(std::size_t n = main_.size() - 1; n > 0 && authors.missing(); --n) {
		const Commit& commit = objects_.at(main_[n]);
		for(const json& op : deltaOf(commit))
			authors.creditPath(detail::segmentsOf(op.value("path", "")), commit.author);
	}
	authors.creditAll("file"); // as the screen file has them
	json credited = authors.take();
	keepFirstWriters(credited);
	creditPreviewed(credited);
	creditFeatures(credited);
	return credited;
}


// A person's restore or revert brings back what someone else wrote, and it stays theirs: whoever first wrote that text
// on main. One walk over the history, however many entries it answers for
void Repository::keepFirstWriters(json& credited) const {
	const json program = runningProgram(mainDocument());
	if(!program.is_object()) return; // a static app: no definition to credit
	FirstWriters writers(program, credited);
	if(writers.done()) return;
	const json first = documentOf(objects_.at(main_.front()));
	if(first.contains("program")) {
		const json whole = {{"path", ""}, {"value", first}};
		eachWritten(whole, [&](const std::string& file, const std::string& name, const json& text) {
			writers.settle(file, name, text, "file");
		});
	}
	for(std::size_t n = 1; n < main_.size() && !writers.done(); ++n) {
		const Commit& commit = objects_.at(main_[n]);
		for(const json& op : deltaOf(commit))
			eachWritten(op, [&](const std::string& file, const std::string& name, const json& text) {
				writers.settle(file, name, text, commit.author);
			});
	}
	writers.finish();
}

// What is shown is the proposal's: nothing it changed has the authority of what it replaced
void Repository::creditPreviewed(json& credited) const {
	if(!previewing_) return;
	const Proposal* shown = findProposal(*previewing_);
	const json section = runningProgram(document_);
	if(!shown || !section.is_object()) return;
	const json& main = mainDocument();
	for(const auto& [file, entries] : section.items()) {
		if(!entries.is_object()) continue;
		for(const auto& [name, text] : entries.items())
			if(const json* before = entryIn(main, file, name); !before || *before != text)
				credited[file][name] = shown->author;
	}
}

} // namespace affresco::vcs
