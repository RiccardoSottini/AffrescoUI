/// @file
/// @brief The machine (Machine.hpp): one function per instruction, and calls on its own stack.

#include "Machine.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <array>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace affresco::lisp {

namespace {

/// Most values the stack holds: its share of the memory, whatever the call depth.
constexpr std::size_t maxStack = std::size_t{1} << 22U;
/// The stack a run leaves behind it, in values or frames: past this one is given back when its event ends.
constexpr std::size_t keptStack = std::size_t{1} << 14U;
/// Most runs inside runs (a host's primitive calling back, macro expansion): each uses the native stack.
constexpr std::size_t maxNestedRuns = 64;

/// @brief Counts a nested run, for as long as it lasts.
class RunGuard {
public:
	/// @brief Enters a run.
	/// @param nesting The counter.
	explicit RunGuard(std::size_t& nesting) noexcept : nesting_(nesting) { ++nesting_; }
	~RunGuard() { --nesting_; }
	RunGuard(const RunGuard&) = delete;
	RunGuard& operator=(const RunGuard&) = delete;

private:
	std::size_t& nesting_; ///< See the constructor.
};

} // namespace

void Machine::leaveRegion() {
	if(--regions_ != 0) return;
	heap.release(region_);
	if(stack_.capacity() > keptStack && frames_.empty()) std::vector<Value>().swap(stack_);
	if(frames_.capacity() > keptStack && frames_.empty()) std::vector<Frame>().swap(frames_);
}

Machine::Machine(Interpreter& owner, const Limits& limits)
    : owner_(owner), limits_(limits), budget_(limits.budget), macroOperations_(limits.macroOperations) {
	noAuthors_ = authorSet("");
	ran_ = noAuthors_;
	const Budget unlimited{static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)};
	const Budget start = budget_;
	budget_ = unlimited; // what the language itself costs to load isn't a program's
	defineNumbers(*this);
	defineCollections(*this);
	defineText(*this);
	definePrelude(*this);
	budget_ = start;
	heap.setBudget(&budget_);
	stack_.reserve(1024);
}

const std::array<Machine::Handle, static_cast<std::size_t>(Op::Count)>& Machine::handles() noexcept {
	static const std::array<Handle, static_cast<std::size_t>(Op::Count)> table = {&Machine::opConst,
	                                                                              &Machine::opLocal,
	                                                                              &Machine::opSetLocal,
	                                                                              &Machine::opCapture,
	                                                                              &Machine::opSelf,
	                                                                              &Machine::opGlobal,
	                                                                              &Machine::opPop,
	                                                                              &Machine::opJump,
	                                                                              &Machine::opJumpIfFalse,
	                                                                              &Machine::opJumpIfFalseKeep,
	                                                                              &Machine::opJumpIfTrueKeep,
	                                                                              &Machine::opJumpIfNotNilKeep,
	                                                                              &Machine::opCall,
	                                                                              &Machine::opTailCall,
	                                                                              &Machine::opReturn,
	                                                                              &Machine::opClosure,
	                                                                              &Machine::opBuildList,
	                                                                              &Machine::opBuildVector,
	                                                                              &Machine::opBuildMap,
	                                                                              &Machine::opSplice,
	                                                                              &Machine::opRecur,
	                                                                              &Machine::opThrow,
	                                                                              &Machine::opTryBegin,
	                                                                              &Machine::opTryEnd};
	return table;
}

Value Machine::call(const Value& function, std::span<const Value> arguments) {
	const std::vector<Value> copied(arguments.begin(), arguments.end()); // they may stand on the stack
	if(frames_.empty() && nesting_ == 0) ran_ = noAuthors_;              // a primitive calling back is no new start
	const std::size_t floor = frames_.size();
	const std::size_t height = stack_.size();
	const std::size_t handlers = handlers_.size();
	const RunGuard guard(nesting_);
	if(nesting_ > maxNestedRuns) {
		const auto [place, definition] = currentPlace();
		throw LispError(place.file ? *place.file : "", place.line, place.column, definition,
		                "calls through macros or primitives nested too deeply", true);
	}
	stack_.push_back(function);
	stack_.insert(stack_.end(), copied.begin(), copied.end());
	try {
		invoke(static_cast<std::uint32_t>(copied.size()), false);
		run(floor);
	} catch(const Problem& problem) { // raised by the call itself, before a frame took it
		unwind(floor);
		stack_.resize(height);
		handlers_.resize(handlers);
		const auto [place, definition] = currentPlace();
		throw LispError(place.file ? *place.file : "", place.line, place.column, definition, problem.message,
		                problem.exhausted);
	} catch(...) {
		unwind(floor);
		stack_.resize(height);
		handlers_.resize(handlers);
		throw;
	}
	return pop();
}

