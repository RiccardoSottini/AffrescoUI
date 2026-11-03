/// @file
/// @brief Programs (Program.hpp): the split into entries, loading, and the pure build into a tree.

#include <affresco/lisp/Program.hpp>

#include "Compiler.hpp"
#include "Definitions.hpp"
#include "Machine.hpp"
#include "Reader.hpp"
#include "TopLevel.hpp"

#include <affresco/notation/Lexer.hpp>

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace affresco::lisp {

namespace {

/// @brief Where a form's text ends: past the comment that ends its line.
/// @param text The text.
/// @param end Where the form ends.
/// @return The end.
std::size_t pastSameLineComment(std::string_view text, std::size_t end) {
	std::size_t at = end;
	while(at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == ',')) ++at;
	if(at >= text.size() || text[at] != ';') return end;
	const std::size_t line = text.find('\n', at);
	at = line == std::string_view::npos ? text.size() : line;
	while(at > end && (text[at - 1] == '\r' || text[at - 1] == ' ' || text[at - 1] == '\t')) --at;
	return at;
}

/// @brief Where the next entry's text starts: the first character that isn't a space.
/// @param text The text.
/// @param at Where to look from.
/// @return The start.
std::size_t pastSpaces(std::string_view text, std::size_t at) {
	while(at < text.size() && std::string_view(" \t\r\n,").contains(text[at])) ++at;
	return at;
}

/// @brief The last character of a text that isn't a space, plus one.
/// @param text The text.
/// @return The end.
std::size_t trimmedEnd(std::string_view text) {
	std::size_t at = text.size();
	while(at > 0 && std::string_view(" \t\r\n,").contains(text[at - 1])) --at;
	return at;
}

/// @brief Advances a line and a column over a text.
/// @param text The text.
/// @param line The line.
/// @param column The column.
void advance(std::string_view text, std::size_t& line, std::size_t& column) {
	for(const char c : text) {
		if(c == '\n') {
			++line;
			column = 1;
		} else if((static_cast<unsigned char>(c) & 0xC0U) != 0x80U) {
			++column; // a UTF-8 continuation byte isn't a character
		}
	}
}

/// @brief A section of the tree: its members by name, so that the order of definitions doesn't change it.
using Section = std::map<std::string, json>;

/// @brief Adds a section to the tree, if it has anything; `defscreen main` may have given some of it.
/// @param tree The tree.
/// @param key The section's key.
/// @param section The section.
/// @throws Problem If `defscreen main` gave something other than an object there, or a member both give.
void addSection(json& tree, const char* key, Section& section) {
	if(section.empty()) return;
	json object = tree.contains(key) ? std::move(tree[key]) : json::object();
	if(!object.is_object())
		throw Problem(std::string("defscreen main gives \"") + key + "\" as something other than a map");
	for(auto& [name, value] : section) {
		if(object.contains(name))
			throw Problem("\"" + name + "\" of \"" + key + "\" is given by defscreen main and by a definition");
		object[name] = std::move(value);
	}
	tree[key] = std::move(object);
}

/// @brief A command's action: its options, its docstring as the description, and the step that runs its function.
/// @param options What it was given.
/// @param name Its name, its function's too.
/// @param doc Its docstring.
/// @param parameters Its function's parameters, what a caller by name gives.
/// @return The action.
json commandAction(json options, const std::string& name, const std::string& doc,
                   const std::vector<std::string>& parameters) {
	if(!doc.empty() && !options.contains("description")) options["description"] = doc;
	if(!options.contains("do")) options["do"] = json::array({{{"lisp", name}}});
	if(!parameters.empty() && !options.contains("arguments")) options["arguments"] = parameters;
	return options;
}

/// @brief Refuses a hook or advice whose function the program doesn't define: a host's primitive is no hook, so a step
/// can't be run this way, and neither can what isn't there yet.
/// @param machine The machine.
/// @param definition The hook or advice.
/// @throws Problem If it isn't a function of the program.
void checkExtensionFunction(const Machine& machine, const Definition& definition) {
	if(definition.kind == "remove-hook" || definition.targets.empty()) return;
	const Definition* function = machine.definition(definition.targets.back());
	if(!function || (function->kind != "defn" && function->kind != "defcommand"))
		throw Problem("\"" + definition.name + "\" names \"" + notation::Lexer::excerpt(definition.targets.back()) +
		              "\", which is no function of the program");
}

/// @brief Adds the names a piece of compiled code uses, with the functions inside it.
/// @brief What a definition's code uses by name, and how.
struct Uses {
	std::set<std::string> names;                         ///< Every name.
	std::map<std::string, std::set<std::string>> called; ///< Those called with a text written out first: the texts.
	std::set<std::string> otherwise;                     ///< Those used any other way.
};

/// @brief Adds what a piece of code uses by name, and what the functions inside it use.
/// @param code The code, or nullptr.
/// @param uses Gets them.
void addNamesUsed(const Code* code, Uses& uses) {
	if(!code) return;
	std::vector<const Code*> waiting = {code};
	while(!waiting.empty()) {
		const Code* next = waiting.back();
		waiting.pop_back();
		std::vector<bool> named(next->globals.size(), false);
		for(const NamedCall& call : next->named) {
			named[call.global] = true;
			uses.called[next->globals[call.global]->name].insert(call.text);
		}
		for(std::size_t at = 0; at < next->globals.size(); ++at) {
			uses.names.insert(next->globals[at]->name);
			if(!named[at]) uses.otherwise.insert(next->globals[at]->name);
		}
		waiting.insert(waiting.end(), next->functions.begin(), next->functions.end());
	}
}

/// @brief What a definition's code uses by name.
/// @param definition The definition.
/// @return The names, and how.
Uses namesUsedBy(const Definition& definition) {
	Uses uses;
	addNamesUsed(definition.data, uses);
	if(const Global* global = definition.global) {
		addNamesUsed(global->thunk, uses);
		if(global->value.kind() == Kind::Function)
			addNamesUsed(static_cast<const ClosureObject*>(global->value.pointer())->code, uses);
	}
	return uses;
}

/// @brief The names a definition calls with a text written out first, those texts, and which it also uses otherwise.
/// @param uses What it uses.
/// @return Them.
CallsByName namedCallsOf(const Uses& uses) {
	CallsByName named;
	for(const auto& [name, texts] : uses.called) {
		named.texts.emplace(name, std::vector<std::string>(texts.begin(), texts.end()));
		if(uses.otherwise.contains(name)) named.otherwise.insert(name);
	}
	return named;
}

/// @brief A setting: its `type`, its `default`, then its other options, and its docstring as the description.
/// @param options What it was given.
/// @param value Its value.
/// @param doc Its docstring.
/// @return The setting.
json customSetting(const json& options, json value, const std::string& doc) {
	json setting = json::object();
	if(options.contains("type")) setting["type"] = options["type"];
	setting["default"] = std::move(value);
	for(const auto& [key, member] : options.items()) setting[key] = member;
	if(!doc.empty() && !setting.contains("description")) setting["description"] = doc;
	return setting;
}

/// @brief What a build gathers: the tree's parts, each in its order.
struct BuiltParts {
	json main = json::object();              ///< `defscreen main`'s properties and `root`.
	Section screens;                         ///< The other screens.
	Section actions;                         ///< The commands.
	Section keys;                            ///< The shortcuts.
	Section settings;                        ///< The settings.
	std::map<std::string, Section> sections; ///< What `defstate`, `defcomputed`... add to the other sections.
};

/// @brief Adds a member to a section: one defined twice, as `deftokens` can, is an error naming it.
/// @param section The section.
/// @param kind The form that adds it.
/// @param name The member.
/// @param value Its value.
/// @throws Problem If the section has it already.
void addMember(Section& section, std::string_view kind, const std::string& name, json value) {
	if(!section.emplace(name, std::move(value)).second)
		throw Problem("\"" + notation::Lexer::excerpt(name) + "\" is given twice, the second time by " +
		              std::string(kind));
}

/// @brief Builds the part of a form that defines a member of a section (sectionOfForm()).
/// @param machine The machine.
/// @param definition The definition.
/// @param data What it built.
/// @param parts Gets it.
void buildSectionMember(Machine& machine, const Definition& definition, const Value& data, BuiltParts& parts) {
	const std::string& name = definition.name;
	const std::string member = name.substr(name.find('.') + 1);
	const std::string section(sectionOfForm(definition.kind));
	if(definition.kind == "defstate") {
		addMember(parts.sections["state"], definition.kind, member, valueToTree(machine, data.at(0), name));
		json options = valueToTree(machine, data.at(1), name);
		if(options.contains("type")) addMember(parts.sections["stateTypes"], definition.kind, member, options["type"]);
	} else if(definition.kind == "deftokens") {
		json tokens = valueToTree(machine, data, name);
		if(!tokens.is_object()) throw Problem("deftokens takes a map of tokens");
		for(auto& [token, value] : tokens.items())
			addMember(parts.sections[section], definition.kind, token, std::move(value));
	} else {
		addMember(parts.sections[section], definition.kind, member, valueToTree(machine, data, name));
	}
}

/// @brief Builds one definition's part of the tree.
/// @param machine The machine.
/// @param definition The definition.
/// @param parts Gets it.
void buildDefinition(Machine& machine, const Definition& definition, BuiltParts& parts) {
	if(!definition.data) return;
	const Value data = machine.runThunk(definition.data);
	const std::string& name = definition.name;
	if(definition.kind == "defkey") {
		parts.keys[name.substr(std::string_view("defkey ").size())] = valueToTree(machine, data, name);
	} else if(definition.kind == "defcommand") {
		parts.actions[name] = commandAction(valueToTree(machine, data, name), name, definition.doc,
		                                    Interpreter::parametersOf(definition.global->value));
	} else if(definition.kind == "defcustom") {
		const Value value = machine.force(*definition.global);
		parts.settings[name] =
		    customSetting(valueToTree(machine, data, name), valueToTree(machine, value, name), definition.doc);
	} else if(!sectionOfForm(definition.kind).empty()) {
		buildSectionMember(machine, definition, data, parts);
	} else if(name == "main") {
		// A named tree: a loop over a temporary's items() reads it after it is gone
		json settings = valueToTree(machine, data.at(0), name);
		for(auto& [key, member] : settings.items()) parts.main[key] = std::move(member);
		parts.main["root"] = valueToTree(machine, data.at(1), name);
	} else {
		json screen = valueToTree(machine, data.at(1), name);
		parts.screens[name] = std::move(screen);
	}
}

} // namespace

