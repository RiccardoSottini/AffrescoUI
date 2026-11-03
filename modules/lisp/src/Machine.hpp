#pragma once

/// @file
/// @brief The machine: globals, definitions, the stack and the instructions (README.md, "How it runs"). Calls and
/// returns use its own stack, never the native one, so recursion ends at a limit with an error, however deep.

#include "Objects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::lisp {

/// How deep a value is walked (compared, printed): deeper values are an error, not a native stack overflow.
inline constexpr int maxValueDepth = 1024;
/// How deep a form is compiled, macro output included.
inline constexpr int maxCompileDepth = 512;

/// @brief A definition of the running program.
struct Definition {
	std::string kind;                   ///< `defn`, `defscreen`...
	std::string name;                   ///< Its address (Entry::name).
	std::string doc;                    ///< Its docstring, or "".
	std::string author;                 ///< Its author, or "".
	const AuthorSet* authors = nullptr; ///< Its author and those of the macros it came through.
	Place place;                        ///< Where it is.
	Global* global = nullptr;           ///< The global it defines, if any.
	const Code* data = nullptr;         ///< Builds what it adds to the tree, if anything.
	std::function<void()> pending;      ///< Compiles data.
	std::vector<std::string> targets;   ///< What an `add-hook` or an `advice` names.
};

/// @brief Who code being compiled belongs to.
struct CompileContext {
	std::string definition;             ///< The definition, or "".
	std::string author;                 ///< Its author, or "".
	const AuthorSet* authors = nullptr; ///< Its author and those of the macros it came through.
	bool library = false; ///< The language's own: a global already defined is taken as a constant, so that
	                      ///< a program redefining `count` doesn't change what `reduce` does.
	bool view = false;    ///< Code that builds a view: each backquote in it is a template whose `~form` that needs
	                      ///< the running app becomes a binding (`defview`, `defcomponent`).
};

/// @brief A use of a global a definition's code makes.
struct GlobalUse {
	const Global* global = nullptr; ///< The global.
	Place place;                    ///< Where it is used.
	std::string definition;         ///< The definition it is in.
};

/// @brief The machine behind an Interpreter.
class Machine {
public:
	/// @brief Makes one, with the language's own functions.
	/// @param owner The interpreter it serves.
	/// @param limits Its limits.
	Machine(Interpreter& owner, const Limits& limits);

	Heap heap; ///< What values point at.

	/// @brief The interpreter it serves. @return It.
	[[nodiscard]] Interpreter& owner() noexcept { return owner_; }
	/// @brief Its limits. @return Them.
	[[nodiscard]] const Limits& limits() const noexcept { return limits_; }

	// Globals and definitions (Definitions.cpp)
	/// @brief A global's cell, made if it is new: its address stays the same for the machine's life.
	/// @param name Its name.
	/// @return The cell.
	Global& global(std::string_view name);
	/// @brief A global's cell, if it exists.
	/// @param name Its name.
	/// @return The cell, or nullptr.
	[[nodiscard]] Global* findGlobal(std::string_view name) const;
	/// @brief Registers a function of the language's own.
	/// @param name Its name.
	/// @param function It.
	/// @param spread `apply`: see NativeObject::spread.
	void defineNative(std::string_view name, NativeFunction function, bool spread = false);
	/// @brief Registers a host's primitive.
	/// @param name Its name.
	/// @param primitive It.
	void definePrimitive(const std::string& name, Primitive primitive);
	/// @brief Runs a top-level form that is a definition.
	/// @param form The form.
	/// @param author Its author.
	/// @param authors Its author and those of the macros it came through; nullptr for just the author.
	/// @param strict Whether a name taken is an error, as in a program; else it replaces, as `eval` does.
	/// @return The definition.
	/// @throws LispError If the form is wrong.
	Definition& define(const Value& form, const std::string& author, const AuthorSet* authors, bool strict);
	/// @brief Tells whether a list is a top-level definition form.
	/// @param form The form.
	/// @return true if it is.
	[[nodiscard]] static bool isDefinitionForm(const Value& form);
	/// @brief Compiles every definition not compiled yet.
	void compilePending();
	/// @brief Compiles a global, if it is waiting to be.
	/// @param cell The global.
	static void ensureCompiled(Global& cell);
	/// @brief A definition by name.
	/// @param name Its name.
	/// @return It, or nullptr.
	[[nodiscard]] const Definition* definition(std::string_view name) const;
	/// @brief Every definition, in order. @return Them.
	[[nodiscard]] const std::vector<std::unique_ptr<Definition>>& definitions() const noexcept { return definitions_; }
	/// @brief Notes a use of a global, for undefined names.
	/// @param use The use.
	void noteUse(GlobalUse use) { uses_.push_back(std::move(use)); }
	/// @brief Every use of a global noted. @return Them.
	[[nodiscard]] const std::vector<GlobalUse>& uses() const noexcept { return uses_; }
	/// @brief Expands a macro call.
	/// @param macro The macro.
	/// @param arguments The forms it is given.
	/// @param place The call.
	/// @return The form it gives.
	Value expand(Global& macro, std::span<const Value> arguments, Place place);
	/// @brief How deep compiling is now, for its limit. @return A reference to the counter.
	[[nodiscard]] int& compileDepth() noexcept { return compileDepth_; }
	/// @brief A symbol's number for `gensym`, reset for each definition so that one doesn't move another's.
	/// @return A reference to the counter.
	[[nodiscard]] std::uint64_t& gensymCounter() noexcept { return gensym_; }

