#pragma once

/// @file
/// @brief format::Programs: a document's live Lisp program (RFC 0005): handlers and bindings call its functions, which
/// read and change the running app only through the existing steps, each checked against who wrote the code that got
/// there (SECURITY.md, "The Lisp language").

#include "Expression.hpp"

#include "affresco/core/Json.hpp"

#include <affresco/lisp/Interpreter.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {
class Panel;
} // namespace affresco

namespace affresco::format {

struct Engine;
struct StepType;

/// @brief What a step may need of whoever reaches it from a program.
enum class StepKind : std::uint8_t {
	None,    ///< Not a step.
	Free,    ///< Any author's code may run it: what a revision could add to a handler.
	Control, ///< Controls changes or reaches outside the screen: only the person's and the app's file's code.
	Review,  ///< Decides on changes: that code, and only from the person's own input.
};

/// @brief What a step needs. A step this file doesn't name as free or review is a control step, so a new one is refused
/// until someone says otherwise.
/// @param name The step's key.
/// @return Its kind; StepKind::None if it isn't a step.
[[nodiscard]] StepKind stepKind(std::string_view name);

/// @brief The step a word of the language runs: its own name, but for those the language has for something else.
/// @param word The word.
/// @return The step's key.
[[nodiscard]] std::string_view stepOfWord(std::string_view word) noexcept;

/// @brief Tells whether an author's code has the authority of the person: the app's own file, or the person.
/// @param author The author: a commit's, as VersionControl::programAuthors() gives it.
/// @return true if it has.
[[nodiscard]] bool isPersonsAuthor(std::string_view author) noexcept;

/// @brief Who a program's entry that nobody is credited with counts as: no one, with the least authority.
inline constexpr std::string_view unknownAuthor = "unknown";

/// @brief Who the steps now running come from, while code of a program runs them, and later for what it scheduled.
struct StepAuthority {
	std::vector<std::string> authors; ///< Every author whose code led here.
};

/// @brief The live program of a document: one interpreter per version of its `program` section, replaced whole when the
/// section changes, so that a removed definition goes, an undo is exact, and what a run allocates goes when it ends.
///
/// Invariants:
/// - Every primitive passes through check(), and every step reached from a program through checkStep().
/// - The authority weighed is that of every author whose code has run since the outermost call began, and those whose
///   code scheduled it: never more than the least of them.
/// - A value read for the screen (read()) runs only what reads.
/// - Nothing a function gets or gives but JSON outlives a call: a callback is a definition's name and JSON.
/// Callers: the step runner, the expression function `lisp`, the engine on a change. Main thread only.
class Programs {
public:
	/// @brief The program of an engine's document.
	/// @param engine The runtime; outlives this object.
	explicit Programs(Engine& engine) : engine_(engine) {}