Value Machine::runThunk(const Code* code) {
	if(frames_.empty() && nesting_ == 0) ran_ = noAuthors_; // a primitive calling back is no new start
	const std::size_t floor = frames_.size();
	const std::size_t height = stack_.size();
	const std::size_t handlers = handlers_.size();
	const RunGuard guard(nesting_);
	pushThunk(code, nullptr);
	try {
		run(floor);
	} catch(...) {
		unwind(floor);
		stack_.resize(height);
		handlers_.resize(handlers);
		throw;
	}
	return pop();
}

Value Machine::force(Global& global) {
	ensureCompiled(global);
	if(global.state == Global::State::Lazy) {
		if(frames_.empty() && nesting_ == 0) ran_ = noAuthors_; // a primitive calling back is no new start
		const std::size_t floor = frames_.size();
		const std::size_t height = stack_.size();
		const RunGuard guard(nesting_);
		global.state = Global::State::Running;
		pushThunk(global.thunk, &global);
		try {
			run(floor);
		} catch(...) {
			unwind(floor);
			stack_.resize(height);
			throw;
		}
	}
	if(global.state != Global::State::Ready)
		throw LispError("", 0, 0, "", "\"" + notation::Lexer::excerpt(global.name) + "\" is not defined");
	return global.value;
}

void Machine::pushThunk(const Code* code, Global* storeTo) {
	stack_.emplace_back(); // where a function would stand
	Frame frame;
	frame.code = code;
	frame.base = stack_.size();
	frame.authors = authorsFor(code);
	frame.storeTo = storeTo;
	noteRan(code);
	stack_.resize(stack_.size() + code->slots);
	frames_.push_back(frame);
}

const AuthorSet* Machine::authorsFor(const Code* code) {
	return frames_.empty() ? code->authors : unite(frames_.back().authors, code->authors);
}

void Machine::run(std::size_t floor) {
	for(;;) {
		try {
			while(frames_.size() > floor) step();
			return;
		} catch(const Problem& problem) {
			raise(problem, floor);
		}
	}
}

void Machine::step() {
	Frame& frame = frames_.back();
	const Instruction& instruction = frame.code->ops[frame.ip++];
	spend(1);
	(this->*handles()[static_cast<std::size_t>(instruction.op)])(instruction);
}

std::pair<Place, std::string> Machine::currentPlace() const {
	for(const Frame& frame : std::views::reverse(frames_)) {
		const Code* code = frame.code->library ? frame.origin : frame.code;
		if(!code) continue;
		const std::uint32_t at = frame.code->library ? frame.originIp : frame.ip;
		const std::uint32_t ip = at > 0 ? at - 1 : 0;
		return {ip < code->places.size() ? code->places[ip] : Place{}, code->definition};
	}
	return {};
}

void Machine::raise(const Problem& problem, std::size_t floor) {
	const std::size_t lowest = [&] {
		std::size_t index = handlers_.size();
		while(index > 0 && handlers_[index - 1].frames > floor) --index;
		return index;
	}();
	if(problem.exhausted || handlers_.size() == lowest) {
		const auto [place, definition] = currentPlace();
		handlers_.resize(lowest);
		unwind(floor);
		throw LispError(place.file ? *place.file : "", place.line, place.column, definition, problem.message,
		                problem.exhausted);
	}
	const Handler handler = handlers_.back();
	handlers_.pop_back();
	unwind(handler.frames);
	stack_.resize(handler.height);
	if(problem.hasValue) {
		stack_.push_back(problem.value);
	} else {
		std::vector<MapEntry> entries;
		entries.push_back({"message", heap.string(problem.message)});
		stack_.push_back(heap.map(std::move(entries)));
	}
	frames_.back().ip = handler.target;
}

