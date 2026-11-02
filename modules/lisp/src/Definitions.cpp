/// @file
/// @brief Globals and definitions (Machine.hpp): what the definition forms mean, and who wrote what.

#include "Compiler.hpp"
#include "Definitions.hpp"
#include "Machine.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace affresco::lisp {

namespace {

/// @brief The definition forms that hold code or data of their own, and how many items they need at least.
/// @param head A form's head.
/// @return The least items, or 0 if it is no such form.
std::size_t leastItems(std::string_view head) noexcept {
	if(head == "def" || head == "defscreen" || head == "defkey" || head == "defcustom" || head == "add-hook") return 3;
	if(head == "defn" || head == "defmacro" || head == "defcommand" || head == "advice") return 3;
	return 0;
}

/// @brief Reports a problem with a definition form.
/// @param form The form.
/// @param definition The definition's name, if known.
/// @param message What is wrong.
/// @throws LispError Always.
[[noreturn]] void failDefinition(const Value& form, const std::string& definition, const std::string& message) {
	const Place place = form.place();
	throw LispError(place.file ? *place.file : "", place.line, place.column, definition, message);
}

/// @brief Where a definition form's docstring would be: a string, not the last item, at an index.
/// @param form The form.
/// @param at The index.
/// @return Its text, or "" if there is none.
std::string docAt(const Value& form, std::size_t at) {
	if(at + 1 >= form.count() || form.at(at).kind() != Kind::String) return "";
	return std::string(form.at(at).text());
}

/// @brief Makes `(name items...)` with a form's place.
/// @param machine The machine.
/// @param name The head.
/// @param items The rest.
/// @param place The place.
/// @return The list.
Value listOf(Machine& machine, std::string_view name, std::vector<Value> items, Place place) {
	items.insert(items.begin(), machine.heap.symbol(name, place));
	return machine.heap.sequence(Kind::List, std::move(items), place);
}

/// @brief The `:key value` options of a definition form, as a map.
/// @param machine The machine.
/// @param form The form.
/// @param at Where they start; set to past them.
/// @param definition Its name, for an error.
/// @return The map.
Value optionsOf(Machine& machine, const Value& form, std::size_t& at, const std::string& definition) {
	std::vector<MapEntry> entries;
	while(at < form.count() && form.at(at).kind() == Kind::Keyword) {
		if(at + 1 >= form.count())
			failDefinition(form, definition,
			               "the key :" + notation::Lexer::excerpt(form.at(at).text()) + " has no value");
		const std::string_view key = form.at(at).text();
		if(std::ranges::find(entries, key, &MapEntry::key) != entries.end())
			failDefinition(form, definition, "the key :" + notation::Lexer::excerpt(key) + " twice");
		entries.push_back({key, form.at(at + 1)});
		at += 2;
	}
	return machine.heap.map(std::move(entries), form.place());
}

/// @brief Makes what compiles a closure of a function form into a global.
/// @param machine The machine.
/// @param global The global.
/// @param form The function form: `(any name doc? [params] body...)`.
/// @param context Who the code belongs to.
/// @return What compiles it.
std::function<void()> compileFunctionInto(Machine& machine, Global& global, const Value& form, CompileContext context) {
	return [&machine, &global, form, context = std::move(context)] {
		const std::uint64_t saved = std::exchange(machine.gensymCounter(), 0);
		const Code* code = Compiler(machine, context).function(form);
		machine.gensymCounter() = saved;
		auto* closure = machine.heap.make<ClosureObject>();
		closure->code = code;
		global.value = Value::object(Kind::Function, closure);
		global.state = Global::State::Ready;
	};
}

/// @brief Makes what compiles a `def`'s value into a global: worked out when it is first read.
/// @param machine The machine.
/// @param global The global.
/// @param form The value's form.
/// @param context Who the code belongs to.
/// @return What compiles it.
std::function<void()> compileValueInto(Machine& machine, Global& global, const Value& form, CompileContext context) {
	return [&machine, &global, form, context = std::move(context)] {
		const std::uint64_t saved = std::exchange(machine.gensymCounter(), 0);
		global.thunk = Compiler(machine, context).thunk(form);
		machine.gensymCounter() = saved;
		global.state = Global::State::Lazy;
	};
}

/// @brief Sets a global waiting to be compiled.
/// @param global The global.
/// @param pending What compiles it.
/// @param macro Whether it is a macro.
void setPending(Global& global, std::function<void()> pending, bool macro) {
	global.pending = std::move(pending);
	global.state = Global::State::Deferred;
	global.macro = macro;
	global.library = false;
}

} // namespace

bool isDefinitionWord(std::string_view word) noexcept { return leastItems(word) != 0; }