	/// @brief Takes the document's program, if it changed: its definitions with their authors, compiled and with their
	/// values worked out, purely.
	/// @throws DocumentError If it can't be loaded: the version is refused.
	void load();
	/// @brief What evaluating a text gave.
	struct Evaluated {
		std::string printed;              ///< The last form's value, as Lisp; a definition's address.
		std::optional<std::string> error; ///< What went wrong: names and kinds, no value.
	};
	/// @brief Tells whether the document has a program. @return true if it has.
	[[nodiscard]] bool hasProgram() const noexcept;
	/// @brief Evaluates a text the person's own input gave (the step `evaluate` has checked that): each definition
	/// becomes an entry of the program in one revision of theirs, then each other form runs for its value as code of
	/// theirs.
	/// @param text The text.
	/// @param file The file for a definition no file has.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The variables.
	/// @return What it gave.
	[[nodiscard]] Evaluated evaluate(const std::string& text, const std::string& file, Panel& sender, const json& event,
	                                 const Scope& locals);
	/// @brief Runs the functions of a hook point, each sealed and on its share of the budget (ProgramExtensions.cpp).
	/// @param hook The hook.
	/// @param arguments What they are given, if their authors may see it.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The variables.
	/// @param where Its place, for errors.
	void runHooks(const std::string& hook, std::vector<json> arguments, Panel& sender, const json& event,
	              const Scope& locals, const std::string& where);
	/// @brief Tells whether an action of the document that isn't a function has advice.
	/// @param name The action.
	/// @return true if it has; always false for a document without a program.
	[[nodiscard]] bool advisesAction(std::string_view name) const;
	/// @brief Runs an action with its advice.
	/// @param name The action.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The variables.
	/// @param body What the action does.
	void runAdvised(const std::string& name, Panel& sender, const json& event, const Scope& locals,
	                const std::function<void()>& body);
	/// @brief Tells whether a name is a function of the program, which a handler may run by name.
	/// @param name The name.
	/// @return true if it is; always false for a document without a program.
	[[nodiscard]] bool defines(std::string_view name) const;
	/// @brief Runs a function for a handler, its parameters taken by name from the variables, `event` from the event.
	/// @param name The function.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The variables.
	/// @param where Its place, for errors.
	/// @throws DocumentError If it fails, or is refused.
	void run(const std::string& name, Panel& sender, const json& event, const Scope& locals, const std::string& where);
	/// @brief Runs a function for a handler, with its arguments.
	/// @param name The function.
	/// @param arguments Its arguments.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The variables.
	/// @param where Its place, for errors.
	/// @throws DocumentError If it fails, or is refused.
	void runWith(const std::string& name, std::vector<json> arguments, Panel& sender, const json& event,
	             const Scope& locals, const std::string& where);
	/// @brief Calls a function for a value the screen reads: nothing it reaches can have an effect.
	/// @param name The function.
	/// @param arguments Its arguments.
	/// @param locals The variables.
	/// @param event The event.
	/// @param where Its place, for errors.
	/// @return What it gives.
	/// @throws DocumentError If it fails, or tries to do anything.
	[[nodiscard]] json read(const std::string& name, std::vector<json> arguments, const Scope& locals,
	                        const json& event, const std::string& where);
	/// @brief Refuses a step that code of a program led to, if its authors may not run it. Only called with one of them
	/// running (Engine::authority).
	/// @param step The step.
	/// @param where Its place, for the error.
	/// @throws DocumentError If they may not.
	void checkStep(const json& step, const std::string& where) const;
	/// @brief Refuses an action the application registered, or nobody handles, if the authors running may not reach it:
	/// it is the application's code. Only called with one of them running.
	/// @param name The action.
	/// @param where Its place, for the error.
	/// @throws DocumentError If they may not.
	void checkNative(const std::string& name, const std::string& where) const;

private:
	/// @brief Where a function is called from: what its steps and reads see.
	struct Frame {
		Panel* sender = nullptr;       ///< The panel; nullptr for a value read, which runs no step.
		const json* event = nullptr;   ///< The event.
		const Scope* locals = nullptr; ///< The variables.
	};
	class Entered;
	class Linked;
	struct Share;

