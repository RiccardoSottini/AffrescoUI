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
#include <utility>
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
	/// Every author whose code has run since the outermost call began, those that returned too: what such code gave
	/// back (a function, a name to run) is in what is running now, so it is what a host weighs authority by.
	std::span<const std::string> ran;
	Place place; ///< The call.
	/// The text its first argument is, when the code that calls it names it with that text written out there
	/// (`(invoke "files.read")`, Program::namedCalls()); else empty. What such code reaches by that name is in its own
	/// text, so a host may judge it by what was judged of that text, as a step written in a document is.
	std::string_view named;
};

/// @brief The choke point: called before every call of a host's primitive. It throws Refusal to refuse.
using Gate = std::function<void(const CallContext&)>;

/// @brief A function the host gives the language.
struct Primitive {
	std::string capability{}; ///< What it needs, for the gate: "" if it needs nothing.
	bool pure = false;        ///< It reads and changes nothing outside its arguments, so build code may call it.
	/// No definition of a program may take its name: code calling it means it in every file, a shared add-on's too
	/// (an expression's function, `now`), as a definition would replace it for all of them.
	bool reserved = false;
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

/// @brief A definition that joins something else rather than defining a name: `add-hook`, `remove-hook` or `advice`.
struct Extension {
	std::string kind;                 ///< `add-hook`, `remove-hook` or `advice`.
	std::string name;                 ///< Its address, as the entry's.
	std::vector<std::string> targets; ///< Its arguments, as written, in order: `after-save`, `notify-saved`.
	std::string doc;                  ///< Its docstring, or "".
	std::string author;               ///< Its author.
	std::vector<std::string> authors; ///< Its author and those of the macros it came through.
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
	/// @brief Names a variable of the running app that code reads by its bare word, through `%read` as a path's
	/// (`preview`, `locale`), where no local or definition of the program has that name, and where the word isn't
	/// called: `(preview ...)` is still the function of that name. Code compiled from then on reads it so.
	/// @param name The word.
	void addAppVariable(const std::string& name);
	/// @brief Sets the choke point (Gate). Without one, every primitive runs.
	/// @param gate The gate.
	void setGate(Gate gate);
	/// @brief While on, a primitive that isn't Primitive::pure is refused, gate or not: this is what makes a build
	/// pure.
	/// @param pure Whether it is on.
	void setPure(bool pure) noexcept;

	/// @brief A region: while it lives, what the interpreter makes is given back when it ends, so that a long-running
	/// app doesn't grow with each event. Nested ones join the outermost, and no value made inside may be read after it.
	/// A global worked out, or anything compiled, inside it keeps what the region made.
	class Transient {
	public:
		/// @brief Starts one, unless one is running.
		/// @param interpreter The interpreter; outlives it.
		explicit Transient(Interpreter& interpreter);
		/// @brief Ends the one it started, giving back what it made.
		~Transient();
		Transient(const Transient&) = delete;
		Transient& operator=(const Transient&) = delete;
		Transient(Transient&&) = delete;
		Transient& operator=(Transient&&) = delete;

	private:
		Machine& machine_; ///< The machine.
	};

	/// @brief Reads and runs text at the top level: definitions take effect at once, and may replace earlier ones.
	/// @param text The text.
	/// @param file Its name, for errors.
	/// @param author Who wrote it.
	/// @return The last form's value.
	/// @throws LispError If it can't be read or run.
	Value eval(std::string_view text, const std::string& file = "", const std::string& author = "");
	/// @brief Evaluates one form for its value, as code of an author: it can't define anything. Macros are expanded
	/// first with nothing reaching outside, as when a program loads; then it runs with the primitives, the gate and the
	/// authors as any function does, the macros' authors among them.
	/// @param text The form's text.
	/// @param file Its name, for errors.
	/// @param author Who wrote it.
	/// @return Its value.
	/// @throws LispError If it can't be read or run, or isn't one form.
	Value evaluate(std::string_view text, const std::string& file, const std::string& author);
	/// @brief Tells whether a form is a call of a macro that gives a definition (or a `do` of them). Nothing runs but
	/// the macro, with nothing reaching outside.
	/// @param text The form's text.
	/// @return true if it is.
	/// @throws LispError If it can't be read, or the macro fails.
	[[nodiscard]] bool expandsToDefinition(std::string_view text);
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