void Machine::unwind(std::size_t count) noexcept {
	while(frames_.size() > count) {
		if(Global* global = frames_.back().storeTo) global->state = Global::State::Lazy;
		frames_.pop_back();
	}
}

void Machine::invoke(std::uint32_t count, bool tail) {
	const Value callee = stack_[stack_.size() - count - 1];
	switch(callee.kind()) {
	case Kind::Function:
		enter(static_cast<const ClosureObject*>(callee.pointer()), count, tail);
		return;
	case Kind::Native:
		callNative(static_cast<const NativeObject*>(callee.pointer()), count, tail);
		return;
	case Kind::Keyword:
	case Kind::Map: { // (:a m), (m :a), each with a default
		if(count < 1 || count > 2) throw Problem("a keyword or a map as a function takes one or two arguments");
		const std::span<const Value> arguments(stack_.data() + stack_.size() - count, count);
		const Value map = callee.kind() == Kind::Map ? callee : arguments[0];
		if(map.kind() != Kind::Map && !map.isNil())
			throw Problem("a keyword as a function takes a map, and was given " + std::string(describeKind(map)));
		const std::string_view key = callee.kind() == Kind::Map ? keyText(arguments[0]) : callee.text();
		bool found = false;
		const Value member = map.lookup(key, &found);
		finishCall(found || count == 1 ? member : arguments[1], count);
		return;
	}
	default:
		throw Problem("this isn't a function: it is " + std::string(describeKind(callee)));
	}
}

void Machine::bindArguments(const Code* code, std::uint32_t count) {
	if(count < code->arity || (count > code->arity && !code->variadic)) {
		const std::string name =
		    code->name.empty() ? "this function" : "\"" + notation::Lexer::excerpt(code->name) + "\"";
		throw Problem(name + " takes " + (code->variadic ? "at least " : "") + std::to_string(code->arity) +
		              " arguments, and was given " + std::to_string(count));
	}
	if(!code->variadic) return;
	const std::size_t extra = count - code->arity;
	const auto first = stack_.end() - static_cast<std::ptrdiff_t>(extra);
	std::vector<Value> rest(first, stack_.end());
	stack_.erase(first, stack_.end());
	stack_.push_back(heap.sequence(Kind::Vector, std::move(rest)));
}

void Machine::enter(const ClosureObject* closure, std::uint32_t count, bool tail) {
	const Code* code = closure->code;
	bindArguments(code, count);
	noteRan(code);
	const std::uint32_t params = code->arity + (code->variadic ? 1 : 0);
	if(tail) {
		Frame& frame = frames_.back();
		const auto arguments = stack_.end() - static_cast<std::ptrdiff_t>(params);
		stack_[frame.base - 1] = Value::object(Kind::Function, closure);
		std::move(arguments, stack_.end(), stack_.begin() + static_cast<std::ptrdiff_t>(frame.base));
		stack_.resize(frame.base + code->slots);
		frame.authors = unite(frame.authors, code->authors); // whoever it replaces stays on the record
		if(!frame.code->library) { // an error in the language's own function names who called it
			frame.origin = frame.code;
			frame.originIp = frame.ip;
		}
		frame.code = code;
		frame.closure = closure;
		frame.ip = 0;
		return;
	}
	if(frames_.size() >= limits_.callDepth)
		exhausted("calls nested more than " + std::to_string(limits_.callDepth) + " deep");
	Frame frame;
	frame.code = code;
	frame.closure = closure;
	frame.base = stack_.size() - params;
	frame.authors = authorsFor(code);
	stack_.resize(frame.base + code->slots);
	if(stack_.size() > maxStack) exhausted("the stack holds more than " + std::to_string(maxStack) + " values");
	frames_.push_back(frame);
}

