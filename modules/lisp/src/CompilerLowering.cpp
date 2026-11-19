/// @file
/// @brief A binding's form written as the expression machine's text (Compiler.hpp): operators, the language's own
/// functions the expressions have under another name (`count` is `length`, `(:title card)` is `card.title`), and the
/// functions on each item (`(filter (fn [x] ...) list)` is `filter(list, ...)`, the item named `it`), so that a binding
/// runs at an expression's speed. What it can't write is a call of the program's function, `lisp('name', ...)`.

#include "Compiler.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <span>
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

/// @brief An operator of the language that takes one operand, written around it: `(not x)` is `!(x)`.
struct AroundOperator {
	std::string_view word;  ///< The language's word.
	std::string_view open;  ///< What an expression writes before the operand.
	std::string_view close; ///< What it writes after.
};

/// Those operators.
constexpr std::array<AroundOperator, 3> aroundOperators{{{"not", "!(", ")"},
                                                         {"truthy?", "!(!(", "))"}, // an expression's own truth
                                                         {"-", "-(", ")"}}};

/// The language's functions of one argument that an expression writes as an operator around it: `(inc x)` is `(x + 1)`.
constexpr std::array<AroundOperator, 11> aroundFunctions{{{"inc", "(", " + 1)"},
                                                          {"dec", "(", " - 1)"},
                                                          {"nil?", "(", " == null)"},
                                                          {"some?", "(", " != null)"},
                                                          {"zero?", "(", " == 0)"},
                                                          {"pos?", "(", " > 0)"},
                                                          {"neg?", "(", " < 0)"},
                                                          {"even?", "((", " % 2) == 0)"},
                                                          {"odd?", "((", " % 2) != 0)"},
                                                          {"true?", "(", " == true)"},
                                                          {"false?", "(", " == false)"}}};

/// @brief The operator a list of one operand is, from a table.
/// @param table The table.
/// @param form The list.
/// @param head The function it calls (Compiler::calledName()), or "" for its first word.
/// @return Its entry, or null.
const AroundOperator* aroundOf(std::span<const AroundOperator> table, const Value& form, std::string_view head = "") {
	if(form.count() != 2) return nullptr;
	if(head.empty()) head = headWord(form);
	const auto found = std::ranges::find(table, head, &AroundOperator::word);
	return found == table.end() ? nullptr : &*found;
}

/// @brief Tells whether a form is a function a binding can write: `(fn [params] ...)`.
/// @param item The form.
/// @return true if it is.
bool isItemFunction(const Value& item) {
	return headWord(item) == "fn" && item.count() >= 2 && item.at(1).kind() == Kind::Vector;
}

/// @brief Tells whether a call is `(sort-by f > list)`, which sorts from the greatest.
/// @param form The call.
/// @return true if it is.
bool isDescendingSort(const Value& form) {
	return headWord(form) == "sort-by" && form.count() == 4 && form.at(2).kind() == Kind::Symbol &&
	       form.at(2).text() == ">";
}

/// @brief One of the language's functions that an expression has under its own name, with the same arguments.
struct SameFunction {
	std::string_view word;       ///< The language's word.
	std::string_view expression; ///< The expression's function.
	std::size_t fewest = 1;      ///< The fewest arguments.
	std::size_t most = 1;        ///< The most.
};

/// Those functions.
constexpr std::array<SameFunction, 17> sameFunctions{{{"count", "length", 1, 1},
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
                                                      {"empty?", "empty", 1, 1},
                                                      {"clojure.string/trim", "trim", 1, 1},
                                                      {"clojure.string/split", "split", 2, 2},
                                                      {"clojure.string/includes?", "contains", 2, 2}}};

/// The functions of `clojure.string` the language also has bare, lowered as those. Not `join`: on nil the function
/// gives "", the expression an error (reference/bindings.md).
constexpr std::array<std::string_view, 2> sameAsBare = {"lower-case", "upper-case"};

/// @brief The function a binding's call names, for the tables here: its first word, with a `clojure.string` function
/// the language also has bare written bare.
/// @param called The function it calls (Compiler::calledName()).
/// @return Its word.
std::string_view tableWord(std::string_view called) {
	constexpr std::string_view strings = "clojure.string/";
	if(called.starts_with(strings) && std::ranges::contains(sameAsBare, called.substr(strings.size())))
		return called.substr(strings.size());
	return called;
}

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

