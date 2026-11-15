#pragma once

/// @file
/// @brief The compiler: forms into a Code's instructions, macros expanded on the way. Its recursion follows a form's
/// nesting and stops at maxCompileDepth, macro output included.

#include "Machine.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
	/// @brief The special forms' names, for Interpreter::callableNames(). @return Them, in no order.
	[[nodiscard]] static std::vector<std::string_view> specialNames();

	/// @brief Compiles a form as code that takes nothing.
	/// @param form The form.
	/// @return The code.
	const Code* thunk(const Value& form);
	/// @brief Compiles a form as a template that takes nothing: words stay data, `~` runs.
	/// @param form The form.
	/// @return The code.
	const Code* templateThunk(const Value& form);
	/// @brief Compiles `(fn name? "doc"? [params] body...)`, or `(fn name? "doc"? ([params] body...) ...)`.
	/// @param form The form.
	/// @return The code of its function, which captures nothing here.
	/// @throws LispError For a view's function of many arities: a view takes one list of parameters.
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
		Value path{};           ///< The dotted word a `let` gave it, for its truth tests (noteTruthTest()), or nil.
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
	/// A `let`'s name in a binding holds its value's text instead (`parts`): the expression machine has no local names,
	/// so `(let [x e] body)` is `body` with `e` written wherever `x` stands.
	struct Rename {
		std::string_view name; ///< The parameter, or the `let`'s name.
		std::string as;        ///< What the expression calls it: `it`, `index` or `acc`; "" for a `let`'s name.
		bool hidden = false;   ///< A function inside gives the same name another meaning: it can't be written.
		std::vector<BindingPart> parts{}; ///< A `let`'s name: its value's text, in parentheses where it needs them.
		unsigned uses = 0;                ///< A `let`'s name: the items its value's text reads (itemBit()).
		std::size_t size = 0;             ///< A `let`'s name: how long its value's text is (textSize()).
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
	/// @param depth How deep in `and` and `or` it is: as deep as compile() goes, and no deeper, since compile() checks
	/// the test only after it is noted.
	void noteTruthTest(const Value& test, int depth = 0);
	/// @brief Notes a word of noteTruthTest() that reads a path of the running app: dotted, its head no local and the
	/// whole no defined global.
	/// @param word The word.
	/// @param test The test it is in, whose place is noted when the word has none.
	void noteTruthPath(const Value& word, const Value& test);
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
	/// @brief Emits a push of a global: in the library's code, the language's own (Machine::languageGlobal()).
	/// @param name Its name.
	/// @param place Where it is used.
	void compileGlobal(std::string_view name, Place place);
	/// @brief Compiles a qualified word (Namespaces.hpp): the global it names, whatever the locals are.
	/// @param form The symbol.
	/// @return false if it isn't one.
	bool compileQualified(const Value& form);
	/// @brief The name a call calls: its head word, qualified words resolved (`str/join` is `clojure.string/join`).
	/// @param form The form.
	/// @return The name, or "" if its head is no word.
	std::string_view calledName(const Value& form);
	/// @brief The global a name is, if it exists: in the library's code, the language's own.
	/// @param name The name.
	/// @return The cell, or nullptr.
	[[nodiscard]] Global* lookUp(std::string_view name) const;
	/// @brief Refuses a macro of the language's expansion where a word it calls means something else: a local (a
	/// binding's name too), or a definition of the program (LibraryEntry::uses).
	/// @param macro The macro.
	/// @param place The call.
	void checkUses(const Global& macro, Place place);
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
	/// @brief Tells whether a list whose head is no word needs the running app: `(:title card)` when the map does,
	/// `((fn ...) x)` when any part does.
	/// @param form The list.
	/// @param depth How deep it is.
	/// @return true if it does.
	bool isLiveHeadlessCall(const Value& form, int depth);
	/// @brief Tells whether a call needs the running app by its head alone, a word that is no local's.
	/// @param head The head.
	/// @return true or false if the head decides, else nothing: what the call is given decides.
	std::optional<bool> isLiveHead(std::string_view head);
	/// @brief Tells whether `(fn [params] body...)` needs the running app: what its body reads, its parameters being
	/// its own.
	/// @param form The form.
	/// @param depth How deep it is.
	/// @return true if it does.
	bool isLiveFunction(const Value& form, int depth);
	/// @brief Tells whether a `let` needs the running app: one of its values does, or its body, its names being its
	/// own, as a function's parameters are.
	/// @param form The `let`.
	/// @param depth How deep it is.
	/// @return true if it does.
	bool isLiveLet(const Value& form, int depth);
	/// @brief Tells whether what a call is given needs the running app.
	/// @param form The call.
	/// @param depth How deep it is.
	/// @param takesFunctions The call's function takes functions: a word it is given that names one is that function.
	/// @return true if one of its arguments does.
	bool areArgumentsLive(const Value& form, int depth, bool takesFunctions);
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
	/// @brief Tells whether a word is a local's, or a name a binding being checked or written gives (a function's
	/// parameter, a `let`'s name): a macro of that name is hidden there.
	/// @param name The word.
	/// @return true if it is.
	bool isBindingName(std::string_view name);
	/// @brief Tells whether a call's head, as written, names an item the binding writes or a local: never when it is
	/// qualified (`str/join`), which names the language's or a host's function.
	/// @param written The head.
	/// @return true if it does.
	bool isItemOrLocal(std::string_view written);
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
	/// @brief Writes a read of a member, `(get x k)`, `(first x)` or `(get-in x [k...])`, as `x[k]` or `x.k`.
	/// @param form The call.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @return Whether it is one.
	bool lowerMember(const Value& form, int depth, std::vector<BindingPart>& parts);
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
	/// @brief Writes a `let` as its body, each name's value written where the name stands (Rename::parts).
	/// @param form The list.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @return Whether it is a `let`.
	/// @throws LispError For a pattern, a body of several forms, or a text past maxBindingText.
	bool lowerLet(const Value& form, int depth, std::vector<BindingPart>& parts);
	/// @brief Tells whether a value's text is one word, a path or a value the build works out, which needs no
	/// parentheses where a `let`'s name stands: `state.user`, `it.cards[0]`.
	/// @param parts The text.
	/// @return true if it is.
	[[nodiscard]] static bool standsAlone(const std::vector<BindingPart>& parts);
	/// @brief How long a binding's text is, a value the build works out counting workedPartSize.
	/// @param parts The text.
	/// @return Its length.
	[[nodiscard]] static std::size_t textSize(const std::vector<BindingPart>& parts);
	/// @brief Refuses a binding whose `let`s' names, written in, make its text longer than maxBindingText.
	/// @param form The binding's form.
	/// @param parts Its text.
	/// @throws LispError If it is.
	void checkLetText(const Value& form, const std::vector<BindingPart>& parts) const;
	/// @brief Refuses a binding too large as an expression.
	/// @param form Where.
	/// @throws LispError Always.
	[[noreturn]] void failTooLarge(const Value& form) const;
	/// @brief Writes `(if test then else?)` or `(when test then)` as `(test ? then : else)`, if it is one.
	/// @param form The list.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @return Whether it is.
	bool lowerIf(const Value& form, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes `(cond a x b y :else z)` as `(a ? x : (b ? y : z))`, if it is one.
	/// @param form The list.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @return Whether it is.
	bool lowerCond(const Value& form, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes a call of a special form or of one of the language's own functions as the expression's operator
	/// or function, if the expression has it.
	/// @param form The list.
	/// @param global Its head's global, or null.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	/// @return Whether it did.
	bool lowerBuiltIn(const Value& form, const Global* global, int depth, std::vector<BindingPart>& parts);
	/// @brief Writes a call of an expression's own function: one on each item takes a function whose parameters are
	/// named as it names them, `it`, `index` or `acc`.
	/// @param form The list.
	/// @param depth How deep it is.
	/// @param parts Gets the text.
	void lowerOwnFunction(const Value& form, int depth, std::vector<BindingPart>& parts);
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
	/// @brief Compiles a try's body and its catch, `(catch name form ...)` or `(catch Class name form ...)`.
	/// @param form The try.
	/// @param end Where its body ends.
	/// @param handler The catch.
	/// @param position Where the try is.
	void tryCatch(const Value& form, std::size_t end, const Value& handler, Position position);
	/// @brief Compiles some of a form's items, the value of the last; nil if there are none.
	/// @param form The form.
	/// @param from The first.
	/// @param end Past the last.
	/// @param position Where the last is.
	void compileForms(const Value& form, std::size_t from, std::size_t end, Position position);
	void formThrow(const Value& form, Position position);
	void formMap(const Value& form, Position position);
	void formMisplaced(const Value& form, Position position);
	void formRepeat(const Value& form, Position position);
	void formView(const Value& form, Position position);
	/// @brief Reads `repeat`'s options after its item's name and its list, `:index name :key form`, and checks the
	/// names are plain.
	/// @param spec The `[name list ...]` vector.
	/// @param index Gets the index's name, if given.
	/// @param key Gets the key's form, if given.
	void repeatOptions(const Value& spec, Value& index, Value& key);
	/// @brief Writes what `repeat` runs for each item: `(let [card 'card i 'i] [(do body...) (%view key)])`, the item
	/// and the index being names of the running app.
	/// @param form The `repeat` form.
	/// @param name The item's name.
	/// @param index The index's name, or nil.
	/// @param key The key's form, or nil.
	/// @return The form.
	Value repeatBody(const Value& form, const Value& name, const Value& index, const Value& key);

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
	/// @brief Where a function form's parameters are, or its first clause: past its name and its docstring.
	/// @param form The form.
	/// @param from The index of its name, doc or parameters.
	/// @return The index.
	static std::size_t parametersAt(const Value& form, std::size_t from);
	/// @brief Tells whether a function form has clauses, `([a] ...)`, where its parameters would be.
	/// @param form The form.
	/// @param at Where its parameters would be.
	/// @return true if it has.
	static bool hasClauses(const Value& form, std::size_t at);
	/// @brief Writes a function of many arities, `(fn name ([a] ...) ([a b] ...))`, as one function of any number of
	/// arguments that picks its clause by their count and runs it as a loop, so that recur stays in the clause.
	/// @param form The whole form.
	/// @param at Where its first clause is.
	/// @param name Its name, or empty.
	/// @return `(fn name [& %args] ...)`.
	/// @throws LispError If two clauses take as many arguments, or the one with '&' takes fewer than another.
	Value arities(const Value& form, std::size_t at, std::string_view name);
	/// @brief One clause of a function of many arities as a loop that binds its parameters to the arguments.
	/// @param clause The clause, `([a b & more] body...)`.
	/// @param args The name the arguments are in.
	/// @return `(loop [a (%nth args 0) b (%nth args 1) more (%drop args 2)] body...)`, and how many arguments the
	/// clause takes: n, or -(n + 1) for n then '&'.
	/// @throws LispError If '&' isn't before the last parameter.
	std::pair<Value, std::int64_t> arityClause(const Value& clause, const Value& args);
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
	/// @brief Binds one of a map pattern's options: `:keys` (or `:strs`, the same keys in JSON), `:as`, or `:or`, read
	/// already.
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
	unsigned itemsRead_ = 0;        ///< The items the text being written reads (itemBit()), for Rename::uses.
	std::size_t substituted_ = 0;   ///< How much text the binding being written has from its `let`s' names.
	std::vector<std::string_view> itemNames_;           ///< The parameters of functions a liveness check is inside.
	std::unordered_map<const void*, Value> expansions_; ///< Each macro call's expansion, made once (expanded()).
};

/// @brief A form's head word: a list's first item, if it is a symbol.
/// @param form The form.
/// @return Its text, or "".
[[nodiscard]] std::string_view headWord(const Value& form);

} // namespace affresco::lisp