	// Authors
	/// @brief A set of one author. @param author The author, or "" for the empty set. @return The set.
	const AuthorSet* authorSet(const std::string& author);
	/// @brief Two sets together. @param first One. @param second The other. @return The set.
	const AuthorSet* unite(const AuthorSet* first, const AuthorSet* second);
	/// @brief The empty set. @return It.
	[[nodiscard]] const AuthorSet* noAuthors() const noexcept { return noAuthors_; }
	/// @brief Who has run since the outermost call began, as a set. @return It.
	[[nodiscard]] const AuthorSet* ranSet() const noexcept { return ran_; }
	/// @brief Sets who has run: only ever back to what it was around a run that gives nothing back, or wider.
	/// @param ran The set.
	void setRanSet(const AuthorSet* ran) noexcept { ran_ = ran; }
	/// @brief The names in a set.
	/// @param authors The set; nullptr for none.
	/// @return The names, sorted.
	[[nodiscard]] std::vector<std::string> authorNames(const AuthorSet* authors) const;
	/// @brief Whether a build's purity is on. @return It.
	[[nodiscard]] bool pure() const noexcept { return pure_; }
	/// @brief Every author on the stack. @return Their names, sorted.
	[[nodiscard]] std::vector<std::string> authorsOnStack() const;
	/// @brief Every author whose code has run since the outermost call began. @return Their names, sorted.
	[[nodiscard]] std::vector<std::string> authorsThatRan() const;

	// Running (Machine.cpp)
	/// @brief Calls a function.
	/// @param function The function.
	/// @param arguments Its arguments.
	/// @return What it returns.
	/// @throws LispError If it fails and nothing catches it.
	Value call(const Value& function, std::span<const Value> arguments);
	/// @brief Runs code that takes nothing.
	/// @param code The code.
	/// @return What it gives.
	/// @throws LispError If it fails and nothing catches it.
	Value runThunk(const Code* code);
	/// @brief A global's value, working it out if it hasn't been.
	/// @param global The global.
	/// @return The value.
	/// @throws LispError If it is undefined or working it out fails.
	Value force(Global& global);
	/// @brief Enters a region (Interpreter::Transient): the outermost starts it, and what it runs starts a new record
	/// of who ran.
	void enterRegion() {
		if(regions_++ != 0) return;
		region_ = heap.mark();
		if(frames_.empty() && nesting_ == 0) ran_ = noAuthors_;
	}
	/// @brief Leaves a region: the outermost ends it, and gives back the stack a deep run grew.
	void leaveRegion();
	/// @brief Works out every global defined by a `def` and not yet.
	void workOutDefinitions();
	/// @brief Sets the choke point. @param gate The gate.
	void setGate(Gate gate) { gate_ = std::move(gate); }
	/// @brief Sets whether primitives that aren't pure are refused. @param pure Whether they are.
	void setPure(bool pure) noexcept { pure_ = pure; }
	/// @brief What running may still spend. @return A reference to it.
	[[nodiscard]] Budget& budget() noexcept { return budget_; }
	/// @brief Spends operations. @param count How many.
	/// @throws Problem If the budget runs out.
	void spend(std::size_t count) {
		if(budget_.operations < count)
			exhausted(expanding_ ? "the budget for macro expansion ran out" : "the budget for work ran out");
		budget_.operations -= count;
	}
	/// @brief Spends what going through a text costs: what is done per byte, as an operation is about as long as going
	/// through this many.
	/// @param bytes Its size.
	/// @throws Problem If the budget runs out.
	void spendText(std::size_t bytes) { spend(bytes / 64); }
	/// @brief Spends what finding a key in a map costs, beyond an operation: comparing a long key, and going through
	/// the members of a map viewed from JSON, which has no index.
	/// @param map The map.
	/// @param key The key.
	/// @throws Problem If the budget runs out.
	void spendLookup(const Value& map, std::string_view key) {
		spend(key.size() / 64 + (map.isView() ? map.count() / 16 : 0));
	}
	/// @brief A Problem saying a limit ran out.
	/// @param what Which.
	/// @throws Problem Always.
	[[noreturn]] static void exhausted(const std::string& what) { throw Problem(what, true); }

