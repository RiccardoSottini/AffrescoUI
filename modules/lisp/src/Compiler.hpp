#pragma once

/// @file
/// @brief The compiler: forms into a Code's instructions, macros expanded on the way. Its recursion follows a form's
/// nesting and stops at maxCompileDepth, macro output included.

#include "Machine.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace affresco::lisp {

/// @brief Compiles forms.
class Compiler {
public:
	/// @brief Makes a compiler.
	/// @param machine The machine.
	/// @param context Who the code belongs to.
	Compiler(Machine& machine, CompileContext context);

	/// @brief Tells whether a name is a special form's: a program can't redefine it.
	/// @param name The name.
	/// @return true if it is.
	[[nodiscard]] static bool isSpecial(std::string_view name);

	/// @brief Compiles a form as code that takes nothing.
	/// @param form The form.
	/// @return The code.
	const Code* thunk(const Value& form);
	/// @brief Compiles a form as a template that takes nothing: words stay data, `~` runs.
	/// @param form The form.
	/// @return The code.
	const Code* templateThunk(const Value& form);
	/// @brief Compiles `(fn name? "doc"? [params] body...)`.
	/// @param form The form.
	/// @return The code of its function, which captures nothing here.
	const Code* function(const Value& form);

private:
	/// @brief What can follow a form.
	struct Position {
		bool tail = false;  ///< Its value is the function's: a call there replaces the function.
		bool recur = false; ///< `recur` may stand here.
	};
	/// @brief A local variable.
	struct Local {
		std::string_view name;  ///< Its name.
		std::uint32_t slot = 0; ///< Its slot.
	};
	/// @brief Where a `recur` goes.
	struct RecurTarget {
		std::uint32_t target = 0; ///< The instruction.
		std::uint32_t first = 0;  ///< The first slot it sets.
		std::uint32_t count = 0;  ///< How many.
	};
	/// @brief A function being compiled.
	struct Function {
		Function* parent = nullptr;                 ///< The function it is in.
		Code* code = nullptr;                       ///< What it becomes.
		std::string_view self;                      ///< Its own name, or "".
		std::vector<Local> locals;                  ///< The variables in scope.
		std::vector<std::string_view> captureNames; ///< What it captures, as Code::captures.
		std::uint32_t nextSlot = 0;                 ///< The next free slot.
		std::vector<RecurTarget> recurs;            ///< The `loop`s it is in, then the function itself.
		const AuthorSet* authors = nullptr;         ///< Its authors so far.
	};
	/// @brief What a variable name is.
	struct Variable {
		enum class Kind : std::uint8_t { None, Local, Capture, Self } kind = Kind::None; ///< Where it is.
		std::uint32_t index = 0;                                                         ///< Its index there.
	};
	using SpecialForm = void (Compiler::*)(const Value&, Position);
	/// @brief A part of a binding's text: text as it is, or a form the build works out.
	struct BindingPart {
		std::string text;               ///< The text.
		Value form;                     ///< The form, if `worked`.
		bool worked = false;            ///< It is a form.
		std::vector<BindingPart> inner; ///< For a form that uses locals: its text when one of them is a name of the
		                                ///< running app (a symbol), else it is worked out (`worked`).
		std::vector<std::string_view> locals; ///< Those locals, when `inner` is used.
		bool asText = false; ///< A `worked` part of a joined text: written as text when it is one (`%text`).
	};
	/// @brief A name a function in a binding gives its item, its index or the value so far: `(fn [card] ...)` in
	/// `(filter (fn [card] ...) list)` writes `card` as `it`.
	struct Rename {
		std::string_view name; ///< The parameter.
		std::string as;        ///< What the expression calls it: `it`, `index` or `acc`.
		bool hidden = false;   ///< A function inside gives the same name another meaning: it can't be written.
	};

	/// @brief The special forms by name. @return The table.
	static const std::unordered_map<std::string_view, SpecialForm>& specialForms();

