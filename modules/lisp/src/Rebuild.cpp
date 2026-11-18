/// @file
/// @brief The build of a later version of a program in part (Rebuild.hpp, Program::rebuild()): only the definitions
/// that changed run, and those that use them; what the others gave is taken from the earlier build's record.
///
/// Where it can't be sure it gives what a build in full gives, it builds in full. That is when a macro, a hook or an
/// advice changes or goes, as a macro changes what any code means and a hook or an advice what a function does; when an
/// entry isn't a definition (a macro's call at the top level or `do`, which defines whatever it gives); when the
/// program has namespaces; when a definition would use one the build in part didn't load; when what a build found
/// (undefined names, truth tests) didn't come in the order of the definitions; and when anything fails, so that the
/// build in full says why, as it always did.

#include "Rebuild.hpp"

#include "Definitions.hpp"
#include "TopLevel.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <atomic>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): a thread's own count, read by its tests
thread_local RebuildStatistics statistics; ///< See rebuildStatistics().

/// @brief What a build in part runs and loads, by the program's entries.
struct PartPlan {
	std::vector<std::optional<std::size_t>> before; ///< Each entry's definition in the record, if it had one.
	std::vector<bool> changed;  ///< Each entry's text, file or author differs from the record's, or it is new.
	std::vector<bool> affected; ///< Each entry runs: it changed, or uses one that did, or one that went.
	/// Each entry is loaded: it runs, it is a macro, its name is the language's or a primitive's (which a library macro
	/// may call), or what is loaded uses it.
	std::vector<bool> loaded;
	std::vector<std::string> removed; ///< The definitions the record has and the program doesn't.
};

/// @brief What a build in part ran.
struct PartBuilt {
	std::vector<std::optional<DefinitionBuilt>> built; ///< By entry, what each that ran gave.
	Budget spent;                                      ///< What it spent.
	std::size_t macroSpent = 0;                        ///< What its macros' expansion spent.
	std::size_t compiled = 0;                          ///< The definitions it loaded.
};

/// @brief The word that starts a form's text: `defn` for `(defn f ...)`.
/// @param text The text.
/// @return It, or "" if the text doesn't start with a form that has one.
std::string headOf(std::string_view text) {
	try {
		notation::Lexer lexer(text, true);
		if(lexer.next().kind != notation::TokenKind::OpenNode) return "";
		const notation::Token word = lexer.next();
		return word.kind == notation::TokenKind::Word ? std::string(word.text) : "";
	} catch(const DocumentError&) {
		return "";
	}
}

/// @brief Whether a change of a definition of this kind may change what any other means: a macro, a hook, an advice,
/// and what isn't a definition.
/// @param kind Its form's word.
/// @return true if so.
bool reachesAnywhere(std::string_view kind) {
	return kind == "defmacro" || isExtensionWord(kind) || !isDefinitionWord(kind);
}

/// @brief Calls a function with each word of a text, and each word's part before a `.` or `/`: what its code may use.
/// @param text The text.
/// @param function The function.
void forEachWord(std::string_view text, const std::function<void(std::string_view)>& function) {
	try {
		notation::Lexer lexer(text, true);
		for(notation::Token token = lexer.next(); token.kind != notation::TokenKind::End; token = lexer.next()) {
			if(token.kind != notation::TokenKind::Word) continue;
			function(token.text);
			if(const std::size_t cut = token.text.find_first_of("./"); cut != std::string_view::npos && cut > 0)
				function(token.text.substr(0, cut));
		}
	} catch(const DocumentError&) {
		function(""); // what doesn't read fails to load, and the build in full says why
	}
}

/// @brief The other entries a text names: a view's expression calls a function by its name, which its compiled code
/// doesn't hold, yet what it compiles to depends on whether the function is defined; a build in part loads each, and
/// builds again what names one that changed or went.
/// @param text An entry's text.
/// @param own Its name.
/// @param names Each entry's place, by its name.
/// @return Their names, sorted.
std::vector<std::string> entryWords(std::string_view text, std::string_view own,
                                    const std::map<std::string, std::size_t>& names) {
	std::set<std::string, std::less<>> found;
	forEachWord(text, [&](std::string_view word) {
		if(word != own && !found.contains(word))
			if(const auto entry = names.find(std::string(word)); entry != names.end()) found.insert(entry->first);
	});
	return {found.begin(), found.end()};
}

