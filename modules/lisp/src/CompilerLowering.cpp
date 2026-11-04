/// @file
/// @brief A binding's form written as the expression machine's text (Compiler.hpp): operators, the language's own
/// functions the expressions have under another name (`count` is `length`, `(:title card)` is `card.title`), and the
/// functions on each item (`(filter (fn [x] ...) list)` is `filter(list, ...)`, the item named `it`), so that a binding
/// runs at an expression's speed. What it can't write is a call of the program's function, `lisp('name', ...)`.

#include "Compiler.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <array>
#include <ranges>
#include <string>
#include <string_view>
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
constexpr std::array<InfixOperator, 14> infixOperators{{{"=", "==", 2, 2},
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
                                                        {"or", "||", 1, 16},
                                                        {"or-else", "??", 2, 16}}};

/// @brief One of the language's functions that an expression has under its own name, with the same arguments.
struct SameFunction {
	std::string_view word;       ///< The language's word.
	std::string_view expression; ///< The expression's function.
	std::size_t fewest = 1;      ///< The fewest arguments.
	std::size_t most = 1;        ///< The most.
};

/// Those functions.
constexpr std::array<SameFunction, 14> sameFunctions{{{"count", "length", 1, 1},
                                                      {"range", "range", 1, 1},
                                                      {"sort", "sort", 1, 1},
                                                      {"concat", "concat", 1, 16},
                                                      {"merge", "merge", 1, 16},
                                                      {"keys", "keys", 1, 1},
                                                      {"vals", "values", 1, 1},
                                                      {"reverse", "reverse", 1, 1},
                                                      {"min", "min", 1, 16},
                                                      {"max", "max", 1, 16},
                                                      {"abs", "abs", 1, 1},
                                                      {"upper-case", "upper", 1, 1},
                                                      {"lower-case", "lower", 1, 1},
                                                      {"empty?", "empty", 1, 1}}};

/// @brief A function on each item, and what its function is given, in order: `(reduce (fn [acc x] ...) init list)`.
struct ItemFunction {
	std::string_view word;                 ///< The language's word.
	std::string_view expression;           ///< The expression's function.
	std::array<std::string_view, 2> names; ///< What the expression calls the function's parameters.
	std::size_t parameters = 1;            ///< How many the function takes.
};

/// Those functions.
constexpr std::array<ItemFunction, 8> itemFunctions{{{"filter", "filter", {"it", "index"}, 2},
                                                     {"remove", "filter", {"it", "index"}, 2},
                                                     {"map", "map", {"it", "index"}, 2},
                                                     {"map-indexed", "map", {"index", "it"}, 2},
                                                     {"find", "find", {"it", "index"}, 2},
                                                     {"some", "some", {"it", "index"}, 2},
                                                     {"every?", "every", {"it", "index"}, 2},
                                                     {"sort-by", "sort", {"it", "index"}, 2}}};

