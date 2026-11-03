/// @file
/// @brief The way out of a program: its primitives, each a step or a read (Programs.hpp), and the step `lisp`, which
/// runs a function from a handler. One rule set governs both: a primitive makes the step and runs it as a handler
/// would.

#include "Programs.hpp"

#include "Engine.hpp"
#include "ExpressionFunctions.hpp"
#include "Limits.hpp"

#include <affresco/lisp/Value.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace affresco::format {

namespace {

/// The steps the language says itself: a program has `if`, `loop`, a function's end. `stop` is a function's end too.
constexpr auto unspoken =
    std::to_array<std::string_view>({"if", "forEach", "while", "stop", "note", "lisp", "runHooks"});

/// @brief The word of the language a step is, for the two the language has for something else.
/// @param step The step's key.
/// @return The word.
std::string wordOfStep(std::string_view step) {
	if(step == "remove") return "remove-state";
	if(step == "update") return "update-record";
	return std::string(step);
}

/// @brief The capability a step's primitive needs.
/// @param step The step's key.
/// @return "review", "control" or "step".
std::string_view capabilityOf(std::string_view step) {
	const StepKind kind = stepKind(step);
	return kind == StepKind::Review ? "review" : kind == StepKind::Control ? "control" : "step";
}

/// @brief Tells whether a step's first value is the function it runs later, with the arguments after it.
/// @param step The step's key.
/// @return true for `after` and `debounce`.
bool schedules(std::string_view step) { return step == "after" || step == "debounce"; }

/// @brief Tells whether a step's key holds steps, where a function runs: `then`, `do`, `onDone`...
/// @param key The key.
/// @return true if it does.
bool holdsSteps(std::string_view key) {
	const bool handler = key.size() > 2 && key.starts_with("on") && key[2] >= 'A' && key[2] <= 'Z';
	return handler || key == "then" || key == "else" || key == "do" || key == "loading";
}

/// @brief Tells whether a value is a function.
/// @param value The value.
/// @return true if it is.
bool isFunction(const lisp::Value& value) {
	return value.kind() == lisp::Kind::Function || value.kind() == lisp::Kind::Native;
}

/// @brief Runs a "lisp" step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runProgramStep(const Running& s) {
	if(!s.step.contains("lisp")) return std::nullopt;
	Engine& engine = s.steps.engine();
	const std::string name = string(s.step["lisp"], s.where + ".lisp");
	if(!s.step.contains("with")) {
		engine.programs.run(name, s.sender, s.event, s.locals, s.where);
		return true;
	}
	const json with = s.value("with");
	if(!with.is_array()) fail(s.where + ".with", "expected a list: the function's arguments, in order");
	std::vector<json> arguments;
	arguments.reserve(with.size());
	for(const json& argument : with) arguments.push_back(argument);
	engine.programs.runWith(name, std::move(arguments), s.sender, s.event, s.locals, s.where);
	return true;
}

} // namespace

void registerProgramSteps(StepRegistry& registry) {
	registry.add({"program", {{"lisp", {"with"}}}, runProgramStep});
	registerLoopSteps(registry);
}

void Programs::definePrimitives(lisp::Interpreter& interpreter) {
	interpreter.definePrimitive("%read",
	                            {.capability = "read",
	                             .pure = false,
	                             .call = [this](lisp::Interpreter& in, std::span<const lisp::Value> arguments) {
		                             return readPath(in, arguments);
	                             }});
	defineSteps(interpreter);
	defineFunctions(interpreter);
	defineLoopPrimitives(interpreter);
}

void Programs::defineSteps(lisp::Interpreter& interpreter) {
	for(const StepGroup& group : builtInSteps()->groups())
		for(const StepType& type : group.steps) {
			if(std::ranges::find(unspoken, type.name) != unspoken.end()) continue;
			interpreter.definePrimitive(
			    wordOfStep(type.name),
			    {.capability = std::string(capabilityOf(type.name)),
				 .pure = false,
				 .call = [this, &type](lisp::Interpreter& in, std::span<const lisp::Value> arguments) {
				     return runStepPrimitive(type, in, arguments);
			     }});
		}
}

