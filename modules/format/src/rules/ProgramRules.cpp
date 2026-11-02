/// @file
/// @brief format::Rules on programs (RFC 0005, VERSION_CONTROL.md, "Programs"): the "program" section, what a document
/// builds, the edit of one keyword inside a definition, and what a change of definitions does beyond what it builds.

#include "ProgramRules.hpp"

#include "ExpressionFunctions.hpp"
#include "Programs.hpp"

#include "affresco/format/Rules.hpp"

#include <affresco/lisp/Document.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace affresco::format {

using expect::fail;

namespace {

/// @brief A JSON Pointer's segments, unescaped.
/// @param pointer The pointer.
/// @return Them; each with where it starts in the pointer.
std::vector<std::pair<std::string, std::size_t>> segmentsOf(const std::string& pointer) {
	std::vector<std::pair<std::string, std::size_t>> segments;
	std::size_t start = 1;
	while(start <= pointer.size()) {
		const std::size_t slash = std::min(pointer.find('/', start), pointer.size());
		std::string segment = pointer.substr(start, slash - start);
		for(std::size_t at = segment.find('~'); at != std::string::npos; at = segment.find('~', at + 1))
			if(at + 1 < segment.size() && (segment[at + 1] == '0' || segment[at + 1] == '1'))
				segment.replace(at, 2, segment[at + 1] == '1' ? "/" : "~");
		segments.emplace_back(std::move(segment), start - 1);
		start = slash + 1;
	}
	return segments;
}

/// @brief The definition a document's program has at an address.
/// @param document The document.
/// @param file The file.
/// @param name The definition's address.
/// @return Its text, or nullptr.
const json* entryAt(const json& document, const std::string& file, const std::string& name) {
	const auto section = document.find("program");
	if(section == document.end() || !section->is_object()) return nullptr;
	const auto entries = section->find(file);
	if(entries == section->end() || !entries->is_object()) return nullptr;
	const auto text = entries->find(name);
	return text == entries->end() || !text->is_string() ? nullptr : &*text;
}

/// @brief The kinds of definition that do something beyond what they build: a function a handler may call one day, what
/// a macro writes, a value they read, a hook.
/// @param kind The form's head.
/// @return Whether it is one.
bool isBehaviour(const std::string& kind) { return kind != "defscreen" && kind != "defkey" && kind != "defcustom"; }

/// @brief What a change does to one view's definitions.
struct DefinitionChange {
	std::string verb; ///< "adds", "changes" or "removes".
	std::string name; ///< The definition's address.
	std::string file; ///< Its file.
	std::string kind; ///< Its head.
};

/// @brief A definition's head, read from its text.
/// @param text The text, which has been built.
/// @return The head, or "" if it can't be read.
std::string kindOf(const std::string& text) {
	try {
		const std::vector<lisp::Entry> entries = lisp::splitEntries(text, "");
		return entries.empty() ? "" : entries.front().kind;
	} catch(const DocumentError&) {
		return "";
	}
}

/// @brief The definitions a change adds, changes or removes.
/// @param before The view before it.
/// @param after The view after it.
/// @return Them, by file and name.
std::vector<DefinitionChange> definitionChanges(const json& before, const json& after) {
	std::vector<DefinitionChange> changes;
	const json none = json::object();
	const json& was = before.contains("program") ? before["program"] : none;
	const json& is = after.contains("program") ? after["program"] : none;
	if(was == is) return changes;
	for(const auto& [file, entries] : is.items()) {
		if(!entries.is_object()) continue;
		for(const auto& [name, text] : entries.items()) {
			const json* old = entryAt(before, file, name);
			if(!old) changes.push_back({"adds", name, file, kindOf(text.get<std::string>())});
			else if(*old != text) changes.push_back({"changes", name, file, kindOf(text.get<std::string>())});
		}
	}
	for(const auto& [file, entries] : was.items()) {
		if(!entries.is_object()) continue;
		for(const auto& [name, text] : entries.items())
			if(!entryAt(after, file, name)) changes.push_back({"removes", name, file, kindOf(text.get<std::string>())});
	}
	return changes;
}

/// @brief Marks the definitions from which a control or a review step can be reached.
/// @param used What each uses by name.
/// @return Those definitions.
std::set<std::string> reachingControl(const std::map<std::string, std::vector<std::string>>& used) {
	std::map<std::string, std::vector<std::string>> usedBy; // who uses each definition, to follow it back
	std::vector<std::string> waiting;
	std::set<std::string> control;
	for(const auto& [name, names] : used)
		for(const std::string& word : names) {
			if(used.contains(word)) {
				usedBy[word].push_back(name);
				continue;
			}
			// `run` takes a computed name, so it may reach anything: as a computed "run" does in a handler
			const StepKind kind = stepKind(stepOfWord(word));
			const bool reaches = kind == StepKind::Control || kind == StepKind::Review || word == "run";
			if(reaches && control.insert(name).second) waiting.push_back(name);
		}
	while(!waiting.empty()) {
		const std::string name = std::move(waiting.back());
		waiting.pop_back();
		for(const std::string& caller : usedBy[name])
			if(control.insert(caller).second) waiting.push_back(caller);
	}
	return control;
}

} // namespace

