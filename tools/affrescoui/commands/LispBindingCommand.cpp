/// @file
/// @brief `affrescoui lisp-binding`: an expression's text (`{{state.view == 'board'}}`, or a text with values in it)
/// written as the Lisp form a program writes for it (`~(= state.view "board")`), and `--prove`, which translates every
/// such text in some JSON files, builds each form back to a binding as a program does, and compares the two parsed
/// trees (SCREEN_FORMAT.md, "Writing an app as a program").

#include "Cli.hpp"

#include "ExpressionFunctions.hpp"
#include "ExpressionMachine.hpp"
#include "ExpressionParser.hpp"

#include <affresco/format/Evaluate.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>
#include <affresco/notation/Words.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace affresco::cli {

namespace {

/// @brief What can't be written as a Lisp form, and why.
class Untranslatable : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

/// @brief An expression's operator and the language's word for it.
struct Operator {
	std::string_view name; ///< The operator: "$equals".
	std::string_view word; ///< The language's word: "=".
};

/// The operators written as a call of the language's word, operands in order.
constexpr std::array<Operator, 13> operators{{{"$equals", "="},
                                              {"$notEquals", "not="},
                                              {"$less", "<"},
                                              {"$greater", ">"},
                                              {"$lessOrEqual", "<="},
                                              {"$greaterOrEqual", ">="},
                                              {"$multiply", "*"},
                                              {"$divide", "/"},
                                              {"$mod", "mod"},
                                              {"$and", "and"},
                                              {"$or", "or"},
                                              {"$coalesce", "or-else"},
                                              {"$not", "not"}}};

/// The expression's functions the language has under another name, with the same arguments.
const std::map<std::string, std::string, std::less<>> renamed = {
    {"length", "count"}, {"values", "vals"}, {"upper", "upper-case"}, {"lower", "lower-case"}, {"empty", "empty?"}};

/// @brief Tells whether a name is a plain word an expression and the language both read the same way.
/// @param name The name.
/// @return true if it is.
bool isPlainKey(std::string_view name) {
	const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
	return !name.empty() && letter(name.front()) &&
	       std::ranges::all_of(name, [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}

/// @brief Writes an operator tree (ExpressionParser.hpp) as a Lisp form.
class Translator {
public:
	/// @brief Prepares to translate.
	/// @param language A fresh interpreter: what the language defines, which a path can't be named.
	explicit Translator(lisp::Interpreter& language) : language_(language) {}

	/// @brief Writes a tree.
	/// @param tree The tree.
	/// @return The form's text.
	/// @throws Untranslatable If it can't.
	std::string form(const json& tree) {
		if(!tree.is_object()) return literal(tree);
		if(tree.size() != 1 || !tree.begin().key().starts_with('$')) throw Untranslatable("not an operator tree");
		const std::string& name = tree.begin().key();
		const json& arguments = tree.begin().value();
		if(name == "$value") return literal(arguments);
		if(name == "$get") return path(arguments);
		if(name == "$object") return object(arguments);
		if(name == "$if") return conditional(arguments);
		if(name == "$add") return add(arguments);
		if(name == "$subtract")
			return arguments.size() == 2 && arguments[0] == 0 ? "(- " + form(arguments[1]) + ")" : call("-", arguments);
		if(name == "$lisp") return programCall(arguments);
		if((name == "$equals" || name == "$notEquals") && arguments.size() == 2 && arguments[1].is_null())
			return std::string(name == "$equals" ? "(nil? " : "(some? ") + form(arguments[0]) + ")";
		if(const auto* found = std::ranges::find(operators, name, &Operator::name); found != operators.end())
			return call(found->word, name == "$not" ? json::array({arguments}) : arguments);
		return function(name.substr(1), arguments);
	}

	/// @brief The names a path starts with that the language has too: they read the running app only as a local's,
	/// a `repeat`'s item or a component's parameter, which hides the language's.
	/// @return Them.
	[[nodiscard]] const std::set<std::string>& localNames() const noexcept { return localNames_; }

	/// @brief Tells whether a word is a special form or a definition's, which no local can hide.
	/// @param word The word.
	/// @return true if it is.
	static bool isForm(std::string_view word) {
		static const std::set<std::string_view> forms = {"quote",
		                                                 "backquote",
		                                                 "if",
		                                                 "do",
		                                                 "when",
		                                                 "cond",
		                                                 "and",
		                                                 "or",
		                                                 "let",
		                                                 "loop",
		                                                 "recur",
		                                                 "fn",
		                                                 "try",
		                                                 "throw",
		                                                 "catch",
		                                                 "unquote",
		                                                 "unquote-splicing",
		                                                 "def",
		                                                 "defn",
		                                                 "defmacro",
		                                                 "defscreen",
		                                                 "defcommand",
		                                                 "defkey",
		                                                 "defcustom",
		                                                 "defhook",
		                                                 "add-hook",
		                                                 "remove-hook",
		                                                 "advice",
		                                                 "defview",
		                                                 "defcomponent",
		                                                 "defstate",
		                                                 "defcomputed",
		                                                 "defstyle",
		                                                 "defmenu",
		                                                 "deftokens",
		                                                 "include",
		                                                 "repeat"};
		return forms.contains(word);
	}

	/// @brief The names of the program's functions the text calls through `lisp(...)`.
	/// @return Them.
	[[nodiscard]] const std::set<std::string>& programNames() const noexcept { return programNames_; }

private:
	/// @brief A literal: text, a number, true, false, nil, or a list.
	/// @param value The value.
	/// @return Its form.
	std::string literal(const json& value) {
		if(value.is_null()) return "nil";
		if(value.is_array()) {
			std::string text = "[";
			for(const json& item : value) text.append(text.size() > 1 ? " " : "").append(form(item));
			return text + "]";
		}
		if(value.is_object()) throw Untranslatable("an object where a value was expected");
		if(value.is_number_float()) { // 0.000001, not 1e-06
			std::array<char, 400> digits{};
			const auto end = std::to_chars(digits.data(), digits.data() + digits.size(), value.get<double>(),
			                               std::chars_format::fixed);
			const std::string text(digits.data(), end.ptr);
			return text.contains('.') ? text : text + ".0";
		}
		return value.dump(-1, ' ', false, json::error_handler_t::replace);
	}

	/// @brief `(word a b ...)`.
	/// @param word The head.
	/// @param arguments The operands.
	/// @return The form.
	std::string call(std::string_view word, const json& arguments) {
		std::string text = "(" + std::string(word);
		for(const json& argument : arguments) text.append(" ").append(form(argument));
		return text + ")";
	}

	/// @brief A path: `state.items`, `(get state.items state.at)`, `(get-in x [a :b c])`.
	/// @param keys Its root, then its keys.
	/// @return The form.
	std::string path(const json& keys) {
		if(keys.empty()) throw Untranslatable("an empty path");
		std::string base;
		if(keys[0].is_string()) {
			const std::string& root = keys[0].get_ref<const std::string&>();
			if(!isPlainKey(root)) throw Untranslatable("the name \"" + root + "\" isn't a plain word");
			if(isLanguageName(root) && root != "it" && root != "index" && root != "acc") {
				if(notation::isLanguageWord(root) && Translator::isForm(root))
					throw Untranslatable("the name \"" + root + "\" is a form of the language: rename it");
				localNames_.insert(root); // only as a repeat's or a component's name, which hides the language's
			}
			base = root;
		} else {
			base = form(keys[0]);
		}
		bool word = keys[0].is_string();
		std::vector<std::string> rest;
		for(std::size_t at = 1; at < keys.size(); ++at) {
			const json& key = keys[at];
			const bool name = key.is_string() && isPlainKey(key.get_ref<const std::string&>());
			const bool index = key.is_number_unsigned() || (key.is_number_integer() && key.get<std::int64_t>() >= 0);
			if(word && rest.empty() && (name || index)) {
				base += "." + (name ? key.get<std::string>() : key.dump());
				continue;
			}
			word = false;
			rest.push_back(key.is_string() ? (name ? ":" + key.get<std::string>() : key.dump()) : form(key));
		}
		if(rest.empty()) return base;
		if(rest.size() == 1 && rest[0].starts_with(':')) return "(" + rest[0] + " " + base + ")";
		if(rest.size() == 1) return "(get " + base + " " + rest[0] + ")";
		std::string text = "(get-in " + base + " [";
		for(std::size_t at = 0; at < rest.size(); ++at) text.append(at ? " " : "").append(rest[at]);
		return text + "])";
	}

	/// @brief `{:key value ...}`.
	/// @param fields The object's fields.
	/// @return The form.
	std::string object(const json& fields) {
		std::string text = "{";
		for(const auto& [key, value] : fields.items()) {
			text.append(text.size() > 1 ? " " : "")
			    .append(isPlainKey(key) ? ":" + key : json(key).dump())
			    .append(" ")
			    .append(form(value));
		}
		return text + "}";
	}

	/// @brief `(if c a b)`, or `(cond ...)` for a chain.
	/// @param arguments Condition, then, otherwise.
	/// @return The form.
	std::string conditional(const json& arguments) {
		if(arguments.size() != 3) throw Untranslatable("a condition without two branches");
		const json& otherwise = arguments[2];
		if(!(otherwise.is_object() && otherwise.contains("$if")))
			return "(if " + form(arguments[0]) + " " + form(arguments[1]) + " " + form(otherwise) + ")";
		std::string text = "(cond " + form(arguments[0]) + " " + form(arguments[1]);
		const json* next = &otherwise;
		while(next->is_object() && next->contains("$if") && (*next)["$if"].size() == 3) {
			text.append(" ").append(form((*next)["$if"][0])).append(" ").append(form((*next)["$if"][1]));
			next = &(*next)["$if"][2];
		}
		return text + " :else " + form(*next) + ")";
	}

	/// @brief `+`: `(str "a" x)` when one of its first two operands is text, so it joins text; else `(+ ...)`.
	/// @param arguments The operands.
	/// @return The form.
	std::string add(const json& arguments) {
		const auto isText = [&](std::size_t at) {
			return arguments.size() > at && arguments[at].is_object() && arguments[at].contains("$value") &&
			       arguments[at]["$value"].is_string();
		};
		return call(isText(0) || isText(1) ? "str" : "+", arguments); // text in the first two: it joins text
	}

	/// @brief `lisp('name', ...)`: a call of the program's function.
	/// @param arguments Its name, then its arguments.
	/// @return The form.
	std::string programCall(const json& arguments) {
		if(!arguments.is_array() || arguments.empty() || !arguments[0].is_object() ||
		   !arguments[0].contains("$value") || !arguments[0]["$value"].is_string())
			throw Untranslatable("lisp() with a name that isn't written out");
		const std::string name = arguments[0]["$value"].get<std::string>();
		programNames_.insert(name);
		return call(name, json(arguments.begin() + 1, arguments.end()));
	}

	/// @brief `(fn [it] body)`, or with more parameters.
	/// @param names Its parameters.
	/// @param body Its body.
	/// @return The form.
	std::string lambda(std::string_view names, const json& body) {
		return "(fn [" + std::string(names) + "] " + form(body) + ")";
	}

	/// @brief Tells whether an item's expression reads `index`.
	/// @param tree The expression.
	/// @return true if it does.
	static bool readsIndex(const json& tree) {
		if(tree.is_object() && tree.size() == 1 && tree.contains("$get") && !tree["$get"].empty() &&
		   tree["$get"][0] == "index")
			return true;
		if(tree.is_object() || tree.is_array())
			return std::ranges::any_of(tree, [](const json& item) { return readsIndex(item); });
		return false;
	}

	/// @brief A function on each item: `filter(list, c)` is `(filter (fn [it] c) list)`.
	/// @param name The function.
	/// @param arguments Its arguments.
	/// @return The form, or "" if it is no such function.
	std::string itemFunction(std::string_view name, const json& arguments) {
		static const std::map<std::string_view, std::string_view> same = {
		    {"filter", "filter"}, {"find", "find"}, {"some", "some"}, {"every", "every?"}};
		if(arguments.is_array() && arguments.size() == 1 && (name == "sort" || name == "sum" || name == "count"))
			return "(" + std::string(name) + " " + form(arguments[0]) + ")";
		if(!arguments.is_array() || arguments.size() < 2) return "";
		const json& list = arguments[0];
		const json& body = arguments[1];
		const std::string params = readsIndex(body) ? "it index" : "it";
		if(const auto found = same.find(name); found != same.end() && arguments.size() == 2)
			return "(" + std::string(found->second) + " " + lambda(params, body) + " " + form(list) + ")";
		if(name == "map" && arguments.size() == 2)
			return readsIndex(body) ? "(map-indexed " + lambda("index it", body) + " " + form(list) + ")"
			                        : "(map " + lambda("it", body) + " " + form(list) + ")";
		if(name == "count" && arguments.size() == 2)
			return "(count (filter " + lambda(params, body) + " " + form(list) + "))";
		if(name == "sort" && (arguments.size() == 2 || arguments.size() == 3)) {
			const json order = arguments.size() == 3 ? arguments[2] : json{{"$value", "asc"}};
			if(order != json{{"$value", "asc"}} && order != json{{"$value", "desc"}})
				throw Untranslatable("sort with an order that isn't written 'asc' or 'desc'");
			const bool descending = order == json{{"$value", "desc"}};
			return "(sort-by " + lambda(params, body) + (descending ? " > " : " ") + form(list) + ")";
		}
		if(name == "reduce" && (arguments.size() == 2 || arguments.size() == 3))
			return "(reduce " + lambda("acc it", body) + " " + (arguments.size() == 3 ? form(arguments[2]) : "nil") +
			       " " + form(list) + ")";
		if(name == "sum" || name == "scan") {
			std::string text =
			    "(" + std::string(name) + " " + form(list) + " " + lambda(name == "scan" ? "acc it" : params, body);
			for(std::size_t at = 2; at < arguments.size(); ++at) text.append(" ").append(form(arguments[at]));
			return text + ")";
		}
		return "";
	}

	/// @brief An expression's function: `(name args...)`, under the language's name when it has another.
	/// @param name The function.
	/// @param arguments Its argument, or its arguments.
	/// @return The form.
	std::string function(const std::string& name, const json& arguments) {
		const format::detail::Function* row = format::detail::findOperator("$" + name);
		if(!row) throw Untranslatable("the unknown function " + name);
		if(row->group == format::detail::Group::Items) {
			std::string text = itemFunction(name, arguments);
			if(text.empty()) throw Untranslatable(name + " in a way the language has no form for");
			return text;
		}
		const json list = row->takes == format::detail::Takes::One ? json::array({arguments}) : arguments;
		if(name == "join" && list.size() == 2) return "(join " + form(list[1]) + " " + form(list[0]) + ")";
		if(name == "count" && list.size() == 1) return "(count " + form(list[0]) + ")";
		if(const auto found = renamed.find(name); found != renamed.end()) return call(found->second, list);
		if(isLanguageName(name) && name != "concat" && name != "merge" && name != "keys" && name != "reverse" &&
		   name != "min" && name != "max" && name != "abs" && name != "range")
			throw Untranslatable("the function " + name +
			                     ", which the language has under the same name for "
			                     "something else");
		return call(name, list);
	}

	/// @brief Tells whether the language defines a name, so that a path can't be called by it.
	/// @param name The name.
	/// @return true if it does.
	bool isLanguageName(const std::string& name) const {
		return notation::isLanguageWord(name) || language_.global(name).has_value() || name == "true" ||
		       name == "false" || name == "nil";
	}

	lisp::Interpreter& language_;        ///< See the constructor.
	std::set<std::string> programNames_; ///< See programNames().
	std::set<std::string> localNames_;   ///< See localNames().
};

/// @brief A text's parts: text as it is, and the expressions inside `{{ }}`.
struct TextPart {
	std::string text;        ///< The text, or the expression.
	bool expression = false; ///< It is an expression.
};

/// @brief Cuts a text into its parts, as a template is read (a `}}` inside quotes or an object doesn't close one).
/// @param text The text.
/// @return The parts.
std::vector<TextPart> partsOf(std::string_view text) {
	std::vector<TextPart> parts;
	std::size_t at = 0;
	while(at < text.size()) {
		const std::size_t open = text.find("{{", at);
		const std::size_t close = open == std::string_view::npos ? open : format::detail::closing(text, open + 2);
		if(close == std::string_view::npos) {
			parts.push_back({std::string(text.substr(at)), false});
			break;
		}
		if(open > at) parts.push_back({std::string(text.substr(at, open - at)), false});
		parts.push_back({std::string(text.substr(open + 2, close - open - 2)), true});
		at = close + 2;
	}
	return parts;
}

/// @brief Writes a value's text as the form a program writes for it: `~form`, or the text itself without `{{`.
/// @param text The text.
/// @param translator The translator.
/// @return The form, after `~`.
/// @throws Untranslatable If it can't.
std::string translate(std::string_view text, Translator& translator) {
	const std::vector<TextPart> parts = partsOf(text);
	if(parts.size() == 1 && parts[0].expression) return translator.form(format::detail::parse(parts[0].text, "text"));
	std::string form = "(str";
	for(const TextPart& part : parts)
		form.append(" ").append(part.expression ? translator.form(format::detail::parse(part.text, "text"))
		                                        : json(part.text).dump(-1, ' ', false, json::error_handler_t::replace));
	return form + ")";
}

/// @brief Tells whether a tree reads nothing but the names an item function gives (`it`, `index`, `acc`).
/// @param tree The tree.
/// @return true if it does.
bool readsNothing(const json& tree) {
	if(tree.is_object() && tree.size() == 1 && tree.begin().key() == "$get") {
		const json& keys = tree.begin().value();
		if(keys.empty() || !keys[0].is_string()) return false;
		const std::string& root = keys[0].get_ref<const std::string&>();
		return (root == "it" || root == "index" || root == "acc") &&
		       std::all_of(keys.begin() + 1, keys.end(), [](const json& key) { return readsNothing(key); });
	}
	if(tree.is_object() && tree.size() == 1) {
		const std::string& key = tree.begin().key();
		if(key == "$lisp" || key == "$now" || key == "$today" || key == "$t") return false;
	}
	if(tree.is_structured()) return std::ranges::all_of(tree, [](const json& item) { return readsNothing(item); });
	return true;
}

/// @brief A value as the tree of a literal.
/// @param value The value.
/// @return The tree.
json literalTree(const json& value) {
	if(value.is_string()) return {{"$value", value}};
	if(value.is_array()) {
		json items = json::array();
		for(const json& item : value) items.push_back(literalTree(item));
		return items;
	}
	if(value.is_object()) {
		json fields = json::object();
		for(const auto& [key, member] : value.items()) fields[key] = literalTree(member);
		return {{"$object", fields}};
	}
	return value;
}

/// @brief An operator tree, made comparable: the forms the language writes another way, as they are written.
/// @param tree The tree.
/// @return It, with `count(l)` as `length(l)`, `count(l, c)` as `length(filter(l, c))`, `reduce(l, e)` with a null
/// start, and `sort(l, k, 'asc')` as `sort(l, k)`.
json normal(const json& tree) {
	if(tree.is_array()) {
		json items = json::array();
		for(const json& item : tree) items.push_back(normal(item));
		return items;
	}
	if(!tree.is_object()) return tree;
	json result = json::object();
	for(const auto& [key, value] : tree.items()) result[key] = normal(value);
	if(result.size() != 1) return result;
	if(result.begin().key() != "$value" && result.begin().key() != "$get" && readsNothing(result)) {
		try { // what reads nothing, the build may work out: compared as its value
			return literalTree(format::evaluateExpression(result, json::object(), "proof"));
			// NOLINTNEXTLINE(bugprone-empty-catch): it can't be worked out alone, as an item's part
		} catch(const std::exception&) {
		}
	}
	const std::string key = result.begin().key();
	json& value = result.begin().value();
	if(key == "$count" && value.is_array() && value.size() == 1) return {{"$length", value[0]}};
	if(key == "$range" && value.is_number_unsigned() && value.get<std::size_t>() <= 10000) { // worked out at the build
		json items = json::array();
		for(std::size_t at = 0; at < value.get<std::size_t>(); ++at) items.push_back(at);
		return items;
	}
	if(key == "$add" && value.is_array() && std::ranges::all_of(value, [](const json& item) {
		   return item.is_object() && item.contains("$value") && item["$value"].is_string();
	   })) { // text joined at the build
		std::string text;
		for(const json& item : value) text += item["$value"].get<std::string>();
		return {{"$value", text}};
	}
	if(key == "$subtract" && value.is_array() && value.size() == 2 && value[0] == 0 && value[1].is_number())
		return value[1].is_number_float() ? json(-value[1].get<double>()) : json(-value[1].get<std::int64_t>());
	if(key == "$count" && value.is_array() && value.size() == 2) return {{"$length", {{"$filter", value}}}};
	if(key == "$reduce" && value.is_array() && value.size() == 2) value.push_back(nullptr);
	if(key == "$sort" && value.is_array() && value.size() == 3 && value[2] == json{{"$value", "asc"}}) value.erase(2);
	return result;
}

/// @brief Builds a form back to what a program puts in the tree for it.
/// @param form The form's text.
/// @param calls The program's functions it calls, defined as functions that read the app.
/// @param locals The names it reads that the language has: each a repeat's item.
/// @return The value built.
json buildBack(const std::string& form, const std::set<std::string>& calls, const std::set<std::string>& locals) {
	std::string text;
	for(const std::string& name : calls) text += "(defn " + name + " \"\" [& a] state.x)\n";
	// A name the language has is a repeat's item there, as in a program
	std::string node = "`(node :x ~" + form + ")";
	for(const std::string& name : locals) node = "`(node :child ~(repeat [" + name + " state.list] " + node + "))";
	text += "(defscreen main (node :child ~" + node + "))\n";
	lisp::Program program;
	program.addFile(text, "proof.lisp");
	json built = program.build()["root"];
	while(built.contains("child")) built = json(built["child"]);
	return built["x"];
}

/// @brief Tells whether a text built back means what the original does: the same trees, part by part; a form the
/// build worked out must be the value the original gives.
/// @param original The original text.
/// @param built What the form built.
/// @return An empty string if it does, else what differs.
std::string compare(const std::string& original, const json& built) {
	const std::vector<TextPart> want = partsOf(original);
	if(!built.is_string() || !built.get_ref<const std::string&>().contains("{{")) {
		const json value = format::evaluateExpression(original, json::object(), "proof");
		return value == built ? "" : "built to the value " + built.dump() + ", not " + value.dump();
	}
	const std::vector<TextPart> got = partsOf(built.get_ref<const std::string&>());
	if(want.size() == 1 && want[0].expression && got.size() > 1) { // 'a' + x, written as the text a{{x}}
		json joined = json::array();
		for(const TextPart& part : got)
			joined.push_back(part.expression ? format::detail::parse(part.text, "b") : json{{"$value", part.text}});
		return normal(format::detail::parse(want[0].text, "a")) == normal(json{{"$add", joined}})
		           ? ""
				   : "built to " + built.dump();
	}
	if(want.size() != got.size()) return "built to " + built.dump();
	for(std::size_t at = 0; at < want.size(); ++at) {
		if(want[at].expression != got[at].expression) return "built to " + built.dump();
		if(!want[at].expression) {
			if(want[at].text != got[at].text) return "built to " + built.dump();
			continue;
		}
		if(normal(format::detail::parse(want[at].text, "a")) != normal(format::detail::parse(got[at].text, "b")))
			return "built to " + built.dump();
	}
	return "";
}

/// @brief Adds every text with `{{` in a JSON value.
/// @param value The value.
/// @param texts Gets them.
void textsIn(const json& value, std::vector<std::string>& texts) {
	if(value.is_string()) {
		if(value.get_ref<const std::string&>().contains("{{")) texts.push_back(value.get<std::string>());
	} else if(value.is_structured()) {
		for(const json& item : value) textsIn(item, texts);
	}
}

/// @brief Translates and proves every text with `{{` in some JSON files.
/// @param paths The files, or folders to search for `.json` files.
/// @param verbose Whether to list each text that fails.
/// @return Success if every one is proven.
Exit prove(const std::vector<std::string>& paths, bool verbose) {
	std::vector<std::filesystem::path> files;
	for(const std::string& path : paths) {
		if(!std::filesystem::is_directory(path)) {
			files.emplace_back(path);
			continue;
		}
		for(const auto& entry : std::filesystem::recursive_directory_iterator(path)) // a locale's messages aren't
			if(entry.is_regular_file() && entry.path().extension() == ".json" &&
			   entry.path().parent_path().filename() != "locales")
				files.push_back(entry.path());
	}
	std::ranges::sort(files);
	lisp::Interpreter language;
	std::size_t total = 0;
	std::size_t proven = 0;
	std::map<std::string, std::size_t> reasons;
	for(const std::filesystem::path& file : files) {
		std::vector<std::string> texts;
		try {
			textsIn(json::parse(readFile(file)), texts);
		} catch(const std::exception&) {
			continue; // not JSON: nothing to translate
		}
		for(const std::string& text : texts) {
			++total;
			std::string problem;
			try {
				Translator translator(language);
				const std::string form = translate(text, translator);
				problem = compare(text, buildBack(form, translator.programNames(), translator.localNames()));
			} catch(const std::exception& error) {
				problem = error.what();
			}
			if(problem.empty()) {
				++proven;
				continue;
			}
			std::string reason = problem.substr(0, problem.find(':'));
			++reasons[reason.size() > 80 ? reason.substr(0, 80) : reason];
			if(verbose) std::cout << file.generic_string() << ": " << text << "\n    " << problem << '\n';
		}
	}
	std::cout << proven << " of " << total << " texts proven ("
	          << (total ? static_cast<double>(proven * 1000 / total) / 10.0 : 100.0) << "%)\n";
	for(const auto& [reason, count] : reasons) std::cout << "  " << count << "  " << reason << '\n';
	return proven == total ? Exit::Success : Exit::Problems;
}

} // namespace

Exit lispBinding(std::span<const std::string_view> args) {
	const std::array<std::string_view, 0> valued{};
	const std::array<std::string_view, 3> flags = {"--prove", "--verbose", "--batch"};
	const Arguments arguments(args, valued, flags);
	lisp::Interpreter language;
	if(arguments.flag("--batch")) { // a JSON list of texts in, a list of {form} or {error} out, for a tool
		std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
		json answers = json::array();
		for(const json& text : json::parse(input)) {
			try {
				Translator translator(language);
				answers.push_back(
				    {{"form", translate(text.get<std::string>(), translator)}, {"locals", translator.localNames()}});
			} catch(const std::exception& error) {
				answers.push_back({{"error", error.what()}});
			}
		}
		std::cout << answers.dump() << '\n';
		return Exit::Success;
	}
	if(arguments.positional().empty()) throw UsageError("give a text, or --prove and files or folders");
	if(arguments.flag("--prove")) return prove(arguments.positional(), arguments.flag("--verbose"));
	for(const std::string& text : arguments.positional()) {
		try {
			Translator translator(language);
			std::cout << (text.contains("{{") ? "~" + translate(text, translator) : json(text).dump()) << '\n';
		} catch(const std::exception& error) {
			throw FileProblem(text + ": " + error.what());
		}
	}
	return Exit::Success;
}

} // namespace affresco::cli
