/// @file
/// @brief A template's `~form` that needs the running app (Compiler.hpp). What can be known at the build is worked out
/// at the build; a form that reads a path, or names what nothing defines, stays in the tree as a binding: the
/// expression machine's own text when it can be (CompilerLowering.cpp), a call of the program's function otherwise
/// (`lisp('name', ...)`). In a template that builds a view, a local may hold a name of the running app (a symbol:
/// `repeat`'s item, a component's parameter): a form using one is a binding too, which the build decides as it runs.

#include "Compiler.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <array>
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

namespace {

/// @brief Whether a text is one placeholder and nothing else, as the expression machine finds a placeholder's end
/// (quotes and braces inside it skipped): its value is then the expression's, not text.
/// @param text The text, without spaces around it.
/// @return true if so.
bool isWholePlaceholder(std::string_view text) noexcept {
	if(!text.starts_with("{{") || !text.ends_with("}}")) return false;
	char quote = 0;
	int braces = 0;
	for(std::size_t i = 2; i + 1 < text.size(); ++i) {
		if(quote) {
			if(text[i] == '\\') ++i;
			else if(text[i] == quote) quote = 0;
		} else if(text[i] == '\'' || text[i] == '"') {
			quote = text[i];
		} else if(text[i] == '{') {
			++braces;
		} else if(text[i] == '}' && braces > 0) {
			--braces;
		} else if(text[i] == '}' && text[i + 1] == '}') {
			return i == text.size() - 2;
		}
	}
	return false;
}

} // namespace

std::string keptSpaces(std::string text) {
	const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; };
	std::size_t start = 0;
	while(start < text.size() && space(text[start])) ++start;
	std::size_t end = text.size();
	while(end > start && space(text[end - 1])) --end;
	if((start == 0 && end == text.size()) || end - start < 4) return text;
	const std::string_view inner = std::string_view(text).substr(start, end - start);
	if(!isWholePlaceholder(inner)) return text; // text already
	std::string kept = start > 0 ? "{{" + expressionText(text.substr(0, start)) + "}}" : "";
	kept.append(inner);
	if(end < text.size()) kept.append("{{" + expressionText(text.substr(end)) + "}}");
	return kept;
}

std::string joinedText(std::string text) {
	text = keptSpaces(std::move(text));
	// "{{x}}", as when the texts around x were empty: x's text, not its value
	return isWholePlaceholder(text) ? "{{''}}" + text : text;
}

namespace {

/// The language's functions that take a function: a word they are given that names one is that function.
constexpr std::array<std::string_view, 14> functionTakers = {"apply",   "map",    "map-indexed", "mapcat",   "filter",
                                                             "remove",  "find",   "some",        "every?",   "reduce",
                                                             "sort-by", "update", "update-in",   "run-hooks"};

} // namespace

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
	if(const Definition* definition = machine_.definition(name);
	   global && !global->library && definition &&
	   (definition->kind == "defn" || definition->kind == "defcommand" || definition->kind == "defview"))
		return true; // a function is no value of a tree: a word that names one, uncalled, is a name of the running app
	if(namesFunction(global) && !name.contains('.') && !functionArgument_)
		return true; // the language's or a host's function, uncalled: a `:let`'s `name`, a repeat's `max`
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
	// (sort-by count xs): the language's function a function of the language is given stays that function
	const bool takesFunctions = !local && std::ranges::find(functionTakers, head) != functionTakers.end();
	return std::ranges::any_of(std::views::iota(std::size_t{1}, form.count()), [&](std::size_t at) {
		const bool argument = std::exchange(functionArgument_, takesFunctions && form.at(at).kind() == Kind::Symbol);
		const bool live = isLive(form.at(at), depth + 1);
		functionArgument_ = argument;
		return live;
	});
}

bool Compiler::namesFunction(const Global* global) {
	if(!global || global->state != Global::State::Ready) return false;
	return global->value.kind() == Kind::Function || global->value.kind() == Kind::Native;
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
	case Kind::List: {
		const Value call = expanded(form);
		// A macro may write any form: a map, a vector, a word, a constant
		return call.kind() == Kind::List ? isLiveCall(call, depth) : isLive(call, depth + 1);
	}
	default:
		return false;
	}
}

Value Compiler::expanded(const Value& form) {
	Value at = form;
	for(int times = 0; times <= maxCompileDepth; ++times) {
		const std::string_view head = headWord(at);
		if(head.empty() || isLocalWord(head) || isSpecial(head)) return at;
		Global* global = machine_.findGlobal(head);
		if(!global || !global->macro) return at;
		// Once a call: the liveness check, the binding and the code all read the same expansion, its gensyms too
		if(const auto known = expansions_.find(at.pointer()); known != expansions_.end()) {
			at = known->second;
			continue;
		}
		std::vector<Value> arguments;
		arguments.reserve(at.count() - 1);
		for(std::size_t item = 1; item < at.count(); ++item) arguments.push_back(at.at(item));
		const Value result = machine_.expand(*global, arguments, at.place());
		if(const Value macro = global->value; macro.kind() == Kind::Function) // its author wrote this code
			current_->authors =
			    machine_.unite(current_->authors, static_cast<const ClosureObject*>(macro.pointer())->code->authors);
		expansions_.emplace(at.pointer(), result);
		at = result;
	}
	fail(form, "macros nested too deeply");
}

