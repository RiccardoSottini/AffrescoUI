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

std::vector<std::string> Interpreter::authorsOnStack() const { return machine_->authorsOnStack(); }

std::optional<std::string> Interpreter::leastAuthority(const std::function<int(std::string_view)>& rank) const {
	const std::vector<std::string> authors = machine_->authorsOnStack();
	if(authors.empty()) return std::nullopt;
	return *std::ranges::min_element(authors, {}, [&](const std::string& author) { return rank(author); });
}

std::string Interpreter::authorOf(const Value& function) const {
	return function.kind() == Kind::Function ? static_cast<const ClosureObject*>(function.pointer())->code->author : "";
}

void Interpreter::setBudget(const Budget& budget) noexcept { machine_->budget() = budget; }

Budget Interpreter::budget() const noexcept { return machine_->budget(); }

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