void Programs::defineFunctions(lisp::Interpreter& interpreter) {
	for(const detail::Function& function : detail::functions()) {
		if(!function.apply || function.group != detail::Group::Functions || function.name == "lisp") continue;
		const std::string name(function.name);
		if(interpreter.global(name)) continue; // the language's own wins
		interpreter.definePrimitive(
		    name, {.capability = "read",
			       .pure = false,
			       .call = [this, word = name](lisp::Interpreter& in, std::span<const lisp::Value> arguments) {
			           return callFunction(word, in, arguments);
		           }});
	}
}

lisp::Value Programs::viewOf(lisp::Interpreter& interpreter, json value) {
	const std::size_t size = approximateBytes(value, bytesPerEvent);
	lisp::Budget left = interpreter.budget();
	if(left.bytes < size || left.operations < size / 16) {
		const auto [place, definition] = interpreter.place();
		throw lisp::LispError(place.file ? *place.file : "", place.line, place.column, definition,
		                      "the budget for work ran out", true);
	}
	left.bytes -= size;
	left.operations -= size / 16;
	interpreter.setBudget(left);
	return interpreter.viewJson(std::make_shared<const json>(std::move(value)));
}

lisp::Value Programs::readPath(lisp::Interpreter& interpreter, std::span<const lisp::Value> arguments) {
	if(arguments.size() != 1 || arguments[0].kind() != lisp::Kind::String) throw lisp::Refusal("a path is a string");
	const auto [place, definition] = interpreter.place();
	json value = outside(interpreter, [&] {
		return format::lookup(arguments[0].text(), engine_.document.context(*frame_->locals, *frame_->event),
		                      "lisp " + definition);
	});
	return viewOf(interpreter, std::move(value));
}

lisp::Value Programs::callFunction(const std::string& name, lisp::Interpreter& interpreter,
                                   std::span<const lisp::Value> arguments) {
	if(arguments.size() > 32) throw lisp::Refusal("\"" + name + "\" takes at most 32 arguments here");
	Scope scope{frame_->locals, nullptr, json::object(), {}, {}};
	std::string text = "{{" + name + "(";
	for(std::size_t at = 0; at < arguments.size(); ++at) {
		const std::string variable = "lisp" + std::to_string(at);
		scope.vars[variable] = interpreter.toTree(arguments[at]);
		text.append(at == 0 ? "" : ", ").append(variable);
	}
	text += ")}}";
	const auto [place, definition] = interpreter.place();
	try {
		json value = outside(interpreter, [&] {
			return format::evaluate(json(text), engine_.document.context(scope, *frame_->event),
			                        "lisp " + definition + "." + name);
		});
		return viewOf(interpreter, std::move(value));
	} catch(const lisp::LispError&) {
		throw;
	} catch(const DocumentError& error) {
		throw lisp::Refusal(error.what());
	}
}

json Programs::inertValue(lisp::Interpreter& interpreter, const lisp::Value& value, Scope& scope) {
	json written = interpreter.toTree(value);
	if(!written.is_structured() && !(written.is_string() && written.get_ref<const std::string&>().contains("{{")))
		return written;
	const std::string variable = "lisp" + std::to_string(scope.vars.size());
	scope.vars[variable] = std::move(written);
	return "{{" + variable + "}}"; // read as it is: what it holds is never evaluated
}

json Programs::callbackSteps(lisp::Interpreter& interpreter, const lisp::Value& function,
                             std::span<const lisp::Value> arguments, Scope& scope) {
	const std::optional<std::string> name = interpreter.definitionNameOf(function);
	if(!name)
		throw lisp::Refusal(
		    "a callback is a function a definition gives, by its name: what a closure holds goes when the "
		    "event ends");
	json step = {{"lisp", *name}};
	if(!arguments.empty()) {
		json with = json::array();
		for(const lisp::Value& argument : arguments) with.push_back(inertValue(interpreter, argument, scope));
		step["with"] = std::move(with);
	}
	return json::array({std::move(step)});
}

