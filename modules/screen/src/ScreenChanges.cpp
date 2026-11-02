/// @file
/// @brief Screen::fileChanges(): a change as a reviewer reads it, file by file (VERSION_CONTROL.md, "The changes, file
/// by file"): each file's entries before and after, printed in a notation, with what the change does besides.

#include "ScreenImpl.hpp"

#include "affresco/core/TextDiff.hpp"
#include "affresco/notation/Notation.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

namespace {

/// The longest text a file's side may print to, in bytes: past it, the file is listed without its text or counts.
constexpr std::size_t mostFileText = 4U << 20;

/// @brief What a request names: the two sides of a change.
struct Sides {
	json before;      ///< The document before it.
	json after;       ///< The document after it.
	json patch;       ///< From one to the other.
	json description; ///< Who, what and how it stands.
};

/// @brief A path's first member.
/// @param pointer The path.
/// @return Its key, as written in the pointer.
[[nodiscard]] std::string_view sectionOf(std::string_view pointer) {
	if(pointer.empty()) return pointer;
	pointer.remove_prefix(1);
	return pointer.substr(0, pointer.find('/'));
}

/// @brief The paths a patch touches, without assets, which are listed on their own.
/// @param patch The patch.
/// @return The paths, as written.
[[nodiscard]] std::set<std::string> touchedPaths(const json& patch) {
	std::set<std::string> paths;
	for(const json& operation : patch) {
		for(const char* key : {"path", "from"})
			if(operation.contains(key) && operation[key].is_string()) {
				std::string path = operation[key].get<std::string>();
				if(sectionOf(path) != "assets" && sectionOf(path) != "program") paths.insert(std::move(path));
			}
	}
	return paths;
}

/// @brief A file's part of a document, in the document's order. The live state is left out but for what the change
/// touches: it is the running app's and not what the file says, and it can be large.
/// @param value The value.
/// @param pointer Its path.
/// @param file The file.
/// @param sources Who holds what.
/// @param touched The paths the change touches.
/// @param out Where the file's members go.
void project(const json& value, const std::string& pointer, const std::string& file, const detail::SourceMap& sources,
             const std::set<std::string>& touched, json& out) {
	for(const auto& [key, member] : value.items()) {
		const std::string path = detail::SourceMap::child(pointer, key);
		if(pointer.empty() && (key == "assets" || key == "program")) continue; // listed on their own
		if(sectionOf(path) == "state") {
			const auto near = [&path](const std::string& t) {
				return t == path || t.starts_with(path + '/') || path.starts_with(t + '/');
			};
			if(std::ranges::none_of(touched, near)) continue;
		}
		if(member.is_object() && (sources.hasEntriesBelow(path) || sectionOf(path) == "state")) {
			json part = json::object();
			project(member, path, file, sources, touched, part);
			if(!part.empty()) out[key] = std::move(part);
		} else if(sources.ownerOf(path) == file) {
			out[key] = member;
		}
	}
}

/// @brief Splits a text into lines.
/// @param text The text.
/// @return Its lines, views into it.
[[nodiscard]] std::vector<std::string_view> linesOf(std::string_view text) {
	std::vector<std::string_view> lines;
	while(!text.empty()) {
		const std::size_t end = text.find('\n');
		lines.push_back(text.substr(0, end));
		text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
	}
	return lines;
}

/// @brief Prints a value as the file would be read.
/// @param value The value.
/// @param lisp Whether in the Lisp notation.
/// @return The text; nothing for an empty side.
[[nodiscard]] std::string printed(const json& value, bool lisp) {
	if(value.empty()) return {};
	std::string text = lisp ? notation::write(value, notation::Notation::Lisp) : value.dump(1, '\t');
	while(text.ends_with('\n')) text.pop_back(); // a last line break would show as an empty last line
	return text;
}

/// @brief What a request names, with the documents on both sides.
/// @param versions The version control.
/// @param committed main's head as a document, its live state in it.
/// @param request `proposal`, or `from` and `to`.
/// @return The sides.
/// @throws DocumentError If there is no such proposal or commit.
[[nodiscard]] Sides sidesOf(const VersionControl& versions, const json& committed, const json& request) {
	Sides sides;
	if(request.contains("proposal")) {
		const int id = expect::integer(request["proposal"], "proposal");
		const std::vector<ProposalInfo> open = versions.proposals();
		const auto found = std::ranges::find(open, id, &ProposalInfo::id);
		if(found == open.end()) expect::fail("proposal", "there is no open proposal " + std::to_string(id));
		sides.patch = versions.proposalDiff(id);
		sides.before = committed;
		sides.after = committed.patch(sides.patch);
		sides.description = {{"kind", "proposal"},      {"proposal", id},
		                     {"author", found->author}, {"message", found->message},
		                     {"head", found->head},     {"reason", found->reason},
		                     {"policy", found->policy}, {"behaviour", found->behaviour},
		                     {"person", found->person}, {"conflicts", found->conflicts},
		                     {"assets", found->assets}, {"run", found->run},
		                     {"impact", found->impact}};
		return sides;
	}
	const json to = request.value("to", json("HEAD"));
	const CommitInfo newer = versions.resolve(to);
	// The commit's own parent when only `to` is named, as `git show` compares it
	const json from = request.contains("from") ? request["from"] : json(newer.number > 0 ? newer.number - 1 : 0);
	sides.patch = versions.diff(from, to);
	sides.before = committed.patch(versions.diff("HEAD", from));
	sides.after = committed.patch(versions.diff("HEAD", to));
	sides.description = {{"kind", "commit"},
	                     {"commit", newer.id},
	                     {"revision", newer.number},
	                     {"author", newer.author},
	                     {"message", newer.message},
	                     {"head", newer.id},
	                     {"from", from},
	                     {"to", to},
	                     {"impact", versions.impact(from, to)}};
	return sides;
}

/// @brief The assets a patch brings or removes, for a commit (a proposal's say so themselves).
/// @param patch The patch.
/// @return `[{"name", "removed"}]`.
[[nodiscard]] json assetsOfPatch(const json& patch) {
	json assets = json::array();
	for(const json& operation : patch) {
		const std::string path = operation.value("path", std::string());
		if(sectionOf(path) != "assets" || path.size() <= std::string_view("/assets/").size()) continue;
		json asset = {{"name", path.substr(std::string_view("/assets/").size())}};
		if(operation.value("op", std::string()) == "remove") asset["removed"] = true;
		assets.push_back(std::move(asset));
	}
	return assets;
}

/// @brief One file's row: its counts and, when asked for, its texts.
/// @param shown The name to show.
/// @param original The file's text before the change.
/// @param modified Its text after it.
/// @param withText Whether to give the texts.
/// @return `{"file", "added", "removed", "before"?, "after"?}`, or with `"tooLarge": true`.
[[nodiscard]] json textRow(const std::string& shown, const std::string& original, const std::string& modified,
                           bool withText) {
	json row = {{"file", shown}};
	if(original.size() > mostFileText || modified.size() > mostFileText) {
		row["tooLarge"] = true;
		return row;
	}
	const std::vector<std::string_view> oldLines = linesOf(original);
	const std::vector<std::string_view> newLines = linesOf(modified);
	std::size_t added = 0;
	std::size_t removed = 0;
	for(const DiffEdit& edit : diffLines(oldLines, newLines)) {
		if(edit.kind == DiffEdit::Kind::Insert) added += edit.count;
		else if(edit.kind == DiffEdit::Kind::Delete) removed += edit.count;
	}
	row["added"] = added;
	row["removed"] = removed;
	if(withText) {
		row["before"] = original;
		row["after"] = modified;
	}
	return row;
}

/// @brief A program's file as a reviewer reads it: its definitions' texts, comments and all, one after another.
/// @param document The document.
/// @param file The file.
/// @return The text; nothing if the document has no such file.
[[nodiscard]] std::string programText(const json& document, const std::string& file) {
	std::string text;
	const auto section = document.find("program");
	if(section == document.end() || !section->contains(file)) return text;
	for(const auto& [name, entry] : (*section)[file].items()) {
		if(!text.empty()) text += "\n\n";
		text += entry.get<std::string>();
	}
	return text;
}

/// @brief The rows of the program's files a change touches.
/// @param sides The two sides.
/// @param wanted The file to give the text of.
/// @param files Gets them.
void addProgramRows(const Sides& sides, const std::string& wanted, json& files) {
	std::set<std::string> names;
	for(const json* side : {&sides.before, &sides.after})
		if(const auto section = side->find("program"); section != side->end())
			for(const auto& [file, entries] : section->items()) names.insert(file);
	for(const std::string& file : names) {
		const std::string before = programText(sides.before, file), after = programText(sides.after, file);
		if(before != after) files.push_back(textRow(file, before, after, file == wanted));
	}
}

/// @brief What a side of a change builds, as a reviewer reads it: the tree without the source or the live state.
/// @param rules The rules.
/// @param side The side, its live state in it.
/// @param lisp Whether in the Lisp notation.
/// @return The text; nothing if it holds no program.
[[nodiscard]] std::string builtText(const DocumentRules& rules, const json& side, bool lisp) {
	json source = side;
	source.erase("state");
	std::optional<json> tree = rules.built(source);
	if(!tree) return {};
	tree->erase("program");
	return printed(*tree, lisp);
}

/// @brief The name a reviewer knows a file by: its path from the screen's folder, without the climb to a shared folder.
/// @param file The file's absolute path, or empty for the screen itself.
/// @param entry The screen's own name.
/// @param base The screen's folder.
/// @return The name.
[[nodiscard]] std::string shownName(const std::string& file, const std::string& entry,
                                    const std::filesystem::path& base) {
	if(file.empty()) return entry;
	std::string name = std::filesystem::path(file).lexically_relative(base).generic_string();
	while(name.starts_with("../")) name.erase(0, 3);
	return name.empty() ? file : name;
}

} // namespace