/// @brief Marks the entries whose text names one of some names, which no record's words hold: a name that came, in the
/// earlier version's record, or one that went, in the later's (entryWords() keeps only the entries of its version).
/// @param entries The entries.
/// @param words The names.
/// @param marked Each entry's mark, set for those that name one.
/// @param found Gets the names of those it marked.
void markNaming(const std::vector<Entry>& entries, const std::set<std::string, std::less<>>& words,
                std::vector<bool>& marked, std::vector<std::string>& found) {
	if(words.empty()) return;
	for(std::size_t at = 0; at < entries.size(); ++at) {
		if(marked[at]) continue;
		bool names = false;
		forEachWord(entries[at].text, [&](std::string_view word) {
			names = names || (word != entries[at].name && words.contains(word));
		});
		if(!names) continue;
		marked[at] = true;
		found.push_back(entries[at].name);
	}
}

/// @brief Marks the entries that run: those that changed, and those that use one that changed or went, over and over.
/// @param previous The record.
/// @param entries The entries.
/// @param names Each entry's place, by its name.
/// @param plan Gets them.
void markAffected(const BuildRecord& previous, const std::vector<Entry>& entries,
                  const std::map<std::string, std::size_t>& names, PartPlan& plan) {
	std::vector<std::string> waiting(plan.removed.begin(), plan.removed.end());
	std::set<std::string, std::less<>> came;
	for(std::size_t at = 0; at < entries.size(); ++at)
		if(plan.changed[at]) {
			plan.affected[at] = true;
			waiting.emplace_back(entries[at].name);
			if(!plan.before[at]) came.insert(entries[at].name);
		}
	markNaming(entries, came, plan.affected, waiting); // the record's words of them were left out: they weren't entries
	while(!waiting.empty()) {
		const auto users = previous.users.find(waiting.back());
		waiting.pop_back();
		if(users == previous.users.end()) continue;
		for(const std::size_t user : users->second) {
			const auto entry = names.find(previous.definitions[user]->name);
			if(entry == names.end() || plan.affected[entry->second]) continue;
			plan.affected[entry->second] = true;
			waiting.emplace_back(entries[entry->second].name);
		}
	}
}

/// @brief Marks the entries to load: those that run, the macros, those named as the language's own, and what they
/// use, over and over: a changed entry's every word, another's uses as the record has them.
/// @param previous The record.
/// @param entries The entries.
/// @param names Each entry's place, by its name.
/// @param shadows Whether a name is the language's or a primitive's.
/// @param plan Gets them.
void markLoaded(const BuildRecord& previous, const std::vector<Entry>& entries,
                const std::map<std::string, std::size_t>& names, const std::function<bool(std::string_view)>& shadows,
                PartPlan& plan) {
	std::vector<std::size_t> waiting;
	const auto load = [&](std::size_t at) {
		if(plan.loaded[at]) return;
		plan.loaded[at] = true;
		waiting.push_back(at);
	};
	for(std::size_t at = 0; at < entries.size(); ++at) {
		const std::optional<std::size_t> was = plan.before[at]; // an entry unchanged has one
		const bool macro = !plan.changed[at] && was && previous.definitions[*was]->kind == "defmacro";
		if(plan.affected[at] || macro || shadows(entries[at].name)) load(at);
	}
	const auto use = [&](std::string_view name) {
		if(const auto found = names.find(std::string(name)); found != names.end()) load(found->second);
	};
	while(!waiting.empty()) {
		const std::size_t at = waiting.back();
		waiting.pop_back();
		if(plan.changed[at]) {
			forEachWord(entries[at].text, use);
		} else if(const std::optional<std::size_t> was = plan.before[at]) {
			for(const std::string& name : previous.definitions[*was]->uses) use(name);
			for(const std::string& name : previous.definitions[*was]->words) use(name);
		}
	}
}