std::optional<ReadForm> readUncharged(Heap& heap, Reader& reader, Budget& budget) {
	heap.setBudget(nullptr);
	try {
		std::optional<ReadForm> read = reader.next();
		heap.setBudget(&budget);
		return read;
	} catch(...) {
		heap.setBudget(&budget);
		throw;
	}
}

namespace {

/// @brief Counts a level of top-level forms inside each other, for as long as it lasts.
class TopLevelDepth {
public:
	/// @brief Enters a level.
	/// @param machine The machine.
	/// @param where The closest place known: what a macro gives may have none.
	explicit TopLevelDepth(Machine& machine, Place where) : machine_(machine) {
		if(++machine_.compileDepth() <= maxCompileDepth) return;
		--machine_.compileDepth();
		throw LispError(where.file ? *where.file : "", where.line, where.column, "", "macros nested too deeply", true);
	}
	~TopLevelDepth() { --machine_.compileDepth(); }
	TopLevelDepth(const TopLevelDepth&) = delete;
	TopLevelDepth& operator=(const TopLevelDepth&) = delete;

private:
	Machine& machine_; ///< See the constructor.
};

/// @brief A form's items after its head.
/// @param form The form.
/// @return The items.
std::vector<Value> argumentsOf(const Value& form) {
	std::vector<Value> arguments;
	arguments.reserve(form.count() - 1);
	for(std::size_t at = 1; at < form.count(); ++at) arguments.push_back(form.at(at));
	return arguments;
}

/// @brief The macro a form calls, if it does.
/// @param machine The machine.
/// @param form The form.
/// @return The macro's global, or nullptr.
Global* macroOf(const Machine& machine, const Value& form) {
	const std::string_view head = headWord(form);
	Global* global = head.empty() ? nullptr : machine.findGlobal(head);
	return global && global->macro ? global : nullptr;
}

} // namespace