std::pair<std::string, std::string> definitionAddress(const Value& form) {
	const std::string head(headWord(form));
	if(head.empty() || form.count() < 2)
		failDefinition(form, "", "a program's top level holds definitions, each with a name");
	if(head == "defkey") {
		if(form.at(1).text().empty()) failDefinition(form, "", "defkey takes a key first");
		return {head, head + " " + std::string(form.at(1).text())};
	}
	if(head == "add-hook" || head == "advice") {
		std::string name = head;
		for(std::size_t at = 1; at < form.count(); ++at) {
			if(at >= 2 && !docAt(form, at).empty()) continue;
			if(form.at(at).text().empty()) failDefinition(form, "", head + " names what it joins, with words");
			name.append(" ").append(form.at(at).text());
		}
		return {head, name};
	}
	if(form.at(1).kind() != Kind::Symbol) failDefinition(form, "", head + " takes a name first");
	return {head, std::string(form.at(1).text())};
}

Global& Machine::global(std::string_view name) {
	if(Global* found = findGlobal(name)) return *found;
	Global& made = globalCells_.emplace_back();
	made.name = std::string(name);
	globals_.emplace(made.name, &made);
	return made;
}

Global* Machine::findGlobal(std::string_view name) const {
	const auto found = globals_.find(name);
	return found == globals_.end() ? nullptr : found->second;
}

void Machine::defineNative(std::string_view name, NativeFunction function, bool spread) {
	auto* native = heap.make<NativeObject>();
	native->name = std::string(name);
	native->function = function;
	native->spread = spread;
	Global& cell = global(name);
	cell.value = Value::object(Kind::Native, native);
	cell.state = Global::State::Ready;
	cell.library = true;
}

void Machine::definePrimitive(const std::string& name, Primitive primitive) {
	auto* native = heap.make<NativeObject>();
	native->name = name;
	native->host = std::move(primitive);
	Global& cell = global(name);
	cell.value = Value::object(Kind::Native, native);
	cell.state = Global::State::Ready;
	cell.macro = false;
	cell.library = false;
}

bool Machine::isDefinitionForm(const Value& form) noexcept { return isDefinitionWord(headWord(form)); }

Definition& Machine::define(const Value& form, const std::string& author, const AuthorSet* authors, bool strict) {
	const std::string head(headWord(form));
	auto [kind, name] = definitionAddress(form);
	const std::string shown = notation::Lexer::excerpt(name);
	if(form.count() < leastItems(head)) failDefinition(form, name, head + " is missing what it defines");
	if(head != "defkey" && head != "add-hook" && head != "advice") {
		if(Compiler::isSpecial(name))
			failDefinition(form, name, "\"" + shown + "\" is a form of the language, and can't be redefined");
		if(name.starts_with('%')) failDefinition(form, name, "a name starting with % is the language's own");
	}
	const auto taken = definitionNames_.find(name);
	if(strict && taken != definitionNames_.end())
		failDefinition(form, name,
		               "\"" + shown + "\" is defined twice: at " + describePlace(definitions_[taken->second]->place) +
		                   " and at " + describePlace(form.place()));
	auto definition = std::make_unique<Definition>();
	definition->kind = kind;
	definition->name = name;
	definition->author = author;
	definition->authors = authors ? authors : authorSet(author);
	definition->place = form.place();
	Definition* made = definition.get();
	if(taken != definitionNames_.end()) {
		definitions_[taken->second] = std::move(definition);
		compiled_ = std::min(compiled_, taken->second);
	} else {
		definitionNames_.emplace(name, definitions_.size());
		definitions_.push_back(std::move(definition));
	}
	const CompileContext context{name, author, made->authors, false};
	std::size_t at = 2;
	made->doc = docAt(form, at);
	if(head == "defn" || head == "defmacro") {
		made->global = &global(name);
		setPending(*made->global, compileFunctionInto(*this, *made->global, form, context), head == "defmacro");
	} else if(head == "def") {
		if(!made->doc.empty()) ++at;
		if(at + 1 != form.count()) failDefinition(form, name, "def takes a name, maybe a docstring, and a value");
		made->global = &global(name);
		setPending(*made->global, compileValueInto(*this, *made->global, form.at(at), context), false);
	} else {
		defineVocabulary(*made, form, context);
	}
	return *made;
}

void Machine::defineVocabulary(Definition& definition, const Value& form, const CompileContext& context) {
	const std::string& kind = definition.kind;
	const std::string& name = definition.name;
	const Place place = form.place();
	std::size_t at = 2;
	Value data;
	if(kind == "defcommand") {
		if(!definition.doc.empty()) ++at;
		data = optionsOf(*this, form, at, name);
		if(at >= form.count() || form.at(at).kind() != Kind::Vector)
			failDefinition(form, name, "defcommand takes its parameters, a vector, before its body");
		std::vector<Value> function = {form.at(1)};
		for(std::size_t item = at; item < form.count(); ++item) function.push_back(form.at(item));
		definition.global = &global(name);
		setPending(
		    *definition.global,
		    compileFunctionInto(*this, *definition.global, listOf(*this, "fn", std::move(function), place), context),
		    false);
	} else if(kind == "defcustom") {
		definition.doc = docAt(form, 3);
		at = definition.doc.empty() ? 3 : 4;
		data = optionsOf(*this, form, at, name);
		if(at != form.count())
			failDefinition(form, name, "defcustom takes a name, a value, maybe a docstring, and options");
		definition.global = &global(name);
		setPending(*definition.global, compileValueInto(*this, *definition.global, form.at(2), context), false);
	} else if(kind == "defscreen") {
		if(!definition.doc.empty()) ++at;
		const Value options = optionsOf(*this, form, at, name);
		if(at + 1 != form.count()) failDefinition(form, name, "defscreen holds one root node, after its options");
		const std::vector<Value> both = {options, form.at(at)};
		data = heap.sequence(Kind::Vector, std::span<const Value>(both), place);
	} else if(kind == "defkey") {
		if(!definition.doc.empty()) ++at;
		if(at + 1 != form.count())
			failDefinition(form, name, "defkey takes a key, maybe a docstring, and what it runs");
		data = form.at(at);
	} else { // add-hook, advice: recorded, and a later stage gives them their behaviour
		for(std::size_t item = 1; item < form.count(); ++item) {
			if(item >= 2 && form.at(item).kind() == Kind::String && item + 1 < form.count()) continue;
			definition.targets.emplace_back(form.at(item).text());
		}
		return;
	}
	definition.pending = [this, &definition, data, context] {
		const std::uint64_t saved = std::exchange(gensym_, 0);
		definition.data = Compiler(*this, context).templateThunk(data);
		gensym_ = saved;
	};
}