void Machine::callNative(const NativeObject* native, std::uint32_t count, bool tail) {
	if(native->spread) { // (apply f a b items): the items go on the stack, and f is called
		if(count < 2) throw Problem("apply takes a function and a list of arguments");
		const Value items = stack_.back();
		if(!items.isSequence() && !items.isNil())
			throw Problem("apply's last argument is a list or a vector, not " + std::string(describeKind(items)));
		stack_.pop_back();
		spend(items.count());
		if(stack_.size() + items.count() > maxStack)
			exhausted("the stack holds more than " + std::to_string(maxStack) + " values");
		for(std::size_t at = 0; at < items.count(); ++at) stack_.push_back(items.at(at));
		const auto spread = static_cast<std::uint32_t>(count - 2 + items.count());
		stack_.erase(stack_.end() - static_cast<std::ptrdiff_t>(spread) - 2); // apply itself
		invoke(spread, tail);
		return;
	}
	if(native->function) {
		const std::span<const Value> arguments(stack_.data() + stack_.size() - count, count);
		finishCall(native->function(*this, arguments), count);
		return;
	}
	finishCall(callHost(native, std::vector<Value>(stack_.end() - count, stack_.end())), count);
}

Value Machine::callHost(const NativeObject* native, std::vector<Value> arguments) {
	if(pure_ && !native->host.pure)
		throw Problem("\"" + notation::Lexer::excerpt(native->name) +
		              "\" reaches outside the program, and building can't");
	try {
		if(gate_) {
			const std::vector<std::string> authors = authorsOnStack();
			const std::vector<std::string> ran = authorsThatRan();
			const auto [place, definition] = currentPlace();
			gate_({native->name, native->host.capability, authors, ran, place});
		}
		return native->host.call(owner_, arguments);
	} catch(const Refusal& refusal) {
		throw Problem(refusal.what());
	}
}

void Machine::finishCall(Value result, std::uint32_t count) { // a copy: it may stand on the stack
	stack_.resize(stack_.size() - count - 1);
	stack_.push_back(result);
}

std::vector<Value> Machine::takeBuilt(std::uint32_t count) {
	const auto first = stack_.end() - static_cast<std::ptrdiff_t>(count);
	std::vector<Value> items;
	items.reserve(count);
	for(auto item = first; item != stack_.end(); ++item) {
		if(item->kind() != Kind::Splice) {
			items.push_back(*item);
			continue;
		}
		const Value spread = static_cast<const SpliceObject*>(item->pointer())->value;
		if(!spread.isSequence() && !spread.isNil())
			throw Problem("~@ spreads a list or a vector, not " + std::string(describeKind(spread)));
		spend(spread.count());
		for(std::size_t at = 0; at < spread.count(); ++at) items.push_back(spread.at(at));
	}
	stack_.erase(first, stack_.end());
	return items;
}

void Machine::opConst(const Instruction& instruction) {
	stack_.push_back(frames_.back().code->constants[instruction.a]);
}

void Machine::opLocal(const Instruction& instruction) { stack_.push_back(stack_[frames_.back().base + instruction.a]); }

void Machine::opSetLocal(const Instruction& instruction) {
	stack_[frames_.back().base + instruction.a] = stack_.back();
	stack_.pop_back();
}

void Machine::opCapture(const Instruction& instruction) {
	stack_.push_back(frames_.back().closure->captured[instruction.a]);
}

void Machine::opSelf(const Instruction& /*instruction*/) {
	stack_.push_back(Value::object(Kind::Function, frames_.back().closure));
}

void Machine::opGlobal(const Instruction& instruction) {
	Global& global = *frames_.back().code->globals[instruction.a];
	if(global.state == Global::State::Deferred) ensureCompiled(global);
	switch(global.state) {
	case Global::State::Ready:
		stack_.push_back(global.value);
		return;
	case Global::State::Lazy: // work it out, then run this instruction again
		--frames_.back().ip;
		global.state = Global::State::Running;
		pushThunk(global.thunk, &global);
		return;
	case Global::State::Running:
		throw Problem("the value of \"" + notation::Lexer::excerpt(global.name) + "\" depends on itself");
	default:
		throw Problem("\"" + notation::Lexer::excerpt(global.name) + "\" is not defined");
	}
}

void Machine::opPop(const Instruction& /*instruction*/) { stack_.pop_back(); }

void Machine::opJump(const Instruction& instruction) { frames_.back().ip = instruction.a; }

void Machine::opJumpIfFalse(const Instruction& instruction) {
	if(!pop().truthy()) frames_.back().ip = instruction.a;
}

void Machine::opJumpIfFalseKeep(const Instruction& instruction) {
	if(!stack_.back().truthy()) frames_.back().ip = instruction.a;
	else stack_.pop_back();
}