	/// @brief Compiles a form.
	/// @param form The form.
	/// @param position What can follow it.
	void compile(const Value& form, Position position);
	/// @brief Compiles a condition's test, noting one that tests a path of the running app with the language's truth
	/// (Program::truthTests()): a dotted word, `(not word)`, or an operand of `and` or `or` holding one.
	/// @param test The test.
	void compileTest(const Value& test);
	/// @brief Notes a test of compileTest(), and the operands of an `and` or an `or` in it.
	/// @param test The test.
	void noteTruthTest(const Value& test);
	/// @brief Compiles a list: a special form, a macro call or a call.
	/// @param form The list.
	/// @param position What can follow it.
	void compileList(const Value& form, Position position);
	/// @brief Compiles a call.
	/// @param form The list.
	/// @param position What can follow it.
	void compileCall(const Value& form, Position position);
	/// @brief Compiles a word.
	/// @param form The symbol.
	void compileSymbol(const Value& form);
	/// @brief Compiles a vector literal.
	/// @param form The vector.
	void compileVector(const Value& form);
	/// @brief Compiles a map literal.
	/// @param form The map.
	void compileMap(const Value& form);
	/// @brief Compiles forms in order, the value being the last's.
	/// @param form The list holding them.
	/// @param from The first one's index.
	/// @param position What can follow them.
	void compileBody(const Value& form, std::size_t from, Position position);
	/// @brief Emits a push of a global. @param name Its name. @param place Where it is used.
	void compileGlobal(std::string_view name, Place place);
	/// @brief Compiles a dotted word that is a path: a local's member (`room.name`), or what the running app reads
	/// (`state.count`), when no definition has that name.
	/// @param form The symbol.
	/// @return false if it isn't one.
	bool compilePath(const Value& form);