Value evalTopLevel(Machine& machine, const Value& form, const std::string& author, Place where) {
	const Place place = form.place().line != 0 ? form.place() : where;
	const TopLevelDepth depth(machine, place);
	if(Machine::isDefinitionForm(form)) {
		const Definition& definition = machine.define(form, author, nullptr, false);
		machine.compilePending();
		return machine.heap.symbol(definition.name);
	}
	if(headWord(form) == "do") {
		Value result;
		for(std::size_t at = 1; at < form.count(); ++at) result = evalTopLevel(machine, form.at(at), author, place);
		return result;
	}
	if(Global* macro = macroOf(machine, form))
		return evalTopLevel(machine, machine.expand(*macro, argumentsOf(form), place), author, place);
	CompileContext context;
	context.author = author;
	return machine.runThunk(Compiler(machine, std::move(context)).thunk(form));
}

void defineTopLevel(Machine& machine, const Value& form, const std::string& author, const AuthorSet* authors,
                    Place where) {
	const Place place = form.place().line != 0 ? form.place() : where;
	const TopLevelDepth depth(machine, place);
	if(Machine::isDefinitionForm(form)) {
		static_cast<void>(machine.define(form, author, authors, true));
	} else if(headWord(form) == "do") {
		for(std::size_t at = 1; at < form.count(); ++at) defineTopLevel(machine, form.at(at), author, authors, place);
	} else if(Global* macro = macroOf(machine, form)) {
		const Value expanded = machine.expand(*macro, argumentsOf(form), place);
		const auto* closure = static_cast<const ClosureObject*>(macro->value.pointer());
		defineTopLevel(machine, expanded, author, machine.unite(authors, closure->code->authors), place);
	} else {
		const std::string_view head = headWord(form);
		throw LispError(place.file ? *place.file : "", place.line, place.column, "",
		                "a program's top level holds definitions" +
		                    (head.empty()
		                         ? std::string()
								 : ": \"" + notation::Lexer::excerpt(head) + "\" is neither a definition nor a macro"));
	}
}