/// The longest a binding's text may be once its `let`s' names are written in. Each use of a name writes its value
/// again, and nested `let`s multiply that, so counting the text made bounds the compiler's work whatever the input.
constexpr std::size_t maxBindingText = 4000;

/// What a value the build works out counts for in that text: its own text is known only at the build.
constexpr std::size_t workedPartSize = 16;

/// What a binding says of a form it can't write, which is a choice or a function's work.
constexpr std::string_view notAnExpression =
    "a value read while the app runs is an expression or a call of a function: define a function for this";

/// @brief Tells whether a character may stand outside brackets and quotes in one term.
/// @param c The character.
/// @return true for a letter, a digit, `_` or `.`.
bool isTermCharacter(char c) { return isPlainName(std::string_view(&c, 1)) || (c >= '0' && c <= '9') || c == '.'; }

/// @brief How a character changes the depth of brackets.
/// @param c The character.
/// @return 1 for an opening bracket, -1 for a closing one, else 0.
constexpr int bracketStep(char c) noexcept {
	if(c == '(' || c == '[' || c == '{') return 1;
	return c == ')' || c == ']' || c == '}' ? -1 : 0;
}

/// @brief Tells whether an expression's text is one term, which nothing around it can split: a path
/// (`state.user.name`, `it.cards[0]`), a call (`length(xs)`), a group (`(a + b)`), a text, a list or an object.
/// @param text The text.
/// @return true if it is: nothing outside brackets and quotes but letters, digits, `_` and `.`.
bool isOneTerm(std::string_view text) {
	int depth = 0;
	char quote = 0;
	for(std::size_t at = 0; at < text.size(); ++at) {
		const char c = text[at];
		if(quote) {
			if(c == '\\') ++at;
			else if(c == quote) quote = 0;
		} else if(c == '\'' || c == '"') {
			quote = c;
		} else if(const int step = bracketStep(c); step != 0) {
			depth += step;
		} else if(depth == 0 && !isTermCharacter(c)) {
			return false;
		}
	}
	return !text.empty() && depth == 0 && quote == 0;
}

/// @brief The bit of what an expression calls a function's parameter (Compiler::Rename::uses).
/// @param name `it`, `index` or `acc`.
/// @return Its bit, or 0.
constexpr unsigned itemBit(std::string_view name) noexcept {
	if(name == "it") return 1U;
	if(name == "index") return 2U;
	return name == "acc" ? 4U : 0U;
}

/// The functions on each item that read only the truth of what their function gives, so that its body is a test.
/// Not `some`, which gives that value: its body is a test only where the `some` is one.
constexpr std::array<std::string_view, 4> testingItemFunctions = {"filter", "remove", "find", "every?"};

/// @brief Tells whether a set's member is one an expression's list holds as the set does, `=` being `==` for it: nil,
/// a number, a text, a keyword (the text of its name) or a boolean.
/// @param member The member.
/// @return true if it is.
bool isConstantMember(const Value& member) {
	switch(member.kind()) {
	case Kind::Nil:
	case Kind::Boolean:
	case Kind::Integer:
	case Kind::Unsigned:
	case Kind::Float:
	case Kind::String:
	case Kind::Keyword:
		return true;
	default:
		return false;
	}
}

/// @brief The members of a set the reader made, as an expression's list: `#{:a :b}` is `['a', 'b']`.
/// @param machine The machine, which writes each member.
/// @param set The form.
/// @param truthful Whether to leave out nil and `false`: called as a function, the set gives them, which are no truth.
/// @return The list's text, or nothing if the form isn't such a set, a member isn't a constant (isConstantMember()),
/// or no member is left.
std::optional<std::string> setListText(Machine& machine, const Value& set, bool truthful) {
	if(set.kind() != Kind::Set) return std::nullopt;
	std::string text = "[";
	for(std::size_t at = 0; at < set.count(); ++at) {
		const Value member = set.at(at);
		if(!isConstantMember(member)) return std::nullopt;
		if(truthful && !member.truthy()) continue; // nil and false
		if(text.size() > 1) text += ", ";
		text += expressionOfValue(machine, member);
	}
	if(text.size() == 1) return std::nullopt;
	return text + "]";
}

/// @brief Tells whether a set has a number among its members.
/// @param set The set.
/// @return true if it has.
bool holdsNumber(const Value& set) {
	return std::ranges::any_of(std::views::iota(std::size_t{0}, set.count()),
	                           [&](std::size_t at) { return set.at(at).isNumber(); });
}