bool isRunningWord(std::string_view word) {
	return stepKind(stepOfWord(word)) != StepKind::None || detail::findFunction(word) != nullptr;
}

ProgramReach programReach(const json& document) {
	ProgramReach reach;
	if(!lisp::hasProgram(document)) return reach;
	try {
		const auto built = lisp::builtProgram(document["program"]);
		for(const auto& [name, names] : built->namesUsed) reach.defined.insert(name);
		reach.control = reachingControl(built->namesUsed);
		// NOLINTNEXTLINE(bugprone-empty-catch): the build reports it; nothing counts as the program's meanwhile
	} catch(const DocumentError&) {
	}
	return reach;
}

void collectProgramWrites(const json& document, std::span<const std::string_view> writes,
                          std::multiset<std::string>& found) {
	if(!lisp::hasProgram(document)) return;
	try {
		const auto built = lisp::builtProgram(document["program"]);
		for(const auto& [file, entries] : document["program"].items())
			for(const auto& [name, text] : entries.items()) {
				const auto used = built->namesUsed.find(name);
				if(used == built->namesUsed.end()) continue;
				for(const std::string_view write : writes)
					if(std::ranges::find(used->second, std::string(write == "update" ? "update-record" : write)) !=
					   used->second.end())
						found.insert(json{{write, true}, {"definition", name}, {"text", text}}.dump());
			}
		// NOLINTNEXTLINE(bugprone-empty-catch): the build reports it
	} catch(const DocumentError&) {
	}
}

void checkProgramSection(const json& value, const std::string& name) {
	static_cast<void>(name);
	lisp::checkProgramShape(value);
}

void checkProgramChange(const json& before, const json& after) {
	if(!lisp::hasProgram(after) || !lisp::hasProgram(before) || before["program"] == after["program"]) return;
	std::set<std::string> defined;
	for(const auto& [file, entries] : before["program"].items())
		for(const auto& [name, text] : entries.items()) defined.insert(name);
	const auto was = lisp::builtProgram(before["program"]);
	const auto is = lisp::builtProgram(after["program"]);
	for(const lisp::UndefinedName& use : is->undefined) {
		if(!defined.contains(use.name)) continue;
		const bool already =
		    std::ranges::any_of(was->undefined, [&](const lisp::UndefinedName& old) { return old.name == use.name; });
		if(!already)
			fail("revision", "the program no longer defines \"" + use.name + "\", which \"" + use.definition +
			                     "\" uses: remove or change that too");
	}
}

void addProgramLines(const json& before, const json& after, std::vector<std::string>& lines) {
	for(const DefinitionChange& change : definitionChanges(before, after))
		if(isBehaviour(change.kind))
			lines.push_back(change.verb + " the definition '" + change.name + "' of " + change.file);
}

std::size_t programLogic(const json& before, const json& after) {
	std::size_t count = 0;
	for(const DefinitionChange& change : definitionChanges(before, after))
		if(isBehaviour(change.kind)) ++count;
	return count;
}

std::optional<json> Rules::built(const json& document) const {
	if(!lisp::hasProgram(document)) return std::nullopt;
	return lisp::buildDocument(document);
}

// "/program/<file>/<name>/:key(/:key...)" becomes "replace /program/<file>/<name>" with the whole new text
void Rules::rewriteOperation(const json& document, json& operation) const {
	const std::string pointer = operation["path"].get<std::string>();
	const auto segments = segmentsOf(pointer);
	if(segments.size() < 4 || !segments[3].first.starts_with(':')) return;
	const std::string where = "revision (" + pointer + ")";
	const auto op = operation.find("op");
	const std::string kind = op != operation.end() && op->is_string() ? op->get<std::string>() : "";
	if(kind != "set" && kind != "replace")
		fail(where, "only \"set\" and \"replace\" reach a keyword's value: add or remove a keyword by replacing "
		            "the whole definition");
	if(!operation.contains("value")) fail(where, "expected a value");
	const std::string& name = segments[2].first;
	const json* text = entryAt(document, segments[1].first, name);
	if(!text) fail(where, "no definition there: read the program's addresses with get_screen");
	std::vector<std::string> keywords;
	for(std::size_t i = 3; i < segments.size(); ++i) {
		if(!segments[i].first.starts_with(':')) fail(where, "each segment after the definition is a :keyword");
		keywords.push_back(segments[i].first.substr(1));
	}
	json replaced = {{"op", "replace"}, {"path", pointer.substr(0, segments[3].second)}};
	replaced["value"] = lisp::replaceKeyword(text->get_ref<const std::string&>(), keywords,
	                                         lisp::keywordValueText(operation["value"]), name);
	operation = std::move(replaced);
}

} // namespace affresco::format