void Machine::compilePending() {
	for(; compiled_ < definitions_.size(); ++compiled_) {
		Definition& definition = *definitions_[compiled_];
		if(definition.global) ensureCompiled(*definition.global);
		if(definition.pending) std::exchange(definition.pending, nullptr)();
	}
}

void Machine::ensureCompiled(Global& cell) {
	if(cell.state != Global::State::Deferred) return;
	if(!cell.pending)
		throw Problem("\"" + notation::Lexer::excerpt(cell.name) + "\" is used while it is being compiled");
	std::exchange(cell.pending, nullptr)();
}

const Definition* Machine::definition(std::string_view name) const {
	const auto found = definitionNames_.find(std::string(name));
	return found == definitionNames_.end() ? nullptr : definitions_[found->second].get();
}

Value Machine::expand(Global& macro, std::span<const Value> arguments, Place place) {
	/// Keeps macro expansion on its own budget, until it ends.
	class OnMacroBudget {
	public:
		explicit OnMacroBudget(Machine& machine) : machine_(machine), outer_(!machine.expanding_) {
			if(!outer_) return;
			saved_ = std::exchange(machine_.budget_.operations, machine_.macroOperations_);
			machine_.expanding_ = true;
		}
		~OnMacroBudget() {
			if(!outer_) return;
			machine_.macroOperations_ = std::exchange(machine_.budget_.operations, saved_);
			machine_.expanding_ = false;
		}
		OnMacroBudget(const OnMacroBudget&) = delete;
		OnMacroBudget& operator=(const OnMacroBudget&) = delete;

	private:
		Machine& machine_;
		bool outer_;
		std::size_t saved_ = 0;
	};
	const OnMacroBudget budget(*this);
	try {
		ensureCompiled(macro);
		spend(16); // an expansion costs something, however little it does
	} catch(const Problem& problem) {
		throw LispError(place.file ? *place.file : "", place.line, place.column, "", problem.message,
		                problem.exhausted);
	}
	return call(macro.value, arguments);
}

const AuthorSet* Machine::authorSet(const std::string& author) {
	std::vector<std::uint32_t> members;
	if(!author.empty()) {
		const auto known = std::ranges::find(authorNames_, author);
		members.push_back(static_cast<std::uint32_t>(known - authorNames_.begin()));
		if(known == authorNames_.end()) authorNames_.push_back(author);
	}
	const auto found = authorIndex_.find(members);
	if(found != authorIndex_.end()) return found->second;
	const AuthorSet* made = &authorSets_.emplace_back(AuthorSet{members});
	authorIndex_.emplace(std::move(members), made);
	return made;
}

const AuthorSet* Machine::unite(const AuthorSet* first, const AuthorSet* second) {
	if(first == second || second == noAuthors_) return first;
	if(first == noAuthors_) return second;
	const auto known = authorUnions_.find({first, second});
	if(known != authorUnions_.end()) return known->second;
	std::vector<std::uint32_t> members;
	std::ranges::set_union(first->authors, second->authors, std::back_inserter(members));
	const AuthorSet* made = nullptr;
	if(const auto found = authorIndex_.find(members); found != authorIndex_.end()) {
		made = found->second;
	} else {
		made = &authorSets_.emplace_back(AuthorSet{members});
		authorIndex_.emplace(std::move(members), made);
	}
	authorUnions_.emplace(std::make_pair(first, second), made);
	return made;
}

std::vector<std::string> Machine::authorsOnStack() const {
	std::vector<std::string> names;
	if(frames_.empty()) return names;
	for(const std::uint32_t author : frames_.back().authors->authors) names.push_back(authorNames_[author]);
	std::ranges::sort(names);
	return names;
}

std::string describePlace(Place place) {
	std::string text = place.file && !place.file->empty() ? *place.file + ": " : "";
	return text + "line " + std::to_string(place.line) + ", column " + std::to_string(place.column);
}

} // namespace affresco::lisp