	// A top-level template's `~form` that needs the running app (CompilerBindings.cpp)
	/// @brief Tells whether a word needs the running app: a path, or what nothing defines.
	/// @param name The word.
	/// @return true if it does.
	bool isLiveWord(std::string_view name);
	/// @brief Tells whether a global holds a function, which no tree can hold.
	/// @param global The global, or null.
	/// @return true if it does.
	static bool namesFunction(const Global* global);
	/// @brief Tells whether a list needs the running app: its head does, or what it is given.
	/// @param form The list.
	/// @param depth How deep it is.
	/// @return true if it does.
	bool isLiveCall(const Value& form, int depth);
	/// @brief Tells whether a form needs the running app: it names something that nothing defines, or a path.
	/// @param form The form.
	/// @param depth How deep it is.
	/// @return true if it does: what it gives is written as a binding.
	bool isLive(const Value& form, int depth);
	/// @brief Tells whether a function needs the running app: what it reaches by name reads a path, is a host's
	/// primitive that isn't pure, or is what nothing defines.
	/// @param function The function's global.
	/// @return true if it does.
	bool reachesRunningApp(Global& function);
	/// @brief Compiles a form that needs the running app to the text of a binding: `"{{!state.open}}"`.
	/// @param form The form.
	void compileBinding(const Value& form);
	/// @brief Compiles a form that needs the running app to a name of the running app: a symbol whose text is its
	/// expression in parentheses, as `(t('key'))`, which a view's templates then write as they write a repeat's item.
	/// @param form The form.
	void compileLiveName(const Value& form);
	/// @brief Compiles a view's call in a template: an argument that needs the running app is given as a name of it
	/// (compileLiveName()), so `(caption (t "key"))` works out its binding in the view.
	/// @param form The call.
	/// @param depth How deep it is.
	void compileViewCall(const Value& form, int depth);
	/// @brief Compiles a `~form` of a template that builds a view: a binding if it needs the running app, or if a local
	/// it uses holds a name of the running app (a symbol, as `repeat` and a component's parameters give), decided when
	/// it runs; else its value.
	/// @param written The form.
	/// @param depth How deep it is.
	void compileViewValue(const Value& written, int depth);
	/// @brief A form with the macro its head names expanded, again until its head is no macro: in a binding a macro
	/// writes the form that is then written as an expression.
	/// @param form The form.
	/// @return The form expanded, or itself.
	Value expanded(const Value& form);
	/// @brief Tells whether a form is worked out at the build whatever it uses: a view's call, a special form that
	/// isn't an operator, a macro's call, or one of the language's own forms for views (`handler`, `repeat`).
	/// @param form The form.
	/// @return true if it is.
	bool buildsAtOnce(const Value& form);
	/// @brief Tells whether a word is a local's, or a path through one (`card.title`).
	/// @param name The word.
	/// @return true if it is.
	bool isLocalWord(std::string_view name);
	/// @brief Adds the locals a form uses, each once.
	/// @param form The form.
	/// @param depth How deep it is.
	/// @param locals Gets them.
	void localsIn(const Value& form, int depth, std::vector<std::string_view>& locals);
	/// @brief Emits a test that is true when one of some locals holds a symbol.
	/// @param locals The locals.
	void emitSymbolicTest(const std::vector<std::string_view>& locals);
	/// @brief Emits what makes a binding's text from its parts, without the braces it may have among them.
	/// @param parts The parts.
	/// @param joined Whose text `str` joined, which stays text even if it is one placeholder.
	void emitParts(const std::vector<BindingPart>& parts, bool joined = false);
	/// @brief Adds a part the build works out, or, if it uses a local, one whose text depends on that local.
	/// @param form The form.
	/// @param depth How deep it is.
	/// @param parts Gets it.
	void addWorkedPart(const Value& form, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes one of the language's own functions as an expression, when the expression has it.
	/// @param form The call.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @return Whether it did.
	bool lowerLibrary(const Value& form, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes a call of a function on each item: `(filter (fn [x] ...) list)` as `filter(list, ...)`.
	/// @param form The call.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @return Whether it is one.
	bool lowerItemFunction(const Value& form, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes a function's body, its parameters named as the expression names them.
	/// @param function The `(fn [params] body)` form.
	/// @param names What the expression calls each parameter, in order.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	void lowerFunctionBody(const Value& function, std::span<const std::string_view> names, int depth,
	                       std::vector<BindingPart>& parts);
	/// @brief Writes a member's key after what it is read from: `.title`, `['a b']` or `[index]`.
	/// @param key The key's form.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	void lowerKey(const Value& key, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes `str` as the expression's `+`, which joins text.
	/// @param form The call.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	void lowerStr(const Value& form, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes a word: a path, a local's value, or an item's name.
	/// @param word The symbol.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	void lowerWord(const Value& word, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes a form as an expression, into parts of a binding's text.
	/// @param written The form.
	/// @param depth How deep it is.
	/// @param parts Gets the text, and the forms the build works out.
	void lowerForm(const Value& written, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes a list's items as an expression's, between an opening and a ')'.
	/// @param form The list.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @param open What comes first.
	/// @param separator What goes between the items.
	void lowerList(const Value& form, int depth, std::vector<BindingPart>& parts, const std::string& open,
	               const std::string& separator);
	/// @brief Writes a list as an expression's operator, if it is one.
	/// @param form The list.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @return Whether it is.
	bool lowerOperator(const Value& form, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes a list as an expression: an operator, a function of the program, or an expression's own function.
	/// @param form The list.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	void lowerCall(const Value& form, int depth, std::vector<BindingPart>& parts);

	// The special forms (CompilerForms.cpp)
	void formQuote(const Value& form, Position position);
	void formBackquote(const Value& form, Position position);
	void formIf(const Value& form, Position position);
	void formDo(const Value& form, Position position);
	void formWhen(const Value& form, Position position);
	void formCond(const Value& form, Position position);
	void formAnd(const Value& form, Position position);
	void formOr(const Value& form, Position position);
	void formLet(const Value& form, Position position);
	void formLoop(const Value& form, Position position);
	void formRecur(const Value& form, Position position);
	void formFn(const Value& form, Position position);
	void formTry(const Value& form, Position position);
	void formThrow(const Value& form, Position position);
	void formMap(const Value& form, Position position);
	void formMisplaced(const Value& form, Position position);
	void formRepeat(const Value& form, Position position);
	void formView(const Value& form, Position position);

	/// @brief Compiles a template: words stay data, `~` runs.
	/// @param form The form.
	/// @param level How many backquotes it is inside.
	/// @param depth How deep it is in the template.
	void quasi(const Value& form, int level, int depth);
	/// @brief Compiles a template's `~` or `~@`.
	/// @param form It.
	/// @param level How many backquotes it is inside.
	/// @param depth How deep it is in the template.
	void quasiUnquote(const Value& form, int level, int depth);
	/// @brief Compiles a template's list or vector.
	/// @param form It.
	/// @param from The first item.
	/// @param kind Kind::List or Kind::Vector.
	/// @param level How many backquotes it is inside.
	/// @param depth How deep it is in the template.
	void quasiSequence(const Value& form, std::size_t from, Kind kind, int level, int depth);
	/// @brief Tells whether a template has anything that runs at a level.
	/// @param form The form.
	/// @param level How many backquotes it is inside.
	/// @param depth How deep it is in the template.
	/// @return true if it has.
	bool quasiRuns(const Value& form, int level, int depth);

	/// @brief Runs a compilation, so that a limit running out inside it names the place.
	/// @param compilation What compiles.
	/// @return The code.
	const Code* compiling(const std::function<const Code*()>& compilation) const;
	/// @brief Starts a function's compilation. @param state Its state. @param name Its own name, or "".
	void begin(Function& state, std::string_view name);
	/// @brief Ends a function's compilation: its value is the top. @param state Its state. @return The code.
	const Code* finish(Function& state);
	/// @brief Compiles a function's parameters and body into a new function.
	/// @param form The whole form.
	/// @param from The index of its name, doc or parameters.
	/// @return The code.
	const Code* compileFunction(const Value& form, std::size_t from);
	/// @brief Compiles a function's parameters.
	/// @param params The parameter vector.
	void parameters(const Value& params);

	/// @brief Binds a pattern to the value in a slot: a symbol, a vector or a map.
	/// @param pattern The pattern.
	/// @param slot The slot.
	void destructure(const Value& pattern, std::uint32_t slot);
	/// @brief Binds a vector pattern: `[a b & more :as all]`.
	/// @param pattern The pattern.
	/// @param slot The slot.
	void destructureVector(const Value& pattern, std::uint32_t slot);
	/// @brief Binds a map pattern: `{:keys [a b] :or {a 1} :as m x :x}`.
	/// @param pattern The pattern.
	/// @param slot The slot.
	void destructureMap(const Value& pattern, std::uint32_t slot);
	/// @brief Binds one of a map pattern's options: `:keys`, `:as`, or `:or`, read already.
	/// @param pattern The pattern.
	/// @param word The option.
	/// @param value Its value.
	/// @param defaults The pattern's `:or`, or nil.
	/// @param slot The slot of the map.
	void destructureOption(const Value& pattern, std::string_view word, const Value& value, const Value& defaults,
	                       std::uint32_t slot);
	/// @brief Binds one member of a map pattern.
	/// @param pattern The name or the pattern.
	/// @param key The key.
	/// @param defaults The pattern's `:or` map, or nil.
	/// @param slot The slot of the map.
	void destructureMember(const Value& pattern, std::string_view key, const Value& defaults, std::uint32_t slot);
	/// @brief Emits a call of one of the language's own functions on a slot's value and a constant.
	/// @param name The function.
	/// @param slot The slot.
	/// @param argument The constant.
	/// @param place Where it comes from.
	void emitNativeOnSlot(std::string_view name, std::uint32_t slot, const Value& argument, Place place);
	/// @brief Compiles the bindings of `let` or `loop`.
	/// @param bindings The vector.
	void bindings(const Value& bindings);

	/// @brief The variable a name is, looking through enclosing functions.
	/// @param function The function to look from.
	/// @param name The name.
	/// @return What it is.
	Variable resolve(Function& function, std::string_view name);
	/// @brief A new slot. @return It.
	std::uint32_t newSlot();
	/// @brief Emits an instruction.
	/// @param op What it does.
	/// @param a Its first operand.
	/// @param b Its second operand.
	/// @return Its index.
	std::uint32_t emit(Op op, std::uint32_t a = 0, std::uint32_t b = 0);
	/// @brief Emits a push of a constant. @param value The constant.
	void emitConstant(const Value& value);
	/// @brief Points a jump at the next instruction. @param jump The jump's index.
	void patch(std::uint32_t jump);
	/// @brief Reports a problem with a form, at its place.
	/// @param form The form.
	/// @param message What is wrong.
	/// @throws LispError Always.
	[[noreturn]] void fail(const Value& form, const std::string& message) const;
	/// @brief Reports a problem at a place.
	/// @param place The place.
	/// @param message What is wrong.
	/// @throws LispError Always.
	[[noreturn]] void failAt(Place place, const std::string& message) const;

	Machine& machine_;            ///< See the constructor.
	CompileContext context_;      ///< See the constructor.
	Function* current_ = nullptr; ///< The function being compiled.
	Place place_;                 ///< The place of what is being compiled.
	bool liveBindings_ = false;   ///< In a top-level template: a `~form` that needs the running app becomes a binding.
	bool inView_ = false;         ///< Building a view: a backquote is a template whose `~form` may become a binding.
	bool compilingHead_ = false;  ///< Compiling a call's head: a word there is the function, never a variable.
	bool functionArgument_ = false; ///< Checking an argument of a function that takes functions (CompilerBindings.cpp).
	bool symbolic_ = false;         ///< Writing a binding where the locals hold names of the running app.
	std::vector<Rename> renames_;   ///< The parameters of the functions a binding is writing, innermost last.
	std::vector<std::string_view> itemNames_;           ///< The parameters of functions a liveness check is inside.
	std::unordered_map<const void*, Value> expansions_; ///< Each macro call's expansion, made once (expanded()).
};

/// @brief A form's head word: a list's first item, if it is a symbol.
/// @param form The form.
/// @return Its text, or "".
[[nodiscard]] std::string_view headWord(const Value& form);

} // namespace affresco::lisp
