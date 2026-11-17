/// @file
/// @brief Features that hold Lisp (FeaturePrograms.hpp): their names, their build, what their code reaches, and their
/// hooks and advice.

#include "FeaturePrograms.hpp"
#include "ContributionPoints.hpp"
#include "FeatureManifest.hpp"
#include "Limits.hpp"
#include "ExpressionFunctions.hpp"
#include "Programs.hpp"

#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <map>
#include <set>
#include <span>

namespace affresco::format {

namespace {

/// Longest word after a feature's prefix in a definition's address.
constexpr std::size_t maxWordLength = 64;

/// The definitions whose address is `<feature>-<word>`.
constexpr auto dashedForms = std::to_array<std::string_view>(
    {"defview", "defn", "def", "defmacro", "defstyle", "defcomponent", "defpart", "defcustom", "defhook"});

/// The definitions whose address is written as the form wants: a key's chord, an extension's targets.
constexpr auto writtenForms = std::to_array<std::string_view>({"defkey", "add-hook", "remove-hook", "advice"});

/// The definitions a feature can't hold, and why.
constexpr auto refusedForms = std::to_array<std::pair<std::string_view, std::string_view>>(
    {{"defscreen", "a feature has no window: it adds to the app's"},
	 {"defdefaults", "a feature's members are its own, never defaults for the app"},
	 {"defmenu", "a feature adds menu items with a defpart's :menus, never a menu of its own"},
	 {"deftokens", "a feature has no tokens of its own: it uses the app's"},
	 {"include", "a feature's files are all in its record"},
	 {"ns", "a feature's names are written in full, with its prefix"}});

/// The steps a feature's code may run.
constexpr auto featureSteps = std::to_array<std::string_view>(
    {"set", "toggle", "push", "remove-state", "after", "debounce", "run", "copy", "log"});

/// The steps among them that write state, whose path must be written out under `state.<feature>`.
constexpr auto writingSteps = std::to_array<std::string_view>({"set", "toggle", "push", "remove-state"});

/// The definitions whose code runs in the app: their bodies read only what they are given and the feature's own.
constexpr auto functionForms = std::to_array<std::string_view>({"defn", "defcommand"});

/// The roots of the app's values a definition may read by name.
constexpr auto valueRoots = std::to_array<std::string_view>({"app.", "state.", "computed.", "settings."});

/// @brief Tells whether a list of names holds one.
/// @param names The names.
/// @param name The name.
/// @return true if it does.
template <std::size_t N> bool among(const std::array<std::string_view, N>& names, std::string_view name) {
	return std::ranges::find(names, name) != names.end();
}

/// @brief Tells whether a text is a word after a feature's prefix: letters, digits, '_', '-' and '.', starting with a
/// letter or '_'.
/// @param word The text.
/// @return true if it is one, and at most maxWordLength long.
bool featureWord(std::string_view word) {
	const auto part = [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
		       c == '.';
	};
	return !word.empty() && word.size() <= maxWordLength && part(word.front()) && word.front() != '-' &&
	       word.front() != '.' && std::ranges::all_of(word, part);
}

/// @brief Tells whether a name is inside a feature's namespace: `<feature><separator><word>`.
/// @param name The name.
/// @param feature The feature.
/// @param separator '.' or '-'.
/// @return true if it is.
bool namespaced(std::string_view name, std::string_view feature, char separator) {
	return name.size() > feature.size() + 1 && name.starts_with(feature) && name[feature.size()] == separator &&
	       featureWord(name.substr(feature.size() + 1));
}

/// @brief Tells whether a name is inside a feature's namespace, with either separator.
/// @param name The name.
/// @param feature The feature.
/// @return true if it is `<feature>.<word>` or `<feature>-<word>`.
bool ownName(std::string_view name, std::string_view feature) {
	return namespaced(name, feature, '.') || namespaced(name, feature, '-');
}

/// @brief A feature program's place, as problems name it.
/// @param feature The feature.
/// @return "features.<feature>.provides.program".
std::string programPlace(const std::string& feature) { return std::format("features.{}.provides.program", feature); }

/// @brief The sections each definition form builds by itself: a defpart's are those it names.
/// @param kind The form.
/// @return The section, or "".
std::string_view sectionOfKind(std::string_view kind) {
	constexpr auto sections =
	    std::to_array<std::pair<std::string_view, std::string_view>>({{"defstate", "state"},
		                                                              {"defcomputed", "computed"},
		                                                              {"defcommand", "actions"},
		                                                              {"defkey", "keys"},
		                                                              {"defcustom", "settings"},
		                                                              {"defstyle", "styles"},
		                                                              {"defcomponent", "components"}});
	for(const auto& [form, section] : sections)
		if(form == kind) return section;
	return {};
}

/// @brief Why a feature can't hold a form, if it can't.
/// @param kind The form.
/// @return The reason, or "" if it may.
std::string_view refusalOf(std::string_view kind) {
	for(const auto& [form, why] : refusedForms)
		if(form == kind) return why;
	return {};
}

/// @brief A definition's name, from its address: a section's definitions are kept under "<section>.<name>", a
/// defpart's under "part.<name>" (lisp's addresses), the others under their names.
/// @param kind The definition's form.
/// @param address Its address.
/// @return The name.
std::string_view nameAt(std::string_view kind, std::string_view address) {
	std::size_t prefix = 0;
	if(const std::string_view section = sectionOfKind(kind);
	   !section.empty() && kind != "defcommand" && kind != "defkey" && kind != "defcustom")
		prefix = section.size() + 1;
	else if(kind == "defpart") prefix = std::string_view("part.").size();
	address.remove_prefix(std::min(address.size(), prefix));
	return address;
}

/// @brief The problem with a command's name, if any.
/// @param feature The feature.
/// @param name The name.
/// @return What is wrong, or "".
std::string commandNameProblem(const std::string& feature, std::string_view name) {
	if(!namespaced(name, feature, '.')) return std::format("a command's name must start with '{}.'", feature);
	return name.substr(feature.size() + 1).starts_with("adapted.") ? "\"adapted.\" names are the adapter's" : "";
}

/// @brief The problem with one definition's address, if any.
/// @param feature The feature.
/// @param kind The definition's form.
/// @param address Its address.
/// @return What is wrong, or "".
std::string addressProblem(const std::string& feature, std::string_view kind, std::string_view address) {
	const std::string_view name = nameAt(kind, address);
	if(const std::string_view refused = refusalOf(kind); !refused.empty())
		return std::format("a feature can't hold a {}: {}", kind, refused);
	if(kind == "defstate" || kind == "defcomputed")
		return name == feature ? "" : std::format("a {} of a feature is named '{}'", kind, feature);
	if(kind == "defcommand") return commandNameProblem(feature, name);
	if(among(dashedForms, kind))
		return namespaced(name, feature, '-') ? "" : std::format("a {}'s name must start with '{}-'", kind, feature);
	if(among(writtenForms, kind)) return "";
	return std::format("a feature can't hold a {}", kind);
}

/// @brief Tells whether a character ends a word of a program's text.
/// @param c The character.
/// @return true for a space, a bracket, a quote or a comment's start.
bool endsWord(char c) { return std::string_view(" \t\r\n()[]{}\"';`~@,^").contains(c); }

/// @brief The form a definition's text starts: the first word after its comments and its '('.
/// @param text The text.
/// @return The word ("defn"), or "".
std::string headWord(std::string_view text) {
	std::size_t at = 0;
	while(at < text.size()) {
		if(text[at] == ';') at = std::min(text.size(), text.find('\n', at));
		else if(text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n' || text[at] == ',') ++at;
		else break;
	}
	if(at >= text.size() || text[at] != '(') return {};
	const std::size_t start = ++at;
	while(at < text.size() && !endsWord(text[at])) ++at;
	return std::string(text.substr(start, at - start));
}

/// @brief The app's paths a definition's text names as words (`state.other.x`, `app.editor.text`): outside its
/// strings and comments, each word starting with one of the roots.
/// @param text The text.
/// @param roots The roots ("app.", ...).
/// @return Them, in order, each once.
std::vector<std::string> pathsWritten(std::string_view text, std::span<const std::string_view> roots) {
	std::vector<std::string> found;
	for(std::size_t at = 0; at < text.size();) {
		const char c = text[at];
		if(c == ';') {
			at = std::min(text.size(), text.find('\n', at));
		} else if(c == '"') {
			for(++at; at < text.size() && text[at] != '"'; ++at)
				if(text[at] == '\\') ++at;
			++at;
		} else if(endsWord(c)) {
			++at;
		} else {
			const std::size_t start = at;
			while(at < text.size() && !endsWord(text[at])) ++at;
			const std::string_view word = text.substr(start, at - start);
			if(std::ranges::any_of(roots, [&](std::string_view root) { return word.starts_with(root); }) &&
			   std::ranges::find(found, word) == found.end())
				found.emplace_back(word);
		}
	}
	return found;
}

/// @brief A build's error as a problem, at the definition it names.
/// @param feature The feature.
/// @param error The error.
/// @return "features.<feature>.provides.program.<file>.<definition>: ... (line L, column C)".
std::string buildProblem(const std::string& feature, const lisp::LispError& error) {
	std::string place = programPlace(feature);
	if(!error.file().empty()) place.append(".").append(error.file());
	if(!error.definition().empty()) place.append(".").append(error.definition());
	const std::string at =
	    error.line() == 0 ? std::string() : std::format(" (line {}, column {})", error.line(), error.column());
	return error.exhausted() ? std::format("{}: its build ran out of its budget: {}{}", place, error.message(), at)
	                         : std::format("{}: {}{}", place, error.message(), at);
}

/// @brief The definitions of a feature's program, read without building it.
/// @param feature The feature.
/// @param section Its program.
/// @param problems Receives why it can't be read.
/// @return Its entries; none if it can't be read.
std::vector<lisp::Entry> entriesOf(const std::string& feature, const json& section,
                                   std::vector<std::string>& problems) {
	try {
		std::vector<lisp::Entry> entries = lisp::programOf(section).entries();
		for(lisp::Entry& entry : entries)
			if(entry.kind.empty()) entry.kind = headWord(entry.text);
		return entries;
	} catch(const DocumentError& error) {
		problems.push_back(std::format("{}: {}", programPlace(feature), plainMessage(error)));
		return {};
	}
}

/// The steps the language says itself, which a program can't name as steps (Programs::defineSteps()).
constexpr auto unspokenSteps = std::to_array<std::string_view>({"if", "forEach", "while", "stop", "note", "lisp"});

/// The app's bare values a running program reads by name (Programs' app variables): the app's, never a feature's.
constexpr auto appVariables = std::to_array<std::string_view>(
    {"window",       "revision", "proposals",   "history",  "preview",     "mode",  "theme",
	 "locale",       "screen",   "context",     "commands", "keybindings", "menus", "settings",
	 "settingsList", "services", "connections", "auth",     "requests",    "peers", "windows"});

/// @brief Tells whether a word of a program is a step when it runs: every step is a primitive of its word, but those
/// the language says itself, and `remove` and `update`, which are the language's (their steps are `remove-state`
/// and `update-record`).
/// @param word The word.
/// @return true if it is.
bool stepWord(std::string_view word) {
	if(word == "remove" || word == "update") return false;
	const std::string_view step = stepOfWord(word);
	return !among(unspokenSteps, step) && stepKind(step) != StepKind::None;
}

/// @brief The definitions that may have built a section, for a problem to name: those whose form builds it, and the
/// defparts that name it (or every defpart, when none does by its text).
/// @param entries The feature's definitions.
/// @param section The section.
/// @return Their addresses, "a, b".
std::string buildersOf(const std::vector<lisp::Entry>& entries, std::string_view section) {
	std::vector<std::string_view> found;
	const std::string keyword = std::format(":{}", section);
	for(const lisp::Entry& entry : entries)
		if(sectionOfKind(entry.kind) == section || (entry.kind == "defstate" && section == "stateTypes") ||
		   (entry.kind == "defpart" && entry.text.contains(keyword)))
			found.emplace_back(entry.name);
	if(found.empty())
		for(const lisp::Entry& entry : entries)
			if(entry.kind == "defpart") found.emplace_back(entry.name);
	std::string text;
	for(const std::string_view name : found) text.append(text.empty() ? "" : ", ").append(name);
	return text.empty() ? std::string("the program") : text;
}

/// @brief A copy of a feature's built views without the ids a view's node repeats from its key, so that they check
/// as a JSON feature's views do.
/// @param views The built views.
/// @return Them, each node without an "id" equal to its key.
json viewsWithoutIds(const json& views) {
	json result = views;
	for(auto& [point, nodes] : result.items())
		if(nodes.is_object())
			for(auto& [id, node] : nodes.items())
				if(node.is_object() && node.value("id", json()) == json(id)) node.erase("id");
	return result;
}

/// @brief Menus as composition merges them, `{ "<menu>": { "<item>": {...} } }`: a menu's "items" read as its items.
/// @param menus The built menus (an object).
/// @return Them.
json flatMenus(const json& menus) {
	json result = json::object();
	for(const auto& [menu, items] : menus.items())
		result[menu] = items.is_object() && items.contains("items") ? items["items"] : items;
	return result;
}

/// @brief Checks a built menu's items: each an object named in the feature's namespace.
/// @param feature The feature.
/// @param items The menu's items.
/// @param at The menu's place.
/// @param problems Receives each item outside.
void checkMenuItems(const std::string& feature, const json& items, const std::string& at,
                    std::vector<std::string>& problems) {
	if(!items.is_object()) {
		problems.push_back(at + ": expected an object of items");
		return;
	}
	for(const auto& [item, entry] : items.items())
		if(!ownName(item, feature) || !entry.is_object())
			problems.push_back(std::format("{}.{}: an item must be an object named '{}.<word>'", at, item, feature));
}

/// @brief Checks one built section's members: each inside the feature's namespace, in the shape a feature gives.
/// @param feature The feature.
/// @param section The section's name.
/// @param members The section (an object).
/// @param place The section's place.
/// @param problems Receives each member outside.
void checkMembers(const std::string& feature, const std::string& section, const json& members, const std::string& place,
                  std::vector<std::string>& problems) {
	for(const auto& [member, value] : members.items()) {
		const std::string at = std::format("{}.{}", place, member);
		if(section == "state" || section == "computed") {
			if(member != feature) problems.push_back(std::format("{}: a feature's {} is '{}'", at, section, feature));
		} else if(section == "actions" || section == "exports") {
			if(!namespaced(member, feature, '.'))
				problems.push_back(std::format("{}: the name must start with '{}.'", at, feature));
		} else if(section == "menus") {
			checkMenuItems(feature, value, at, problems);
		} else if(section != "keys" && section != "views" && !ownName(member, feature)) {
			problems.push_back(std::format("{}: the name must start with '{}-' or '{}.'", at, feature, feature));
		}
	}
}

/// @brief Adds the steps written as data in what a feature's program built that its code couldn't run: the app runs
/// them later, from a key, a menu or a view, with no feature's authority to check them then.
/// @param feature The feature.
/// @param value A part, or a value inside it.
/// @param place Its place.
/// @param problems Receives each step.
/// @param depth How deep it is.
void checkDataSteps(const std::string& feature, const json& value, const std::string& place,
                    std::vector<std::string>& problems, int depth) {
	if(depth > maxNesting) return;
	if(value.is_object()) {
		for(const auto& [key, item] : value.items())
			if(stepKind(key) != StepKind::None && !featureMayRun(key, item, feature))
				problems.push_back(std::format("{}: a feature's code may not run the step '{}' so: it writes only its "
				                               "own state and runs its own commands and what the app hands it",
				                               place, key));
		for(const auto& [key, item] : value.items())
			if(item.is_structured()) checkDataSteps(feature, item, std::format("{}.{}", place, key), problems, depth + 1);
	} else if(value.is_array()) {
		for(std::size_t index = 0; index < value.size(); ++index)
			if(value[index].is_structured())
				checkDataSteps(feature, value[index], std::format("{}[{}]", place, index), problems, depth + 1);
	}
}

/// @brief Joins the sections a feature's program built to its parts: only the parts a feature provides, as objects,
/// each member in its namespace.
/// @param feature The feature.
/// @param place The program's place, for problems.
/// @param built The feature being built.
void joinBuiltSections(const std::string& feature, const std::string& place, BuiltFeature& built) {
	for(const auto& [section, members] : built.program->tree.items()) {
		if(section == "format" || section == "version") continue;
		const std::string at =
		    std::format("{}.{}", place, built.entries.empty() ? "" : buildersOf(built.entries, section));
		if(!among(featureParts, section)) {
			built.problems.push_back(std::format("{}: builds '{}', which a feature can't give", at, section));
			continue;
		}
		if(!members.is_object()) {
			built.problems.push_back(std::format("{}: builds '{}' as something other than an object", at, section));
			continue;
		}
		checkMembers(feature, section, members, std::format("{} (built: {})", at, section), built.problems);
		built.parts[section] = section == "views"   ? viewsWithoutIds(members)
		                       : section == "menus" ? flatMenus(members)
		                                            : members;
	}
}

/// @brief Checks what a feature's program built: only the parts a feature provides, each member in its namespace, its
/// views and exports as a JSON feature's, and no control step.
/// @param feature The feature.
/// @param built The feature being built: its tree's sections join its parts.
void checkBuiltTree(const std::string& feature, BuiltFeature& built) {
	const std::string place = programPlace(feature);
	joinBuiltSections(feature, place, built);
	try {
		if(built.parts.contains("views")) checkProvidedViews(built.parts["views"], feature, place + ".views");
		if(built.parts.contains("exports")) checkExportsSection(built.parts["exports"], place + ".exports");
	} catch(const DocumentError& error) {
		built.problems.push_back(plainMessage(error));
	}
	for(const std::string& step : controlStepsIn(built.parts, place))
		built.problems.push_back(step + " is a control step: a feature that uses one is installed by the host");
	for(const auto& [section, part] : built.parts.items())
		if(section != "state" && section != "settings" && section != "styles" && section != "exports")
			checkDataSteps(feature, part, std::format("{}.{}", place, section), built.problems, 1);
}

/// @brief Adds the problems with the addresses of a feature's definitions.
/// @param feature The feature.
/// @param entries Its definitions.
/// @param problems Receives each, with the definition's place.
void addNameProblems(const std::string& feature, const std::vector<lisp::Entry>& entries,
                     std::vector<std::string>& problems) {
	for(const lisp::Entry& entry : entries)
		if(const std::string why = addressProblem(feature, entry.kind, entry.name); !why.empty())
			problems.push_back(std::format("{}.{}.{}: {}", programPlace(feature), entry.file, entry.name, why));
}

/// @brief Tells whether a path of the app is the feature's own: `state.<feature>`, `computed.<feature>` or
/// `settings.<feature>-...`/`settings.<feature>.<...>`, or below one.
/// @param path The path ("state.wordCount.characters").
/// @param feature The feature.
/// @return true if it is.
bool ownPath(std::string_view path, std::string_view feature) {
	for(const std::string_view root : {std::string_view("state."), std::string_view("computed.")})
		if(path.starts_with(root)) {
			const std::string_view rest = path.substr(root.size());
			return rest.starts_with(feature) &&
			       (rest.size() == feature.size() || rest[feature.size()] == '.' || rest[feature.size()] == '[');
		}
	if(path.starts_with("settings.")) return ownName(path.substr(9), feature);
	return false;
}

/// @brief Tells whether a word is an argument of a `#(...)` function: `%`, `%&` or `%1` to `%9`.
/// @param word The word.
/// @return Whether it is.
bool lambdaArgument(std::string_view word) {
	return word == "%" || word == "%&" || (word.size() == 2 && word[1] >= '1' && word[1] <= '9');
}

/// @brief What a feature's code may reach, for one definition.
struct Reach {
	const std::string& feature;                         ///< The feature.
	const BuiltFeature& built;                          ///< What its program built.
	const json& needs;                                  ///< Its needs.
	const std::set<std::string, std::less<>>& defined;  ///< Its definitions' addresses.
	const std::set<std::string, std::less<>>& commands; ///< Its commands.
	std::vector<std::string>& problems;                 ///< Receives each problem.

	/// @brief Adds a problem of a definition.
	/// @param entry The definition.
	/// @param message What is wrong.
	void problem(const lisp::Entry& entry, const std::string& message) const {
		problems.push_back(std::format("{}.{}.{}: {}", programPlace(feature), entry.file, entry.name, message));
	}

	/// @brief The texts a definition calls a name with, written out, if it calls it no other way.
	/// @param entry The definition.
	/// @param word The name.
	/// @return Them, or nullptr if it calls it with a computed text or uses it as a value.
	const std::vector<std::string>* writtenTexts(const lisp::Entry& entry, const std::string& word) const {
		const auto calls = built.program->namedCalls.find(entry.name);
		if(calls == built.program->namedCalls.end() || calls->second.otherwise.contains(word)) return nullptr;
		const auto texts = calls->second.texts.find(word);
		return texts == calls->second.texts.end() ? nullptr : &texts->second;
	}

	/// @brief Checks a step a definition uses.
	/// @param entry The definition.
	/// @param word The step's word.
	void checkStep(const lisp::Entry& entry, const std::string& word) const {
		if(!among(featureSteps, word)) {
			problem(entry, std::format("uses the step '{}', which a feature's code can't run (a feature that needs it "
			                           "is installed by the host)",
			                           word));
			return;
		}
		const bool writes = among(writingSteps, word);
		if(!writes && word != "run") return;
		const std::vector<std::string>* texts = writtenTexts(entry, word);
		if(!texts) {
			problem(entry, std::format("'{}' must name {} written out", word,
			                           writes ? std::format("its path, \"state.{}...\",", feature) : "what it runs"));
			return;
		}
		for(const std::string& text : *texts)
			if(writes && !(text.starts_with("state.") && ownPath(text, feature)))
				problem(entry, std::format(R"('{}' writes "{}": a feature writes only under "state.{}")", word, text,
				                           feature));
			else if(!writes && !runnable(text))
				problem(entry,
				        std::format(R"(runs "{}": a feature runs only its own commands and the actions it needs )"
				                    R"(("app.<need>" of kind action))",
				                    text));
	}

	/// @brief Tells whether a feature may run something by name.
	/// @param text The name: one of its commands, or `app.<need>`.
	/// @return true if it may.
	[[nodiscard]] bool runnable(std::string_view text) const {
		if(commands.contains(text)) return true;
		if(!text.starts_with("app.")) return false;
		const auto need = needs.find(std::string(text.substr(4)));
		return need != needs.end() && need->is_object() && need->value("kind", "") == "action";
	}

	/// @brief Checks one name a definition uses, not a path: a step, an app variable, or something undefined.
	/// @param entry The definition.
	/// @param word The name.
	/// @param function Whether the definition is a function.
	/// @param undefined What it uses that nothing defines.
	void checkName(const lisp::Entry& entry, const std::string& word, bool function,
	               const std::set<std::string, std::less<>>& undefined) const {
		if(stepWord(word)) checkStep(entry, word);
		else if(function && among(appVariables, word))
			problem(entry, std::format("reads '{}': pass it to the function as an argument from the view", word));
		else if(undefined.contains(word) && !detail::findFunction(word) && !among(appVariables, word))
			problem(entry,
			        std::format("uses '{}', which the feature doesn't define and the language doesn't have", word));
	}

	/// @brief Checks the names a definition's text writes that the language keeps for what it compiles (`%read`, which
	/// a path compiles to, reads any path), and the function that would make one (`symbol`).
	/// @param entry The definition.
	/// @param names The names its code uses.
	void checkKeptNames(const lisp::Entry& entry, const std::vector<std::string>& names) const {
		constexpr std::array<std::string_view, 1> percent = {"%"};
		for(const std::string& word : pathsWritten(entry.text, percent))
			if(!lambdaArgument(word))
				problem(entry, std::format("uses '{}', a name the language keeps for what it compiles", word));
		if(std::ranges::find(names, "symbol") != names.end())
			problem(entry, "uses 'symbol': a feature's code writes out the names it uses");
	}

	/// @brief Checks what one definition uses.
	/// @param entry The definition.
	/// @param names The names its code uses.
	/// @param undefined What it uses that nothing defines.
	void check(const lisp::Entry& entry, const std::vector<std::string>& names,
	           const std::set<std::string, std::less<>>& undefined) const {
		const bool function = among(functionForms, entry.kind);
		// The paths a function reads: the words of its text (the build reads them as the app's, and doesn't list
		// them), and those its macros give
		std::vector<std::string> paths = function ? pathsWritten(entry.text, valueRoots) : std::vector<std::string>();
		checkKeptNames(entry, names);
		for(const std::string& word : names) {
			if(defined.contains(word) || word.starts_with('%')) continue;
			if(std::ranges::any_of(valueRoots, [&](std::string_view root) { return word.starts_with(root); })) {
				if(function && std::ranges::find(paths, word) == paths.end()) paths.push_back(word);
				continue;
			}
			checkName(entry, word, function, undefined);
		}
		for(const std::string& path : paths)
			if(!ownPath(path, feature))
				problem(entry, std::format("reads '{}': pass it to the function as an argument from the view", path));
	}
};

/// @brief What the points a feature is placed in offer its extensions.
struct Offered {
	std::set<std::string, std::less<>> hooks;   ///< The hook points they list, that the app declares with `defhook`.
	std::set<std::string, std::less<>> listed;  ///< The hook points they list, declared or not.
	std::set<std::string, std::less<>> actions; ///< The actions of the action exports they receive.
};

/// @brief Adds what one point offers: the hook points it lists, and the actions of the action exports it receives.
/// @param point The point.
/// @param exports The app's exports.
/// @param offered Receives them.
void addOffers(const json& point, const json& exports, Offered& offered) {
	for(const json& hook : point.value("hooks", json::array()))
		if(hook.is_string()) offered.listed.insert(hook.get<std::string>());
	for(const json& exported : point.value("receives", json::array())) {
		if(!exported.is_string() || !exports.contains(exported.get<std::string>())) continue;
		if(const json& entry = exports[exported.get<std::string>()]; exportKind(entry) == "action")
			if(std::string action = actionTarget(entry); !action.empty()) offered.actions.insert(std::move(action));
	}
}

/// @brief The hook points of a list that the app's program declares with `defhook`.
/// @param document The app's document.
/// @param listed The hook points.
/// @return Those it declares.
std::set<std::string, std::less<>> declaredHooks(const json& document,
                                                 const std::set<std::string, std::less<>>& listed) {
	std::set<std::string, std::less<>> declared;
	if(!lisp::hasProgram(document)) return declared;
	try {
		for(const lisp::Entry& entry : lisp::programOf(document["program"]).entries())
			if(headWord(entry.text) == "defhook" && listed.contains(entry.name)) declared.insert(entry.name);
		// NOLINTNEXTLINE(bugprone-empty-catch): the app's own check reports it; meanwhile it declares no hook
	} catch(const DocumentError&) {
	}
	return declared;
}

/// @brief What the points a feature is placed in offer its hooks and advice.
/// @param document The app's document.
/// @param placed The points.
/// @return What they offer.
Offered offeredBy(const json& document, const std::vector<std::string>& placed) {
	Offered offered;
	const json points = document.value("points", json::object());
	const json exports = document.value("exports", json::object());
	if(!points.is_object() || !exports.is_object()) return offered;
	for(const std::string& name : placed)
		if(const auto point = points.find(name); point != points.end() && point->is_object())
			addOffers(*point, exports, offered);
	if(!offered.listed.empty()) offered.hooks = declaredHooks(document, offered.listed);
	return offered;
}

/// @brief Why an extension of a feature can't join the app, if it can't.
/// @param extension The extension.
/// @param defined The feature's definitions.
/// @param offered What its points offer.
/// @return What is wrong, or "".
std::string extensionProblem(const lisp::Extension& extension, const std::set<std::string, std::less<>>& defined,
                             const Offered& offered) {
	if(extension.targets.empty()) return "names nothing to join";
	const std::string& target = extension.targets.front();
	if(!defined.contains(extension.targets.back()))
		return std::format("its function '{}' must be one of the feature's own", extension.targets.back());
	if(extension.kind == "advice")
		return offered.actions.contains(target)
		           ? ""
				   : std::format("advises '{}': a feature advises only an action export a point it is placed in "
				                 "receives",
				                 target);
	if(offered.hooks.contains(target)) return "";
	return offered.listed.contains(target)
	           ? std::format("the app declares no hook point '{}' (defhook)", target)
			   : std::format("joins the hook '{}': no point the feature is placed in lists it (points.<name>.hooks)",
			                 target);
}

} // namespace

std::vector<std::string> programNameProblems(const std::string& feature, const json& section) {
	std::vector<std::string> problems;
	addNameProblems(feature, entriesOf(feature, section, problems), problems);
	return problems;
}

BuiltFeature builtFeature(const std::string& feature, const json& provides) {
	BuiltFeature built;
	const json& section = provides.at("program");
	built.entries = entriesOf(feature, section, built.problems);
	addNameProblems(feature, built.entries, built.problems);
	if(!built.problems.empty()) return built;
	lisp::keepFeatureBuilds(); // the app's own section stays kept beside each feature's
	try {
		built.program = lisp::builtProgram(section);
	} catch(const lisp::LispError& error) {
		built.problems.push_back(buildProblem(feature, error));
		return built;
	} catch(const DocumentError& error) {
		built.problems.push_back(std::format("{}: {}", programPlace(feature), plainMessage(error)));
		return built;
	}
	checkBuiltTree(feature, built);
	if(!built.problems.empty()) built.parts = json::object();
	return built;
}

std::vector<std::string> programReachProblems(const std::string& feature, const BuiltFeature& built,
                                              const json& needs) {
	std::vector<std::string> problems;
	if(!built.program) return problems;
	std::set<std::string, std::less<>> defined, commands;
	for(const lisp::Entry& entry : built.entries) {
		defined.insert(entry.name);
		if(entry.kind == "defcommand") commands.insert(entry.name);
	}
	std::map<std::string, std::set<std::string, std::less<>>, std::less<>> undefined;
	for(const lisp::UndefinedName& use : built.program->undefined) undefined[use.definition].insert(use.name);
	const json noNeeds = json::object();
	const Reach reach{feature, built, needs.is_object() ? needs : noNeeds, defined, commands, problems};
	const std::set<std::string, std::less<>> none;
	const std::vector<std::string> nothing;
	for(const lisp::Entry& entry : built.entries) {
		const auto used = built.program->namesUsed.find(entry.name);
		const auto names = undefined.find(entry.name);
		reach.check(entry, used == built.program->namesUsed.end() ? nothing : used->second,
		            names == undefined.end() ? none : names->second);
	}
	return problems;
}

std::vector<std::string> programExtensionProblems(const std::string& feature, const BuiltFeature& built,
                                                  const json& document, const std::vector<std::string>& placed,
                                                  std::vector<FeatureExtension>& accepted) {
	std::vector<std::string> problems;
	if(!built.program || built.program->extensions.empty()) return problems;
	std::set<std::string, std::less<>> defined;
	std::map<std::string, std::string, std::less<>> files;
	for(const lisp::Entry& entry : built.entries) {
		defined.insert(entry.name);
		files.emplace(entry.name, entry.file);
	}
	const Offered offered = offeredBy(document, placed);
	for(const lisp::Extension& extension : built.program->extensions) {
		if(const std::string why = extensionProblem(extension, defined, offered); !why.empty()) {
			const auto file = files.find(extension.name);
			problems.push_back(std::format("{}.{}.{}: {}", programPlace(feature),
			                               file == files.end() ? std::string() : file->second, extension.name, why));
		} else accepted.push_back({feature, extension.kind, extension.name, extension.targets.front()});
	}
	return problems;
}

} // namespace affresco::format