bool Compiler::buildsAtOnce(const Value& form) {
	const std::string_view head = headWord(form);
	if(head.empty() || isLocalWord(head)) return false;
	if(isSpecial(head))
		return head != "if" && head != "and" && head != "or" && head != "cond" && head != "when" && head != "fn";
	const Global* global = machine_.findGlobal(head);
	return global && (global->view || (global->library && head == "handler"));
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

void Compiler::compileViewValue(const Value& written, int depth) {
	const Value form = expanded(written);
	if(const Global* global = machine_.findGlobal(headWord(form));
	   headWord(form) == "handler" && global && global->library && !isLocalWord("handler")) {
		// (handler move-card event.data.from c): each argument as a node's value would be, a path its binding
		emitConstant(global->value);
		for(std::size_t at = 1; at < form.count(); ++at) {
			if(at == 1) compile(form.at(at), {});
			else compileViewValue(form.at(at), depth + 1);
		}
		place_ = form.place();
		emit(Op::Call, static_cast<std::uint32_t>(form.count() - 1));
		return;
	}
	if(buildsAtOnce(form)) {
		if(const std::string_view head = headWord(form); !isLocalWord(head) && !isSpecial(head))
			if(const Global* view = machine_.findGlobal(head); view && view->view) {
				compileViewCall(form, depth);
				return;
			}
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

void Compiler::compileViewCall(const Value& form, int depth) {
	compile(form.at(0), {});
	for(std::size_t at = 1; at < form.count(); ++at) {
		const Value& argument = form.at(at);
		// A function, named, is given as itself: a view may make a handler of it
		const Definition* named = argument.kind() == Kind::Symbol ? machine_.definition(argument.text()) : nullptr;
		const bool function =
		    argument.kind() == Kind::Symbol && !isLocalWord(argument.text()) &&
		    ((named && (named->kind == "defn" || named->kind == "defcommand" || named->kind == "defview")) ||
			 namesFunction(machine_.findGlobal(argument.text())));
		if(!function && isLive(argument, depth + 1)) compileLiveName(argument);
		else compile(argument, {});
	}
	place_ = form.place();
	emit(Op::Call, static_cast<std::uint32_t>(form.count() - 1));
}

void Compiler::compileLiveName(const Value& form) {
	if(form.kind() == Kind::Symbol && !isLocalWord(form.text())) { // a path: the name itself, as 'state.x' gives
		emitConstant(machine_.heap.symbol(form.text(), form.place()));
		return;
	}
	emitConstant(machine_.global("%as-name").value);
	std::vector<BindingPart> parts = {{"{{", {}, false, {}, {}}};
	lowerForm(form, 0, parts);
	parts.push_back({"}}", {}, false, {}, {}});
	if(std::ranges::none_of(parts, &BindingPart::worked)) {
		std::string text;
		for(const BindingPart& part : parts) text += part.text;
		emitConstant(machine_.heap.string(std::move(text)));
	} else {
		const bool live = std::exchange(liveBindings_, false);
		emitParts(parts, false);
		liveBindings_ = live;
	}
	place_ = form.place();
	emit(Op::Call, 1);
}

void Compiler::addWorkedPart(const Value& form, int depth, std::vector<BindingPart>& parts) {
	BindingPart part{"", form, true, {}, {}};
	// Already written for names of the running app: a form still worked out uses them where no binding can (inside a
	// quote or a template of its own), so it has no text of its own to fall back on, and writing it again would loop
	if((form.kind() != Kind::Symbol || form.text().contains('.')) && !symbolic_) localsIn(form, depth, part.locals);
	if(!part.locals.empty()) { // its text, should one of them be a name of the running app
		const bool symbolic = std::exchange(symbolic_, true);
		lowerForm(form, depth, part.inner);
		symbolic_ = symbolic;
	}
	parts.push_back(std::move(part));
}

void Compiler::emitParts(const std::vector<BindingPart>& parts, bool joined) {
	emitConstant(machine_.global(joined ? "%joined" : "%binding").value);
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
		emitConstant(machine_.global(part.asText ? "%text" : "%expression").value);
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
			if(!isLive(form.at(at), 1)) { // worked out at the build: its text, as itself
				addWorkedPart(form.at(at), 1, parts);
				BindingPart& part = parts.back();
				part.asText = true;
				if(!part.inner.empty()) { // a name of the running app after all: a placeholder
					part.inner.insert(part.inner.begin(), {"{{", {}, false, {}, {}});
					part.inner.push_back({"}}", {}, false, {}, {}});
				}
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
		emitConstant(machine_.heap.string(joined ? joinedText(std::move(text)) : keptSpaces(std::move(text))));
		return;
	}
	const bool live = std::exchange(liveBindings_, false);
	emitParts(parts, joined);
	liveBindings_ = live;
}

} // namespace affresco::lisp
