/// @file
/// @brief Programs (Program.hpp): the split into entries, loading, and the pure build into a tree.

#include <affresco/lisp/Program.hpp>

#include "Compiler.hpp"
#include "Definitions.hpp"
#include "Library.hpp"
#include "Machine.hpp"
#include "Namespaces.hpp"
#include "Reader.hpp"
#include "Rebuild.hpp"
#include "TopLevel.hpp"
#include "TreeParts.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

std::size_t pastSameLineComment(std::string_view text, std::size_t end) {
	std::size_t at = end;
	while(at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == ',')) ++at;
	if(at >= text.size() || text[at] != ';') return end;
	const std::size_t line = text.find('\n', at);
	at = line == std::string_view::npos ? text.size() : line;
	while(at > end && (text[at - 1] == '\r' || text[at - 1] == ' ' || text[at - 1] == '\t')) --at;
	return at;
}

namespace {

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

/// @brief Expands a macro's call at the top level, where no compiler sees it: a library macro is refused where the
/// program defines a word it calls (Compiler::checkUses() does the rest inside code).
/// @param machine The machine.
/// @param macro The macro.
/// @param form The call.
/// @param place Where it is.
/// @return The expansion.
/// @throws LispError If the program defines a word the macro calls.
Value expandAtTop(Machine& machine, Global& macro, const Value& form, Place place) {
	Value expanded = machine.expand(macro, argumentsOf(form), place);
	if(macro.library)
		if(const std::string problem = definedUse(machine, macro); !problem.empty())
			throw LispError(place.file ? *place.file : "", place.line, place.column, "", problem);
	return expanded;
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
		return evalTopLevel(machine, expandAtTop(machine, *macro, form, place), author, place);
	CompileContext context;
	context.author = author;
	context.aliases = machine.namespaces().fileAliases(place.file);
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
		const Value expanded = expandAtTop(machine, *macro, form, place);
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
	if(const std::optional<bool> byTokens = isProgramByTokens(text)) return *byTokens; // no form made
	Heap heap;
	Reader reader(heap, text, "");
	const auto first = reader.next();
	if(!first) return false;
	const std::string_view head = headWord(first->form);
	return reader.next().has_value() || isDefinitionWord(head) || head == "ns";
}

std::optional<std::string> firstUnquote(std::string_view text) {
	notation::Lexer lexer(text, true);
	for(notation::Token token = lexer.next(); token.kind != notation::TokenKind::End; token = lexer.next())
		if(token.kind >= notation::TokenKind::Quote && token.kind <= notation::TokenKind::Splice)
			return "the " + std::string(token.text) + " at " + lexer.placeOf(token.offset);
	return std::nullopt;
}

namespace {

/// The forms at the top level of a text, counted from its tokens as they come, up to two.
class TopForms {
public:
	/// @brief Takes the next token.
	/// @param token The token.
	/// @return true once a second form starts, or a bracket closes nothing after a definition (the program's reader
	/// says where); std::nullopt for any other bracket that closes nothing (the reader says so); else false.
	std::optional<bool> take(const notation::Token& token) {
		using notation::TokenKind;
		if(std::exchange(wantHead_, false) && token.kind == TokenKind::Word) head_ = token.text;
		if(token.kind >= TokenKind::Quote && token.kind <= TokenKind::Splice) return prefix();
		if(token.kind == TokenKind::OpenNode || token.kind == TokenKind::OpenList || token.kind == TokenKind::OpenMap)
			return open(token.kind);
		if(token.kind == TokenKind::CloseNode || token.kind == TokenKind::CloseList ||
		   token.kind == TokenKind::CloseMap) {
			if(depth_ == 0) return startsWithDefinition() ? std::optional(true) : std::nullopt;
			--depth_;
			return false;
		}
		return startsForm();
	}