	/// @brief A hook's or an advice's function.
	struct ExtensionEntry {
		std::string function;             ///< The function, by name.
		std::string how;                  ///< `before`, `after` or `around`; "" for a hook.
		std::string label;                ///< How a report calls it.
		std::vector<std::string> authors; ///< Who added it: never empty.
	};
	/// @brief The hooks and advice of one version of the program: they go with it.
	struct Extensions {
		std::map<std::string, std::vector<ExtensionEntry>, std::less<>> hooks;  ///< By hook point.
		std::map<std::string, std::vector<ExtensionEntry>, std::less<>> advice; ///< By what they join.
	};
	/// @brief Takes one hook or advice of a new version into its table.
	/// @param interpreter Its interpreter.
	/// @param extension The hook or advice.
	/// @param table The table.
	/// @throws DocumentError If it names what isn't there.
	void addExtension(const lisp::Interpreter& interpreter, const lisp::Extension& extension, Extensions& table) const;
	/// @brief Reads a new version's hooks and advice, and wraps the functions advised.
	/// @param interpreter Its interpreter.
	/// @throws DocumentError If one names what isn't there.
	void loadExtensions(lisp::Interpreter& interpreter);
	/// @brief The share of the budget each of some sealed runs gets.
	/// @param interpreter The interpreter.
	/// @param count How many.
	/// @return It.
	[[nodiscard]] Share shareOf(const lisp::Interpreter& interpreter, std::size_t count) const;
	/// @brief Runs a hook or `:before` or `:after` advice, sealed and on its share. What goes wrong is reported, and
	/// goes no further.
	/// @param interpreter The interpreter.
	/// @param entry It.
	/// @param joined What it joins, for the report.
	/// @param arguments What it is given, if its authors may see it.
	/// @param share Its share.
	void runSealed(lisp::Interpreter& interpreter, const ExtensionEntry& entry, const std::string& joined,
	               std::span<const lisp::Value> arguments, const Share& share);
	/// @brief Calls a function through its `:around` advice.
	/// @param interpreter The interpreter.
	/// @param arounds The advice, innermost first.
	/// @param index How many of them are still to go.
	/// @param original The function.
	/// @param arguments Its arguments.
	/// @return What the outermost gives.
	lisp::Value callAround(lisp::Interpreter& interpreter, const std::vector<const ExtensionEntry*>& arounds,
	                       std::size_t index, const lisp::Value& original, const std::vector<lisp::Value>& arguments);
	/// @brief Calls a function with all its advice.
	/// @param interpreter The interpreter.
	/// @param table The version's advice.
	/// @param target The function's name.
	/// @param original The function.
	/// @param given Its arguments.
	/// @return What it gives.
	lisp::Value callAdvised(lisp::Interpreter& interpreter, const Extensions& table, const std::string& target,
	                        const lisp::Value& original, std::span<const lisp::Value> given);
	/// @brief Runs a hook point's functions, unless it is already running.
	/// @param interpreter The interpreter.
	/// @param hook The hook.
	/// @param arguments What they are given, if their authors may see it.
	void runHookPoint(lisp::Interpreter& interpreter, const std::string& hook, std::span<const lisp::Value> arguments);
	/// @brief Registers what describes the program, and `run-hooks`.
	/// @param interpreter The interpreter.
	void defineLoopPrimitives(lisp::Interpreter& interpreter);
	/// @brief What limits the code running now in what it reads: a view for each limited reader among its authors.
	/// @param interpreter The interpreter.
	/// @return The views; none for code nobody limited wrote.
	[[nodiscard]] std::vector<json> limitedViews(const lisp::Interpreter& interpreter) const;
	/// @brief What is known of a name, unless a view doesn't show it: never a value.
	/// @param interpreter The interpreter.
	/// @param name The name.
	/// @param views The views.
	/// @return It, or null.
	[[nodiscard]] json describeName(lisp::Interpreter& interpreter, const std::string& name,
	                                const std::vector<json>& views) const;
	/// @brief What `describe-function`, `describe-variable`, `describe-key`, `apropos` and `program-file` do.
	/// @param what Which.
	/// @param interpreter The interpreter.
	/// @param arguments Its one argument.
	/// @return What it gives.
	lisp::Value describePrimitive(std::string_view what, lisp::Interpreter& interpreter,
	                              std::span<const lisp::Value> arguments);
	/// @brief Every name holding a text that a view shows, described.
	/// @param interpreter The interpreter.
	/// @param text The text.
	/// @param views The views.
	/// @return The list.
	[[nodiscard]] json aproposOf(lisp::Interpreter& interpreter, const std::string& text,
	                             const std::vector<json>& views) const;
	/// @brief A program's file, its entries one after another, but what a view doesn't show.
	/// @param file The file.
	/// @param views The views.
	/// @return Its text; "" for none.
	[[nodiscard]] json programFileOf(const std::string& file, const std::vector<json>& views) const;

