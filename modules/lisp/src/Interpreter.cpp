/// @file
/// @brief The interpreter (Interpreter.hpp): the machine, seen from outside. Whatever the machine raises leaves here as
/// a LispError.

#include <affresco/lisp/Interpreter.hpp>

#include "Compiler.hpp"
#include "Definitions.hpp"
#include "Machine.hpp"
#include "Reader.hpp"
#include "TopLevel.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace affresco::lisp {

namespace {

/// @brief The text of an error's place.
/// @param file The file, or "".
/// @param line Its line, or 0.
/// @param column Its column, or 0.
/// @param definition The definition, or "".
/// @return "file: line 3, column 8: in name: ".
std::string placeText(const std::string& file, std::size_t line, std::size_t column, const std::string& definition) {
	std::string text = file.empty() ? "" : file + ": ";
	if(line != 0) text += "line " + std::to_string(line) + ", column " + std::to_string(column) + ": ";
	if(!definition.empty()) text += "in " + definition + ": ";
	return text;
}

} // namespace

LispError::LispError(const std::string& file, std::size_t line, std::size_t column, const std::string& definition,
                     const std::string& message, bool exhausted)
    : DocumentError("AffrescoUI lisp: " + placeText(file, line, column, definition) + message), file_(file),
      line_(line), column_(column), definition_(definition), message_(message), exhausted_(exhausted) {}

Interpreter::Interpreter(Limits limits) : machine_(std::make_unique<Machine>(*this, limits)) {}

Interpreter::~Interpreter() = default;

void Interpreter::definePrimitive(const std::string& name, Primitive primitive) {
	machine_->definePrimitive(name, std::move(primitive));
}

void Interpreter::setGate(Gate gate) { machine_->setGate(std::move(gate)); }

void Interpreter::setPure(bool pure) noexcept { machine_->setPure(pure); }

Value Interpreter::eval(std::string_view text, const std::string& file, const std::string& author) {
	return leavingAsLispError(*machine_, [&] {
		Reader reader(machine_->heap, text, file);
		Value result;
		while(const auto read = readUncharged(machine_->heap, reader, machine_->budget())) {
			result = evalTopLevel(*machine_, read->form, author, read->form.place());
		}
		return result;
	});
}

namespace {

/// @brief Reads a text that must be one form.
/// @param machine The machine.
/// @param reader Its reader.
/// @return The form.
/// @throws LispError If it isn't one form.
Value oneForm(Machine& machine, Reader& reader) {
	const auto read = readUncharged(machine.heap, reader, machine.budget());
	if(!read || readUncharged(machine.heap, reader, machine.budget()))
		throw LispError("", 1, 1, "", "expected one form");
	return read->form;
}

/// @brief Turns building's purity on, for as long as it lives, and back to what it was.
class PureWhile {
public:
	/// @brief Turns it on. @param machine The machine.
	explicit PureWhile(Machine& machine) : machine_(machine), was_(machine.pure()) { machine_.setPure(true); }
	/// @brief Puts it back.
	~PureWhile() { machine_.setPure(was_); }
	PureWhile(const PureWhile&) = delete;
	PureWhile& operator=(const PureWhile&) = delete;
	PureWhile(PureWhile&&) = delete;
	PureWhile& operator=(PureWhile&&) = delete;

private:
	Machine& machine_; ///< The machine.
	bool was_;         ///< What it was.
};

/// @brief The function a value stands for: what a host wrapped, or itself.
/// @param function The value.
/// @return The function.
Value standsFor(const Value& function) {
	return function.kind() == Kind::Native ? static_cast<const NativeObject*>(function.pointer())->wrapped : function;
}

} // namespace

Value Interpreter::evaluate(std::string_view text, const std::string& file, const std::string& author) {
	return leavingAsLispError(*machine_, [&] {
		Reader reader(machine_->heap, text, file);
		const Value form = oneForm(*machine_, reader);
		CompileContext context;
		context.author = author;
		context.authors = machine_->authorSet(author);
		const Code* code = nullptr;
		{
			const PureWhile pure(*machine_); // a macro runs now, and reaches nothing
			code = Compiler(*machine_, std::move(context)).thunk(form);
		}
		return machine_->runThunk(code);
	});
}

bool Interpreter::expandsToDefinition(std::string_view text) {
	return leavingAsLispError(*machine_, [&] {
		Reader reader(machine_->heap, text, "");
		Value form = oneForm(*machine_, reader);
		const PureWhile pure(*machine_);
		for(int depth = 0; depth < 16; ++depth) {
			const std::string_view head = headWord(form);
			if(isDefinitionWord(head)) return true;
			if(head == "do" && form.count() > 1) {
				form = form.at(1);
				continue;
			}
			Global* macro = head.empty() ? nullptr : machine_->findGlobal(head);
			if(!macro || !macro->macro) return false;
			std::vector<Value> arguments;
			for(std::size_t at = 1; at < form.count(); ++at) arguments.push_back(form.at(at));
			form = machine_->expand(*macro, arguments, form.place());
		}
		return false;
	});
}

Value Interpreter::call(const Value& function, std::span<const Value> arguments) {
	return leavingAsLispError(*machine_, [&] { return machine_->call(function, arguments); });
}

std::optional<Value> Interpreter::global(std::string_view name) {
	Global* cell = machine_->findGlobal(name);
	if(!cell || cell->state == Global::State::Undefined) return std::nullopt;
	return leavingAsLispError(*machine_, [&] { return machine_->force(*cell); });
}