/// @brief Notes what the record had of an entry, and whether it changed.
/// @param previous The record.
/// @param entry The entry.
/// @param at Its place.
/// @param shadows Whether a name is the language's or a primitive's.
/// @param plan Gets it.
/// @return false where only a build in full can be sure of the change.
bool markChange(const BuildRecord& previous, const Entry& entry, std::size_t at,
                const std::function<bool(std::string_view)>& shadows, PartPlan& plan) {
	const auto found = previous.index.find(entry.name);
	const DefinitionBuilt* was = found == previous.index.end() ? nullptr : previous.definitions[found->second].get();
	if(was) plan.before[at] = found->second;
	if(was && was->text == entry.text && was->file == entry.file && was->author == entry.author) return true;
	plan.changed[at] = true;
	if(shadows(entry.name)) return false; // a library macro anywhere may call it now, or no more
	const std::string kind = headOf(entry.text);
	if(reachesAnywhere(kind) || (was && reachesAnywhere(was->kind))) return false;
	return !was || was->kind == kind || !previous.extensions; // a hook's function may be no more
}

/// @brief Works out what a build in part runs and loads.
/// @param previous The record.
/// @param entries The program's entries.
/// @param names Each entry's place, by its name.
/// @param shadows Whether a name is the language's or a primitive's.
/// @return The plan, or std::nullopt where only a build in full can be sure, or would load everything anyway.
std::optional<PartPlan> planPart(const BuildRecord& previous, const std::vector<Entry>& entries,
                                 const std::map<std::string, std::size_t>& names,
                                 const std::function<bool(std::string_view)>& shadows) {
	PartPlan plan;
	plan.before.resize(entries.size());
	plan.changed.assign(entries.size(), false);
	plan.affected.assign(entries.size(), false);
	plan.loaded.assign(entries.size(), false);
	for(std::size_t at = 0; at < entries.size(); ++at)
		if(!markChange(previous, entries[at], at, shadows, plan)) return std::nullopt;
	for(const auto& definition : previous.definitions)
		if(!names.contains(definition->name)) {
			if(reachesAnywhere(definition->kind) || previous.extensions || shadows(definition->name))
				return std::nullopt;
			plan.removed.push_back(definition->name);
		}
	markAffected(previous, entries, names, plan);
	markLoaded(previous, entries, names, shadows, plan);
	if(std::ranges::all_of(plan.loaded, std::identity())) return std::nullopt;
	return plan;
}

/// @brief Whether every definition loaded uses only what was loaded of the program's entries: a macro's expansion may
/// use what its call's words don't say.
/// @param machine The machine.
/// @param names Each entry's place, by its name.
/// @param plan The plan.
/// @return true if so.
bool usesOnlyLoaded(const Machine& machine, const std::map<std::string, std::size_t>& names, const PartPlan& plan) {
	for(const auto& definition : machine.definitions())
		for(const std::string& name : namesUsedBy(*definition).names)
			if(const auto found = names.find(name); found != names.end() && !plan.loaded[found->second]) return false;
	return true;
}

/// @brief What a machine's build found: its truth tests, or the names it uses that nothing defines.
/// @param machine The machine.
/// @param truth Its truth tests; else its undefined names.
/// @return Them, in the order found.
std::vector<UndefinedName> foundIn(const Machine& machine, bool truth) {
	std::vector<UndefinedName> found;
	if(truth) {
		for(const auto& [path, place, definition] : machine.truthTests())
			found.push_back({path, place.file ? *place.file : "", place.line, place.column, definition});
		return found;
	}
	for(const GlobalUse& use : machine.uses())
		if(use.global->state == Global::State::Undefined)
			found.push_back({use.global->name, use.place.file ? *use.place.file : "", use.place.line, use.place.column,
			                 use.definition});
	return found;
}