/// @brief Sets the flag of a test for the form being written, and puts it back once that form is written or fails.
class TestScope {
public:
	/// @brief Sets it.
	/// @param flag The compiler's flag, which outlives this.
	/// @param test Its value for the form.
	TestScope(bool& flag, bool test) noexcept : flag_(flag), outer_(std::exchange(flag, test)) {}
	TestScope(const TestScope&) = delete;
	TestScope& operator=(const TestScope&) = delete;
	TestScope(TestScope&&) = delete;
	TestScope& operator=(TestScope&&) = delete;
	/// @brief Puts it back.
	~TestScope() { flag_ = outer_; }

private:
	bool& flag_; ///< See the constructor.
	bool outer_; ///< What it was.
};

} // namespace

std::string pathExpression(std::string_view word) {
	if(word.starts_with('(')) return std::string(word); // an expression a view was given (Compiler::compileLiveName())
	const std::size_t dot = word.find('.');
	const std::string_view first = word.substr(0, dot);
	const std::string text = isPlainName(first) ? std::string(first) : "[" + expressionText(first) + "]";
	return text + (dot == std::string_view::npos ? "" : membersText(word.substr(dot + 1)));
}

void Compiler::lowerForm(const Value& written, int depth, std::vector<BindingPart>& parts, bool test) {
	const TestScope scope(test_, test);
	const Value form = written.kind() == Kind::List ? expanded(written) : written;
	if(depth > maxCompileDepth)
		fail(form, "nested too deeply to compile, more than " + std::to_string(maxCompileDepth) + " levels");
	if(headWord(form) == "throw") // as a case or a condp with no default has, when nothing matches
		fail(form, "a binding can't throw: give a value for when nothing matches, or define a function for this");
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
		if(renamed->hidden && !renamed->as.empty())
			fail(word, "a function inside another, in a binding, can't use the outer one's \"" +
			               notation::Lexer::excerpt(head) + "\": define a function for this");
		if(renamed->hidden)
			fail(word, "a function in a binding can't use \"" + notation::Lexer::excerpt(head) +
			               "\", whose value reads the item of a function around it: define a function for this");
		if(!renamed->as.empty()) {
			itemsRead_ |= itemBit(renamed->as);
			parts.push_back({renamed->as + members, {}, false, {}, {}});
			return;
		}
		substituted_ += renamed->size; // a let's name: its value, written again
		if(substituted_ > maxBindingText) failTooLarge(word);
		itemsRead_ |= renamed->uses;
		parts.insert(parts.end(), renamed->parts.begin(), renamed->parts.end());
		if(!members.empty()) parts.push_back({members, {}, false, {}, {}});
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
                         const std::string& separator, bool tests) {
	parts.push_back({open, {}, false, {}, {}});
	for(std::size_t at = 1; at < form.count(); ++at) {
		if(at != 1) parts.push_back({separator, {}, false, {}, {}});
		lowerForm(form.at(at), depth + 1, parts, tests);
	}
	parts.push_back({")", {}, false, {}, {}});
}

bool Compiler::lowerIf(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = headWord(form);
	const std::size_t operands = form.count() - 1;
	if((head != "if" || (operands != 2 && operands != 3)) && (head != "when" || operands != 2)) return false;
	parts.push_back({"(", {}, false, {}, {}});
	lowerForm(form.at(1), depth + 1, parts, true);
	parts.push_back({" ? ", {}, false, {}, {}});
	lowerForm(form.at(2), depth + 1, parts);
	parts.push_back({" : ", {}, false, {}, {}});
	if(operands == 3) lowerForm(form.at(3), depth + 1, parts);
	else parts.push_back({"null", {}, false, {}, {}});
	parts.push_back({")", {}, false, {}, {}});
	return true;
}

bool Compiler::lowerCond(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::size_t operands = form.count() - 1;
	if(headWord(form) != "cond" || operands < 2 || operands % 2 != 0) return false;
	// (cond a x b y :else z): a ? x : (b ? y : z)
	std::size_t closing = 0;
	for(std::size_t at = 1; at < form.count(); at += 2) {
		const bool last = at + 2 >= form.count();
		if(last && form.at(at).kind() == Kind::Keyword) {
			lowerForm(form.at(at + 1), depth + 1, parts);
			break;
		}
		parts.push_back({"(", {}, false, {}, {}});
		lowerForm(form.at(at), depth + 1, parts, true);
		parts.push_back({" ? ", {}, false, {}, {}});
		lowerForm(form.at(at + 1), depth + 1, parts);
		parts.push_back({" : ", {}, false, {}, {}});
		++closing;
		if(last) parts.push_back({"null", {}, false, {}, {}});
	}
	parts.push_back({std::string(closing, ')'), {}, false, {}, {}});
	return true;
}