json Programs::stepFrom(const StepType& type, lisp::Interpreter& interpreter, std::span<const lisp::Value> arguments,
                        Scope& scope) {
	json step = json::object();
	const auto put = [&](const std::string& key, const lisp::Value& value) {
		// A name or steps given as a value would run what no name in the code says, past what reaches it (Rules)
		if(holdsSteps(key) && key != "loading" && !isFunction(value)) {
			if(value.isNil()) return;
			if(value.kind() == lisp::Kind::Vector && value.count() > 0 && isFunction(value.at(0))) { // [opened path]
				std::vector<lisp::Value> given;
				for(std::size_t item = 1; item < value.count(); ++item) given.push_back(value.at(item));
				step[key] = callbackSteps(interpreter, value.at(0), given, scope);
				return;
			}
			throw lisp::Refusal("\"" + key + "\" of \"" + type.name +
			                    "\" takes a function of the program, by its name: never a name or steps as a value");
		}
		step[key] = holdsSteps(key) && isFunction(value) ? callbackSteps(interpreter, value, {}, scope)
		                                                 : inertValue(interpreter, value, scope);
	};
	std::size_t at = 0;
	if(arguments.empty() || arguments[0].kind() == lisp::Kind::Keyword) step[type.name] = true; // (undo), (endPreview)
	else put(type.name, arguments[at++]);
	if(schedules(type.name)) { // (after 100 tick 1 2)
		if(at >= arguments.size()) throw lisp::Refusal("\"" + type.name + "\" takes how long, and a function");
		step["do"] = callbackSteps(interpreter, arguments[at], arguments.subspan(at + 1), scope);
		return step;
	}
	if((arguments.size() - at) % 2 != 0) { // (set "state.x" 1), (set "state.x" :word)
		if(type.keys.empty())
			throw lisp::Refusal("\"" + type.name + "\" takes `:key value` pairs after its first value");
		put(type.keys.front(), arguments[at++]);
	}
	for(; at + 1 < arguments.size(); at += 2) {
		if(arguments[at].kind() != lisp::Kind::Keyword)
			throw lisp::Refusal("\"" + type.name + "\" takes `:key value` pairs after its first value");
		put(std::string(arguments[at].text()), arguments[at + 1]);
	}
	return step;
}

lisp::Value Programs::runStepPrimitive(const StepType& type, lisp::Interpreter& interpreter,
                                       std::span<const lisp::Value> arguments) {
	Frame& frame = *frame_;
	if(!frame.sender) throw lisp::Refusal("a value read for the screen runs no step");
	Scope scope{frame.locals, nullptr, json::object(), {}, {}};
	const json step = stepFrom(type, interpreter, arguments, scope);

	std::vector<std::string> ran = interpreter.authorsThatRan();
	if(engine_.authority) ran.insert(ran.end(), engine_.authority->authors.begin(), engine_.authority->authors.end());
	const StepAuthority authority{std::move(ran)};
	const AuthorityScope holder(engine_, &authority);
	const auto [place, definition] = interpreter.place();
	try {
		outside(interpreter, [&] {
			return engine_.runner.runSteps(step, *frame.sender, *frame.event, scope,
			                               "lisp " + definition + "." + type.name);
		});
	} catch(const lisp::LispError& error) {
		if(error.exhausted()) throw;
		throw lisp::Refusal(error.message());
	} catch(const DocumentError& error) {
		const WorkBudget& left = work();
		if(left.steps == 0 || left.operations == 0 || left.bytes == 0) throw; // a limit: no `try` catches it
		throw lisp::Refusal(error.what());
	}
	return {};
}

} // namespace affresco::format