/// @brief Runs the definitions a plan says, in a fresh interpreter that has loaded what they need.
/// @param entries The program's entries.
/// @param names Each entry's place, by its name.
/// @param primitives The program's pure primitives.
/// @param plan The plan.
/// @param limits The limits, less what the record's build spent.
/// @return What they gave, or std::nullopt where only a build in full can be sure.
/// @throws LispError If the build fails: the build in full says why.
std::optional<PartBuilt> runPart(const std::vector<Entry>& entries, const std::map<std::string, std::size_t>& names,
                                 const std::vector<std::pair<std::string, Primitive>>& primitives, const PartPlan& plan,
                                 const Limits& limits) {
	Program program;
	for(std::size_t at = 0; at < entries.size(); ++at)
		if(plan.loaded[at]) program.addEntryUnread(entries[at]);
	Interpreter interpreter(limits);
	interpreter.setPure(true);
	for(const auto& [name, primitive] : primitives) interpreter.definePrimitive(name, primitive);
	program.load(interpreter);
	Machine& machine = interpreter.machine();
	return leavingAsLispError(machine, [&] -> std::optional<PartBuilt> {
		machine.workOutDefinitions();
		if(!usesOnlyLoaded(machine, names, plan)) return std::nullopt;
		PartBuilt part;
		part.built.resize(entries.size());
		std::vector<DefinitionBuilt> loaded(machine.definitions().size()); // by the machine's order
		std::map<std::string, std::size_t, std::less<>> order;
		for(std::size_t at = 0; at < loaded.size(); ++at) {
			const Definition& definition = *machine.definitions()[at];
			const std::size_t entry = names.at(definition.name);
			if(plan.affected[entry]) loaded[at] = recordDefinition(machine, definition, nullptr);
			order.emplace(definition.name, at);
		}
		if(!sortByDefinition(foundIn(machine, true), order, loaded, true) ||
		   !sortByDefinition(foundIn(machine, false), order, loaded, false))
			return std::nullopt;
		for(std::size_t at = 0; at < loaded.size(); ++at)
			if(const std::size_t entry = names.at(machine.definitions()[at]->name); plan.affected[entry]) {
				loaded[at].name = entries[entry].name;
				loaded[at].file = entries[entry].file;
				loaded[at].text = entries[entry].text;
				loaded[at].words = entryWords(entries[entry].text, entries[entry].name, names);
				loaded[at].author = entries[entry].author;
				part.built[entry] = std::move(loaded[at]);
			}
		part.spent = {limits.budget.operations - machine.budget().operations,
		              limits.budget.bytes - machine.budget().bytes};
		part.macroSpent = limits.macroOperations - machine.macroOperationsLeft();
		part.compiled = loaded.size();
		return part;
	});
}

/// @brief The definitions of a build in part, in the order a build in full runs them: the macros, then the rest, each
/// in the order of their entries; those that ran as they gave now, the others as the record has them.
/// @param previous The record.
/// @param plan The plan.
/// @param part What ran.
/// @return Them.
std::vector<std::shared_ptr<const DefinitionBuilt>> inBuildOrder(const BuildRecord& previous, const PartPlan& plan,
                                                                 PartBuilt& part) {
	std::vector<std::shared_ptr<const DefinitionBuilt>> each(plan.before.size());
	for(std::size_t at = 0; at < each.size(); ++at)
		if(std::optional<DefinitionBuilt>& ran = part.built[at])
			each[at] = std::make_shared<DefinitionBuilt>(std::move(*ran));
		else if(const std::optional<std::size_t> was = plan.before[at]) each[at] = previous.definitions[*was];
	std::vector<std::shared_ptr<const DefinitionBuilt>> ordered;
	ordered.reserve(each.size());
	for(const bool macros : {true, false})
		for(const auto& definition : each)
			if((definition->kind == "defmacro") == macros) ordered.push_back(definition);
	return ordered;
}

/// @brief Indexes a record's definitions: each one's place, and who uses each name.
/// @param record The record.
void indexRecord(BuildRecord& record) {
	for(std::size_t at = 0; at < record.definitions.size(); ++at) {
		const DefinitionBuilt& definition = *record.definitions[at];
		record.index.emplace(definition.name, at);
		for(const std::string& name : definition.uses) record.users[name].push_back(at);
		for(const std::string& name : definition.words) record.users[name].push_back(at);
		record.extensions = record.extensions || definition.extension.has_value();
	}
}

/// @brief Escapes a name for a JSON pointer.
/// @param name The name.
/// @return It escaped.
std::string pointerPart(std::string_view name) {
	std::string escaped;
	for(const char c : name) {
		if(c == '~') escaped += "~0";
		else if(c == '/') escaped += "~1";
		else escaped += c;
	}
	return escaped;
}

} // namespace