	/// @brief Enters a call: the outermost starts the budget of a tool that runs no event.
	/// @param frame What it is called from.
	/// @return What was running.
	Frame* enter(Frame* frame);
	/// @brief Registers the way out: what reads, every step, the expressions' functions.
	/// @param interpreter The interpreter.
	void definePrimitives(lisp::Interpreter& interpreter);
	/// @brief Registers the steps as primitives.
	/// @param interpreter The interpreter.
	void defineSteps(lisp::Interpreter& interpreter);
	/// @brief Registers the expressions' functions the language doesn't have.
	/// @param interpreter The interpreter.
	void defineFunctions(lisp::Interpreter& interpreter);
	/// @brief The choke point: refuses a primitive its authors, or what is running, may not call.
	/// @param call The call.
	/// @throws lisp::Refusal If they may not.
	void check(const lisp::CallContext& call) const;
	/// @brief Whether authors may run a step, and now.
	/// @param what The step's name, for the reason.
	/// @param kind What it needs.
	/// @param ran The authors whose code led here.
	/// @return Why not; std::nullopt if they may.
	[[nodiscard]] std::optional<std::string> refusal(std::string_view what, StepKind kind,
	                                                 std::span<const std::string> ran) const;
	/// @brief Makes a step from a primitive's arguments.
	/// @param type The step.
	/// @param interpreter The interpreter.
	/// @param arguments Its arguments: the step's first value; then its next, or `:key value` pairs.
	/// @param scope Holds the values that could be taken for an expression.
	/// @return The step.
	/// @throws lisp::Refusal If they aren't what the step takes.
	[[nodiscard]] static json stepFrom(const StepType& type, lisp::Interpreter& interpreter,
	                                   std::span<const lisp::Value> arguments, Scope& scope);
	/// @brief What a primitive of a step does: makes the step from its arguments, and runs it.
	/// @param type The step.
	/// @param interpreter The interpreter.
	/// @param arguments Its arguments: the step's first value; then its next, or `:key value` pairs.
	/// @return nil.
	lisp::Value runStepPrimitive(const StepType& type, lisp::Interpreter& interpreter,
	                             std::span<const lisp::Value> arguments);
	/// @brief Puts a primitive's value in a step so that it is taken as it is, whatever text it holds: no data is ever
	/// evaluated.
	/// @param interpreter The interpreter.
	/// @param value The value.
	/// @param scope Holds it, when it could be taken for an expression.
	/// @return The step's value.
	[[nodiscard]] static json inertValue(lisp::Interpreter& interpreter, const lisp::Value& value, Scope& scope);
	/// @brief The steps that run a named function, for a step's callback.
	/// @param interpreter The interpreter.
	/// @param function The function.
	/// @param arguments Its arguments.
	/// @param scope Holds them.
	/// @return The steps.
	/// @throws lisp::Refusal If it isn't a definition's function: a closure's values go with the event.
	[[nodiscard]] static json callbackSteps(lisp::Interpreter& interpreter, const lisp::Value& function,
	                                        std::span<const lisp::Value> arguments, Scope& scope);
	/// @brief `%read`: the value at a path of the running app.
	/// @param interpreter The interpreter.
	/// @param arguments The path.
	/// @return The value, viewed.
	lisp::Value readPath(lisp::Interpreter& interpreter, std::span<const lisp::Value> arguments);
	/// @brief An expression's function, on values.
	/// @param name Its name.
	/// @param interpreter The interpreter.
	/// @param arguments Its arguments.
	/// @return What it gives, viewed.
	lisp::Value callFunction(const std::string& name, lisp::Interpreter& interpreter,
	                         std::span<const lisp::Value> arguments);
	/// @brief JSON read from the app, as a value: it costs what its size costs.
	/// @param interpreter The interpreter.
	/// @param value The JSON.
	/// @return The value.
	/// @throws lisp::LispError If the budget runs out.
	static lisp::Value viewOf(lisp::Interpreter& interpreter, json value);
	/// @brief The function of a name.
	/// @param interpreter The interpreter.
	/// @param name The name.
	/// @param where Its place, for errors.
	/// @return The function.
	/// @throws DocumentError If it isn't one.
	static lisp::Value functionOf(lisp::Interpreter& interpreter, const std::string& name, const std::string& where);
	/// @brief The work budget of the running event or update.
	/// @return It.
	[[nodiscard]] WorkBudget& work() const;
	/// @brief Runs what a primitive does outside the interpreter, which spends from the event's budget meanwhile: it
	/// may run steps, read values and call functions again.
	/// @tparam Action What it does.
	/// @param interpreter The interpreter.
	/// @param action What it does.
	/// @return What it gives.
	template <class Action> auto outside(lisp::Interpreter& interpreter, const Action& action) const {
		WorkBudget& spent = work();
		const lisp::Budget left = interpreter.budget();
		spent.operations = left.operations;
		spent.bytes = left.bytes;
		/// @brief Gives the interpreter what is left, whatever ends the action.
		class Back {
		public:
			/// @brief Remembers both. @param to The interpreter. @param from The event's budget.
			Back(lisp::Interpreter& to, const WorkBudget& from) : to_(to), from_(from) {}
			/// @brief Gives it.
			~Back() { to_.setBudget({from_.operations, from_.bytes}); }
			Back(const Back&) = delete;
			Back& operator=(const Back&) = delete;
			Back(Back&&) = delete;
			Back& operator=(Back&&) = delete;

		private:
			lisp::Interpreter& to_;  ///< The interpreter.
			const WorkBudget& from_; ///< The event's budget.
		};
		const Back back(interpreter, spent);
		return action();
	}