	/// @brief The current place, for an error from outside the instructions.
	/// @return The place of the first frame that isn't the language's own.
	[[nodiscard]] std::pair<Place, std::string> currentPlace() const;

private:
	/// @brief One function being run.
	struct Frame {
		const Code* code = nullptr;             ///< What it runs.
		const ClosureObject* closure = nullptr; ///< Its closure; nullptr for a thunk.
		std::uint32_t ip = 0;                   ///< The next instruction.
		std::size_t base = 0;                   ///< Where its locals start on the stack.
		const AuthorSet* authors = nullptr;     ///< Everyone it runs under: its callers, and those tail calls replaced.
		Global* storeTo = nullptr;              ///< The `def` it works out, if it is one.
		const Code* origin = nullptr; ///< The program's code that tail-called into the language's own, if any.
		std::uint32_t originIp = 0;   ///< Where it was.
	};
	/// @brief A `try` being run.
	struct Handler {
		std::size_t frames = 0;   ///< How many frames it leaves.
		std::size_t height = 0;   ///< How high the stack stands.
		std::uint32_t target = 0; ///< Where its `catch` is.
	};
	using Handle = void (Machine::*)(const Instruction&); ///< An instruction's function.
	/// @brief The instructions' functions, by Op. @return The table.
	static const std::array<Handle, static_cast<std::size_t>(Op::Count)>& handles() noexcept;