DefinitionBuilt recordDefinition(Machine& machine, const Definition& definition, BuiltParts* parts) {
	DefinitionBuilt built;
	built.name = definition.name;
	built.kind = definition.kind;
	if(isExtensionWord(definition.kind)) {
		built.extension = Extension{definition.kind, definition.name,   definition.targets,
		                            definition.doc,  definition.author, machine.authorNames(definition.authors)};
		checkExtensionFunction(machine, definition);
	}
	Gifts gifts(parts, &built.gifts);
	buildDefinition(machine, definition, gifts);
	const Uses uses = namesUsedBy(definition);
	built.uses.assign(uses.names.begin(), uses.names.end());
	if(CallsByName named = namedCallsOf(uses); !named.texts.empty()) built.named = std::move(named);
	return built;
}

bool sortByDefinition(const std::vector<UndefinedName>& found,
                      const std::map<std::string, std::size_t, std::less<>>& index, std::vector<DefinitionBuilt>& into,
                      bool truth) {
	for(const UndefinedName& item : found) {
		const auto at = index.find(item.definition);
		if(at == index.end()) return false;
		(truth ? into[at->second].truth : into[at->second].undefined).push_back(item);
	}
	return true;
}

void sortByDefinitionOrder(const Machine& machine, std::vector<UndefinedName>& found) {
	if(found.size() < 2) return;
	std::unordered_map<std::string_view, std::size_t> order;
	order.reserve(machine.definitions().size());
	for(const auto& definition : machine.definitions()) order.emplace(definition->name, order.size());
	std::vector<std::pair<std::size_t, std::size_t>> keys; // each item's definition's place, and its own
	keys.reserve(found.size());
	for(const UndefinedName& item : found) {
		const auto at = order.find(item.definition);
		keys.emplace_back(at == order.end() ? order.size() : at->second, keys.size());
	}
	if(std::ranges::is_sorted(keys)) return;
	std::ranges::sort(keys); // the second of each pair keeps the order found among equals
	std::vector<UndefinedName> sorted;
	sorted.reserve(found.size());
	for(const auto& key : keys) sorted.push_back(std::move(found[key.second]));
	found = std::move(sorted);
}

void addPlacesGiven(const DefinitionBuilt& definition, std::vector<std::string>& places) {
	for(const Gift& gift : definition.gifts)
		if(gift.kind == Gift::Kind::Member || gift.kind == Gift::Kind::Default)
			places.push_back("/" + pointerPart(gift.section) + "/" + pointerPart(gift.name));
		else places.push_back("/" + pointerPart(gift.name));
}

std::uint64_t nextBuildSerial() {
	static std::atomic<std::uint64_t> serial = 0;
	return ++serial;
}

void countBuild(bool whole, std::size_t compiled) {
	++(whole ? statistics.whole : statistics.inPart);
	statistics.compiled += compiled;
}

const RebuildStatistics& rebuildStatistics() noexcept { return statistics; }

void Program::keepRecord(Machine& machine, std::vector<DefinitionBuilt> record, const Limits& limits) {
	auto kept = std::make_shared<BuildRecord>();
	kept->serial = nextBuildSerial();
	kept->compiled = record.size();
	std::map<std::string, std::size_t, std::less<>> index;
	for(std::size_t at = 0; at < record.size(); ++at) {
		const auto entry = names_.find(record[at].name);
		if(entry == names_.end()) continue; // a macro's call at the top level defined it
		record[at].file = entries_[entry->second].file;
		record[at].text = entries_[entry->second].text;
		if(!recording_ && namespaces_.empty()) // else it never builds in part
			record[at].words = entryWords(record[at].text, record[at].name, names_);
		record[at].author = entries_[entry->second].author;
		index.emplace(record[at].name, at);
	}
	kept->inPart = !recording_ && namespaces_.empty() && record.size() == entries_.size() &&
	               index.size() == record.size() && sortByDefinition(truth_, index, record, true) &&
	               sortByDefinition(undefined_, index, record, false);
	kept->spent = {limits.budget.operations - machine.budget().operations,
	               limits.budget.bytes - machine.budget().bytes};
	kept->macroSpent = limits.macroOperations - machine.macroOperationsLeft();
	kept->definitions.reserve(record.size());
	for(DefinitionBuilt& definition : record)
		kept->definitions.push_back(std::make_shared<const DefinitionBuilt>(std::move(definition)));
	indexRecord(*kept);
	countBuild(true, kept->compiled);
	record_ = std::move(kept);
}