std::vector<Entry> splitEntries(std::string_view text, const std::string& file, const std::string& author) {
	Heap heap;
	Reader reader(heap, text, file);
	std::vector<Entry> entries;
	std::size_t start = pastSpaces(text, text.starts_with("\xEF\xBB\xBF") ? 3 : 0);
	std::size_t line = 1;
	std::size_t column = 1;
	std::size_t lastEnd = 0;
	advance(text.substr(0, start), line, column);
	while(const auto read = reader.next()) {
		auto [kind, name] = definitionAddress(read->form);
		const std::size_t end = pastSameLineComment(text, read->end);
		lastEnd = end;
		Entry entry{file, std::move(name), std::move(kind), std::string(text.substr(start, end - start)), author,
		            line, column};
		entries.push_back(std::move(entry));
		const std::size_t next = pastSpaces(text, end);
		advance(text.substr(start, next - start), line, column);
		start = next;
	}
	if(!entries.empty() && lastEnd < trimmedEnd(text)) // comments after the last form are the last entry's
		entries.back().text += std::string(text.substr(lastEnd, trimmedEnd(text) - lastEnd));
	return entries;
}

bool isProgram(std::string_view text) {
	Heap heap;
	Reader reader(heap, text, "");
	const auto first = reader.next();
	if(!first) return false;
	return reader.next().has_value() || isDefinitionWord(headWord(first->form));
}

namespace {

/// @brief The place of the first `~`, or a quote, in a text.
/// @param text The text.
/// @return The place as an answer of needsRunning(), or std::nullopt.
std::optional<std::string> firstUnquote(std::string_view text) {
	notation::Lexer lexer(text, true);
	for(notation::Token token = lexer.next(); token.kind != notation::TokenKind::End; token = lexer.next())
		if(token.kind >= notation::TokenKind::Quote && token.kind <= notation::TokenKind::Splice)
			return "the " + std::string(token.text) + " at " + lexer.placeOf(token.offset);
	return std::nullopt;
}

} // namespace

namespace {

/// @brief Whether a text is a program, for a text that may not read.
/// @param text The text.
/// @return true if it reads as one.
bool isProgramQuietly(std::string_view text) {
	try {
		return isProgram(text);
	} catch(const LispError&) {
		return false;
	}
}

} // namespace

std::optional<std::string> needsRunning(std::string_view text) {
	try {
		if(!isProgram(text)) return firstUnquote(text);
		const Entry first = splitEntries(text, "").front();
		return "\"" + notation::Lexer::excerpt(first.name) + "\", at line " + std::to_string(first.line);
	} catch(const LispError& error) { // a program with a form that names nothing; or text the notation's reader reports
		return error.line() != 0 && isProgramQuietly(text)
		           ? std::optional("the form at line " + std::to_string(error.line()))
				   : std::nullopt;
	} catch(const DocumentError&) { // text the notation's reader reports
		return std::nullopt;
	}
}

json buildText(std::string_view text, const std::string& file, const Limits& limits) {
	if(isProgram(text)) {
		Program program;
		program.addFile(text, file);
		return program.build(limits);
	}
	Limits grown = limits;
	grown.budget.operations += text.size(); // a data file costs what its size costs, whatever its size
	grown.budget.bytes += text.size() * 32;
	Interpreter interpreter(grown);
	interpreter.setPure(true);
	Machine& machine = interpreter.machine();
	return leavingAsLispError(machine, [&] {
		Reader reader(machine.heap, text, file);
		const auto read = readUncharged(machine.heap, reader, machine.budget());
		if(!read) {
			const notation::Lexer lexer(text, true);
			const auto [line, column] = lexer.lineAndColumn(text.size());
			throw LispError(file, line, column, "", "expected a value, found the end of the text");
		}
		CompileContext context;
		const Code* code = Compiler(machine, std::move(context)).templateThunk(read->form);
		return valueToTree(machine, machine.runThunk(code), "");
	});
}

