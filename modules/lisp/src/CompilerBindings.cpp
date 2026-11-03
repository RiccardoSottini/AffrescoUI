/// @file
/// @brief A template's `~form` that needs the running app (Compiler.hpp). What can be known at the build is worked out
/// at the build; a form that reads a path, or names what nothing defines, stays in the tree as a binding: the
/// expression machine's own text when it can be (CompilerLowering.cpp), a call of the program's function otherwise
/// (`lisp('name', ...)`). In a template that builds a view, a local may hold a name of the running app (a symbol:
/// `repeat`'s item, a component's parameter): a form using one is a binding too, which the build decides as it runs.

#include "Compiler.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affresco::lisp {

std::string expressionText(std::string_view text) {
	std::string quoted = "'";
	for(const char c : text) {
		if(c == '\n') quoted += "\\n";
		else if(c == '\t') quoted += "\\t";
		else if(c == '\\' || c == '\'') quoted.append("\\").push_back(c);
		else quoted += c;
	}
	return quoted += '\'';
}

bool Compiler::reachesRunningApp(Global& function) {
	std::vector<const Code*> waiting;
	std::unordered_set<const Global*> seen = {&function};
	const auto look = [&](Global& global) {
		if(global.name == "%read") return true;
		if(global.library || global.view) return false; // a view's call is worked out at the build
		if(global.state == Global::State::Deferred && global.pending) Machine::ensureCompiled(global);
		switch(global.state) {
		case Global::State::Undefined:
			return true;
		case Global::State::Lazy:
			waiting.push_back(global.thunk);
			return false;
		case Global::State::Ready:
			if(global.value.kind() == Kind::Function)
				waiting.push_back(static_cast<const ClosureObject*>(global.value.pointer())->code);
			return global.value.kind() == Kind::Native &&
			       !static_cast<const NativeObject*>(global.value.pointer())->host.pure;
		default:
			return false;
		}
	};
	if(look(function)) return true;
	while(!waiting.empty()) {
		const Code* code = waiting.back();
		waiting.pop_back();
		machine_.spend(1);
		waiting.insert(waiting.end(), code->functions.begin(), code->functions.end());
		for(Global* global : code->globals)
			if(seen.insert(global).second && look(*global)) return true;
	}
	return false;
}

bool Compiler::isLocalWord(std::string_view name) {
	return resolve(*current_, name.substr(0, name.find('.'))).kind != Variable::Kind::None;
}

bool Compiler::isLiveWord(std::string_view name) {
	const std::string_view head = name.substr(0, name.find('.'));
	if(std::ranges::find(renames_, head, &Rename::name) != renames_.end()) return true; // an item being written
	if(std::ranges::find(itemNames_, head) != itemNames_.end()) return false;
	if(isLocalWord(name)) return symbolic_; // a name of the running app, when the binding is written for one
	const Global* global = machine_.findGlobal(name);
	return !isSpecial(name) && (!global || global->state == Global::State::Undefined);
}

bool Compiler::isLiveCall(const Value& form, int depth) {
	const std::string_view head = headWord(form);
	if(head.empty()) // (:title card): a keyword reads a member
		return form.count() > 0 && form.at(0).kind() == Kind::Keyword && form.count() == 2 &&
		       isLive(form.at(1), depth + 1);
	const bool local = std::ranges::find(renames_, head, &Rename::name) == renames_.end() && isLocalWord(head);
	if(!local && head == "fn" && form.count() >= 3 && form.at(1).kind() == Kind::Vector) {
		// What a function's body reads: its parameters are its own, not the app's
		const std::size_t before = itemNames_.size();
		for(std::size_t at = 0; at < form.at(1).count(); ++at)
			if(form.at(1).at(at).kind() == Kind::Symbol) itemNames_.push_back(form.at(1).at(at).text());
		const bool live = std::ranges::any_of(std::views::iota(std::size_t{2}, form.count()),
		                                      [&](std::size_t at) { return isLive(form.at(at), depth + 1); });
		itemNames_.resize(before);
		return live;
	}
	if(!local && isSpecial(head)) {
		if(head != "if" && head != "and" && head != "or" && head != "cond" && head != "when")
			return false; // what binds names is compiled as it is
	} else if(!local) {
		const Global* global = machine_.findGlobal(head);
		if(global && (global->macro || global->view)) return false;
		if(!global || global->state == Global::State::Undefined || reachesRunningApp(machine_.global(head)))
			return true;
	}
	return std::ranges::any_of(std::views::iota(std::size_t{1}, form.count()),
	                           [&](std::size_t at) { return isLive(form.at(at), depth + 1); });
}

bool Compiler::isLive(const Value& form, int depth) {
	if(depth > maxCompileDepth)
		fail(form, "nested too deeply to compile, more than " + std::to_string(maxCompileDepth) + " levels");
	machine_.spend(1);
	switch(form.kind()) {
	case Kind::Symbol:
		return isLiveWord(form.text());
	case Kind::Vector:
		return std::ranges::any_of(std::views::iota(std::size_t{0}, form.count()),
		                           [&](std::size_t at) { return isLive(form.at(at), depth + 1); });
	case Kind::Map:
		return std::ranges::any_of(std::views::iota(std::size_t{0}, form.count()),
		                           [&](std::size_t at) { return isLive(form.valueAt(at), depth + 1); });
	case Kind::List:
		return isLiveCall(form, depth);
	default:
		return false;
	}
}