json Program::rebuild(const BuildRecord& previous, const Limits& limits) {
	if(std::optional<json> tree = buildInPart(previous, limits)) return std::move(*tree);
	json tree = build(limits);
	record_->from = previous.serial;
	return tree;
}

namespace {

/// @brief Whether what a record's build spent leaves some of the limits for a build in part.
/// @param previous The record. @param limits The limits.
/// @return true if so.
bool leavesBudget(const BuildRecord& previous, const Limits& limits) {
	return previous.spent.operations < limits.budget.operations && previous.spent.bytes < limits.budget.bytes &&
	       previous.macroSpent < limits.macroOperations;
}

/// @brief Whether a name is one of the language's or a primitive's: a library macro anywhere may call it.
/// @param primitives The program's primitives. @param name The name.
/// @return true if so.
bool shadowsName(const std::vector<std::pair<std::string, Primitive>>& primitives, std::string_view name) {
	static const std::set<std::string, std::less<>> language = [] {
		const std::vector<std::string> all = Interpreter().machine().globalNames();
		return std::set<std::string, std::less<>>(all.begin(), all.end());
	}();
	return language.contains(name) ||
	       std::ranges::any_of(primitives, [name](const auto& primitive) { return primitive.first == name; });
}

/// @brief The places a build in part could have changed: what the definitions that ran gave, before and now, and
/// what those that went gave.
/// @param previous The record. @param plan The plan. @param definitions The build's definitions.
/// @param names Each entry's place, by its name.
/// @return Them.
std::vector<std::string> placesTouched(const BuildRecord& previous, const PartPlan& plan,
                                       const std::vector<std::shared_ptr<const DefinitionBuilt>>& definitions,
                                       const std::map<std::string, std::size_t>& names) {
	std::vector<std::string> places;
	for(const auto& definition : definitions)
		if(const std::size_t entry = names.at(definition->name); plan.affected[entry]) {
			addPlacesGiven(*definition, places);
			if(const std::optional<std::size_t> was = plan.before[entry])
				addPlacesGiven(*previous.definitions[*was], places);
		}
	for(const std::string& name : plan.removed)
		addPlacesGiven(*previous.definitions[previous.index.find(name)->second], places);
	return places;
}

} // namespace

std::optional<json> Program::buildInPart(const BuildRecord& previous, const Limits& limits) {
	if(!previous.inPart || recording_ || !namespaces_.empty() || !leavesBudget(previous, limits)) return std::nullopt;
	const auto shadows = [this](std::string_view name) { return shadowsName(primitives_, name); };
	const std::optional<PartPlan> plan = planPart(previous, entries_, names_, shadows);
	if(!plan) return std::nullopt;
	Limits part = limits;
	part.budget.operations -= previous.spent.operations;
	part.budget.bytes -= previous.spent.bytes;
	part.macroOperations -= previous.macroSpent;
	try {
		std::optional<PartBuilt> ran = runPart(entries_, names_, primitives_, *plan, part);
		if(!ran) return std::nullopt;
		auto record = std::make_shared<BuildRecord>();
		record->definitions = inBuildOrder(previous, *plan, *ran);
		BuiltParts parts;
		for(const auto& definition : record->definitions)
			for(const Gift& gift : definition->gifts) giveAgain(gift, parts);
		json tree = treeOf(parts, defaulted_);
		extensions_.clear();
		undefined_.clear();
		truth_.clear();
		used_.clear();
		named_.clear();
		places_.clear();
		for(const auto& definition : record->definitions) {
			if(definition->extension) extensions_.push_back(*definition->extension);
			undefined_.insert(undefined_.end(), definition->undefined.begin(), definition->undefined.end());
			truth_.insert(truth_.end(), definition->truth.begin(), definition->truth.end());
			used_[definition->name] = definition->uses;
			if(definition->named) named_[definition->name] = *definition->named;
			if(definition->kind == "defstate") defaulted_.emplace("state", definition->name.substr(6));
		}
		record->touched = placesTouched(previous, *plan, record->definitions, names_);
		record->inPart = true;
		record->whole = false;
		record->spent = {previous.spent.operations + ran->spent.operations, previous.spent.bytes + ran->spent.bytes};
		record->macroSpent = previous.macroSpent + ran->macroSpent;
		record->compiled = ran->compiled;
		record->serial = nextBuildSerial();
		record->from = previous.serial;
		indexRecord(*record);
		countBuild(false, record->compiled);
		record_ = std::move(record);
		return tree;
	} catch(const std::exception&) { // the build in full says what is wrong, as it would have
		return std::nullopt;
	}
}

} // namespace affresco::lisp