	/// @brief Runs until the frames are back to a count.
	/// @param floor The count.
	void run(std::size_t floor);
	/// @brief Runs one instruction.
	void step();
	/// @brief Adds a definition, or replaces one of the name.
	/// @param form The form.
	/// @param kind Its kind.
	/// @param name Its name.
	/// @param author Its author.
	/// @param strict Whether a name taken is an error.
	/// @return The definition.
	Definition& addDefinition(const Value& form, const std::string& kind, const std::string& name,
	                          const std::string& author, bool strict);
	/// @brief Defines `defcommand`'s function. @param definition It. @param form Its form. @param context Its code's
	/// owner. @return What it builds, as a template.
	Value commandData(Definition& definition, const Value& form, const CompileContext& context);
	/// @brief Defines `defcustom`'s global. @param definition It. @param form Its form. @param context Its code's
	/// owner.
	/// @return What it builds, as a template.
	Value customData(Definition& definition, const Value& form, const CompileContext& context);
	/// @brief What `defscreen` builds. @param definition It. @param form Its form. @return A vector of its options and
	/// its node, as a template.
	Value screenData(const Definition& definition, const Value& form);
	/// @brief What a form that defines a member of a section builds (`defstate`, `defcomputed`, `defstyle`, `defmenu`,
	/// `deftokens`): its value as a template; `defstate`'s with its options, a vector; `defcomputed`'s as a `~form`.
	/// @param definition It. @param form Its form. @return What it builds.
	Value sectionData(Definition& definition, const Value& form);
	/// @brief What `defcomponent` builds: code that binds each parameter to its own name, as a symbol, and gives
	/// `(%component doc params node)`.
	/// @param definition It. @param form Its form. @return The code.
	Value componentData(Definition& definition, const Value& form);
	/// @brief What the forms that build a tree mean: `defcommand`, `defcustom`, `defscreen`, `defkey`, and the recorded
	/// `add-hook` and `advice`.
	/// @param definition The definition.
	/// @param form The form.
	/// @param context Who its code belongs to.
	void defineVocabulary(Definition& definition, const Value& form, const CompileContext& context);
	/// @brief Handles a problem: the innermost `try` above the floor catches it, else it leaves as a LispError.
	/// @param problem The problem.
	/// @param floor The frames this run may not unwind.
	void raise(const Problem& problem, std::size_t floor);
	/// @brief Unwinds the frames down to a count, giving back the `def`s being worked out.
	/// @param count The count.
	void unwind(std::size_t count) noexcept;
	/// @brief Calls what stands under the top arguments.
	/// @param count How many arguments.
	/// @param tail Whether this frame's work is done: a closure replaces it.
	void invoke(std::uint32_t count, bool tail);
	/// @brief Enters a closure.
	/// @param closure The closure.
	/// @param count How many arguments stand on the stack.
	/// @param tail Whether it replaces the frame.
	void enter(const ClosureObject* closure, std::uint32_t count, bool tail);
	/// @brief Checks a closure's arguments and gathers its rest parameter.
	/// @param code Its code.
	/// @param count How many arguments stand on the stack.
	void bindArguments(const Code* code, std::uint32_t count);
	/// @brief Calls a native function or a host's primitive.
	/// @param native It.
	/// @param count How many arguments.
	/// @param tail Whether this frame's work is done.
	void callNative(const NativeObject* native, std::uint32_t count, bool tail);
	/// @brief The choke point: everything a host's primitive goes through.
	/// @param native It.
	/// @param arguments Its arguments.
	/// @return What it returns.
	Value callHost(const NativeObject* native, std::vector<Value> arguments);
	/// @brief The text a primitive's first argument is, when the code running names the primitive with it written out
	/// there (CallContext::named).
	/// @param native The primitive.
	/// @param arguments Its arguments.
	/// @return The text, or empty.
	[[nodiscard]] std::string_view namedBy(const NativeObject* native, std::span<const Value> arguments) const;
	/// @brief Gives a call's result, in place of the function and its arguments.
	/// @param result The result.
	/// @param count How many arguments.
	void finishCall(Value result, std::uint32_t count);
	/// @brief Pushes a frame for a thunk.
	/// @param code The thunk.
	/// @param storeTo The `def` it works out, if any.
	void pushThunk(const Code* code, Global* storeTo);
	/// @brief The authors of a new frame on top of this one.
	/// @param code The code it runs.
	/// @return The set.
	const AuthorSet* authorsFor(const Code* code);
	/// @brief Notes that code is about to run, for authorsThatRan(). @param code The code.
	void noteRan(const Code* code) {
		if(code->authors != ran_ && code->authors != noAuthors_) ran_ = unite(ran_, code->authors);
	}
	/// @brief Starts a new record of who ran, for a run with nothing around it. Inside a region it is the region's
	/// start that does, so that who a host says led to a run (Interpreter::addAuthorsThatRan()) stays on the record,
	/// whatever the stack holds.
	void startRecord() noexcept {
		if(frames_.empty() && nesting_ == 0 && regions_ == 0) ran_ = noAuthors_;
	}
	/// @brief Takes a Build's items off the stack, spreading what `~@` marked.
	/// @param count How many values.
	/// @return The items.
	std::vector<Value> takeBuilt(std::uint32_t count);
	/// @brief Pops the top. @return It.
	Value pop() {
		const Value top = stack_.back();
		stack_.pop_back();
		return top;
	}

	void opConst(const Instruction& instruction);
	void opLocal(const Instruction& instruction);
	void opSetLocal(const Instruction& instruction);
	void opCapture(const Instruction& instruction);
	void opSelf(const Instruction& instruction);
	void opGlobal(const Instruction& instruction);
	void opPop(const Instruction& instruction);
	void opJump(const Instruction& instruction);
	void opJumpIfFalse(const Instruction& instruction);
	void opJumpIfFalseKeep(const Instruction& instruction);
	void opJumpIfTrueKeep(const Instruction& instruction);
	void opJumpIfNotNilKeep(const Instruction& instruction);
	void opCall(const Instruction& instruction);
	void opTailCall(const Instruction& instruction);
	void opReturn(const Instruction& instruction);
	void opClosure(const Instruction& instruction);
	void opBuildList(const Instruction& instruction);
	void opBuildVector(const Instruction& instruction);
	void opBuildMap(const Instruction& instruction);
	void opSplice(const Instruction& instruction);
	void opRecur(const Instruction& instruction);
	void opThrow(const Instruction& instruction);
	void opTryBegin(const Instruction& instruction);
	void opTryEnd(const Instruction& instruction);