void Program::addFile(std::string_view text, const std::string& file, const std::string& author) {
	for(Entry& entry : splitEntries(text, file, author)) addEntry(std::move(entry));
}

void Program::addEntry(Entry entry) {
	Heap heap;
	Reader reader(heap, entry.text, entry.file, entry.line, entry.column);
	const auto read = reader.next();
	if(!read || reader.next())
		throw LispError(entry.file, entry.line, entry.column, entry.name, "an entry's text is one form");
	auto [kind, address] = definitionAddress(read->form);
	if(address != entry.name)
		throw LispError(entry.file, entry.line, entry.column, entry.name, "an entry's text defines its own name");
	if(const auto [known, added] = names_.try_emplace(entry.name, entries_.size()); !added) {
		const Entry& other = entries_[known->second];
		throw LispError(entry.file, entry.line, entry.column, entry.name,
		                "\"" + notation::Lexer::excerpt(entry.name) + "\" is defined twice: at " + other.file +
		                    ": line " + std::to_string(other.line) + " and at " + entry.file + ": line " +
		                    std::to_string(entry.line));
	}
	entry.kind = std::move(kind);
	entries_.push_back(std::move(entry));
}

void Program::definePure(const std::string& name, Primitive primitive) {
	if(!primitive.pure || !primitive.capability.empty())
		throw std::invalid_argument("a primitive build code may call is pure and needs no capability");
	primitives_.emplace_back(name, std::move(primitive));
}

void Program::load(Interpreter& interpreter) const {
	Machine& machine = interpreter.machine();
	static_cast<void>(leavingAsLispError(machine, [&] {
		std::vector<std::pair<Value, const Entry*>> forms;
		forms.reserve(entries_.size());
		for(const Entry& entry : entries_) {
			Reader reader(machine.heap, entry.text, entry.file, entry.line, entry.column);
			const auto read = readUncharged(machine.heap, reader, machine.budget());
			if(!read || readUncharged(machine.heap, reader, machine.budget()))
				throw LispError(entry.file, entry.line, entry.column, entry.name, "an entry's text is one form");
			if(definitionAddress(read->form).second != entry.name)
				throw LispError(entry.file, entry.line, entry.column, entry.name,
				                "an entry's text defines its own name");
			forms.emplace_back(read->form, &entry);
		}
		const auto authorsOf = [&machine](const Entry& entry) { return machine.authorSet(entry.author); };
		for(const auto& [form, entry] : forms)
			if(headWord(form) == "defmacro")
				static_cast<void>(machine.define(form, entry->author, authorsOf(*entry), true));
		for(const auto& [form, entry] : forms)
			if(headWord(form) != "defmacro")
				defineTopLevel(machine, form, entry->author, authorsOf(*entry), form.place());
		machine.compilePending();
		return true;
	}));
}

json Program::build(const Limits& limits) {
	Interpreter interpreter(limits);
	interpreter.setPure(true);
	for(const auto& [name, primitive] : primitives_) interpreter.definePrimitive(name, primitive);
	load(interpreter);
	Machine& machine = interpreter.machine();
	extensions_.clear();
	undefined_.clear();
	used_.clear();
	named_.clear();
	return leavingAsLispError(machine, [&] {
		machine.workOutDefinitions(); // a value is worked out here, purely: nothing later can change one
		BuiltParts parts;
		for(const auto& definition : machine.definitions()) {
			if(isExtensionWord(definition->kind)) {
				extensions_.push_back({definition->kind, definition->name, definition->targets, definition->doc,
				                       definition->author, machine.authorNames(definition->authors)});
				checkExtensionFunction(machine, *definition);
			}
			buildDefinition(machine, *definition, parts);
			const Uses uses = namesUsedBy(*definition);
			used_[definition->name] = {uses.names.begin(), uses.names.end()};
			if(CallsByName named = namedCallsOf(uses); !named.texts.empty())
				named_[definition->name] = std::move(named);
		}
		for(const GlobalUse& use : machine.uses())
			if(use.global->state == Global::State::Undefined)
				undefined_.push_back({use.global->name, use.place.file ? *use.place.file : "", use.place.line,
				                      use.place.column, use.definition});
		json tree = {{"format", "affrescoui-screen"}, {"version", 2}};
		for(auto& [key, member] : parts.main.items()) tree[key] = std::move(member);
		addSection(tree, "screens", parts.screens);
		addSection(tree, "actions", parts.actions);
		addSection(tree, "keys", parts.keys);
		addSection(tree, "settings", parts.settings);
		for(auto& [key, section] : parts.sections) addSection(tree, key.c_str(), section);
		return tree;
	});
}

} // namespace affresco::lisp
