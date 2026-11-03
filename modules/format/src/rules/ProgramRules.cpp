/// @file
/// @brief format::Rules on programs (RFC 0005, VERSION_CONTROL.md, "Programs"): the "program" section, what a document
/// builds, the edit of one keyword inside a definition, and what a change of definitions does beyond what it builds.

#include "ProgramRules.hpp"

#include "ExpressionFunctions.hpp"
#include "Programs.hpp"

#include "affresco/format/Rules.hpp"

#include <affresco/lisp/Document.hpp>

#include <algorithm>
#include <array>
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

/// @brief The kinds of definition that only build a part of the tree, with nothing a handler can call: a section's
/// member (`defstate`, `defcomputed`, `defstyle`, `defcomponent`, `defmenu`, `deftokens`) or several (`defpart`,
/// `defdefaults`). What one does is in what it builds, judged as a JSON section is.
/// @param kind The form's head.
/// @return Whether it is one.
bool isBuiltOnly(std::string_view kind) {
	static constexpr std::array<std::string_view, 8> kinds = {"defstate", "defcomputed", "defstyle", "defcomponent",
	                                                          "defmenu",  "deftokens",   "defpart",  "defdefaults"};
	return std::ranges::find(kinds, kind) != kinds.end();
}

/// @brief The kinds of definition that do something beyond what they build: a function a handler may call one day, what
/// a macro writes, a value they read, a hook.
/// @param kind The form's head.
/// @return Whether it is one.
bool isBehaviour(const std::string& kind) {
	return kind != "defscreen" && kind != "defkey" && kind != "defcustom" && !isBuiltOnly(kind);
}

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

/// @brief What a `run` written out may name, in the order the run step looks (Steps::runNamedAction): an action of the
/// document, then a function of the program, then the application's own.
struct RunTargets {
	const json* actions = nullptr; ///< The document's actions, or nullptr.
	/// Those that may run a control step, or nullptr when that isn't known: then any may.
	const std::set<std::string>* controlActions = nullptr;
	std::map<std::string, std::vector<std::string>> advice; ///< The functions that advise each action.
};

/// @brief What a definition's words reach: the definitions they name, and whether one reaches a control or a review
/// step by itself.
class Reach {
public:
	/// @brief Sets it up.
	/// @param used What each definition uses by name.
	/// @param named What each only calls with a text written out first.
	/// @param targets What a `run` written out may name.
	Reach(const std::map<std::string, std::vector<std::string>>& used, const lisp::NamedCalls& named,
	      const RunTargets& targets)
	    : used_(used), named_(named), targets_(targets) {}

	/// @brief The definitions from which a control or a review step can be reached.
	/// @return Them.
	std::set<std::string> control() {
		for(const auto& [name, names] : used_)
			for(const std::string& word : names) note(name, word);
		while(!waiting_.empty()) {
			const std::string name = std::move(waiting_.back());
			waiting_.pop_back();
			for(const std::string& caller : usedBy_[name]) reaches(caller);
		}
		return std::move(control_);
	}

private:
	/// @brief Notes what one word of a definition reaches.
	/// @param name The definition.
	/// @param word The word.
	void note(const std::string& name, const std::string& word) {
		if(used_.contains(word)) {
			usedBy_[word].push_back(name);
			return;
		}
		const std::vector<std::string>* texts = textsOf(name, word);
		if(word == "run") {
			// A computed name may run anything, as a computed "run" in a handler; a name written out runs what it names
			if(!texts) return reaches(name);
			for(const std::string& target : *texts) follow(name, target);
			return;
		}
		const std::string_view step = stepOfWord(word);
		const StepKind kind = stepKind(step);
		if(kind != StepKind::Control && kind != StepKind::Review) return;
		const bool byUse =
		    kind == StepKind::Control && texts &&
		    std::ranges::all_of(*texts, [&](const std::string& text) { return judgedByUse(step, text); });
		if(!byUse) reaches(name);
	}

	/// @brief Notes what a `run` of a name written out reaches, as the run step finds it.
	/// @param name The definition.
	/// @param target What it runs.
	void follow(const std::string& name, const std::string& target) {
		if(target.contains("{{")) return reaches(name);
		if(targets_.actions && targets_.actions->is_object() && targets_.actions->contains(target)) {
			if(!targets_.controlActions || targets_.controlActions->contains(target)) reaches(name);
			if(const auto advice = targets_.advice.find(target); advice != targets_.advice.end())
				for(const std::string& function : advice->second) {
					if(used_.contains(function)) usedBy_[function].push_back(name);
					else reaches(name);
				}
			return;
		}
		if(used_.contains(target)) usedBy_[target].push_back(name);
		else reaches(name); // the application's own action: its code, which may do anything
	}

	/// @brief The texts a definition only calls a word with.
	/// @param name The definition.
	/// @param word The word.
	/// @return Them, or nullptr if it uses the word any other way.
	[[nodiscard]] const std::vector<std::string>* textsOf(const std::string& name, const std::string& word) const {
		const auto calls = named_.find(name);
		if(calls == named_.end() || calls->second.otherwise.contains(word)) return nullptr;
		const auto texts = calls->second.texts.find(word);
		return texts == calls->second.texts.end() ? nullptr : &texts->second;
	}

	/// @brief Marks a definition as reaching one.
	/// @param name It.
	void reaches(const std::string& name) {
		if(control_.insert(name).second) waiting_.push_back(name);
	}

