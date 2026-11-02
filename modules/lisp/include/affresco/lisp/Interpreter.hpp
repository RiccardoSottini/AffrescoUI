#pragma once

/// @file
/// @brief The language's interpreter: it reads, compiles and runs Lisp under limits, and is the one way out to the host
/// (README.md). A host registers named primitives; every call of one passes through one choke point, which can ask who
/// is on the stack.

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Value.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::lisp {

class Machine;

/// @brief What running may spend. The operations and bytes have the meaning format's WorkBudget gives them.
struct Budget {
	std::size_t operations = 10000000; ///< Operations it may still run (operationsPerEvent).
	std::size_t bytes = 64U << 20U;    ///< Bytes it may still allocate (bytesPerEvent).
};

/// @brief The limits of one interpreter.
struct Limits {
	Budget budget;                         ///< What running may spend, from setBudget() on.
	std::size_t callDepth = 20000;         ///< Calls waiting for a return; the native stack isn't used for them.
	std::size_t macroOperations = 1000000; ///< What macro expansion may spend in operations, on its own budget.
};

/// @brief A problem in Lisp text or while it runs, and where.
///
/// what() names the place: "AffrescoUI lisp: killring.lisp: line 3, column 8: in kill-line: ...". The message says only
/// what the error's own form says: names and kinds, never a value read from elsewhere.
class LispError : public DocumentError {
public:
	/// @brief A problem at a place.
	/// @param file The file, or "".
	/// @param line Its line, from 1, or 0.
	/// @param column Its column, from 1, or 0.
	/// @param definition The definition it is in, or "".
	/// @param message What is wrong.
	/// @param exhausted Whether a limit ran out.
	LispError(const std::string& file, std::size_t line, std::size_t column, const std::string& definition,
	          const std::string& message, bool exhausted = false);

	/// @brief The file. @return It, or "".
	[[nodiscard]] const std::string& file() const noexcept { return file_; }
	/// @brief The line, from 1. @return It, or 0.
	[[nodiscard]] std::size_t line() const noexcept { return line_; }
	/// @brief The column, from 1. @return It, or 0.
	[[nodiscard]] std::size_t column() const noexcept { return column_; }
	/// @brief The definition it is in. @return It, or "".
	[[nodiscard]] const std::string& definition() const noexcept { return definition_; }
	/// @brief What is wrong, without the place. @return It.
	[[nodiscard]] const std::string& message() const noexcept { return message_; }
	/// @brief Whether a limit ran out (work, depth, allocation, macro expansion). @return true if so.
	[[nodiscard]] bool exhausted() const noexcept { return exhausted_; }

private:
	std::string file_;       ///< See file().
	std::size_t line_;       ///< See line().
	std::size_t column_;     ///< See column().
	std::string definition_; ///< See definition().
	std::string message_;    ///< See message().
	bool exhausted_;         ///< See exhausted().
};

/// @brief A refusal a primitive or the gate throws: a condition the program can catch, with this text as its message.
class Refusal : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

class Interpreter;

/// @brief One call of a host's primitive, as the gate sees it.
struct CallContext {
	std::string_view primitive;           ///< Its name.
	std::string_view capability;          ///< The capability it needs, or "".
	std::span<const std::string> authors; ///< Every author on the stack (Interpreter::authorsOnStack()).
	Place place;                          ///< The call.
};

/// @brief The choke point: called before every call of a host's primitive. It throws Refusal to refuse.
using Gate = std::function<void(const CallContext&)>;

/// @brief A function the host gives the language.
struct Primitive {
	std::string capability{}; ///< What it needs, for the gate: "" if it needs nothing.
	bool pure = false;        ///< It reads and changes nothing outside its arguments, so build code may call it.
	/// What it does. It must not keep its span. It may throw Refusal.
	std::function<Value(Interpreter&, std::span<const Value>)> call{};
};

/// @brief What is known of a definition, for `describe-*` and for a host.
struct DefinitionInfo {
	std::string kind;   ///< `defn`, `defcommand`...
	std::string name;   ///< Its name.
	std::string doc;    ///< Its docstring, or "".
	std::string author; ///< Its author, or "".
	Place place;        ///< Where it is.
};

/// @brief An interpreter: a global environment, a heap and a stack, and its limits. Not thread-safe.
class Interpreter {
public:
	/// @brief Makes one with the language's own functions, and no way out.
	/// @param limits Its limits.
	explicit Interpreter(Limits limits = {});
	/// @brief Frees everything it made.
	~Interpreter();
	Interpreter(const Interpreter&) = delete;
	Interpreter& operator=(const Interpreter&) = delete;

	/// @brief Registers a host's primitive, callable by name from then on; a function already of that name is replaced
	/// only if it is a primitive too.
	/// @param name Its name.
	/// @param primitive What it is.
	void definePrimitive(const std::string& name, Primitive primitive);
	/// @brief Sets the choke point (Gate). Without one, every primitive runs.
	/// @param gate The gate.
	void setGate(Gate gate);
	/// @brief While on, a primitive that isn't Primitive::pure is refused, gate or not: this is what makes a build
	/// pure.
	/// @param pure Whether it is on.
	void setPure(bool pure) noexcept;

	/// @brief Reads and runs text at the top level: definitions take effect at once, and may replace earlier ones.
	/// @param text The text.
	/// @param file Its name, for errors.
	/// @param author Who wrote it.
	/// @return The last form's value.
	/// @throws LispError If it can't be read or run.
	Value eval(std::string_view text, const std::string& file = "", const std::string& author = "");
	/// @brief Calls a function.
	/// @param function The function, or anything callable.
	/// @param arguments Its arguments.
	/// @return What it returns.
	/// @throws LispError If it fails.
	Value call(const Value& function, std::span<const Value> arguments);
	/// @brief A global's value, working it out if it hasn't been.
	/// @param name The global's name.
	/// @return The value, or std::nullopt if there is none.
	/// @throws LispError If working it out fails.
	[[nodiscard]] std::optional<Value> global(std::string_view name);
	/// @brief What is known of a definition.
	/// @param name Its name.
	/// @return It, or std::nullopt if there is none.
	[[nodiscard]] std::optional<DefinitionInfo> describe(std::string_view name) const;

	/// @brief Every author on the stack: the functions being run and those a tail call replaced. A closure runs with
	/// its own author; a definition that used a macro counts the macro's author too. The language's own functions have
	/// none. While a primitive is called this is what the gate is given.
	/// @return The authors, sorted.
	[[nodiscard]] std::vector<std::string> authorsOnStack() const;
	/// @brief The least authority on the stack (RFC 0005).
	/// @param rank What an author may do, higher for more.
	/// @return The author with the lowest rank, or std::nullopt with none on the stack.
	[[nodiscard]] std::optional<std::string> leastAuthority(const std::function<int(std::string_view)>& rank) const;
	/// @brief The author of a function, which it keeps wherever it goes.
	/// @param function A closure.
	/// @return Its author; "" for anything else.
	[[nodiscard]] std::string authorOf(const Value& function) const;

	/// @brief Sets what running may spend from now on. @param budget The budget.
	void setBudget(const Budget& budget) noexcept;
	/// @brief What it may still spend. @return The budget.
	[[nodiscard]] Budget budget() const noexcept;

	/// @brief A string, in the heap. @param text Its text. @return The value.
	[[nodiscard]] Value makeString(std::string_view text);
	/// @brief A vector, in the heap. @param items Its items. @return The value.
	[[nodiscard]] Value makeVector(std::span<const Value> items);
	/// @brief A view of JSON the interpreter keeps alive until it goes: nothing is converted (Value::view()).
	/// @param value The JSON.
	/// @return The value.
	[[nodiscard]] Value viewJson(std::shared_ptr<const json> value);
	/// @brief A value as the JSON tree it stands for: a list is a node, a symbol or a keyword a string.
	/// @param value The value.
	/// @return The tree.
	/// @throws LispError If it holds what no JSON tree holds (a function), a list that isn't a node, or is too deep or
	/// too large for the budget.
	[[nodiscard]] json toTree(const Value& value);
	/// @brief A value as Lisp text. @param value The value. @return The text.
	/// @throws LispError If it is too deep or too large for the budget.
	[[nodiscard]] std::string print(const Value& value);

	/// @brief Internal: the machine. @return It.
	[[nodiscard]] Machine& machine() noexcept { return *machine_; }

private:
	std::unique_ptr<Machine> machine_; ///< See machine().
};

} // namespace affresco::lisp
