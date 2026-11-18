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
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

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

/// @brief Marks the entries that run: those that changed, and those that use one that changed or went, over and over.
/// @param previous The record.
/// @param entries The entries.
/// @param names Each entry's place, by its name.
/// @param plan Gets them.
void markAffected(const BuildRecord& previous, const std::vector<Entry>& entries,
                  const std::map<std::string, std::size_t>& names, PartPlan& plan) {
	std::vector<std::string_view> waiting(plan.removed.begin(), plan.removed.end());
	for(std::size_t at = 0; at < entries.size(); ++at)
		if(plan.changed[at]) {
			plan.affected[at] = true;
			waiting.emplace_back(entries[at].name);
		}
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
		const bool macro = !plan.changed[at] && previous.definitions[*plan.before[at]]->kind == "defmacro";
		if(plan.affected[at] || macro || shadows(entries[at].name)) load(at);
	}
	const auto use = [&](std::string_view name) {
		if(const auto found = names.find(std::string(name)); found != names.end()) load(found->second);
	};
	while(!waiting.empty()) {
		const std::size_t at = waiting.back();
		waiting.pop_back();
		if(plan.changed[at]) forEachWord(entries[at].text, use);
		else
			for(const std::string& name : previous.definitions[*plan.before[at]]->uses) use(name);
	}
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
	for(std::size_t at = 0; at < entries.size(); ++at) {
		const Entry& entry = entries[at];
		const auto found = previous.index.find(entry.name);
		const DefinitionBuilt* was = found == previous.index.end() ? nullptr : previous.definitions[found->second].get();
		if(was) plan.before[at] = found->second;
		if(was && was->text == entry.text && was->file == entry.file && was->author == entry.author) continue;
		plan.changed[at] = true;
		const std::string kind = headOf(entry.text);
		if(reachesAnywhere(kind) || (was && reachesAnywhere(was->kind))) return std::nullopt;
		if(was && was->kind != kind && previous.extensions) return std::nullopt; // a hook's function may be no more
	}
	for(const auto& definition : previous.definitions)
		if(!names.contains(definition->name)) {
			if(reachesAnywhere(definition->kind) || previous.extensions) return std::nullopt;
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
                                 const std::vector<std::pair<std::string, Primitive>>& primitives,
                                 const PartPlan& plan, const Limits& limits) {
	Program program;
	for(std::size_t at = 0; at < entries.size(); ++at)
		if(plan.loaded[at]) program.addEntryUnread(entries[at]);
	Interpreter interpreter(limits);
	interpreter.setPure(true);
	for(const auto& [name, primitive] : primitives) interpreter.definePrimitive(name, primitive);
	program.load(interpreter);
	Machine& machine = interpreter.machine();
	return leavingAsLispError(machine, [&]() -> std::optional<PartBuilt> {
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
		each[at] = part.built[at] ? std::make_shared<const DefinitionBuilt>(std::move(*part.built[at]))
		                          : previous.definitions[*plan.before[at]];
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
		built.extension = Extension{definition.kind,   definition.name,   definition.targets,
		                            definition.doc,    definition.author, machine.authorNames(definition.authors)};
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
	std::map<std::string_view, std::size_t> order;
	for(const auto& definition : machine.definitions()) order.emplace(definition->name, order.size());
	const auto place = [&](const UndefinedName& item) {
		const auto at = order.find(item.definition);
		return at == order.end() ? order.size() : at->second;
	};
	std::ranges::stable_sort(found, {}, place);
}

void addPlacesGiven(const DefinitionBuilt& definition, std::vector<std::string>& places) {
	for(const Gift& gift : definition.gifts)
		if(gift.kind == Gift::Kind::Member || gift.kind == Gift::Kind::Default)
			places.push_back("/" + pointerPart(gift.section) + "/" + pointerPart(gift.name));
		else
			places.push_back("/" + pointerPart(gift.name));
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

std::optional<json> Program::buildInPart(const BuildRecord& previous, const Limits& limits) {
	if(!previous.inPart || recording_ || !namespaces_.empty() || previous.spent.operations >= limits.budget.operations ||
	   previous.spent.bytes >= limits.budget.bytes || previous.macroSpent >= limits.macroOperations)
		return std::nullopt;
	const auto shadows = [this](std::string_view name) {
		static const std::set<std::string, std::less<>> language = [] {
			const std::vector<std::string> all = Interpreter().machine().globalNames();
			return std::set<std::string, std::less<>>(all.begin(), all.end());
		}();
		return language.contains(name) ||
		       std::ranges::any_of(primitives_, [name](const auto& primitive) { return primitive.first == name; });
	};
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
		for(const auto& definition : record->definitions)
			if(const std::size_t entry = names_.at(definition->name); plan->affected[entry]) {
				addPlacesGiven(*definition, record->touched);
				if(plan->before[entry]) addPlacesGiven(*previous.definitions[*plan->before[entry]], record->touched);
			}
		for(const std::string& name : plan->removed)
			addPlacesGiven(*previous.definitions[previous.index.find(name)->second], record->touched);
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