	Engine& engine_;                                   ///< The runtime.
	std::shared_ptr<lisp::Interpreter> live_;          ///< The program now; running code holds its own.
	json loaded_;                                      ///< The section it was loaded from.
	Frame* frame_ = nullptr;                           ///< What the running function is called from.
	int reading_ = 0;                                  ///< Values being read: nothing but reading runs.
	mutable WorkBudget ownBudget_;                     ///< With no event or update running (a tool).
	bool ownInput_ = false;                            ///< What the person's own input asked to evaluate is running.
	std::shared_ptr<const Extensions> extensions_;     ///< The hooks and advice of live_.
	std::shared_ptr<const Extensions> extensionsNext_; ///< Those of what load() is loading.
	std::vector<std::string> hooksRunning_;            ///< The hook points being run.
};

/// @brief Links a running interpreter's budget to the event's while it lives: the event's going in, and back when it
/// ends, whatever ends it.
class Programs::Linked {
public:
	/// @brief Gives the interpreter what the event may still do.
	/// @param programs The programs.
	/// @param interpreter The interpreter.
	Linked(const Programs& programs, lisp::Interpreter& interpreter)
	    : work_(programs.work()), interpreter_(interpreter) {
		interpreter_.setBudget({work_.operations, work_.bytes});
	}
	/// @brief Gives the event what is left.
	~Linked() {
		const lisp::Budget left = interpreter_.budget();
		work_.operations = left.operations;
		work_.bytes = left.bytes;
	}
	Linked(const Linked&) = delete;
	Linked& operator=(const Linked&) = delete;
	Linked(Linked&&) = delete;
	Linked& operator=(Linked&&) = delete;

private:
	WorkBudget& work_;               ///< The event's.
	lisp::Interpreter& interpreter_; ///< The interpreter's.
};

/// @brief A call of a function, as long as it lasts: it holds its interpreter, whatever happens to the program
/// meanwhile, what it allocates goes when it ends, and what it spends is the event's.
class Programs::Entered {
public:
	/// @brief Enters.
	/// @param programs The programs.
	/// @param frame What it is called from.
	explicit Entered(Programs& programs, Frame frame)
	    : programs_(programs), interpreter_(programs.live_), frame_(frame), outer_(programs.enter(&frame_)),
	      transient_(*interpreter_), linked_(programs, *interpreter_) {}
	/// @brief Leaves.
	~Entered() { programs_.frame_ = outer_; }
	Entered(const Entered&) = delete;
	Entered& operator=(const Entered&) = delete;
	Entered(Entered&&) = delete;
	Entered& operator=(Entered&&) = delete;

	/// @brief The interpreter. @return It.
	[[nodiscard]] lisp::Interpreter& interpreter() const noexcept { return *interpreter_; }

private:
	Programs& programs_;                             ///< The programs.
	std::shared_ptr<lisp::Interpreter> interpreter_; ///< Running code keeps its version.
	Frame frame_;                                    ///< What it is called from.
	Frame* outer_;                                   ///< The call around it.
	lisp::Interpreter::Transient transient_;         ///< What it allocates.
	Linked linked_;                                  ///< What it spends.
};

/// @brief Sets who the steps now running come from, as long as it lives.
class AuthorityScope {
public:
	/// @brief Sets it.
	/// @param engine The runtime.
	/// @param authority Who, or nullptr for none: the screen's own steps.
	AuthorityScope(Engine& engine, const StepAuthority* authority) noexcept;
	/// @brief Puts back what it was.
	~AuthorityScope();
	AuthorityScope(const AuthorityScope&) = delete;
	AuthorityScope& operator=(const AuthorityScope&) = delete;
	AuthorityScope(AuthorityScope&&) = delete;
	AuthorityScope& operator=(AuthorityScope&&) = delete;

private:
	Engine& engine_;             ///< The runtime.
	const StepAuthority* outer_; ///< What it was.
};

} // namespace affresco::format
