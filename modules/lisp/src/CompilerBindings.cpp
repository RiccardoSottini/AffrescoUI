/// @file
/// @brief A top-level template's `~form` that needs the running app (Compiler.hpp). What can be known at the build is
/// worked out at the build; a form that reads a path, or names what nothing defines, stays in the tree as a binding:
/// the expression machine's own text when it can be, a call of the program's function otherwise (`lisp('name', ...)`).

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

namespace {

/// @brief An operator of the language that an expression writes the same way.
struct InfixOperator {
	std::string_view word;       ///< The language's word.
	std::string_view expression; ///< What an expression writes.
	std::size_t fewest = 2;      ///< The fewest operands it takes as an operator.
	std::size_t most = 2;        ///< The most.
};

/// The language's operators an expression has. Another count of operands is a call of the language's function.
constexpr std::array<InfixOperator, 13> infixOperators{{{"=", "==", 2, 2},
                                                        {"not=", "!=", 2, 2},
                                                        {"<", "<", 2, 2},
                                                        {">", ">", 2, 2},
                                                        {"<=", "<=", 2, 2},
                                                        {">=", ">=", 2, 2},
                                                        {"+", "+", 2, 16},
                                                        {"-", "-", 2, 16},
                                                        {"*", "*", 2, 16},
                                                        {"/", "/", 2, 2},
                                                        {"mod", "%", 2, 2},
                                                        {"and", "&&", 1, 16},
                                                        {"or", "||", 1, 16}}};

/// @brief Tells whether a word is what an expression calls a function or a key without quotes.
/// @param word The word.
/// @return true if it is.
bool isPlainName(std::string_view word) {
	const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
	return !word.empty() && letter(word.front()) &&
	       std::ranges::all_of(word, [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}

/// @brief Writes a dotted word as an expression's path: `state.my-key.0` is `state['my-key'][0]`.
/// @param word The word.
/// @return The path.
std::string pathText(std::string_view word) {
	std::string text;
	std::size_t at = 0;
	while(at <= word.size()) {
		const std::size_t end = std::min(word.find('.', at), word.size());
		const std::string_view segment = word.substr(at, end - at);
		if(at != 0 && !segment.empty() && std::ranges::all_of(segment, [](char c) { return c >= '0' && c <= '9'; }))
			text.append("[").append(segment).append("]");
		else if(isPlainName(segment)) text.append(at == 0 ? "" : ".").append(segment);
		else text.append("[").append(expressionText(segment)).append("]");
		at = end + 1;
	}
	return text;
}

} // namespace

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
		if(global.library) return false;
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

bool Compiler::isLiveWord(std::string_view name) {
	if(resolve(*current_, name.substr(0, name.find('.'))).kind != Variable::Kind::None) return false;
	const Global* global = machine_.findGlobal(name);
	return !isSpecial(name) && (!global || global->state == Global::State::Undefined);
}

bool Compiler::isLiveCall(const Value& form, int depth) {
	const std::string_view head = headWord(form);
	if(head.empty()) return false;
	const bool local = resolve(*current_, head.substr(0, head.find('.'))).kind != Variable::Kind::None;
	if(!local && isSpecial(head)) {
		if(head != "if" && head != "and" && head != "or") return false; // what binds names is compiled as it is
	} else if(!local) {
		const Global* global = machine_.findGlobal(head);
		if(global && global->macro) return false;
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

void Compiler::compileBinding(const Value& form) {
	std::vector<BindingPart> parts = {{"{{", {}, false}};
	lowerForm(form, 0, parts);
	parts.push_back({"}}", {}, false});
	const auto worked = std::ranges::any_of(parts, &BindingPart::worked);
	if(!worked) {
		std::string text;
		for(const BindingPart& part : parts) text += part.text;
		emitConstant(machine_.heap.string(text));
		return;
	}
	const auto count = static_cast<std::uint32_t>(parts.size());
	emitConstant(machine_.global("%binding").value);
	const bool live = std::exchange(liveBindings_, false);
	for(const BindingPart& part : parts) {
		if(!part.worked) {
			emitConstant(machine_.heap.string(part.text));
			continue;
		}
		emitConstant(machine_.global("%expression").value);
		compile(part.form, {});
		emit(Op::Call, 1);
	}
	liveBindings_ = live;
	emit(Op::Call, count);
}

void Compiler::lowerForm(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(depth > maxCompileDepth)
		fail(form, "nested too deeply to compile, more than " + std::to_string(maxCompileDepth) + " levels");
	if(!isLive(form, depth)) {
		parts.push_back({"", form, true});
		return;
	}
	if(form.kind() == Kind::Symbol) {
		parts.push_back({pathText(form.text()), {}, false});
	} else if(form.kind() == Kind::List) {
		lowerCall(form, depth, parts);
	} else if(form.kind() == Kind::Vector) {
		parts.push_back({"[", {}, false});
		for(std::size_t at = 0; at < form.count(); ++at) {
			if(at != 0) parts.push_back({", ", {}, false});
			lowerForm(form.at(at), depth + 1, parts);
		}
		parts.push_back({"]", {}, false});
	} else {
		parts.push_back({"{", {}, false});
		for(std::size_t at = 0; at < form.count(); ++at) {
			parts.push_back({(at != 0 ? ", " : "") + expressionText(form.keyAt(at)) + ": ", {}, false});
			lowerForm(form.valueAt(at), depth + 1, parts);
		}
		parts.push_back({"}", {}, false});
	}
}

void Compiler::lowerList(const Value& form, int depth, std::vector<BindingPart>& parts, const std::string& open,
                         const std::string& separator) {
	parts.push_back({open, {}, false});
	for(std::size_t at = 1; at < form.count(); ++at) {
		if(at != 1) parts.push_back({separator, {}, false});
		lowerForm(form.at(at), depth + 1, parts);
	}
	parts.push_back({")", {}, false});
}

bool Compiler::lowerOperator(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = headWord(form);
	const std::size_t operands = form.count() - 1;
	if(head == "if" && (operands == 2 || operands == 3)) {
		parts.push_back({"(", {}, false});
		lowerForm(form.at(1), depth + 1, parts);
		parts.push_back({" ? ", {}, false});
		lowerForm(form.at(2), depth + 1, parts);
		parts.push_back({" : ", {}, false});
		if(operands == 3) lowerForm(form.at(3), depth + 1, parts);
		else parts.push_back({"null", {}, false});
		parts.push_back({")", {}, false});
		return true;
	}
	if(head == "not" && operands == 1) {
		lowerList(form, depth, parts, "!(", "");
		return true;
	}
	const auto infix = std::ranges::find(infixOperators, head, &InfixOperator::word);
	if(infix == infixOperators.end() || operands < infix->fewest || operands > infix->most) return false;
	lowerList(form, depth, parts, "(", " " + std::string(infix->expression) + " ");
	return true;
}

void Compiler::lowerCall(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = headWord(form);
	const Global* global = machine_.findGlobal(head);
	const bool defined = global && global->state != Global::State::Undefined;
	// A program's own function of that name isn't the expression's operator
	if((isSpecial(head) || (defined && global->library)) && lowerOperator(form, depth, parts)) return;
	if(isSpecial(head) || resolve(*current_, head.substr(0, head.find('.'))).kind != Variable::Kind::None)
		fail(form,
		     "a value read while the app runs is an expression or a call of a function: define a function for this");
	if(defined) {
		lowerList(form, depth, parts, "lisp(" + expressionText(head) + (form.count() > 1 ? ", " : ""), ", ");
		return;
	}
	if(!isPlainName(head))
		fail(form, "nothing defines \"" + notation::Lexer::excerpt(head) + "\", and no expression can call it");
	lowerList(form, depth, parts, std::string(head) + "(", ", ");
}

} // namespace affresco::lisp