	/// @brief Whether the text, all its tokens taken, is one definition form.
	/// @return std::nullopt if a bracket is left open (the reader says so), else whether it is. A definition left open
	/// is a program, whose reader says where.
	[[nodiscard]] std::optional<bool> oneDefinition() const {
		if(depth_ != 0) return startsWithDefinition() ? std::optional(true) : std::nullopt;
		return forms_ == 1 && startsWithDefinition(); // or a file with only its ns form
	}

	/// @brief Whether the first form is a definition or an ns form.
	/// @return Whether it is.
	[[nodiscard]] bool startsWithDefinition() const { return isDefinitionWord(head_) || head_ == "ns"; }

private:
	/// @brief Takes a 'x, `x or ~x: at the top, the form it prefixes is the same form.
	/// @return true once a second form starts.
	bool prefix() {
		if(depth_ == 0 && !prefixed_ && ++forms_ == 2) return true;
		if(depth_ == 0) prefixed_ = true;
		return false;
	}

	/// @brief Takes an opening bracket.
	/// @param kind Which.
	/// @return true once a second form starts.
	bool open(notation::TokenKind kind) {
		const bool first = depth_ == 0 && !prefixed_ && forms_ == 0;
		if(startsForm()) return true;
		wantHead_ = first && kind == notation::TokenKind::OpenNode;
		++depth_;
		return false;
	}

	/// @brief Counts a form starting at the top level.
	/// @return true once there are two.
	bool startsForm() {
		if(depth_ != 0) return false;
		const bool second = !prefixed_ && ++forms_ == 2;
		prefixed_ = false;
		return second;
	}