	const std::map<std::string, std::vector<std::string>>& used_; ///< What each definition uses.
	const lisp::NamedCalls& named_;                               ///< What each calls with a text written out.
	const RunTargets& targets_;                                   ///< What a run written out may name.
	std::map<std::string, std::vector<std::string>> usedBy_;      ///< Who uses each definition, to follow it back.
	std::vector<std::string> waiting_;                            ///< Those found to reach one, not followed back yet.
	std::set<std::string> control_;                               ///< Those found to reach one.
};

/// @brief What a `run` written out may name in a document.
/// @param document The document.
/// @param built What its program built.
/// @param controlActions Its actions that may run a control step, or nullptr when that isn't known.
/// @return Them.
RunTargets runTargetsOf(const json& document, const lisp::BuiltProgram& built,
                        const std::set<std::string>* controlActions) {
	RunTargets targets;
	if(const auto actions = document.find("actions"); actions != document.end()) targets.actions = &*actions;
	targets.controlActions = controlActions;
	for(const lisp::Extension& extension : built.extensions)
		if(extension.kind == "advice" && !built.namesUsed.contains(extension.targets.front()))
			targets.advice[extension.targets.front()].push_back(extension.targets.back());
	return targets;
}

/// @brief Marks the definitions from which a control or a review step can be reached.
/// @param used What each uses by name.
/// @param named What each only calls with a text written out first.
/// @param targets What a `run` written out may name.
/// @return Those definitions.
std::set<std::string> reachingControl(const std::map<std::string, std::vector<std::string>>& used,
                                      const lisp::NamedCalls& named, const RunTargets& targets) {
	return Reach(used, named, targets).control();
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
			const RunTargets targets =
			    runTargetsOf(after, *built, nullptr); // any action may: the review is the safe side
			if(!actions && !hooksMeetControl(*built, reachingControl(built->namesUsed, built->namedCalls, targets)))
				return;
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

ProgramReach programReach(const json& document, const std::set<std::string>* controlActions) {
	ProgramReach reach;
	if(!lisp::hasProgram(document)) return reach;
	try {
		const auto built = lisp::builtProgram(document["program"]);
		for(const auto& [name, names] : built->namesUsed) reach.defined.insert(name);
		reach.control = reachingControl(usedWithExtensions(*built), built->namedCalls,
		                                runTargetsOf(document, *built, controlActions));
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

void collectProgramUses(const json& document, std::vector<std::string>& found) {
	if(!lisp::hasProgram(document)) return;
	try {
		const auto built = lisp::builtProgram(document["program"]);
		// What a macro writes into a definition is in its use too, as its text doesn't change with it
		std::string macros;
		for(const auto& [file, entries] : document["program"].items())
			for(const auto& [name, text] : entries.items())
				if(kindOf(text.get<std::string>()) == "defmacro") macros += "\n" + text.get<std::string>();
		for(const auto& [file, entries] : document["program"].items())
			for(const auto& [name, text] : entries.items()) {
				const auto calls = built->namedCalls.find(name);
				if(calls == built->namedCalls.end()) continue;
				for(const char* step : {"invoke", "request", "subscribe"}) {
					// Every call written out, used otherwise too or not: the check when it runs lets each through
					const auto texts = calls->second.texts.find(step);
					if(texts == calls->second.texts.end()) continue;
					// "<step> <name>", then what judges a change of its arguments: the text that gives them
					for(const std::string& method : texts->second)
						if(judgedByUse(step, method)) {
							std::string use = std::string(step) + " ";
							use.append(method).append("\n").append(text.get_ref<const std::string&>()).append(macros);
							found.push_back(std::move(use));
						}
				}
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

namespace {

/// @brief The names some definition of a program uses, or std::nullopt if it can't be built.
/// @param document The document.
/// @return Them; none without a program.
std::optional<std::set<std::string>> namesUsedIn(const json& document) {
	std::set<std::string> names;
	if(!lisp::hasProgram(document)) return names;
	try {
		for(const auto& [name, words] : lisp::builtProgram(document["program"])->namesUsed)
			for(const std::string& word : words)
				if(word != name) names.insert(word);
	} catch(const DocumentError&) {
		return std::nullopt;
	}
	return names;
}

/// @brief Tells whether a definition's text, before or after a change, is a screen's, or one that only builds a part of
/// the tree (isBuiltOnly()).
/// @param view The view.
/// @param change The change.
/// @return true if it is, or there is no text there.
bool screenOrNone(const json& view, const DefinitionChange& change) {
	const json* text = entryAt(view, change.file, change.name);
	if(!text) return true;
	const std::string kind = kindOf(text->get<std::string>());
	return kind == "defscreen" || isBuiltOnly(kind);
}

} // namespace

bool programRunsChanged(const json& before, const json& after) {
	const std::vector<DefinitionChange> changes = definitionChanges(before, after);
	if(changes.empty()) return false;
	const std::optional<std::set<std::string>> was = namesUsedIn(before), is = namesUsedIn(after);
	if(!was || !is) return true; // what can't be built can't be told apart: every definition counts
	return std::ranges::any_of(changes, [&](const DefinitionChange& change) {
		// A screen's text builds a tree and nothing else, as a function can't be in a tree: what it does is judged in
		// what it builds, unless another definition uses its name
		const bool screen = screenOrNone(before, change) && screenOrNone(after, change);
		return !screen || was->contains(change.name) || is->contains(change.name);
	});
}

// A rule on the actions, or on what "*" may make one: /actions, /actions/save, /*/save
bool Rules::changesCodeUnder(const json& before, const json& after, std::string_view pattern) const {
	const std::string_view first = pattern.substr(0, pattern.find('/', 1));
	if(first != "/actions" && first != "/*") return false;
	return programRunsChanged(before, after);
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