bool Compiler::buildsAtOnce(const Value& form) {
	const std::string_view head = headWord(form);
	if(head.empty() || isLocalWord(head)) return false;
	if(isSpecial(head))
		return head != "if" && head != "and" && head != "or" && head != "cond" && head != "when" && head != "fn";
	const Global* global = machine_.findGlobal(head);
	return global && (global->macro || global->view || (global->library && head == "handler"));
}

void Compiler::localsIn(const Value& form, int depth, std::vector<std::string_view>& locals) {
	if(depth > maxCompileDepth)
		fail(form, "nested too deeply to compile, more than " + std::to_string(maxCompileDepth) + " levels");
	machine_.spend(1);
	if(form.kind() == Kind::Symbol) {
		const std::string_view head = form.text().substr(0, form.text().find('.'));
		if(isLocalWord(head) && std::ranges::find(locals, head) == locals.end()) locals.push_back(head);
		return;
	}
	if(headWord(form) == "quote") return;
	if(form.kind() == Kind::Map) {
		for(std::size_t at = 0; at < form.count(); ++at) localsIn(form.valueAt(at), depth + 1, locals);
	} else if(form.isSequence()) {
		for(std::size_t at = 0; at < form.count(); ++at) localsIn(form.at(at), depth + 1, locals);
	}
}

void Compiler::emitSymbolicTest(const std::vector<std::string_view>& locals) {
	emitConstant(machine_.global("%symbolic?").value);
	for(const std::string_view local : locals) compileSymbol(machine_.heap.symbol(local, place_));
	emit(Op::Call, static_cast<std::uint32_t>(locals.size()));
}

void Compiler::compileViewValue(const Value& form, int depth) {
	if(buildsAtOnce(form)) {
		compile(form, {});
		return;
	}
	if(isLive(form, depth)) {
		compileBinding(form);
		return;
	}
	std::vector<std::string_view> locals;
	localsIn(form, depth, locals);
	if(locals.empty()) {
		compile(form, {});
		return;
	}
	// A local may hold a name of the running app: then the form is a binding, else what it gives now
	emitSymbolicTest(locals);
	const std::uint32_t plain = emit(Op::JumpIfFalse);
	const bool symbolic = std::exchange(symbolic_, true);
	compileBinding(form);
	symbolic_ = symbolic;
	const std::uint32_t end = emit(Op::Jump);
	patch(plain);
	compile(form, {});
	patch(end);
}

void Compiler::addWorkedPart(const Value& form, int depth, std::vector<BindingPart>& parts) {
	BindingPart part{"", form, true, {}, {}};
	if(form.kind() != Kind::Symbol || form.text().contains('.')) localsIn(form, depth, part.locals);
	if(!part.locals.empty()) { // its text, should one of them be a name of the running app
		const bool symbolic = std::exchange(symbolic_, true);
		lowerForm(form, depth, part.inner);
		symbolic_ = symbolic;
	}
	parts.push_back(std::move(part));
}

void Compiler::emitParts(const std::vector<BindingPart>& parts) {
	emitConstant(machine_.global("%binding").value);
	for(const BindingPart& part : parts) {
		if(!part.worked) {
			emitConstant(machine_.heap.string(part.text));
			continue;
		}
		std::uint32_t end = 0;
		if(!part.locals.empty()) {
			emitSymbolicTest(part.locals);
			const std::uint32_t plain = emit(Op::JumpIfFalse);
			emitParts(part.inner);
			end = emit(Op::Jump);
			patch(plain);
		}
		emitConstant(machine_.global("%expression").value);
		compile(part.form, {});
		emit(Op::Call, 1);
		if(!part.locals.empty()) patch(end);
	}
	emit(Op::Call, static_cast<std::uint32_t>(parts.size()));
}

void Compiler::compileBinding(const Value& form) {
	std::vector<BindingPart> parts;
	const std::string_view head = headWord(form);
	const Global* str = head == "str" && !isLocalWord(head) ? machine_.findGlobal(head) : nullptr;
	const bool joined = str && str->library && form.count() > 2 &&
	                    std::ranges::all_of(std::views::iota(std::size_t{1}, form.count()), [&](std::size_t at) {
		                    return form.at(at).kind() != Kind::String || !form.at(at).text().contains("{{");
	                    });
	if(joined) { // (str "Hello, " name) is the text "Hello, {{name}}"
		for(std::size_t at = 1; at < form.count(); ++at) {
			if(form.at(at).kind() == Kind::String) {
				parts.push_back({std::string(form.at(at).text()), {}, false, {}, {}});
				continue;
			}
			parts.push_back({"{{", {}, false, {}, {}});
			lowerForm(form.at(at), 1, parts);
			parts.push_back({"}}", {}, false, {}, {}});
		}
	} else {
		parts.push_back({"{{", {}, false, {}, {}});
		lowerForm(form, 0, parts);
		parts.push_back({"}}", {}, false, {}, {}});
	}
	if(std::ranges::none_of(parts, &BindingPart::worked)) {
		std::string text;
		for(const BindingPart& part : parts) text += part.text;
		emitConstant(machine_.heap.string(text));
		return;
	}
	const bool live = std::exchange(liveBindings_, false);
	emitParts(parts);
	liveBindings_ = live;
}

} // namespace affresco::lisp