	std::size_t depth_ = 0; ///< How deep in brackets the tokens are.
	std::size_t forms_ = 0; ///< The forms started at the top level.
	bool prefixed_ = false; ///< After 'x, `x or ~x at the top: the form it prefixes is the same form.
	bool wantHead_ = false; ///< The token after the first form's own "(".
	std::string_view head_; ///< The first form's head.
};

} // namespace

std::optional<bool> isProgramByTokens(std::string_view text) {
	try {
		notation::Lexer lexer(text, true);
		TopForms top;
		for(notation::Token token = lexer.next(); token.kind != notation::TokenKind::End; token = lexer.next()) {
			const std::optional<bool> second = top.take(token);
			if(!second || *second) return second;
		}
		return top.oneDefinition();
	} catch(const DocumentError&) {
		return std::nullopt;
	}
}

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
	const std::optional<Value> read = readEntry(heap, entry.text, entry.file, entry.line, entry.column);
	if(!read) throw LispError(entry.file, entry.line, entry.column, entry.name, "an entry's text is one form");
	auto [kind, address] = definitionAddress(*read);
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

void Program::addEntryUnread(Entry entry) {
	if(const auto [known, added] = names_.try_emplace(entry.name, entries_.size()); !added) {
		const Entry& other = entries_[known->second];
		throw LispError(entry.file, entry.line, entry.column, entry.name,
		                "\"" + notation::Lexer::excerpt(entry.name) + "\" is defined twice: at " + other.file +
		                    ": line " + std::to_string(other.line) + " and at " + entry.file + ": line " +
		                    std::to_string(entry.line));
	}
	entries_.push_back(std::move(entry));
}

void Program::declareNamespace(const std::string& file, std::string text) {
	if(std::ranges::any_of(namespaces_, [&file](const auto& known) { return known.first == file; }))
		throw LispError(file, 1, 1, std::string(namespaceEntry), "a file has one ns form");
	namespaces_.emplace_back(file, std::move(text));
}

void Program::definePure(const std::string& name, Primitive primitive) {
	if(!primitive.pure || !primitive.capability.empty())
		throw std::invalid_argument("a primitive build code may call is pure and needs no capability");
	primitives_.emplace_back(name, std::move(primitive));
}

void Program::load(Interpreter& interpreter, bool everything) const {
	Machine& machine = interpreter.machine();
	static_cast<void>(leavingAsLispError(machine, [&] {
		std::vector<std::pair<Value, const Entry*>> forms;
		forms.reserve(entries_.size());
		for(const Entry& entry : entries_) {
			machine.heap.setBudget(nullptr); // reading is bounded by the text's size, so it spends nothing
			std::optional<Value> read;
			try {
				read = readEntry(machine.heap, entry.text, entry.file, entry.line, entry.column);
			} catch(...) {
				machine.heap.setBudget(&machine.budget());
				throw;
			}
			machine.heap.setBudget(&machine.budget());
			if(!read) throw LispError(entry.file, entry.line, entry.column, entry.name, "an entry's text is one form");
			if(definitionAddress(*read).second != entry.name)
				throw LispError(entry.file, entry.line, entry.column, entry.name,
				                "an entry's text defines its own name");
			forms.emplace_back(*read, &entry);
		}
		if(!namespaces_.empty() || !machine.namespaces().programless()) {
			std::map<std::string, std::set<std::string, std::less<>>, std::less<>> definedIn;
			for(const Entry& entry : entries_) definedIn[entry.file].insert(entry.name);
			applyNamespaces(machine, namespaces_, definedIn);
		}
		const auto authorsOf = [&machine](const Entry& entry) { return machine.authorSet(entry.author); };
		for(const auto& [form, entry] : forms)
			if(headWord(form) == "defmacro")
				static_cast<void>(machine.define(form, entry->author, authorsOf(*entry), true));
		for(const auto& [form, entry] : forms)
			if(headWord(form) != "defmacro")
				defineTopLevel(machine, form, entry->author, authorsOf(*entry), form.place());
		machine.compilePending(!everything);
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
	places_.clear();
	record_.reset();
	std::vector<NodeNote> notes; // what the tree builder notes, if this build records
	if(recording_) machine.nodeNotes = &notes;
	return leavingAsLispError(machine, [&] {
		machine.workOutDefinitions(); // a value is worked out here, purely: nothing later can change one
		BuiltParts parts;
		std::vector<DefinitionBuilt> kept;
		kept.reserve(machine.definitions().size());
		for(const auto& definition : machine.definitions()) {
			kept.push_back(recordDefinition(machine, *definition, &parts));
			if(kept.back().extension) extensions_.push_back(*kept.back().extension);
			used_[definition->name] = kept.back().uses;
			if(kept.back().named) named_[definition->name] = *kept.back().named;
		}
		keepTestsAndUndefined(machine);
		json tree = treeOf(parts, defaulted_);
		for(const auto& definition : machine.definitions()) // a key's starting value, as defvar's: a document's wins
			if(definition->kind == "defstate") defaulted_.emplace("state", definition->name.substr(6));
		keepNodePlaces(notes);
		keepRecord(machine, std::move(kept), limits);
		return tree;
	});
}

void Program::keepTestsAndUndefined(const Machine& machine) {
	truth_.clear();
	for(const auto& [path, place, definition] : machine.truthTests())
		truth_.push_back({path, place.file ? *place.file : "", place.line, place.column, definition});
	for(const GlobalUse& use : machine.uses())
		if(use.global->state == Global::State::Undefined)
			undefined_.push_back({use.global->name, use.place.file ? *use.place.file : "", use.place.line,
			                      use.place.column, use.definition});
	sortByDefinitionOrder(machine, truth_);
	sortByDefinitionOrder(machine, undefined_);
}

void Program::keepNodePlaces(const std::vector<NodeNote>& notes) {
	std::set<std::string_view> seen; // views into notes, which outlive it
	for(const NodeNote& note : notes)
		if(seen.insert(note.id).second)
			places_.push_back({note.id, note.form.file != nullptr ? *note.form.file : "", note.form.line,
			                   note.form.column, note.written.file != nullptr ? *note.written.file : "",
			                   note.written.line, note.written.column});
}

} // namespace affresco::lisp