	/// @brief The names of everything defined, for `apropos`. @return Them, sorted.
	[[nodiscard]] std::vector<std::string> definitionNames() const;
	/// @brief The hooks and advice defined. @return Them, in the order of their definitions.
	[[nodiscard]] std::vector<Extension> extensions() const;
	/// @brief Puts a host's primitive in a function's place: whatever calls that name, however, now calls it. It stands
	/// for the function: parametersOf(), authorOf() and definitionNameOf() answer as the function's.
	/// @param name The function's name.
	/// @param primitive What runs instead.
	/// @return The function it replaces.
	/// @throws std::invalid_argument If that name has no function.
	Value wrapGlobal(const std::string& name, Primitive primitive);
	/// @brief A function made by the host, for this run only: it goes with the values of its event.
	/// @param name Its name, for errors.
	/// @param primitive What it does.
	/// @return The function.
	[[nodiscard]] Value makeFunction(const std::string& name, Primitive primitive);
	/// @brief While it lives, what runs counts in authorsThatRan() as usual; when it ends that goes back to what it
	/// was. Only for a run that gives nothing back to what called it (a hook): what it did can't then be in what runs
	/// next.
	class Sealed {
	public:
		/// @brief Starts one.
		/// @param interpreter The interpreter; outlives it.
		explicit Sealed(Interpreter& interpreter);
		/// @brief Puts it back.
		~Sealed();
		Sealed(const Sealed&) = delete;
		Sealed& operator=(const Sealed&) = delete;
		Sealed(Sealed&&) = delete;
		Sealed& operator=(Sealed&&) = delete;

	private:
		Machine& machine_; ///< The machine.
		const void* ran_;  ///< What it was.
	};
	/// @brief Adds authors to authorsThatRan(): those whose decision is why what runs next runs, though it isn't their
	/// code.
	/// @param authors The authors.
	void addAuthorsThatRan(std::span<const std::string> authors);

	/// @brief Every author on the stack: the functions being run and those a tail call replaced. A closure runs with
	/// its own author; a definition that used a macro counts the macro's author too. The language's own functions have
	/// none. While a primitive is called this is what the gate is given.
	/// @return The authors, sorted.
	[[nodiscard]] std::vector<std::string> authorsOnStack() const;
	/// @brief The least authority on the stack (RFC 0005).
	/// @param rank What an author may do, higher for more.
	/// @return The author with the lowest rank, or std::nullopt with none on the stack.
	[[nodiscard]] std::optional<std::string> leastAuthority(const std::function<int(std::string_view)>& rank) const;
	/// @brief Every author whose code has run since the outermost call began, those that returned too
	/// (CallContext::ran).
	/// @return The authors, sorted.
	[[nodiscard]] std::vector<std::string> authorsThatRan() const;
	/// @brief The author of a function, which it keeps wherever it goes.
	/// @param function A closure.
	/// @return Its author; "" for anything else.
	[[nodiscard]] static std::string authorOf(const Value& function);
	/// @brief Everyone whose code a function holds: its author and those of the macros that wrote it.
	/// @param function A closure.
	/// @return Them, sorted; none for anything else.
	[[nodiscard]] std::vector<std::string> authorsOf(const Value& function) const;
	/// @brief The definition a value is: a function a definition gave, as it is now the value of that name. A callback
	/// that outlives the event is kept by this name (a closure's values go with the event).
	/// @param function The value.
	/// @return Its name; std::nullopt for a function made while running, a primitive or anything else.
	[[nodiscard]] std::optional<std::string> definitionNameOf(const Value& function) const;
	/// @brief A function's parameters, as a handler gives their values by name.
	/// @param function The function.
	/// @return Their names, without a rest parameter; "" for a pattern. Empty for what isn't a function a program
	/// defined.
	[[nodiscard]] static std::vector<std::string> parametersOf(const Value& function);
	/// @brief Tells whether a name is a function of the program (`defn`, `defcommand`), which a handler may run by
	/// name.
	/// @param name The name.
	/// @return true if it is.
	[[nodiscard]] bool isProgramFunction(std::string_view name) const;
	/// @brief Works out every `def` and `defcustom` that isn't yet.
	/// @throws LispError If one fails, naming it.
	void workOutDefinitions();
	/// @brief Where the call being run is, for an error from a primitive.
	/// @return Its place, and the definition it is in.
	[[nodiscard]] std::pair<Place, std::string> place() const;

	/// @brief Sets what running may spend from now on. @param budget The budget.
	void setBudget(const Budget& budget) noexcept;
	/// @brief What it may still spend. @return The budget.
	[[nodiscard]] Budget budget() const noexcept;

	/// @brief How many objects its heap holds, to see that what an event made is gone. @return The count.
	[[nodiscard]] std::size_t objects() const noexcept;

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