bool Compiler::standsAlone(const std::vector<BindingPart>& parts) {
	std::string text;
	for(const BindingPart& part : parts) {
		if(!part.worked) text += part.text;
		else if(part.locals.empty() || standsAlone(part.inner)) text += "(0)"; // a value of the build: one term
		else return false; // or, holding a name of the running app, its text
	}
	return isOneTerm(text);
}

std::size_t Compiler::textSize(const std::vector<BindingPart>& parts) {
	std::size_t size = 0;
	for(const BindingPart& part : parts) size += part.worked ? workedPartSize + textSize(part.inner) : part.text.size();
	return size;
}

bool Compiler::lowerLet(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(headWord(form) != "let") return false;
	if(form.count() != 3) fail(form, std::string(notAnExpression)); // a binding is one value
	const Value pairs = form.at(1);
	if(pairs.kind() != Kind::Vector || pairs.count() % 2 != 0)
		fail(pairs, "expected a vector of names and values, in pairs");
	const std::size_t before = renames_.size();
	for(std::size_t at = 0; at < pairs.count(); at += 2) { // each value sees the names before it
		if(pairs.at(at).kind() != Kind::Symbol || pairs.at(at).text() == "&")
			fail(pairs.at(at), "a let in a binding names plain values: define a function for a pattern");
		Rename name{pairs.at(at).text(), ""};
		// What the value reads counts only where the name is written (lowerWord())
		const unsigned read = std::exchange(itemsRead_, 0U);
		lowerForm(pairs.at(at + 1), depth + 1, name.parts);
		name.uses = std::exchange(itemsRead_, read);
		if(!standsAlone(name.parts)) {
			name.parts.insert(name.parts.begin(), {"(", {}, false, {}, {}});
			name.parts.push_back({")", {}, false, {}, {}});
		}
		name.size = textSize(name.parts);
		renames_.push_back(std::move(name));
	}
	lowerForm(form.at(2), depth + 1, parts);
	renames_.resize(before);
	return true;
}

void Compiler::checkLetText(const Value& form, const std::vector<BindingPart>& parts) const {
	if(substituted_ > 0 && textSize(parts) > maxBindingText) failTooLarge(form);
}

void Compiler::failTooLarge(const Value& form) const {
	fail(form, "this binding is too large as an expression: define a function for it");
}

bool Compiler::lowerOperator(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(lowerLet(form, depth, parts) || lowerIf(form, depth, parts) || lowerCond(form, depth, parts)) return true;
	if(const AroundOperator* around = aroundOf(aroundOperators, form)) {
		lowerList(form, depth, parts, std::string(around->open), "", around->word != "-"); // not and truthy? test
		parts.back().text = around->close;
		return true;
	}
	const std::string_view head = headWord(form);
	const std::size_t operands = form.count() - 1;
	// NOLINTNEXTLINE(readability-qualified-auto): std::array's iterator is a pointer only in some standard libraries
	const auto infix = std::ranges::find(infixOperators, head, &InfixOperator::word);
	if(infix == infixOperators.end() || operands < infix->fewest || operands > infix->most) return false;
	// (if (and a b) ...) tests a and b; (str (or a b)) gives one of them
	lowerList(form, depth, parts, "(", " " + std::string(infix->expression) + " ",
	          test_ && (head == "and" || head == "or"));
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
                                 std::vector<BindingPart>& parts, bool test) {
	const Value params = function.at(1);
	if(function.count() != 3 || params.count() == 0 || params.count() > names.size() ||
	   std::ranges::any_of(std::views::iota(std::size_t{0}, params.count()),
	                       [&](std::size_t at) { return params.at(at).kind() != Kind::Symbol; }))
		fail(function, "a function in a binding takes 1 to " + std::to_string(names.size()) +
		                   " plain parameters and has one form: define a function for more");
	const std::size_t before = renames_.size();
	std::vector<bool> hidden; // an outer item named as one of this function's is out of reach inside it
	unsigned bits = 0;
	for(const std::string_view name : names) bits |= itemBit(name); // all an expression's function gives, named or not
	for(Rename& outer : renames_) {
		hidden.push_back(outer.hidden);
		// A let's name too, when its value reads an item this function names again
		if((outer.as.empty() ? outer.uses : itemBit(outer.as)) & bits) outer.hidden = true;
	}
	for(std::size_t at = 0; at < params.count(); ++at)
		renames_.push_back({params.at(at).text(), std::string(names[at])});
	lowerForm(function.at(2), depth + 1, parts, test);
	renames_.resize(before);
	for(std::size_t at = 0; at < before; ++at) renames_[at].hidden = hidden[at];
}