	Interpreter& owner_;              ///< See owner().
	Limits limits_;                   ///< See limits().
	Budget budget_;                   ///< See budget().
	std::vector<Value> stack_;        ///< Locals and operands.
	std::vector<Frame> frames_;       ///< The functions being run.
	std::vector<Handler> handlers_;   ///< The `try`s being run.
	std::size_t nesting_ = 0;         ///< Runs inside runs: macro expansion, and a host's primitive calling back.
	bool expanding_ = false;          ///< On the macro budget.
	std::size_t macroOperations_ = 0; ///< What macro expansion may still spend.
	Gate gate_;                       ///< See setGate().
	const AuthorSet* ran_ = nullptr;  ///< See authorsThatRan().
	Heap::Mark region_;               ///< Where the outermost region started.
	std::size_t regions_ = 0;         ///< Regions inside each other.
	bool pure_ = false;               ///< See setPure().
	std::deque<Global> globalCells_;  ///< The cells: their addresses stay.
	std::map<std::string_view, Global*, std::less<>> globals_;           ///< The cells by name.
	std::vector<std::unique_ptr<Definition>> definitions_;               ///< See definitions().
	std::map<std::string, std::size_t, std::less<>> definitionNames_;    ///< definitions_'s indexes by name.
	std::size_t compiled_ = 0;                                           ///< How many of definitions_ are compiled.
	std::vector<GlobalUse> uses_;                                        ///< See uses().
	int compileDepth_ = 0;                                               ///< See compileDepth().
	std::uint64_t gensym_ = 0;                                           ///< See gensymCounter().
	std::vector<std::string> authorNames_;                               ///< Authors by number.
	std::deque<AuthorSet> authorSets_;                                   ///< The interned sets.
	std::map<std::vector<std::uint32_t>, const AuthorSet*> authorIndex_; ///< The sets by content.
	std::map<std::pair<const AuthorSet*, const AuthorSet*>, const AuthorSet*> authorUnions_; ///< unite()'s answers.
	const AuthorSet* noAuthors_ = nullptr;                                                   ///< See noAuthors().
};

/// @brief Registers the language's own functions (CoreNumbers.cpp, CoreCollections.cpp, CoreText.cpp).
/// @param machine The machine.
void defineNumbers(Machine& machine);
/// @copydoc defineNumbers
void defineCollections(Machine& machine);
/// @copydoc defineNumbers
void defineText(Machine& machine);
/// @copydoc defineNumbers
void defineViews(Machine& machine);
/// @brief Loads the functions the language writes in itself (Prelude.cpp).
/// @param machine The machine.
void definePrelude(Machine& machine);

/// @brief A text as an expression writes it, quoted (CompilerBindings.cpp).
/// @param text The text.
/// @return `'it\'s'`, with its escapes.
[[nodiscard]] std::string expressionText(std::string_view text);

/// @brief Writes a dotted word as an expression's path: `state.my-key.0` is `state['my-key'][0]`
/// (CompilerLowering.cpp).
/// @param word The word.
/// @return The path.
[[nodiscard]] std::string pathExpression(std::string_view word);

/// @brief A value the build worked out as an expression writes it: a symbol is a name of the running app, its path;
/// anything else its JSON, as a literal (CoreText.cpp).
/// @param machine The machine, whose budget it spends.
/// @param value The value.
/// @return The text.
[[nodiscard]] std::string expressionOfValue(Machine& machine, const Value& value);

/// @brief Writes a value as Lisp text (Tree.cpp).
/// @param machine The machine, whose budget it spends.
/// @param value The value.
/// @param out Gets the text.
/// @param readable Strings in quotes, as `print`; else as `str` shows them.
void printValue(Machine& machine, const Value& value, std::string& out, bool readable);
/// @brief A value as the JSON tree it stands for (Interpreter::toTree()).
/// @param machine The machine, whose budget it spends.
/// @param value The value.
/// @param definition The definition it builds for, for errors, or "".
/// @return The tree.
/// @throws LispError As Interpreter::toTree() does.
[[nodiscard]] json valueToTree(Machine& machine, const Value& value, const std::string& definition);
/// @brief Whether two values are equal: numbers by value, lists and vectors alike, maps whatever their order.
/// @param machine The machine, whose budget it spends.
/// @param first One.
/// @param second The other.
/// @return true if they are.
[[nodiscard]] bool valuesEqual(Machine& machine, const Value& first, const Value& second);

} // namespace affresco::lisp
