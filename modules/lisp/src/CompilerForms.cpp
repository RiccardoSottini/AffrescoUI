/// @file
/// @brief The special forms (Compiler.hpp): what isn't a call. Templates (backquote) are here too.

#include "Compiler.hpp"

#include <affresco/notation/Lexer.hpp>

#include <string>
#include <utility>

namespace affresco::lisp {

const std::unordered_map<std::string_view, Compiler::SpecialForm>& Compiler::specialForms() {
	static const std::unordered_map<std::string_view, SpecialForm> forms = {
	    {"quote", &Compiler::formQuote},
	    {"backquote", &Compiler::formBackquote},
	    {"if", &Compiler::formIf},
	    {"do", &Compiler::formDo},
	    {"when", &Compiler::formWhen},
	    {"cond", &Compiler::formCond},
	    {"and", &Compiler::formAnd},
	    {"or", &Compiler::formOr},
	    {"let", &Compiler::formLet},
	    {"loop", &Compiler::formLoop},
	    {"recur", &Compiler::formRecur},
	    {"fn", &Compiler::formFn},
	    {"try", &Compiler::formTry},
	    {"throw", &Compiler::formThrow},
	    {"%map", &Compiler::formMap},
	    {"catch", &Compiler::formMisplaced},
	    {"unquote", &Compiler::formMisplaced},
	    {"unquote-splicing", &Compiler::formMisplaced},
	    {"def", &Compiler::formMisplaced},
	    {"defn", &Compiler::formMisplaced},
	    {"defmacro", &Compiler::formMisplaced},
	    {"defscreen", &Compiler::formMisplaced},
	    {"defcommand", &Compiler::formMisplaced},
	    {"defkey", &Compiler::formMisplaced},
	    {"defcustom", &Compiler::formMisplaced},
	    {"defhook", &Compiler::formMisplaced},
	    {"add-hook", &Compiler::formMisplaced},
	    {"remove-hook", &Compiler::formMisplaced},
	    {"advice", &Compiler::formMisplaced},
	    {"defview", &Compiler::formMisplaced},
	    {"defcomponent", &Compiler::formMisplaced},
	    {"defstate", &Compiler::formMisplaced},
	    {"defcomputed", &Compiler::formMisplaced},
	    {"defstyle", &Compiler::formMisplaced},
	    {"defmenu", &Compiler::formMisplaced},
	    {"deftokens", &Compiler::formMisplaced},
	    {"include", &Compiler::formMisplaced},
	    {"repeat", &Compiler::formRepeat},
	    {"%view", &Compiler::formView},
	};
	return forms;
}

bool Compiler::isSpecial(std::string_view name) { return specialForms().contains(name); }

void Compiler::formMisplaced(const Value& form, Position /*position*/) {
	const std::string_view head = headWord(form);
	if(head == "catch") fail(form, "catch goes at the end of a try");
	if(head.starts_with("unquote")) fail(form, "~ goes inside a template: a backquoted form, or a data file");
	if(head == "include")
		fail(form, "include names a program's other files: it goes at the top level of the file an app loads, and is "
		           "read with it, never by a revision");
	fail(form, "\"" + notation::Lexer::excerpt(head) + "\" goes at the top level of a program");
}

void Compiler::formQuote(const Value& form, Position /*position*/) {
	if(form.count() != 2) fail(form, "quote takes one form");
	emitConstant(form.at(1));
}

void Compiler::formBackquote(const Value& form, Position /*position*/) {
	if(form.count() != 2) fail(form, "a backquote takes one form");
	// A function's template gives values, whenever it runs; a view's builds the tree, with bindings in it
	const bool live = std::exchange(liveBindings_, inView_);
	quasi(form.at(1), 1, 0);
	liveBindings_ = live;
}

void Compiler::formIf(const Value& form, Position position) {
	if(form.count() < 3 || form.count() > 4) fail(form, "if takes a test, a form and maybe another form");
	compile(form.at(1), {});
	const std::uint32_t otherwise = emit(Op::JumpIfFalse);
	compile(form.at(2), position);
	const std::uint32_t end = emit(Op::Jump);
	patch(otherwise);
	if(form.count() == 4) compile(form.at(3), position);
	else emitConstant({});
	patch(end);
}

void Compiler::formDo(const Value& form, Position position) { compileBody(form, 1, position); }

void Compiler::formWhen(const Value& form, Position position) {
	if(form.count() < 2) fail(form, "when takes a test");
	compile(form.at(1), {});
	const std::uint32_t otherwise = emit(Op::JumpIfFalse);
	compileBody(form, 2, position);
	const std::uint32_t end = emit(Op::Jump);
	patch(otherwise);
	emitConstant({});
	patch(end);
}

void Compiler::formCond(const Value& form, Position position) {
	if(form.count() % 2 == 0) fail(form, "cond takes pairs of a test and a form");
	std::vector<std::uint32_t> ends;
	for(std::size_t at = 1; at + 1 < form.count(); at += 2) {
		const Value test = form.at(at);
		const bool always = test.kind() == Kind::Keyword && test.text() == "else";
		if(always) {
			compile(form.at(at + 1), position);
			ends.push_back(emit(Op::Jump));
			continue;
		}
		compile(test, {});
		const std::uint32_t next = emit(Op::JumpIfFalse);
		compile(form.at(at + 1), position);
		ends.push_back(emit(Op::Jump));
		patch(next);
	}
	emitConstant({});
	for(const std::uint32_t end : ends) patch(end);
}

void Compiler::formAnd(const Value& form, Position position) {
	if(form.count() == 1) {
		emitConstant(Value::boolean(true));
		return;
	}
	std::vector<std::uint32_t> ends;
	for(std::size_t at = 1; at + 1 < form.count(); ++at) {
		compile(form.at(at), {});
		ends.push_back(emit(Op::JumpIfFalseKeep));
	}
	compile(form.at(form.count() - 1), position);
	for(const std::uint32_t end : ends) patch(end);
}

void Compiler::formOr(const Value& form, Position position) {
	if(form.count() == 1) {
		emitConstant({});
		return;
	}
	std::vector<std::uint32_t> ends;
	for(std::size_t at = 1; at + 1 < form.count(); ++at) {
		compile(form.at(at), {});
		ends.push_back(emit(Op::JumpIfTrueKeep));
	}
	compile(form.at(form.count() - 1), position);
	for(const std::uint32_t end : ends) patch(end);
}

void Compiler::bindings(const Value& bindings) {
	if(bindings.kind() != Kind::Vector || bindings.count() % 2 != 0)
		fail(bindings, "expected a vector of names and values, in pairs");
	for(std::size_t at = 0; at < bindings.count(); at += 2) {
		compile(bindings.at(at + 1), {});
		const std::uint32_t slot = newSlot();
		emit(Op::SetLocal, slot);
		destructure(bindings.at(at), slot);
	}
}

void Compiler::formLet(const Value& form, Position position) {
	if(form.count() < 2) fail(form, "let takes a vector of bindings");
	const std::size_t locals = current_->locals.size();
	const std::uint32_t slots = current_->nextSlot;
	bindings(form.at(1));
	compileBody(form, 2, position);
	current_->locals.resize(locals);
	current_->nextSlot = slots;
}

void Compiler::formLoop(const Value& form, Position position) {
	if(form.count() < 2 || form.at(1).kind() != Kind::Vector || form.at(1).count() % 2 != 0)
		fail(form, "loop takes a vector of names and values, in pairs");
	const Value pairs = form.at(1);
	const std::size_t locals = current_->locals.size();
	const std::uint32_t slots = current_->nextSlot;
	const std::uint32_t first = current_->nextSlot;
	const auto count = static_cast<std::uint32_t>(pairs.count() / 2);
	for(std::uint32_t at = 0; at < count; ++at) newSlot(); // the values `recur` sets, side by side
	for(std::uint32_t at = 0; at < count; ++at) {          // each value sees the names before it
		compile(pairs.at(std::size_t{at} * 2 + 1), {});
		emit(Op::SetLocal, first + at);
		destructure(pairs.at(std::size_t{at} * 2), first + at);
	}
	const auto target = static_cast<std::uint32_t>(current_->code->ops.size());
	for(std::uint32_t at = 0; at < count; ++at) // and on every turn, as `recur` set them
		destructure(pairs.at(std::size_t{at} * 2), first + at);
	current_->recurs.push_back({target, first, count});
	compileBody(form, 2, {.tail = position.tail, .recur = true});
	current_->recurs.pop_back();
	current_->locals.resize(locals);
	current_->nextSlot = slots;
}

void Compiler::formRecur(const Value& form, Position position) {
	if(!position.recur) fail(form, "recur goes in tail position of a loop or a function");
	if(current_->recurs.empty()) fail(form, "recur goes inside a loop or a function");
	const RecurTarget target = current_->recurs.back();
	if(form.count() - 1 != target.count)
		fail(form, "recur takes " + std::to_string(target.count) + " values here, one per binding");
	for(std::size_t at = 1; at < form.count(); ++at) compile(form.at(at), {});
	place_ = form.place();
	emit(Op::Recur, target.count, target.first);
	emit(Op::Jump, target.target);
	emitConstant({}); // never reached: the balance of a form that gives a value
}

void Compiler::formFn(const Value& form, Position /*position*/) {
	const Code* code = compileFunction(form, 1);
	current_->code->functions.push_back(code);
	place_ = form.place();
	emit(Op::Closure, static_cast<std::uint32_t>(current_->code->functions.size() - 1));
}

void Compiler::formTry(const Value& form, Position position) {
	if(form.count() < 2 || headWord(form.at(form.count() - 1)) != "catch")
		fail(form, "try ends with (catch name form ...)");
	const Value handler = form.at(form.count() - 1);
	if(handler.count() < 2 || handler.at(1).kind() != Kind::Symbol) fail(handler, "catch takes a name to bind");
	const std::uint32_t begin = emit(Op::TryBegin);
	if(form.count() == 2) emitConstant({});
	for(std::size_t at = 1; at + 1 < form.count(); ++at) {
		compile(form.at(at), {});
		if(at + 2 < form.count()) emit(Op::Pop);
	}
	emit(Op::TryEnd);
	const std::uint32_t end = emit(Op::Jump);
	patch(begin);
	const std::size_t locals = current_->locals.size();
	const std::uint32_t slots = current_->nextSlot;
	const std::uint32_t slot = newSlot();
	emit(Op::SetLocal, slot);
	current_->locals.push_back({handler.at(1).text(), slot});
	compileBody(handler, 2, position);
	current_->locals.resize(locals);
	current_->nextSlot = slots;
	patch(end);
}

void Compiler::formThrow(const Value& form, Position /*position*/) {
	if(form.count() != 2) fail(form, "throw takes one value");
	compile(form.at(1), {});
	place_ = form.place();
	Code& code = *current_->code;
	code.constants.push_back(form.at(1).kind() == Kind::String ? form.at(1) : Value());
	emit(Op::Throw, static_cast<std::uint32_t>(code.constants.size() - 1));
}

void Compiler::formMap(const Value& form, Position /*position*/) {
	if(form.count() % 2 == 0) fail(form, "a map takes pairs of a key and a value");
	for(std::size_t at = 1; at < form.count(); ++at) compile(form.at(at), {});
	place_ = form.place();
	emit(Op::BuildMap, static_cast<std::uint32_t>((form.count() - 1) / 2));
}

bool Compiler::quasiRuns(const Value& form, int level, int depth) {
	if(depth > maxCompileDepth)
		fail(form, "a template nested more than " + std::to_string(maxCompileDepth) + " levels");
	machine_.spend(1);
	const std::string_view head = headWord(form);
	if(head == "%map") return true; // its keys are worked out, or refused, when it runs
	if(head == "unquote" || head == "unquote-splicing") {
		if(level == 1) return true;
		return form.count() == 2 && quasiRuns(form.at(1), level - 1, depth + 1);
	}
	if(head == "backquote" && form.count() == 2) return quasiRuns(form.at(1), level + 1, depth + 1);
	if(form.kind() == Kind::Map) {
		for(std::size_t at = 0; at < form.count(); ++at)
			if(quasiRuns(form.valueAt(at), level, depth + 1)) return true;
		return false;
	}
	if(!form.isSequence()) return false;
	for(std::size_t at = 0; at < form.count(); ++at)
		if(quasiRuns(form.at(at), level, depth + 1)) return true;
	return false;
}

void Compiler::quasiUnquote(const Value& form, int level, int depth) {
	if(headWord(form) == "unquote-splicing" && level == 1) fail(form, "~@ goes inside a list or a vector");
	if(level != 1) {
		quasiSequence(form, 0, Kind::List, level - 1, depth + 1);
	} else if(liveBindings_) {
		compileViewValue(form.at(1), depth);
	} else {
		compile(form.at(1), {});
	}
}

void Compiler::quasi(const Value& form, int level, int depth) {
	if(!quasiRuns(form, level, depth)) {
		emitConstant(form);
		return;
	}
	const std::string_view head = headWord(form);
	if((head == "unquote" || head == "unquote-splicing") && form.count() == 2) {
		quasiUnquote(form, level, depth);
	} else if(head == "backquote" && form.count() == 2) {
		quasiSequence(form, 0, Kind::List, level + 1, depth + 1);
	} else if(head == "%map") {
		if(form.count() % 2 == 0) fail(form, "a map takes pairs of a key and a value");
		for(std::size_t at = 1; at < form.count(); ++at) quasi(form.at(at), level, depth + 1);
		place_ = form.place();
		emit(Op::BuildMap, static_cast<std::uint32_t>((form.count() - 1) / 2));
	} else if(form.kind() == Kind::Map) {
		for(std::size_t at = 0; at < form.count(); ++at) {
			emitConstant(machine_.heap.keyword(form.keyAt(at)));
			quasi(form.valueAt(at), level, depth + 1);
		}
		place_ = form.place();
		emit(Op::BuildMap, static_cast<std::uint32_t>(form.count()));
	} else {
		quasiSequence(form, 0, form.kind(), level, depth + 1);
	}
}

void Compiler::quasiSequence(const Value& form, std::size_t from, Kind kind, int level, int depth) {
	for(std::size_t at = from; at < form.count(); ++at) {
		const Value item = form.at(at);
		if(headWord(item) == "unquote-splicing" && item.count() == 2 && level == 1) {
			compile(item.at(1), {});
			emit(Op::Splice);
		} else {
			quasi(item, level, depth);
		}
	}
	place_ = form.place();
	Code& code = *current_->code;
	code.constants.push_back(form); // a list keeps the place of the form it comes from
	emit(kind == Kind::List ? Op::BuildList : Op::BuildVector, static_cast<std::uint32_t>(form.count() - from),
	     static_cast<std::uint32_t>(code.constants.size() - 1));
}

void Compiler::formView(const Value& form, Position /*position*/) {
	if(form.count() != 2) fail(form, "%view takes one form");
	compileViewValue(form.at(1), 0);
}

void Compiler::formRepeat(const Value& form, Position /*position*/) {
	if(!inView_)
		fail(form, "repeat builds a view's nodes: it goes in a template, a defview, a defcomponent or a defscreen");
	if(form.count() < 3 || form.at(1).kind() != Kind::Vector || form.at(1).count() < 2 || form.at(1).count() % 2 != 0 ||
	   form.at(1).at(0).kind() != Kind::Symbol)
		fail(form, "repeat takes [name list :index name :key form], then the body that gives the node");
	const Value spec = form.at(1);
	const Value name = spec.at(0);
	Value index;
	Value key;
	for(std::size_t at = 2; at < spec.count(); at += 2) {
		const std::string_view option = spec.at(at).kind() == Kind::Keyword ? spec.at(at).text() : "";
		if(option == "index" && spec.at(at + 1).kind() == Kind::Symbol) index = spec.at(at + 1);
		else if(option == "key") key = spec.at(at + 1);
		else fail(spec, "repeat's options are :index name and :key form");
	}
	for(const Value& word : {name, index})
		if(!word.isNil() && (word.text().contains('.') || isSpecial(word.text())))
			fail(spec, "repeat names its item and its index with plain names");
	const Place place = form.place();
	const auto quoted = [&](const Value& word) {
		const std::vector<Value> items = {machine_.heap.symbol("quote", place), word};
		return machine_.heap.sequence(Kind::List, std::span<const Value>(items), place);
	};
	// (let [card 'card i 'i] [(do body...) (%view key)]): the item and the index are names of the running app
	std::vector<Value> bindings = {name, quoted(name)};
	if(!index.isNil()) {
		bindings.push_back(index);
		bindings.push_back(quoted(index));
	}
	std::vector<Value> body = {machine_.heap.symbol("do", place)};
	for(std::size_t at = 2; at < form.count(); ++at) body.push_back(form.at(at));
	std::vector<Value> keyForm = {machine_.heap.symbol("%view", place), key};
	const std::vector<Value> made = {machine_.heap.sequence(Kind::List, std::move(body), place),
	                                 key.isNil() ? key : machine_.heap.sequence(Kind::List, std::move(keyForm), place)};
	const std::vector<Value> let = {machine_.heap.symbol("let", place),
	                                machine_.heap.sequence(Kind::Vector, std::move(bindings), place),
	                                machine_.heap.sequence(Kind::Vector, std::span<const Value>(made), place)};
	emitConstant(machine_.global("%repeat").value);
	compileViewValue(spec.at(1), 0);
	emitConstant(machine_.heap.string(name.text()));
	emitConstant(index.isNil() ? Value() : machine_.heap.string(index.text()));
	compile(machine_.heap.sequence(Kind::List, std::span<const Value>(let), place), {});
	place_ = place;
	emit(Op::Call, 4);
}

} // namespace affresco::lisp