bool Compiler::lowerReduce(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(tableWord(calledName(form)) != "reduce" || form.count() != 4 || !isItemFunction(form.at(1))) return false;
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

bool Compiler::lowerItemFunction(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(lowerSetGiven(form, depth, parts) || lowerReduce(form, depth, parts)) return true;
	const std::string_view head = tableWord(calledName(form));
	// NOLINTNEXTLINE(readability-qualified-auto): std::array's iterator is a pointer only in some standard libraries
	const auto found = std::ranges::find(itemFunctions, head, &ItemFunction::word);
	const bool descending = isDescendingSort(form);
	// A keyword is the function that looks it up, as in Clojure: (sort-by :d xs) is sort(xs, it.d)
	const bool keyword = form.at(1).kind() == Kind::Keyword && head != "map-indexed";
	if(found == itemFunctions.end() || (form.count() != 3 && !descending) || !(keyword || isItemFunction(form.at(1))))
		return false;
	parts.push_back({std::string(found->expression) + "(", {}, false, {}, {}});
	lowerForm(form.at(form.count() - 1), depth + 1, parts);
	parts.push_back({head == "remove" ? ", !(" : ", ", {}, false, {}, {}});
	if(keyword) {
		parts.push_back({"it", {}, false, {}, {}});
		lowerKey(form.at(1), depth, parts);
	} else {
		const bool test = std::ranges::contains(testingItemFunctions, head) || (head == "some" && test_);
		lowerFunctionBody(form.at(1), std::span(found->names).first(found->parameters), depth, parts, test);
	}
	parts.push_back({head == "remove" ? "))" : descending ? ", 'desc')" : ")", {}, false, {}, {}});
	return true;
}

bool Compiler::lowerSetGiven(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = tableWord(calledName(form));
	// (some #{:a} xs) gives the member, which the expression's some doesn't: only its truth is the same
	const bool testing = std::ranges::contains(testingItemFunctions, head) && head != "find";
	if(form.count() != 3 || !(testing || (head == "some" && test_))) return false;
	const std::optional<std::string> members = setListText(machine_, form.at(1), true);
	if(!members) return false;
	parts.push_back({std::string(head == "every?" ? "every"
	                             : head == "some" ? "some"
	                                              : "filter") +
	                     "(",
	                 {},
	                 false,
	                 {},
	                 {}});
	lowerForm(form.at(2), depth + 1, parts);
	parts.push_back(
	    {(head == "remove" ? ", !(contains(" : ", contains(") + *members + ", it)" + (head == "remove" ? "))" : ")"),
		 {},
		 false,
		 {},
		 {}});
	return true;
}

bool Compiler::lowerSetMember(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(form.count() != 3 && form.count() != 2) return false;
	const bool called = form.count() == 2; // (#{:a} x), else (contains? #{:a} x)
	if(!called && tableWord(calledName(form)) != "contains?") return false;
	const Value set = form.at(called ? 0 : 1);
	// Called, the set gives its member: the expression's x where they are equal, but 1.0 is not 1, and false no truth
	const bool test = called && test_;
	if(called && !test && set.kind() == Kind::Set && holdsNumber(set)) return false;
	const std::optional<std::string> members = setListText(machine_, set, test);
	if(!members) return false;
	writeSetMember(*members, form.at(called ? 1 : 2), called && !test, depth, parts);
	return true;
}

void Compiler::writeSetMember(const std::string& members, const Value& value, bool chosen, int depth,
                              std::vector<BindingPart>& parts) {
	std::vector<BindingPart> written;
	lowerForm(value, depth + 1, written);
	if(!chosen) { // contains(L, x)
		parts.push_back({"contains(" + members + ", ", {}, false, {}, {}});
		parts.insert(parts.end(), written.begin(), written.end());
		parts.push_back({")", {}, false, {}, {}});
		return;
	}
	// x is written twice, so nested tests would double the text at each level: bounded as a let's names are
	if(2 * textSize(written) > maxBindingText) failTooLarge(value);
	parts.push_back({"(contains(" + members + ", ", {}, false, {}, {}}); // (contains(L, x) ? x : null)
	parts.insert(parts.end(), written.begin(), written.end());
	parts.push_back({") ? ", {}, false, {}, {}});
	parts.insert(parts.end(), written.begin(), written.end());
	parts.push_back({" : null)", {}, false, {}, {}});
}

bool Compiler::lowerMember(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = headWord(form);
	const std::size_t arguments = form.count() - 1;
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
	return false;
}

bool Compiler::lowerLibrary(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = tableWord(calledName(form));
	const std::size_t arguments = form.count() - 1;
	if(lowerItemFunction(form, depth, parts) || lowerSetMember(form, depth, parts)) return true;
	// NOLINTNEXTLINE(readability-qualified-auto): std::array's iterator is a pointer only in some standard libraries
	if(const auto same = std::ranges::find(sameFunctions, head, &SameFunction::word);
	   same != sameFunctions.end() && arguments >= same->fewest && arguments <= same->most) {
		lowerList(form, depth, parts, std::string(same->expression) + "(", ", ");
		return true;
	}
	if(lowerMember(form, depth, parts)) return true;
	if(const AroundOperator* around = aroundOf(aroundFunctions, form, head)) {
		lowerList(form, depth, parts, std::string(around->open), "");
		parts.back().text = around->close;
		return true;
	}
	if(head == "not-empty" && arguments == 1) { // (not-empty x) is (empty(x) ? null : x)
		std::vector<BindingPart> value;
		lowerForm(form.at(1), depth + 1, value);
		parts.push_back({"(empty(", {}, false, {}, {}});
		parts.insert(parts.end(), value.begin(), value.end());
		parts.push_back({") ? null : ", {}, false, {}, {}});
		parts.insert(parts.end(), value.begin(), value.end());
		parts.push_back({")", {}, false, {}, {}});
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

bool Compiler::lowerCalledValue(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(form.count() != 2) return false;
	if(form.at(0).kind() == Kind::Keyword) { // (:title card) is card.title
		lowerForm(form.at(1), depth + 1, parts);
		lowerKey(form.at(0), depth, parts);
		return true;
	}
	return form.at(0).kind() == Kind::Set && lowerSetMember(form, depth, parts); // (#{:a} x)
}

void Compiler::lowerCall(const Value& form, int depth, std::vector<BindingPart>& parts) {
	if(lowerCalledValue(form, depth, parts)) return;
	const std::string_view written = headWord(form);
	if(written.empty()) fail(form, std::string(notAnExpression)); // ((fn ...) x)
	// An item's name the binding writes, or a local's
	const bool bound = isItemOrLocal(written);
	const std::string_view head = calledName(form);
	const Global* global = bound ? nullptr : machine_.findGlobal(head);
	if(!bound && lowerBuiltIn(form, global, depth, parts)) return;
	if(isSpecial(head) || bound) fail(form, std::string(notAnExpression));
	if(global && global->state != Global::State::Undefined) {
		for(std::size_t at = 1; at < form.count(); ++at) // no expression holds a function to give it
			if(headWord(expanded(form.at(at))) == "fn")
				fail(form.at(at), "a binding can't give a function to \"" + notation::Lexer::excerpt(head) +
				                      "\": define a function for this");
		lowerList(form, depth, parts, "lisp(" + expressionText(head) + (form.count() > 1 ? ", " : ""), ", ");
		return;
	}
	lowerOwnFunction(form, depth, parts);
}

bool Compiler::lowerBuiltIn(const Value& form, const Global* global, int depth, std::vector<BindingPart>& parts) {
	const bool defined = global && global->state != Global::State::Undefined;
	// A program's own function of that name isn't the expression's operator
	if(!isSpecial(calledName(form)) && !(defined && global->library)) return false;
	return lowerOperator(form, depth, parts) || (defined && lowerLibrary(form, depth, parts));
}

void Compiler::lowerOwnFunction(const Value& form, int depth, std::vector<BindingPart>& parts) {
	const std::string_view head = headWord(form);
	if(!isPlainName(head))
		fail(form, "nothing defines \"" + notation::Lexer::excerpt(head) + "\", and no expression can call it");
	// An expression's own function: one on each item takes a function whose parameters are named as it names them
	parts.push_back({std::string(head) + "(", {}, false, {}, {}});
	for(std::size_t at = 1; at < form.count(); ++at) {
		if(at != 1) parts.push_back({", ", {}, false, {}, {}});
		const Value item = form.at(at);
		if(!isItemFunction(item)) {
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