/// @brief Tells whether a word is what an expression calls a function or a key without quotes.
/// @param word The word.
/// @return true if it is.
bool isPlainName(std::string_view word) {
	const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
	return !word.empty() && letter(word.front()) &&
	       std::ranges::all_of(word, [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}

/// @brief Writes the members of a path after its first word: `.name`, `[0]`, `['my-key']`.
/// @param members The dotted members, without the first word.
/// @return The text.
std::string membersText(std::string_view members) {
	std::string text;
	std::size_t at = 0;
	while(at <= members.size() && !members.empty()) {
		const std::size_t end = std::min(members.find('.', at), members.size());
		const std::string_view segment = members.substr(at, end - at);
		if(!segment.empty() && std::ranges::all_of(segment, [](char c) { return c >= '0' && c <= '9'; }))
			text.append("[").append(segment).append("]");
		else if(isPlainName(segment)) text.append(".").append(segment);
		else text.append("[").append(expressionText(segment)).append("]");
		at = end + 1;
	}
	return text;
}

} // namespace

std::string pathExpression(std::string_view word) {
	if(word.starts_with('(')) return std::string(word); // an expression a view was given (Compiler::compileLiveName())
	const std::size_t dot = word.find('.');
	const std::string_view first = word.substr(0, dot);
	const std::string text = isPlainName(first) ? std::string(first) : "[" + expressionText(first) + "]";
	return text + (dot == std::string_view::npos ? "" : membersText(word.substr(dot + 1)));
}

void Compiler::lowerForm(const Value& written, int depth, std::vector<BindingPart>& parts) {
	const Value form = written.kind() == Kind::List ? expanded(written) : written;
	if(depth > maxCompileDepth)
		fail(form, "nested too deeply to compile, more than " + std::to_string(maxCompileDepth) + " levels");
	if(!isLive(form, depth)) {
		addWorkedPart(form, depth, parts);
		return;
	}
	if(form.kind() == Kind::Symbol) {
		lowerWord(form, depth, parts);
	} else if(form.kind() == Kind::List) {
		lowerCall(form, depth, parts);
	} else if(form.kind() == Kind::Vector) {
		parts.push_back({"[", {}, false, {}, {}});
		for(std::size_t at = 0; at < form.count(); ++at) {
			if(at != 0) parts.push_back({", ", {}, false, {}, {}});
			lowerForm(form.at(at), depth + 1, parts);
		}
		parts.push_back({"]", {}, false, {}, {}});
	} else {
		parts.push_back({"{", {}, false, {}, {}});
		for(std::size_t at = 0; at < form.count(); ++at) {
			parts.push_back({(at != 0 ? ", " : "") + expressionText(form.keyAt(at)) + ": ", {}, false, {}, {}});
			lowerForm(form.valueAt(at), depth + 1, parts);
		}
		parts.push_back({"}", {}, false, {}, {}});
	}
}

void Compiler::lowerWord(const Value& word, int depth, std::vector<BindingPart>& parts) {
	const std::string_view text = word.text();
	const std::size_t dot = text.find('.');
	const std::string_view head = text.substr(0, dot);
	const std::string members = dot == std::string_view::npos ? "" : membersText(text.substr(dot + 1));
	if(const auto renamed = std::ranges::find(renames_ | std::views::reverse, head, &Rename::name);
	   renamed != (renames_ | std::views::reverse).end()) {
		if(renamed->hidden)
			fail(word, "a function inside another, in a binding, can't use the outer one's \"" +
			               notation::Lexer::excerpt(head) + "\": define a function for this");
		parts.push_back({renamed->as + members, {}, false, {}, {}});
		return;
	}
	if(isLocalWord(head)) { // its value, or the name of the running app it holds
		addWorkedPart(machine_.heap.symbol(head, word.place()), depth, parts);
		if(!members.empty()) parts.push_back({members, {}, false, {}, {}});
		return;
	}
	parts.push_back({pathExpression(text), {}, false, {}, {}});
}

void Compiler::lowerList(const Value& form, int depth, std::vector<BindingPart>& parts, const std::string& open,
                         const std::string& separator) {
	parts.push_back({open, {}, false, {}, {}});
	for(std::size_t at = 1; at < form.count(); ++at) {
		if(at != 1) parts.push_back({separator, {}, false, {}, {}});
		lowerForm(form.at(at), depth + 1, parts);
	}
	parts.push_back({")", {}, false, {}, {}});
}

bool Compiler::lowerOperator(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = headWord(form);
	const std::size_t operands = form.count() - 1;
	if((head == "if" && (operands == 2 || operands == 3)) || (head == "when" && operands == 2)) {
		parts.push_back({"(", {}, false, {}, {}});
		lowerForm(form.at(1), depth + 1, parts);
		parts.push_back({" ? ", {}, false, {}, {}});
		lowerForm(form.at(2), depth + 1, parts);
		parts.push_back({" : ", {}, false, {}, {}});
		if(operands == 3) lowerForm(form.at(3), depth + 1, parts);
		else parts.push_back({"null", {}, false, {}, {}});
		parts.push_back({")", {}, false, {}, {}});
		return true;
	}
	if(head == "cond" && operands >= 2 && operands % 2 == 0) { // (cond a x b y :else z): a ? x : (b ? y : z)
		std::size_t closing = 0;
		for(std::size_t at = 1; at < form.count(); at += 2) {
			const bool last = at + 2 >= form.count();
			if(last && form.at(at).kind() == Kind::Keyword) {
				lowerForm(form.at(at + 1), depth + 1, parts);
				break;
			}
			parts.push_back({"(", {}, false, {}, {}});
			lowerForm(form.at(at), depth + 1, parts);
			parts.push_back({" ? ", {}, false, {}, {}});
			lowerForm(form.at(at + 1), depth + 1, parts);
			parts.push_back({" : ", {}, false, {}, {}});
			++closing;
			if(last) parts.push_back({"null", {}, false, {}, {}});
		}
		parts.push_back({std::string(closing, ')'), {}, false, {}, {}});
		return true;
	}
	if(head == "not" && operands == 1) {
		lowerList(form, depth, parts, "!(", "");
		return true;
	}
	if(head == "truthy?" && operands == 1) { // an expression's own truth
		lowerList(form, depth, parts, "!(!(", "");
		parts.back().text = "))";
		return true;
	}
	if(head == "-" && operands == 1) {
		lowerList(form, depth, parts, "-(", "");
		return true;
	}
	const auto* infix = std::ranges::find(infixOperators, head, &InfixOperator::word);
	if(infix == infixOperators.end() || operands < infix->fewest || operands > infix->most) return false;
	lowerList(form, depth, parts, "(", " " + std::string(infix->expression) + " ");
	return true;
}

void Compiler::lowerKey(const Value& key, int depth, std::vector<BindingPart>& parts) {
	if((key.kind() == Kind::Keyword || key.kind() == Kind::String) && isPlainName(key.text())) {
		parts.push_back({"." + std::string(key.text()), {}, false, {}, {}});
		return;
	}
	parts.push_back({"[", {}, false, {}, {}});
	if(key.kind() == Kind::Keyword) parts.push_back({expressionText(key.text()), {}, false, {}, {}});
	else lowerForm(key, depth + 1, parts);
	parts.push_back({"]", {}, false, {}, {}});
}

void Compiler::lowerStr(const Value& form, int depth, std::vector<BindingPart>& parts) {
	parts.push_back({"(", {}, false, {}, {}});
	// '+' joins text once one side is text: one of the first two being written as text says so
	const bool text = (form.count() > 1 && form.at(1).kind() == Kind::String) ||
	                  (form.count() > 2 && form.at(2).kind() == Kind::String);
	if(!text) parts.push_back({form.count() > 1 ? "'' + " : "''", {}, false, {}, {}});
	for(std::size_t at = 1; at < form.count(); ++at) {
		if(at != 1) parts.push_back({" + ", {}, false, {}, {}});
		lowerForm(form.at(at), depth + 1, parts);
	}
	parts.push_back({")", {}, false, {}, {}});
}

void Compiler::lowerFunctionBody(const Value& function, std::span<const std::string_view> names, int depth,
                                 std::vector<BindingPart>& parts) {
	const Value params = function.at(1);
	if(function.count() != 3 || params.count() == 0 || params.count() > names.size() ||
	   std::ranges::any_of(std::views::iota(std::size_t{0}, params.count()),
	                       [&](std::size_t at) { return params.at(at).kind() != Kind::Symbol; }))
		fail(function, "a function in a binding takes 1 to " + std::to_string(names.size()) +
		                   " plain parameters and has one form: define a function for more");
	const std::size_t before = renames_.size();
	std::vector<bool> hidden; // an outer item named as one of this function's is out of reach inside it
	for(Rename& outer : renames_) {
		hidden.push_back(outer.hidden);
		if(std::ranges::find(names, std::string_view(outer.as)) != names.end()) outer.hidden = true;
	}
	for(std::size_t at = 0; at < params.count(); ++at)
		renames_.push_back({params.at(at).text(), std::string(names[at])});
	lowerForm(function.at(2), depth + 1, parts);
	renames_.resize(before);
	for(std::size_t at = 0; at < before; ++at) renames_[at].hidden = hidden[at];
}

bool Compiler::lowerItemFunction(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = headWord(form);
	const auto isFunction = [](const Value& item) {
		return headWord(item) == "fn" && item.count() >= 2 && item.at(1).kind() == Kind::Vector;
	};
	if(head == "reduce" && form.count() == 4 && isFunction(form.at(1))) { // (reduce (fn [acc x] ...) init list)
		parts.push_back({"reduce(", {}, false, {}, {}});
		lowerForm(form.at(3), depth + 1, parts);
		parts.push_back({", ", {}, false, {}, {}});
		const std::array<std::string_view, 2> names = {"acc", "it"};
		lowerFunctionBody(form.at(1), names, depth, parts);
		parts.push_back({", ", {}, false, {}, {}});
		lowerForm(form.at(2), depth + 1, parts);
		parts.push_back({")", {}, false, {}, {}});
		return true;
	}
	const auto* found = std::ranges::find(itemFunctions, head, &ItemFunction::word);
	const bool descending = head == "sort-by" && form.count() == 4 && headWord(form) == "sort-by" &&
	                        form.at(2).kind() == Kind::Symbol && form.at(2).text() == ">";
	// A keyword is the function that looks it up, as in Clojure: (sort-by :d xs) is sort(xs, it.d)
	const bool keyword = form.at(1).kind() == Kind::Keyword && head != "map-indexed";
	if(found == itemFunctions.end() || (form.count() != 3 && !descending) || !(keyword || isFunction(form.at(1))))
		return false;
	parts.push_back({std::string(found->expression) + "(", {}, false, {}, {}});
	lowerForm(form.at(form.count() - 1), depth + 1, parts);
	parts.push_back({head == "remove" ? ", !(" : ", ", {}, false, {}, {}});
	if(keyword) {
		parts.push_back({"it", {}, false, {}, {}});
		lowerKey(form.at(1), depth, parts);
	} else {
		lowerFunctionBody(form.at(1), std::span(found->names).first(found->parameters), depth, parts);
	}
	parts.push_back({head == "remove" ? "))" : descending ? ", 'desc')" : ")", {}, false, {}, {}});
	return true;
}

bool Compiler::lowerLibrary(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = headWord(form);
	const std::size_t arguments = form.count() - 1;
	if(lowerItemFunction(form, depth, parts)) return true;
	if(const auto* same = std::ranges::find(sameFunctions, head, &SameFunction::word);
	   same != sameFunctions.end() && arguments >= same->fewest && arguments <= same->most) {
		lowerList(form, depth, parts, std::string(same->expression) + "(", ", ");
		return true;
	}
	if((head == "get" && arguments == 2) || (head == "first" && arguments == 1)) { // x[k], x.k
		lowerForm(form.at(1), depth + 1, parts);
		if(head == "first") parts.push_back({"[0]", {}, false, {}, {}});
		else lowerKey(form.at(2), depth, parts);
		return true;
	}
	if(head == "get-in" && arguments == 2 && form.at(2).kind() == Kind::Vector) {
		lowerForm(form.at(1), depth + 1, parts);
		for(std::size_t at = 0; at < form.at(2).count(); ++at) lowerKey(form.at(2).at(at), depth, parts);
		return true;
	}
	if((head == "inc" || head == "dec") && arguments == 1) {
		lowerList(form, depth, parts, "(", "");
		parts.back().text = head == "inc" ? " + 1)" : " - 1)";
		return true;
	}
	if((head == "nil?" || head == "some?") && arguments == 1) {
		lowerList(form, depth, parts, "(", "");
		parts.back().text = head == "nil?" ? " == null)" : " != null)";
		return true;
	}
	if(head == "join" && arguments == 2) { // (join sep list) is join(list, sep)
		parts.push_back({"join(", {}, false, {}, {}});
		lowerForm(form.at(2), depth + 1, parts);
		parts.push_back({", ", {}, false, {}, {}});
		lowerForm(form.at(1), depth + 1, parts);
		parts.push_back({")", {}, false, {}, {}});
		return true;
	}
	if(head == "str" && arguments >= 1) {
		lowerStr(form, depth, parts);
		return true;
	}
	return false;
}

void Compiler::lowerCall(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(form.count() == 2 && form.at(0).kind() == Kind::Keyword) { // (:title card) is card.title
		lowerForm(form.at(1), depth + 1, parts);
		lowerKey(form.at(0), depth, parts);
		return;
	}
	const std::string_view head = headWord(form);
	const bool renamed = std::ranges::find(renames_, head, &Rename::name) != renames_.end();
	const bool local = !renamed && isLocalWord(head);
	const Global* global = local || renamed ? nullptr : machine_.findGlobal(head);
	const bool defined = global && global->state != Global::State::Undefined;
	// A program's own function of that name isn't the expression's operator
	if(!local && !renamed && (isSpecial(head) || (defined && global->library))) {
		if(lowerOperator(form, depth, parts)) return;
		if(defined && lowerLibrary(form, depth, parts)) return;
	}
	if(isSpecial(head) || local || renamed)
		fail(form,
		     "a value read while the app runs is an expression or a call of a function: define a function for this");
	if(defined) {
		lowerList(form, depth, parts, "lisp(" + expressionText(head) + (form.count() > 1 ? ", " : ""), ", ");
		return;
	}
	if(!isPlainName(head))
		fail(form, "nothing defines \"" + notation::Lexer::excerpt(head) + "\", and no expression can call it");
	// An expression's own function: one on each item takes a function whose parameters are named as it names them
	parts.push_back({std::string(head) + "(", {}, false, {}, {}});
	for(std::size_t at = 1; at < form.count(); ++at) {
		if(at != 1) parts.push_back({", ", {}, false, {}, {}});
		const Value item = form.at(at);
		if(headWord(item) != "fn" || item.count() < 2 || item.at(1).kind() != Kind::Vector) {
			lowerForm(item, depth + 1, parts);
			continue;
		}
		std::vector<std::string_view> names;
		for(std::size_t name = 0; name < item.at(1).count(); ++name) {
			const std::string_view word = item.at(1).at(name).kind() == Kind::Symbol ? item.at(1).at(name).text() : "";
			if(word != "it" && word != "index" && word != "acc")
				fail(item, "a function given to an expression's own function names its parameters it, index or acc");
			names.push_back(word);
		}
		lowerFunctionBody(item, names, depth, parts);
	}
	parts.push_back({")", {}, false, {}, {}});
}

} // namespace affresco::lisp