json Screen::fileChanges(const json& request) const { return impl_->fileChanges(request); }

json Screen::Impl::fileChanges(const json& request) const {
	const VersionControl& versions = repository;
	const Sides sides = sidesOf(versions, repository.committedView(), request);
	const std::set<std::string> touched = touchedPaths(sides.patch);
	const std::string& entry = entryFile;
	const bool lisp =
	    request.value("notation", notation::notationOf(entry) == notation::Notation::Lisp ? "lisp" : "json") == "lisp";
	std::set<std::string> owners;
	for(const std::string& path : touched) owners.insert(sources.ownerOf(path));

	const std::string wanted = request.value("file", std::string());
	json files = json::array();
	for(const std::string& owner : owners) {
		const std::string shown = shownName(owner, entry, screenFolder);
		json before = json::object();
		json after = json::object();
		project(sides.before, "", owner, sources, touched, before);
		project(sides.after, "", owner, sources, touched, after);
		files.push_back(textRow(shown, printed(before, lisp), printed(after, lisp), shown == wanted));
	}
	addProgramRows(sides, wanted, files);
	// Beside the definitions, what they build: it is what the running app shows
	if(const std::string before = builtText(rules, sides.before, lisp), after = builtText(rules, sides.after, lisp);
	   before != after) {
		json row = textRow("what it builds", before, after, wanted == "what it builds");
		row["built"] = true;
		files.push_back(std::move(row));
	}
	json answer = sides.description;
	answer["files"] = std::move(files);
	answer["notation"] = lisp ? "lisp" : "json";
	if(!answer.contains("assets")) answer["assets"] = assetsOfPatch(sides.patch);
	return answer;
}

} // namespace affresco
