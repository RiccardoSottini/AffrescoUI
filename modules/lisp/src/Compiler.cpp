/// @file
/// @brief The compiler (Compiler.hpp): functions, variables, calls, destructuring. The special forms are in
/// CompilerForms.cpp.

#include "Compiler.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>

namespace affresco::lisp {

namespace {

/// @brief Counts a level of a recursion, for as long as it lasts.
class DepthGuard {
public:
	/// @brief Enters a level.
	/// @param depth The counter.
	explicit DepthGuard(int& depth) noexcept : depth_(depth) { ++depth_; }
	~DepthGuard() { --depth_; }
	DepthGuard(const DepthGuard&) = delete;
	DepthGuard& operator=(const DepthGuard&) = delete;

private:
	int& depth_; ///< See the constructor.
};

} // namespace

std::string_view headWord(const Value& form) {
	if(form.kind() != Kind::List || form.count() == 0 || form.at(0).kind() != Kind::Symbol) return {};
	return form.at(0).text();
}

Compiler::Compiler(Machine& machine, CompileContext context) : machine_(machine), context_(std::move(context)) {
	if(!context_.authors) context_.authors = machine_.authorSet(context_.author);
}

const Code* Compiler::compiling(const std::function<const Code*()>& compilation) const {
	try {
		return compilation();
	} catch(const Problem& problem) {
		throw LispError(place_.file ? *place_.file : "", place_.line, place_.column, context_.definition,
		                problem.message, problem.exhausted);
	}
}

const Code* Compiler::thunk(const Value& form) {
	return compiling([&] {
		Function state;
		begin(state, {});
		compile(form, {.tail = true, .recur = false});
		return finish(state);
	});
}

const Code* Compiler::templateThunk(const Value& form) {
	return compiling([&] {
		Function state;
		begin(state, {});
		liveBindings_ = true;
		quasi(form, 1, 0);
		liveBindings_ = false;
		return finish(state);
	});
}

const Code* Compiler::function(const Value& form) {
	return compiling([&] { return compileFunction(form, 1); });
}

void Compiler::begin(Function& state, std::string_view name) {
	state.parent = current_;
	machine_.heap.pin(); // compiled code lasts, and what it holds with it
	state.code = machine_.heap.make<Code>();
	state.code->definition = context_.definition;
	state.code->author = context_.author;
	state.code->library = context_.library;
	state.code->name = std::string(name);
	state.self = name;
	state.authors = context_.authors;
	current_ = &state;
}

const Code* Compiler::finish(Function& state) {
	emit(Op::Return);
	state.code->authors = state.authors;
	current_ = state.parent;
	if(current_) current_->authors = machine_.unite(current_->authors, state.authors); // macros used inside it
	return state.code;
}

void Compiler::compile(const Value& form, Position position) {
	const DepthGuard guard(machine_.compileDepth());
	if(machine_.compileDepth() > maxCompileDepth)
		fail(form, "nested too deeply to compile, more than " + std::to_string(maxCompileDepth) + " levels");
	if(form.place().line != 0) place_ = form.place();
	switch(form.kind()) {
	case Kind::Symbol:
		compileSymbol(form);
		return;
	case Kind::List:
		compileList(form, position);
		return;
	case Kind::Vector:
		compileVector(form);
		return;
	case Kind::Map:
		compileMap(form);
		return;
	default:
		emitConstant(form);
	}
}

void Compiler::compileList(const Value& form, Position position) {
	if(form.count() == 0) {
		emitConstant(form);
		return;
	}
	const std::string_view head = headWord(form);
	if(!head.empty() && resolve(*current_, head).kind == Variable::Kind::None) {
		const auto& specials = specialForms();
		if(const auto special = specials.find(head); special != specials.end()) {
			(this->*special->second)(form, position);
			return;
		}
		if(Global* global = machine_.findGlobal(head); global && global->macro) {
			std::vector<Value> arguments;
			arguments.reserve(form.count() - 1);
			for(std::size_t at = 1; at < form.count(); ++at) arguments.push_back(form.at(at));
			const Value expanded = machine_.expand(*global, arguments, form.place());
			if(const Value macro = global->value; macro.kind() == Kind::Function)
				current_->authors = machine_.unite(current_->authors,
				                                   static_cast<const ClosureObject*>(macro.pointer())->code->authors);
			compile(expanded, position);
			return;
		}
	}
	compileCall(form, position);
}

void Compiler::compileCall(const Value& form, Position position) {
	compile(form.at(0), {});
	for(std::size_t at = 1; at < form.count(); ++at) compile(form.at(at), {});
	place_ = form.place();
	emit(position.tail ? Op::TailCall : Op::Call, static_cast<std::uint32_t>(form.count() - 1));
}

void Compiler::compileSymbol(const Value& form) {
	const std::string_view name = form.text();
	const Variable variable = resolve(*current_, name);
	if(form.place().line != 0) place_ = form.place();
	switch(variable.kind) {
	case Variable::Kind::Local:
		emit(Op::Local, variable.index);
		return;
	case Variable::Kind::Capture:
		emit(Op::Capture, variable.index);
		return;
	case Variable::Kind::Self:
		emit(Op::Self);
		return;
	default:
		break;
	}
	if(isSpecial(name)) fail(form, "\"" + notation::Lexer::excerpt(name) + "\" is a form of the language, not a value");
	if(!compilePath(form)) compileGlobal(name, form.place());
}

bool Compiler::compilePath(const Value& form) {
	const std::string_view name = form.text();
	const std::size_t dot = name.find('.');
	if(dot == std::string_view::npos || context_.library) return false;
	const std::string_view head = name.substr(0, dot);
	if(resolve(*current_, head).kind == Variable::Kind::None) {
		const Global* global = machine_.findGlobal(name);
		if(global && global->state != Global::State::Undefined) return false; // a definition of that name
		compileGlobal("%read", form.place());
		emitConstant(machine_.heap.string(name));
		emit(Op::Call, 1);
		return true;
	}
	std::vector<std::string_view> keys; // room.name.first: a local's members
	for(std::size_t at = dot + 1; at <= name.size();) {
		const std::size_t end = std::min(name.find('.', at), name.size());
		keys.push_back(name.substr(at, end - at));
		at = end + 1;
	}
	const Value get = machine_.global("%get").value;
	for(std::size_t at = 0; at < keys.size(); ++at) emitConstant(get);
	compileSymbol(machine_.heap.symbol(head, form.place()));
	for(const std::string_view key : keys) {
		const bool index =
		    !key.empty() && key.size() < 18 && std::ranges::all_of(key, [](char c) { return c >= '0' && c <= '9'; });
		emitConstant(index ? Value::integer(std::stoll(std::string(key))) : machine_.heap.string(key));
		emit(Op::Call, 2);
	}
	return true;
}

void Compiler::compileGlobal(std::string_view name, Place place) {
	Global& global = machine_.global(name);
	if(global.macro) failAt(place, "\"" + notation::Lexer::excerpt(global.name) + "\" is a macro, not a value");
	if(context_.library && global.state == Global::State::Ready) {
		emitConstant(global.value);
		return;
	}
	current_->code->globals.push_back(&global);
	emit(Op::Global, static_cast<std::uint32_t>(current_->code->globals.size() - 1));
	if(!context_.library) machine_.noteUse({&global, place, context_.definition});
}

void Compiler::compileVector(const Value& form) {
	bool constant = true;
	for(std::size_t at = 0; at < form.count() && constant; ++at) {
		const Kind kind = form.at(at).kind();
		constant = kind != Kind::Symbol && kind != Kind::List && kind != Kind::Vector && kind != Kind::Map;
	}
	if(constant) {
		emitConstant(form);
		return;
	}
	for(std::size_t at = 0; at < form.count(); ++at) compile(form.at(at), {});
	place_ = form.place();
	emit(Op::BuildVector, static_cast<std::uint32_t>(form.count()));
}

void Compiler::compileMap(const Value& form) {
	bool constant = true;
	for(std::size_t at = 0; at < form.count() && constant; ++at) {
		const Kind kind = form.valueAt(at).kind();
		constant = kind != Kind::Symbol && kind != Kind::List && kind != Kind::Vector && kind != Kind::Map;
	}
	if(constant) {
		emitConstant(form);
		return;
	}
	for(std::size_t at = 0; at < form.count(); ++at) {
		emitConstant(machine_.heap.keyword(form.keyAt(at)));
		compile(form.valueAt(at), {});
	}
	place_ = form.place();
	emit(Op::BuildMap, static_cast<std::uint32_t>(form.count()));
}

void Compiler::compileBody(const Value& form, std::size_t from, Position position) {
	if(from >= form.count()) {
		emitConstant({});
		return;
	}
	for(std::size_t at = from; at + 1 < form.count(); ++at) {
		compile(form.at(at), {});
		emit(Op::Pop);
	}
	compile(form.at(form.count() - 1), position);
}

const Code* Compiler::compileFunction(const Value& form, std::size_t from) {
	std::size_t at = from;
	std::string_view name;
	if(at < form.count() && form.at(at).kind() == Kind::Symbol) name = form.at(at++).text();
	if(at < form.count() && form.at(at).kind() == Kind::String && at + 1 < form.count()) ++at; // its docstring
	if(at >= form.count() || form.at(at).kind() != Kind::Vector)
		fail(form, "expected the function's parameters, a vector");
	Function state;
	begin(state, name);
	parameters(form.at(at));
	compileBody(form, at + 1, {.tail = true, .recur = true});
	return finish(state);
}

void Compiler::parameters(const Value& params) {
	Code& code = *current_->code;
	std::vector<std::pair<Value, std::uint32_t>> patterns;
	for(std::size_t at = 0; at < params.count(); ++at) {
		const Value parameter = params.at(at);
		const bool rest = parameter.kind() == Kind::Symbol && parameter.text() == "&";
		if(rest && at + 2 != params.count()) fail(params, "'&' comes before the last parameter");
		const Value pattern = rest ? params.at(++at) : parameter;
		code.variadic = rest;
		code.arity += rest ? 0 : 1;
		if(!rest) code.parameters.emplace_back(pattern.kind() == Kind::Symbol ? pattern.text() : std::string_view());
		patterns.emplace_back(pattern, newSlot());
	}
	const auto count = static_cast<std::uint32_t>(patterns.size());
	current_->recurs.push_back({0, 0, count});
	for(const auto& [pattern, slot] : patterns) destructure(pattern, slot);
}

void Compiler::destructure(const Value& pattern, std::uint32_t slot) {
	const DepthGuard guard(machine_.compileDepth());
	if(machine_.compileDepth() > maxCompileDepth)
		fail(pattern, "a pattern nested more than " + std::to_string(maxCompileDepth) + " levels");
	switch(pattern.kind()) {
	case Kind::Symbol:
		current_->locals.push_back({pattern.text(), slot});
		return;
	case Kind::Vector:
		destructureVector(pattern, slot);
		return;
	case Kind::Map:
		destructureMap(pattern, slot);
		return;
	default:
		if(headWord(pattern) == "%map") {
			destructureMap(pattern, slot);
			return;
		}
		fail(pattern, "expected a name, a vector or a map to bind, not " + std::string(describeKind(pattern)));
	}
}

void Compiler::emitNativeOnSlot(std::string_view name, std::uint32_t slot, const Value& argument, Place place) {
	place_ = place;
	emitConstant(machine_.global(name).value);
	emit(Op::Local, slot);
	emitConstant(argument);
	emit(Op::Call, 2);
}

void Compiler::destructureVector(const Value& pattern, std::uint32_t slot) {
	std::int64_t index = 0;
	for(std::size_t at = 0; at < pattern.count(); ++at) {
		const Value item = pattern.at(at);
		if(item.kind() == Kind::Keyword && item.text() == "as" && at + 1 < pattern.count()) {
			destructure(pattern.at(++at), slot);
			continue;
		}
		const bool rest = item.kind() == Kind::Symbol && item.text() == "&";
		if(rest && at + 1 >= pattern.count()) fail(pattern, "'&' needs a name after it");
		const Value target = rest ? pattern.at(++at) : item;
		emitNativeOnSlot(rest ? "%drop" : "%nth", slot, Value::integer(index++), pattern.place());
		const std::uint32_t into = newSlot();
		emit(Op::SetLocal, into);
		destructure(target, into);
	}
}

void Compiler::destructureMember(const Value& pattern, std::string_view key, const Value& defaults,
                                 std::uint32_t slot) {
	emitNativeOnSlot("%get", slot, machine_.heap.string(key), pattern.place());
	if(pattern.kind() == Kind::Symbol) { // `:or {a 1}`: a name as a key, so read as `(%map a 1)`
		for(std::size_t at = 1; at + 1 < defaults.count() && defaults.kind() == Kind::List; at += 2) {
			if(defaults.at(at).text() != pattern.text()) continue;
			const std::uint32_t jump = emit(Op::JumpIfNotNilKeep);
			compile(defaults.at(at + 1), {});
			patch(jump);
			break;
		}
	}
	const std::uint32_t into = newSlot();
	emit(Op::SetLocal, into);
	destructure(pattern, into);
}

void Compiler::destructureOption(const Value& pattern, std::string_view word, const Value& value, const Value& defaults,
                                 std::uint32_t slot) {
	if(word == "or") return;
	if(word == "as") {
		destructure(value, slot);
		return;
	}
	if(word != "keys" || value.kind() != Kind::Vector)
		fail(pattern, "a map pattern takes :keys, :or, :as, or a name then a key: {x :x}");
	for(std::size_t at = 0; at < value.count(); ++at) {
		if(value.at(at).kind() != Kind::Symbol) fail(pattern, ":keys takes a vector of names");
		destructureMember(value.at(at), value.at(at).text(), defaults, slot);
	}
}

void Compiler::destructureMap(const Value& pattern, std::uint32_t slot) {
	const bool computed = pattern.kind() == Kind::List; // (%map key value ...): a name as a key
	const std::size_t size = computed ? (pattern.count() - 1) / 2 : pattern.count();
	const auto first = [&](std::size_t at) {
		return computed ? pattern.at(1 + at * 2) : machine_.heap.keyword(pattern.keyAt(at));
	};
	const auto second = [&](std::size_t at) { return computed ? pattern.at(2 + at * 2) : pattern.valueAt(at); };
	Value defaults;
	for(std::size_t at = 0; at < size; ++at)
		if(first(at).kind() == Kind::Keyword && first(at).text() == "or") defaults = second(at);
	for(std::size_t at = 0; at < size; ++at) {
		const Value key = first(at);
		if(key.kind() == Kind::Keyword) {
			destructureOption(pattern, key.text(), second(at), defaults, slot);
			continue;
		}
		const Value name = second(at); // {x :x}: the pattern where a key would be
		if(name.kind() != Kind::Keyword && name.kind() != Kind::String)
			fail(pattern, "the key of a pattern is a keyword or a string");
		destructureMember(key, name.text(), defaults, slot);
	}
}

Compiler::Variable Compiler::resolve(Function& function, std::string_view name) {
	machine_.spend((function.locals.size() + function.captureNames.size()) / 8); // both are searched, whatever is written
	for(const Local& local : std::views::reverse(function.locals))
		if(local.name == name) return {Variable::Kind::Local, local.slot};
	if(const auto captured = std::ranges::find(function.captureNames, name); captured != function.captureNames.end())
		return {Variable::Kind::Capture, static_cast<std::uint32_t>(captured - function.captureNames.begin())};
	if(!function.self.empty() && function.self == name) return {Variable::Kind::Self, 0};
	if(!function.parent) return {};
	const Variable outer = resolve(*function.parent, name);
	if(outer.kind == Variable::Kind::None) return {};
	if(outer.kind == Variable::Kind::Self) { // its own closure, seen from inside: captured like any value
		const auto index = static_cast<std::uint32_t>(function.captureNames.size());
		function.captureNames.push_back(name);
		function.code->captures.push_back({true, selfCapture});
		return {Variable::Kind::Capture, index};
	}
	const auto index = static_cast<std::uint32_t>(function.captureNames.size());
	function.captureNames.push_back(name);
	function.code->captures.push_back({outer.kind == Variable::Kind::Local, outer.index});
	return {Variable::Kind::Capture, index};
}

std::uint32_t Compiler::newSlot() {
	const std::uint32_t slot = current_->nextSlot++;
	current_->code->slots = std::max(current_->code->slots, current_->nextSlot);
	return slot;
}

std::uint32_t Compiler::emit(Op op, std::uint32_t a, std::uint32_t b) {
	Code& code = *current_->code;
	machine_.heap.charge(sizeof(Instruction) + sizeof(Place));
	code.ops.push_back({op, a, b});
	code.places.push_back(place_);
	return static_cast<std::uint32_t>(code.ops.size() - 1);
}

void Compiler::emitConstant(const Value& value) {
	Code& code = *current_->code;
	code.constants.push_back(value);
	emit(Op::Const, static_cast<std::uint32_t>(code.constants.size() - 1));
}

void Compiler::patch(std::uint32_t jump) {
	current_->code->ops[jump].a = static_cast<std::uint32_t>(current_->code->ops.size());
}

void Compiler::fail(const Value& form, const std::string& message) const {
	failAt(form.place().line != 0 ? form.place() : place_, message);
}

void Compiler::failAt(Place place, const std::string& message) const {
	throw LispError(place.file ? *place.file : "", place.line, place.column, context_.definition, message);
}

} // namespace affresco::lisp