namespace affresco::lisp {

namespace {

/// @brief Whether a name is one of the language's own globals (or a primitive defined before any program).
/// @param machine A machine that has loaded a program.
/// @param name The name.
/// @return true if so.
bool isLanguageName(const Machine& machine, std::string_view name) {
	static const std::set<std::string, std::less<>> language = [] {
		const std::vector<std::string> all = Interpreter().machine().globalNames();
		return std::set<std::string, std::less<>>(all.begin(), all.end());
	}();
	const Definition* definition = machine.definition(name);
	return language.contains(name) || (definition && definition->global && definition->global->host);
}

/// @brief The entries a redefinition defines again: those that changed, and those that use one that changed or went,
/// over and over, by the uses of the new version's build.
/// @param built The new version's build record.
/// @param seeds The names that changed or went.
/// @param names Each entry of the new version's place, by its name.
/// @param entries The new version's entries.
/// @param removed The names that went.
/// @return Each entry's: whether it is defined again.
std::vector<bool> redefined(const BuildRecord& built, std::vector<std::string> seeds,
                            const std::map<std::string, std::size_t>& names, const std::vector<Entry>& entries,
                            const std::vector<std::string>& removed) {
	std::vector<bool> again(names.size(), false);
	for(const std::string& name : seeds)
		if(const auto found = names.find(name); found != names.end()) again[found->second] = true;
	// The build's words of a name that went were left out: it isn't an entry now
	markNaming(entries, {removed.begin(), removed.end()}, again, seeds);
	while(!seeds.empty()) {
		const auto users = built.users.find(seeds.back());
		seeds.pop_back();
		if(users == built.users.end()) continue;
		for(const std::size_t user : users->second) {
			const std::string& name = built.definitions[user]->name;
			const auto found = names.find(name);
			if(found == names.end() || again[found->second]) continue;
			again[found->second] = true;
			seeds.push_back(name);
		}
	}
	return again;
}

/// @brief What a later version changes, by name.
struct InPlace {
	std::vector<std::string> changed; ///< The entries that changed or came.
	std::vector<std::string> removed; ///< Those that went.
};

/// @brief Whether an entry that changed or came can be defined in place.
/// @param machine The machine, which loaded the earlier version.
/// @param entry The entry.
/// @param had Whether the earlier version had it.
/// @return false where it may reach anywhere, is named as the language's, or is now another kind of definition.
bool changesInPlace(const Machine& machine, const Entry& entry, bool had) {
	const std::string kind = headOf(entry.text);
	return !reachesAnywhere(kind) && !isLanguageName(machine, entry.name) &&
	       (!had || machine.definition(entry.name)->kind == kind);
}

/// @brief What a later version changes of the program an interpreter loaded, if it can be changed in place.
/// @param machine The interpreter's machine, which loaded the earlier version.
/// @param entries The later version's entries. @param names Their places, by name.
/// @param before The earlier version's entries. @param beforeNames Their places, by name.
/// @return The names that changed, then those that went; std::nullopt where only a new interpreter can be sure.
std::optional<InPlace> changeInPlace(const Machine& machine, const std::vector<Entry>& entries,
                                     const std::map<std::string, std::size_t>& names, const std::vector<Entry>& before,
                                     const std::map<std::string, std::size_t>& beforeNames) {
	for(const auto& definition : machine.definitions())
		if(!beforeNames.contains(definition->name) || isExtensionWord(definition->kind)) return std::nullopt; // advice
	InPlace change;
	for(const Entry& entry : entries) {
		const auto was = beforeNames.find(entry.name);
		const Entry* old = was == beforeNames.end() ? nullptr : &before[was->second];
		if(old && old->text == entry.text && old->file == entry.file && old->author == entry.author) continue;
		if(!changesInPlace(machine, entry, old != nullptr)) return std::nullopt;
		change.changed.push_back(entry.name);
	}
	for(const Entry& entry : before)
		if(!names.contains(entry.name)) {
			// A macro gone: what it expanded into keeps its code, and its calls hold nothing of it
			if(isLanguageName(machine, entry.name) || reachesAnywhere(headOf(entry.text))) return std::nullopt;
			change.removed.push_back(entry.name);
		}
	return change;
}

/// @brief Defines again the entries marked, and takes away those that went, in a machine that loaded the program.
/// @param machine The machine.
/// @param entries The later version's entries.
/// @param again Which to define again.
/// @param removed The names that went.
/// @throws LispError If one doesn't read, define or compile: the machine is then half changed.
void defineAgain(Machine& machine, const std::vector<Entry>& entries, const std::vector<bool>& again,
                 const std::vector<std::string>& removed) {
	for(const std::string& name : removed) machine.forgetDefinition(name);
	for(std::size_t at = 0; at < entries.size(); ++at) {
		if(!again[at]) continue;
		const Entry& entry = entries[at];
		machine.heap.setBudget(nullptr); // reading is bounded by the text's size, as load()'s
		const std::optional<Value> read = readEntry(machine.heap, entry.text, entry.file, entry.line, entry.column);
		machine.heap.setBudget(&machine.budget());
		if(!read || definitionAddress(*read).second != entry.name)
			throw LispError(entry.file, entry.line, entry.column, entry.name, "an entry's text defines its own name");
		static_cast<void>(machine.define(*read, entry.author, machine.authorSet(entry.author), false));
	}
	machine.compilePending(true);
	machine.workOutDefinitions();
	std::vector<std::string> order; // load()'s: the macros, then the rest, each in the order of the entries
	for(const bool macros : {true, false})
		for(const Entry& entry : entries)
			if((machine.definition(entry.name)->kind == "defmacro") == macros) order.push_back(entry.name);
	machine.orderDefinitions(order);
}

/// @brief A machine's budget, as it was, back however a change ends.
class RestoredBudget {
public:
	/// @brief Keeps the budget, and gives the machine another. @param machine The machine. @param budget The other.
	RestoredBudget(Machine& machine, const Budget& budget) : machine_(machine), saved_(machine.budget()) {
		machine.budget() = budget;
	}
	RestoredBudget(const RestoredBudget&) = delete;
	RestoredBudget& operator=(const RestoredBudget&) = delete;
	RestoredBudget(RestoredBudget&&) = delete;
	RestoredBudget& operator=(RestoredBudget&&) = delete;
	/// @brief Puts it back.
	~RestoredBudget() {
		machine_.heap.setBudget(&machine_.budget());
		machine_.budget() = saved_;
	}

private:
	Machine& machine_; ///< The machine.
	Budget saved_;     ///< Its budget.
};

} // namespace

bool Program::redefine(Interpreter& interpreter, const Program& before, const BuildRecord& built,
                       const Limits& limits, std::size_t* bytesLeft) const {
	Machine& machine = interpreter.machine();
	if(!namespaces_.empty() || !before.namespaces_.empty() || !machine.namespaces().programless() ||
	   machine.definitions().size() != before.entries_.size())
		return false;
	std::optional<InPlace> change = changeInPlace(machine, entries_, names_, before.entries_, before.names_);
	if(!change) return false;
	if(change->changed.empty() && change->removed.empty()) return true;
	std::vector<std::string> seeds = change->changed;
	seeds.insert(seeds.end(), change->removed.begin(), change->removed.end());
	const std::vector<bool> again = redefined(built, std::move(seeds), names_, entries_, change->removed);
	const RestoredBudget restore(machine, limits.budget);
	return leavingAsLispError(machine, [&] {
		defineAgain(machine, entries_, again, change->removed);
		if(bytesLeft) *bytesLeft = machine.budget().bytes;
		++statistics.redefined;
		statistics.definedAgain += static_cast<std::size_t>(std::ranges::count(again, true));
		return true;
	});
}
} // namespace affresco::lisp