void Machine::opJumpIfTrueKeep(const Instruction& instruction) {
	if(stack_.back().truthy()) frames_.back().ip = instruction.a;
	else stack_.pop_back();
}

void Machine::opJumpIfNotNilKeep(const Instruction& instruction) {
	if(!stack_.back().isNil()) frames_.back().ip = instruction.a;
	else stack_.pop_back();
}

void Machine::opCall(const Instruction& instruction) { invoke(instruction.a, false); }

void Machine::opTailCall(const Instruction& instruction) { invoke(instruction.a, true); }

void Machine::opReturn(const Instruction& /*instruction*/) {
	const Value result = pop();
	const Frame frame = frames_.back();
	frames_.pop_back();
	stack_.resize(frame.base - 1);
	if(frame.storeTo) {
		heap.pin(); // a global lasts, and what it holds with it
		frame.storeTo->value = result;
		frame.storeTo->state = Global::State::Ready;
		return; // the instruction that asked for it runs again
	}
	stack_.push_back(result);
}

void Machine::opClosure(const Instruction& instruction) {
	const Frame& frame = frames_.back();
	const Code* code = frame.code->functions[instruction.a];
	auto* closure = heap.make<ClosureObject>(code->captures.size() * sizeof(Value));
	closure->code = code;
	closure->captured.reserve(code->captures.size());
	for(const CaptureSource& source : code->captures) {
		if(source.index == selfCapture) closure->captured.push_back(Value::object(Kind::Function, frame.closure));
		else
			closure->captured.push_back(source.local ? stack_[frame.base + source.index]
			                                         : frame.closure->captured[source.index]);
	}
	stack_.push_back(Value::object(Kind::Function, closure));
}

void Machine::opBuildList(const Instruction& instruction) {
	const Place place = frames_.back().code->constants[instruction.b].place();
	std::vector<Value> items = takeBuilt(instruction.a);
	stack_.push_back(heap.sequence(Kind::List, std::move(items), place));
}

void Machine::opBuildVector(const Instruction& instruction) {
	const Place place = frames_.back().code->places[frames_.back().ip - 1];
	std::vector<Value> items = takeBuilt(instruction.a);
	stack_.push_back(heap.sequence(Kind::Vector, std::move(items), place));
}

void Machine::opBuildMap(const Instruction& instruction) {
	const auto first = stack_.end() - static_cast<std::ptrdiff_t>(instruction.a) * 2;
	std::vector<MapEntry> entries;
	entries.reserve(instruction.a);
	std::unordered_set<std::string_view> seen;
	for(auto item = first; item != stack_.end(); item += 2) {
		const std::string_view key = keyText(*item);
		if(!seen.insert(key).second) throw Problem("a map with the same key twice");
		entries.push_back({key, *(item + 1)});
	}
	const Place place = frames_.back().code->places[frames_.back().ip - 1];
	stack_.erase(first, stack_.end());
	stack_.push_back(heap.map(std::move(entries), place));
}

void Machine::opSplice(const Instruction& /*instruction*/) {
	auto* splice = heap.make<SpliceObject>();
	splice->value = stack_.back();
	stack_.back() = Value::object(Kind::Splice, splice);
}

void Machine::opRecur(const Instruction& instruction) {
	const std::size_t base = frames_.back().base + instruction.b;
	const auto first = stack_.end() - static_cast<std::ptrdiff_t>(instruction.a);
	std::move(first, stack_.end(), stack_.begin() + static_cast<std::ptrdiff_t>(base));
	stack_.erase(first, stack_.end());
}

void Machine::opThrow(const Instruction& instruction) {
	const Value value = pop();
	const Value written = frames_.back().code->constants[instruction.a];
	// A message from the form itself is shown; one computed from values never is
	throw Problem(written.kind() == Kind::String ? std::string(written.text())
	                                             : "a condition was thrown and nothing caught it",
	              value);
}

void Machine::opTryBegin(const Instruction& instruction) {
	handlers_.push_back({frames_.size(), stack_.size(), instruction.a});
}

void Machine::opTryEnd(const Instruction& /*instruction*/) { handlers_.pop_back(); }

} // namespace affresco::lisp
