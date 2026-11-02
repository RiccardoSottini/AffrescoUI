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

namespace {

/// @brief What each definition uses, with what hooks and advice add: a function runs its advice, and `run-hooks` runs
/// every hook, as nothing says which point it names.
/// @param built What a program built.
/// @return The names each uses.
std::map<std::string, std::vector<std::string>> usedWithExtensions(const lisp::BuiltProgram& built) {
	std::map<std::string, std::vector<std::string>> used = built.namesUsed;
	std::vector<std::string> hooks;
	for(const lisp::Extension& extension : built.extensions) {
		if(extension.kind == "add-hook") hooks.push_back(extension.targets.back());
		if(extension.kind == "advice" && used.contains(extension.targets.front()))
			used[extension.targets.front()].push_back(extension.targets.back());
	}
	if(hooks.empty()) return used;
	for(auto& [name, names] : used)
		if(std::ranges::find(names, "run-hooks") != names.end()) names.insert(names.end(), hooks.begin(), hooks.end());
	return used;
}

/// @brief Tells whether a document's hook points can stand in the way of a control step: a definition that reaches one
/// also runs hooks, or uses what does.
/// @param built What the program built.
/// @param control What reaches a control step.
/// @return true if so.
bool hooksMeetControl(const lisp::BuiltProgram& built, const std::set<std::string>& control) {
	std::set<std::string> running;
	for(const auto& [name, names] : built.namesUsed)
		if(std::ranges::find(names, "run-hooks") != names.end()) running.insert(name);
	for(bool grew = true; grew;) {
		grew = false;
		for(const auto& [name, names] : built.namesUsed)
			if(!running.contains(name) &&
			   std::ranges::any_of(names, [&](const std::string& word) { return running.contains(word); }))
				grew = running.insert(name).second || grew;
	}
	return std::ranges::any_of(running, [&](const std::string& name) { return control.contains(name); });
}

/// @brief The hooks and advice of a document's program, by address.
/// @param document The document.
/// @return Them; none if it can't be built, which the build reports.
std::map<std::string, lisp::Extension> extensionsOf(const json& document) {
	std::map<std::string, lisp::Extension> found;
	if(!lisp::hasProgram(document)) return found;
	try {
		for(const lisp::Extension& extension : lisp::builtProgram(document["program"])->extensions)
			found.emplace(extension.name, extension);
		// NOLINTNEXTLINE(bugprone-empty-catch): the build reports it
	} catch(const DocumentError&) {
	}
	return found;
}

} // namespace

void addExtensionReviewLines(const json& before, const json& after, std::vector<std::string>& lines) {
	const std::map<std::string, lisp::Extension> was = extensionsOf(before), is = extensionsOf(after);
	const auto review = [&](const lisp::Extension& extension, const char* verb) {
		// Advice is what its target now does, whatever it does; a hook is, where a hook point meets a control step or a
		// reviewed action
		if(extension.kind != "advice") {
			const auto built = lisp::builtProgram(after["program"]);
			const bool actions = after.contains("actions") && after["actions"].dump().contains("runHooks");
			if(!actions && !hooksMeetControl(*built, reachingControl(built->namesUsed))) return;
		}
		lines.push_back(std::string(verb) + " '" + extension.name + "': the same review as changing what it joins");
	};
	for(const auto& [name, extension] : is)
		if(!was.contains(name)) review(extension, "adds");
	for(const auto& [name, extension] : was)
		if(!is.contains(name) && lisp::hasProgram(after)) review(extension, "removes");
}

bool isRunningWord(std::string_view word) {
	return stepKind(stepOfWord(word)) != StepKind::None || detail::findFunction(word) != nullptr;
}

ProgramReach programReach(const json& document) {
	ProgramReach reach;
	if(!lisp::hasProgram(document)) return reach;
	try {
		const auto built = lisp::builtProgram(document["program"]);
		for(const auto& [name, names] : built->namesUsed) reach.defined.insert(name);
		reach.control = reachingControl(usedWithExtensions(*built));
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
	if(lisp::hasProgram(before) != lisp::hasProgram(after))
		fail("revision", lisp::hasProgram(after)
		                     ? "permission denied: this app is static, and a revision can't give it a program"
							 : "permission denied: a revision can't take the whole program from an app");
	if(!lisp::hasProgram(after) || before["program"] == after["program"]) return;
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