std::optional<DefinitionInfo> Interpreter::describe(std::string_view name) const {
	const Definition* definition = machine_->definition(name);
	if(!definition) return std::nullopt;
	return DefinitionInfo{definition->kind, definition->name, definition->doc, definition->author, definition->place};
}

std::vector<std::string> Interpreter::definitionNames() const {
	std::vector<std::string> names;
	for(const auto& definition : machine_->definitions()) names.push_back(definition->name);
	std::ranges::sort(names);
	return names;
}

std::vector<Extension> Interpreter::extensions() const {
	std::vector<Extension> found;
	for(const auto& definition : machine_->definitions()) {
		if(!isExtensionWord(definition->kind)) continue;
		found.push_back({definition->kind, definition->name, definition->targets, definition->doc, definition->author,
		                 machine_->authorNames(definition->authors)});
	}
	return found;
}

Value Interpreter::wrapGlobal(const std::string& name, Primitive primitive) {
	Global* cell = machine_->findGlobal(name);
	if(!cell || cell->state != Global::State::Ready || cell->value.kind() != Kind::Function)
		throw std::invalid_argument("no function to wrap: " + name);
	auto* native = machine_->heap.make<NativeObject>();
	native->name = name;
	native->host = std::move(primitive);
	native->wrapped = cell->value;
	const Value original = cell->value;
	cell->value = Value::object(Kind::Native, native);
	return original;
}

Value Interpreter::makeFunction(const std::string& name, Primitive primitive) {
	auto* native = machine_->heap.make<NativeObject>();
	native->name = name;
	native->host = std::move(primitive);
	return Value::object(Kind::Native, native);
}

Interpreter::Sealed::Sealed(Interpreter& interpreter) : machine_(interpreter.machine()), ran_(machine_.ranSet()) {}

Interpreter::Sealed::~Sealed() { machine_.setRanSet(static_cast<const AuthorSet*>(ran_)); }

void Interpreter::addAuthorsThatRan(std::span<const std::string> authors) {
	for(const std::string& author : authors)
		machine_->setRanSet(machine_->unite(machine_->ranSet(), machine_->authorSet(author)));
}

std::vector<std::string> Interpreter::authorsOnStack() const { return machine_->authorsOnStack(); }

std::optional<std::string> Interpreter::leastAuthority(const std::function<int(std::string_view)>& rank) const {
	const std::vector<std::string> authors = machine_->authorsOnStack();
	if(authors.empty()) return std::nullopt;
	return *std::ranges::min_element(authors, {}, [&](const std::string& author) { return rank(author); });
}

std::vector<std::string> Interpreter::authorsThatRan() const { return machine_->authorsThatRan(); }

std::string Interpreter::authorOf(const Value& function) {
	const Value closure = standsFor(function);
	return closure.kind() == Kind::Function ? static_cast<const ClosureObject*>(closure.pointer())->code->author : "";
}

std::vector<std::string> Interpreter::authorsOf(const Value& function) const {
	const Value closure = standsFor(function);
	if(closure.kind() != Kind::Function) return {};
	return machine_->authorNames(static_cast<const ClosureObject*>(closure.pointer())->code->authors);
}

std::optional<std::string> Interpreter::definitionNameOf(const Value& function) const {
	const Value closure = standsFor(function);
	if(closure.kind() != Kind::Function) return std::nullopt;
	const std::string& name = static_cast<const ClosureObject*>(closure.pointer())->code->name;
	const Global* cell = name.empty() ? nullptr : machine_->findGlobal(name);
	if(!cell || cell->state != Global::State::Ready || cell->value.pointer() != function.pointer()) return std::nullopt;
	return name;
}

std::vector<std::string> Interpreter::parametersOf(const Value& function) {
	const Value closure = standsFor(function);
	if(closure.kind() != Kind::Function) return {};
	return static_cast<const ClosureObject*>(closure.pointer())->code->parameters;
}

bool Interpreter::isProgramFunction(std::string_view name) const {
	const Definition* definition = machine_->definition(name);
	return definition && definition->global && (definition->kind == "defn" || definition->kind == "defcommand");
}

void Interpreter::workOutDefinitions() { machine_->workOutDefinitions(); }

std::pair<Place, std::string> Interpreter::place() const { return machine_->currentPlace(); }

Interpreter::Transient::Transient(Interpreter& interpreter) : machine_(interpreter.machine()) {
	machine_.enterRegion();
}

Interpreter::Transient::~Transient() { machine_.leaveRegion(); }

void Interpreter::setBudget(const Budget& budget) noexcept { machine_->budget() = budget; }

Budget Interpreter::budget() const noexcept { return machine_->budget(); }

std::size_t Interpreter::objects() const noexcept { return machine_->heap.size(); }

Value Interpreter::makeString(std::string_view text) {
	return leavingAsLispError(*machine_, [&] { return machine_->heap.string(text); });
}

Value Interpreter::makeVector(std::span<const Value> items) {
	return leavingAsLispError(*machine_, [&] { return machine_->heap.sequence(Kind::Vector, items); });
}

Value Interpreter::viewJson(std::shared_ptr<const json> value) {
	const Value view = Value::view(*value);
	machine_->heap.adopt(std::move(value));
	return view;
}

json Interpreter::toTree(const Value& value) {
	return leavingAsLispError(*machine_, [&] { return valueToTree(*machine_, value, ""); });
}

std::string Interpreter::print(const Value& value) {
	return leavingAsLispError(*machine_, [&] {
		std::string text;
		printValue(*machine_, value, text, true);
		return text;
	});
}

} // namespace affresco::lisp
