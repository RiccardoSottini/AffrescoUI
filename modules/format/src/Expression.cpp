#include "Expression.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace affresco::format {

using expect::fail;

namespace {

/// @brief Reads a whole number written in digits: a literal, or an index in a path (a '-' is an operator).
///
/// Unlike std::stoll, it doesn't throw std::out_of_range, which the callers of evaluate() would let through.
/// @param digits The digits.
/// @return The number, or std::nullopt if it doesn't fit 64 bits.
std::optional<std::int64_t> wholeNumber(std::string_view digits) {
	std::int64_t value = 0;
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
	if(error != std::errc() || end != digits.data() + digits.size()) return std::nullopt;
	return value;
}

/* Variables */

/// @brief Finds the value a path starts from.
/// @param root The variable's name.
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The value; nullptr for "nodes" and "computed", which need the next key (an id, a name).
/// @throws DocumentError If there is no such variable.
const json* variable(const std::string& root, const Context& context, const std::string& where) {
	if(root == "state") return &context.state;
	if(root == "window") return &context.window;
	if(root == "revision") return &context.revision;
	if(root == "event") return &context.event;
	if(root == "computed" && context.computed) return nullptr;
	if(const json* local = context.locals.find(root)) return local;
	if(context.globals.contains(root)) return &context.globals[root];
	if(root == "nodes" && context.nodes) return nullptr;
	fail(where, "unknown variable '" + root +
	                "' (state, event, window, revision, proposals, history, preview, mode, computed, theme, screen, "
	                "nodes, or a name given by repeat, forEach, let, with or a component's params)");
}

/// @brief Follows keys from a variable.
/// @param root The variable's name.
/// @param keys Object keys (strings) and array indices (integers).
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The value, or null if it doesn't exist.
/// @throws DocumentError If the variable is unknown.
json walk(const std::string& root, std::vector<json> keys, const Context& context, const std::string& where) {
	json node;
	const json* current = variable(root, context, where);
	std::size_t first = 0;
	if(!current) { // nodes.<id>: a built panel's geometry; computed.<name>: evaluated now
		if(keys.empty() || !keys.front().is_string()) fail(where, "expected " + root + ".<name>");
		node = (*(root == "computed" ? context.computed : context.nodes))(keys.front().get<std::string>());
		current = &node;
		first = 1;
	}

	for(std::size_t i = first; i < keys.size(); ++i) {
		const json& key = keys[i];
		if(key.is_string() && current->is_object() && current->contains(key.get<std::string>())) {
			current = &(*current)[key.get<std::string>()];
		} else if(key.is_number_integer() && current->is_array() && key.get<std::int64_t>() >= 0 &&
		          static_cast<std::size_t>(key.get<std::int64_t>()) < current->size()) {
			current = &(*current)[static_cast<std::size_t>(key.get<std::int64_t>())];
		} else if(key.is_string() && key.get<std::string>() == "length" && (current->is_array() || current->is_string())) {
			return current->is_array() ? current->size() : current->get<std::string>().size();
		} else {
			return nullptr;
		}
	}
	return *current;
}

/* Text paths (targets of steps, "repeat.in"): "state.items[state.current]" */

/// @brief Splits a path into object keys (strings) and array indices (numbers); `[expr]` indices are evaluated.
/// @param path The path.
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The segments, the variable first.
/// @throws DocumentError If the path is malformed.
std::vector<json> segments(std::string_view path, const Context& context, const std::string& where) {
	std::vector<json> result;
	std::size_t i = 0;

	auto identifier = [&] {
		const std::size_t start = i;
		while(i < path.size() &&
		      (std::isalnum(static_cast<unsigned char>(path[i])) || path[i] == '_' || path[i] == '-'))
			++i;
		if(i == start) fail(where, "invalid path '" + std::string(path) + "'");
		return std::string(path.substr(start, i - start));
	};

	result.emplace_back(identifier());
	while(i < path.size()) {
		if(path[i] == '.') {
			++i;
			result.emplace_back(identifier());
		} else if(path[i] == '[') {
			// [3] or [state.current] or [c + 1]: the inner part can be any expression, with brackets of its own
			int depth = 1;
			const std::size_t start = ++i;
			while(i < path.size() && depth > 0) {
				if(path[i] == '[') ++depth;
				if(path[i] == ']') --depth;
				++i;
			}
			if(depth != 0) fail(where, "unclosed '[' in path '" + std::string(path) + "'");

			const std::string_view inner = path.substr(start, i - start - 1);
			const bool number = !inner.empty() && std::ranges::all_of(inner, [](char c) {
				return std::isdigit(static_cast<unsigned char>(c));
			});
			const std::optional<std::int64_t> index = number ? wholeNumber(inner) : std::nullopt;
			if(number && !index) fail(where, "index '" + std::string(inner) + "' is too large");
			json key = index ? json(*index) : evaluate(compile(inner, where), context, where);
			if(!key.is_number_integer() && !key.is_string())
				fail(where, "index '" + std::string(inner) + "' is neither a number nor a key");
			result.push_back(std::move(key));
		} else {
			fail(where, "invalid path '" + std::string(path) + "'");
		}
	}

	return result;
}

/// @brief Removes surrounding whitespace.
/// @param text The text.
/// @return A view without it.
std::string_view trim(std::string_view text) {
	while(!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
	while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
	return text;
}

/* Expressions: the inside of {{ }} */

/// How an operator takes its arguments.
enum class Takes {
	One,  ///< One value directly: `{"$length": x}`, `len(x)`.
	List, ///< An array of arguments: `{"$max": [a, b]}`, `max(a, b)`.
};

/// Every operator and function (functions are written without the '$' in expressions: `len(x)`, `filter(l, c)`).
constexpr std::pair<std::string_view, Takes> operators[] = {
    {"$if", Takes::List},         {"$and", Takes::List},           {"$or", Takes::List},
    {"$not", Takes::One},         {"$coalesce", Takes::List},      {"$equals", Takes::List},
    {"$notEquals", Takes::List},  {"$greater", Takes::List},       {"$less", Takes::List},
    {"$greaterOrEqual", Takes::List}, {"$lessOrEqual", Takes::List}, {"$add", Takes::List},
    {"$subtract", Takes::List},   {"$multiply", Takes::List},      {"$divide", Takes::List},
    {"$mod", Takes::List},        {"$min", Takes::List},           {"$max", Takes::List},
    {"$clamp", Takes::List},      {"$round", Takes::One},          {"$floor", Takes::One},
    {"$ceil", Takes::One},        {"$abs", Takes::One},            {"$length", Takes::One},
    {"$empty", Takes::One},       {"$concat", Takes::List},        {"$join", Takes::List},
    {"$split", Takes::List},      {"$slice", Takes::List},         {"$contains", Takes::List},
    {"$indexOf", Takes::List},    {"$keys", Takes::One},           {"$values", Takes::One},
    {"$reverse", Takes::One},     {"$range", Takes::One},          {"$upper", Takes::One},
    {"$lower", Takes::One},       {"$trim", Takes::One},           {"$number", Takes::One},
    {"$text", Takes::One},        {"$fixed", Takes::List},         {"$pad", Takes::List},
    {"$plural", Takes::List},     {"$time", Takes::List},          {"$now", Takes::List},
    {"$parseJson", Takes::One},   {"$json", Takes::One},           {"$filter", Takes::List},
    {"$map", Takes::List},        {"$count", Takes::List},         {"$sum", Takes::List},
    {"$find", Takes::List},       {"$some", Takes::List},          {"$every", Takes::List},
    {"$sort", Takes::List},       {"$type", Takes::One},           {"$merge", Takes::List},
    {"$entries", Takes::One}};

/// @brief How an operator takes its arguments.
/// @param name E.g. "$add".
/// @return Its entry in operators, or std::nullopt if there is no such operator.
std::optional<Takes> takes(std::string_view name) {
	const auto it = std::ranges::find(operators, name, &std::pair<std::string_view, Takes>::first);
	return it == std::end(operators) ? std::nullopt : std::optional<Takes>(it->second);
}

/// @brief Makes an operator node.
/// @param name E.g. "$add".
/// @param arguments Its arguments.
/// @return `{name: arguments}`.
json node(std::string_view name, json arguments) {
	json result = json::object();
	result[std::string(name)] = std::move(arguments);
	return result;
}

/// @brief Parses the inside of a `{{ }}` (JS-style) into operator nodes. Literals are `{"$value": x}`, paths
/// `{"$get": [root, key, ...]}`, where a string key is a name and anything else is evaluated.
class Parser {
public:
	/// @brief Prepares to parse.
	/// @param text The expression.
	/// @param where Its place in the document, for errors.
	Parser(std::string_view text, const std::string& where) : text_(text), where_(where) {}

	/// @brief Parses the whole text.
	/// @return The operator tree.
	/// @throws DocumentError If it isn't a valid expression.
	json parse() {
		json result = ternary();
		skip();
		if(position_ < text_.size()) error("unexpected '" + std::string(text_.substr(position_, 1)) + "'");
		return result;
	}

private:
	/// @brief Reports a syntax error with its column.
	/// @param message What is wrong.
	[[noreturn]] void error(const std::string& message) const {
		const std::string shown = text_.size() > 80 ? std::string(text_.substr(0, 77)) + "..." : std::string(text_);
		fail(where_, "in {{" + shown + "}}: " + message + " (at column " + std::to_string(position_ + 1) + ")");
	}

	/// @brief Skips whitespace.
	void skip() {
		while(position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) ++position_;
	}

	/// @brief Consumes a token if it comes next.
	/// @param token The token.
	/// @return true if it was there.
	bool accept(std::string_view token) {
		skip();
		if(!text_.substr(position_).starts_with(token)) return false;
		position_ += token.size();
		return true;
	}

	/// @brief Consumes a token that must come next.
	/// @param token The token.
	void expect(std::string_view token) {
		if(!accept(token)) error("expected '" + std::string(token) + "'");
	}

	/// @brief Joins a chain of the same operator into one node: `a && b && c` is one $and.
	/// @param name The operator.
	/// @param left The left side (maybe already that operator).
	/// @param right The right side.
	/// @return The node.
	static json chain(std::string_view name, json left, json right) {
		if(left.is_object() && left.size() == 1 && left.contains(std::string(name)) &&
		   left[std::string(name)].is_array()) {
			left[std::string(name)].push_back(std::move(right));
			return left;
		}
		return node(name, json::array({std::move(left), std::move(right)}));
	}

	/// @brief Counts how deeply the rules being parsed nest, so `((((...))))` fails instead of overflowing the stack.
	class Nesting {
	public:
		/// @brief Enters one level.
		/// @param parser The parser.
		explicit Nesting(Parser& parser) : parser_(parser) {
			if(++parser_.depth_ > maxDepth) parser_.error("nested too deeply");
		}
		/// @brief Leaves it.
		~Nesting() { --parser_.depth_; }
		Nesting(const Nesting&) = delete;
		Nesting& operator=(const Nesting&) = delete;

	private:
		Parser& parser_; ///< The parser.
	};

	/// @brief condition ? then : else (right-associative, lowest precedence).
	/// @return The node.
	json ternary() {
		const Nesting nesting(*this); // every ( [ { and argument passes here
		json condition = coalesce();
		if(!accept("?")) return condition;
		json then = ternary();
		expect(":");
		json otherwise = ternary();
		return node("$if", json::array({std::move(condition), std::move(then), std::move(otherwise)}));
	}

	/// @brief a ?? b: the first that isn't null.
	/// @return The node.
	json coalesce() {
		json left = disjunction();
		while(accept("??")) left = chain("$coalesce", std::move(left), disjunction());
		return left;
	}

	/// @brief a || b.
	/// @return The node.
	json disjunction() {
		json left = conjunction();
		while(accept("||")) left = chain("$or", std::move(left), conjunction());
		return left;
	}

	/// @brief a && b.
	/// @return The node.
	json conjunction() {
		json left = equality();
		while(accept("&&")) left = chain("$and", std::move(left), equality());
		return left;
	}

	/// @brief a == b, a != b.
	/// @return The node.
	json equality() {
		json left = comparison();
		for(;;) {
			if(accept("==")) left = node("$equals", json::array({std::move(left), comparison()}));
			else if(accept("!=")) left = node("$notEquals", json::array({std::move(left), comparison()}));
			else return left;
		}
	}

	/// @brief a < b, a <= b, a > b, a >= b.
	/// @return The node.
	json comparison() {
		json left = additive();
		for(;;) {
			const char* name = accept("<=")   ? "$lessOrEqual"
			                   : accept(">=") ? "$greaterOrEqual"
			                   : accept("<")  ? "$less"
			                   : accept(">")  ? "$greater"
			                                  : nullptr;
			if(!name) return left;
			left = node(name, json::array({std::move(left), additive()}));
		}
	}

	/// @brief a + b (numbers add, text joins), a - b.
	/// @return The node.
	json additive() {
		json left = multiplicative();
		for(;;) {
			if(accept("+")) left = chain("$add", std::move(left), multiplicative());
			else if(accept("-")) left = node("$subtract", json::array({std::move(left), multiplicative()}));
			else return left;
		}
	}

	/// @brief a * b, a / b, a % b.
	/// @return The node.
	json multiplicative() {
		json left = unary();
		for(;;) {
			if(accept("*")) left = chain("$multiply", std::move(left), unary());
			else if(accept("/")) left = node("$divide", json::array({std::move(left), unary()}));
			else if(accept("%")) left = node("$mod", json::array({std::move(left), unary()}));
			else return left;
		}
	}

	/// @brief !a, -a.
	/// @return The node.
	json unary() {
		const Nesting nesting(*this); // !!!!x
		if(accept("!")) return node("$not", unary());
		if(accept("-")) return node("$subtract", json::array({0, unary()}));
		return postfix();
	}

	/// @brief a.key, a[index].
	/// @return The node.
	json postfix() {
		json value = primary();
		for(;;) {
			json key;
			if(accept(".")) {
				key = name(true);
			} else if(accept("[")) {
				key = ternary();
				expect("]");
				if(key.is_object() && key.contains("$value") && key["$value"].is_string()) key = key["$value"];
			} else {
				return value;
			}
			if(value.is_object() && value.size() == 1 && value.contains("$get")) value["$get"].push_back(std::move(key));
			else value = node("$get", json::array({std::move(value), std::move(key)}));
		}
	}

	/// @brief Reads a name.
	/// @param keyName Whether it follows a '.', where it may start with a digit.
	/// @return The name.
	std::string name(bool keyName) {
		skip();
		const std::size_t start = position_;
		while(position_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[position_])) ||
		                                   text_[position_] == '_' || text_[position_] == '$'))
			++position_;
		if(position_ == start || (!keyName && std::isdigit(static_cast<unsigned char>(text_[start]))))
			error("expected a name");
		return std::string(text_.substr(start, position_ - start));
	}

	/// @brief Literals, ( ), [ ], @colors, function calls and variables.
	/// @return The node.
	json primary() {
		skip();
		if(position_ >= text_.size()) error("expected a value");
		const char c = text_[position_];

		if(std::isdigit(static_cast<unsigned char>(c))) {
			const std::size_t start = position_;
			while(position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
			bool fraction = false;
			if(position_ + 1 < text_.size() && text_[position_] == '.' &&
			   std::isdigit(static_cast<unsigned char>(text_[position_ + 1]))) {
				fraction = true;
				++position_;
				while(position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_])))
					++position_;
			}
			const std::string digits(text_.substr(start, position_ - start));
			if(!fraction) {
				if(const std::optional<std::int64_t> value = wholeNumber(digits)) return *value;
			} else if(const double value = std::strtod(digits.c_str(), nullptr); std::isfinite(value)) {
				return value; // std::stod would throw out_of_range, past evaluate()'s callers
			}
			error("number '" + digits + "' is too large");
		}
		if(c == '\'' || c == '"') return node("$value", quoted(c));
		if(c == '@') { // a colour name, e.g. @accent
			const std::size_t start = position_++;
			while(position_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[position_])) ||
			                                   text_[position_] == '_' || text_[position_] == '-'))
				++position_;
			return node("$value", std::string(text_.substr(start, position_ - start)));
		}
		if(accept("(")) {
			json inner = ternary();
			expect(")");
			return inner;
		}
		if(accept("[")) {
			json items = json::array();
			if(!accept("]")) {
				do items.push_back(ternary());
				while(accept(","));
				expect("]");
			}
			return items; // evaluated item by item
		}
		if(accept("{")) { // { name: value, 'any key': value }
			json fields = json::object();
			if(!accept("}")) {
				do {
					skip();
					const std::string key = position_ < text_.size() && (text_[position_] == '\'' || text_[position_] == '"')
					                            ? quoted(text_[position_])
					                            : name(true);
					expect(":");
					fields[key] = ternary();
				} while(accept(","));
				expect("}");
			}
			return node("$object", std::move(fields));
		}

		const std::string identifier = name(false);
		if(identifier == "true") return true;
		if(identifier == "false") return false;
		if(identifier == "null") return nullptr;
		if(accept("(")) return call(identifier);
		return node("$get", json::array({identifier}));
	}

	/// @brief Reads a quoted string, with \\ escapes.
	/// @param quote ' or ".
	/// @return Its text.
	std::string quoted(char quote) {
		std::string result;
		++position_;
		while(position_ < text_.size() && text_[position_] != quote) {
			char c = text_[position_++];
			if(c == '\\' && position_ < text_.size()) {
				c = text_[position_++];
				if(c == 'n') c = '\n';
				else if(c == 't') c = '\t';
			}
			result += c;
		}
		if(position_ >= text_.size()) error("unclosed string");
		++position_;
		return result;
	}

	/// @brief A function call, after its '(': `len(x)` is `{"$length": x}`, `max(a, b)` is `{"$max": [a, b]}`.
	/// @param function Its name.
	/// @return The node.
	json call(const std::string& function) {
		json arguments = json::array();
		if(!accept(")")) {
			do arguments.push_back(ternary());
			while(accept(","));
			expect(")");
		}
		const std::string name = "$" + (function == "len" ? std::string("length") : function);
		const std::optional<Takes> kind = takes(name);
		if(!kind) error("unknown function '" + function + "'");
		if(*kind == Takes::One) {
			if(arguments.size() != 1) error("'" + function + "' takes one argument");
			return node(name, std::move(arguments[0]));
		}
		return node(name, std::move(arguments));
	}

	static constexpr int maxDepth = 100; ///< Deeper nesting is refused (real expressions use a handful of levels).

	std::string_view text_;    ///< The expression.
	const std::string& where_; ///< Its place, for errors.
	std::size_t position_ = 0; ///< Where parsing is.
	int depth_ = 0;            ///< Nesting now (see Nesting).
};

/* Operators */

/// @brief Reads an operator's number argument.
/// @param value The argument.
/// @param where Its place, for errors.
/// @return The number.
/// @throws DocumentError If it is not a number.
double numberOf(const json& value, const std::string& where) {
	if(!value.is_number()) fail(where, "expected a number, got " + value.dump());
	return value.get<double>();
}

/// @brief Makes an operator's numeric result; integer in, integer out, so indices and counters stay integers.
/// @param value The result.
/// @param integral Whether every argument was an integer.
/// @return An integer when possible, else a double.
json numeric(double value, bool integral) {
	if(integral && std::trunc(value) == value) return static_cast<std::int64_t>(value);
	return value;
}

/// @brief Tells whether every value is an integer.
/// @param values An array.
/// @return true if all are.
bool allIntegers(const json& values) {
	return std::ranges::all_of(values, [](const json& v) { return v.is_number_integer(); });
}

/// @brief Evaluates an operator's arguments: a literal array, e.g. `{"$add": [1, "{{state.n}}"]}`.
/// @param raw The array.
/// @param context What paths can see.
/// @param where Its place, for errors.
/// @return The evaluated arguments.
/// @throws DocumentError If @p raw is not an array or an argument is invalid.
json arguments(const json& raw, const Context& context, const std::string& where) {
	if(!raw.is_array()) fail(where, "expected an array of arguments");
	json values = json::array();
	for(std::size_t i = 0; i < raw.size(); ++i)
		values.push_back(evaluate(raw[i], context, where + "[" + std::to_string(i) + "]"));
	return values;
}

/// @brief Orders two values: numbers by size, text alphabetically, anything else by its JSON text.
/// @param a A value.
/// @param b Another.
/// @return Negative, zero or positive.
int order(const json& a, const json& b) {
	if(a.is_number() && b.is_number()) {
		const double x = a.get<double>(), y = b.get<double>();
		return x < y ? -1 : x > y ? 1 : 0;
	}
	const std::string x = a.is_string() ? a.get<std::string>() : a.dump();
	const std::string y = b.is_string() ? b.get<std::string>() : b.dump();
	return x.compare(y);
}

/// @brief Formats a time like "HH:mm" or "ddd D MMM": YYYY, MM, MMM, DD, D, ddd, HH, mm, ss.
/// @param milliseconds Since 1970, UTC.
/// @param pattern The format.
/// @return The local time as text.
std::string formatTime(double milliseconds, std::string_view pattern) {
	const auto seconds = static_cast<std::time_t>(milliseconds / 1000.0);
	std::tm local{};
	localtime_r(&seconds, &local);

	static constexpr std::pair<std::string_view, const char*> fields[] = {
	    {"YYYY", "%Y"}, {"MMM", "%b"}, {"MM", "%m"}, {"ddd", "%a"}, {"DD", "%d"},
	    {"D", "%e"},    {"HH", "%H"},  {"mm", "%M"}, {"ss", "%S"}};
	std::string result;
	for(std::size_t i = 0; i < pattern.size();) {
		bool matched = false;
		for(const auto& [token, code] : fields) {
			if(!pattern.substr(i).starts_with(token)) continue;
			char buffer[32];
			std::strftime(buffer, sizeof(buffer), code, &local);
			result += std::string(trim(buffer));
			i += token.size();
			matched = true;
			break;
		}
		if(!matched) result += pattern[i++];
	}
	return result;
}

/// @brief Evaluates a function of a list and an expression run once per item (`it`, `index`): filter, map, ...
/// @param name The operator.
/// @param raw Its arguments, unevaluated: [list, expression, ...].
/// @param context What paths can see.
/// @param at Its place, for errors.
/// @return The result.
/// @throws DocumentError If the arguments are invalid.
json applyToItems(const std::string& name, const json& raw, const Context& context, const std::string& at) {
	if(!raw.is_array() || raw.empty()) fail(at, "expected [list, expression]");
	const json list = evaluate(raw[0], context, at);
	if(list.is_null()) return name == "$count" || name == "$sum" ? json(0) : name == "$find" ? json() : json::array();
	if(!list.is_array()) fail(at, "expected a list, got " + list.dump());

	// The expression sees the item as "it" and its position as "index"
	auto each = [&](std::size_t i) {
		if(raw.size() < 2) return list[i];
		const Scope scope{&context.locals, nullptr, {{"it", list[i]}, {"index", i}}};
		const Context inner{context.state,   context.window, context.revision, context.event,
		                    scope,           context.globals, context.nodes,   context.computed};
		return evaluate(raw[1], inner, at);
	};

	if(name == "$filter" || name == "$map") {
		json result = json::array();
		for(std::size_t i = 0; i < list.size(); ++i) {
			json value = each(i);
			if(name == "$map") result.push_back(std::move(value));
			else if(truthy(value)) result.push_back(list[i]);
		}
		return result;
	}
	if(name == "$count") {
		if(raw.size() < 2) return list.size();
		std::size_t n = 0;
		for(std::size_t i = 0; i < list.size(); ++i)
			if(truthy(each(i))) ++n;
		return n;
	}
	if(name == "$sum") {
		double total = 0.0;
		bool integral = true;
		for(std::size_t i = 0; i < list.size(); ++i) {
			const json value = each(i);
			total += numberOf(value, at);
			integral = integral && value.is_number_integer();
		}
		return numeric(total, integral);
	}
	if(name == "$find") {
		for(std::size_t i = 0; i < list.size(); ++i)
			if(truthy(each(i))) return list[i];
		return nullptr;
	}
	if(name == "$some" || name == "$every") {
		const bool some = name == "$some";
		for(std::size_t i = 0; i < list.size(); ++i)
			if(truthy(each(i)) == some) return some;
		return !some;
	}

	// $sort: [list, key?, "desc"?], stable
	std::vector<std::pair<json, std::size_t>> keyed;
	for(std::size_t i = 0; i < list.size(); ++i) keyed.emplace_back(each(i), i);
	const bool descending = raw.size() > 2 && toText(evaluate(raw[2], context, at)) == "desc";
	std::ranges::stable_sort(keyed, [&](const auto& a, const auto& b) {
		const int c = order(a.first, b.first);
		return descending ? c > 0 : c < 0;
	});
	json result = json::array();
	for(const auto& entry : keyed) result.push_back(list[entry.second]);
	return result;
}

/// @brief Evaluates an operator that takes one value: `{"$length": x}`, `len(x)`.
/// @param name The operator, e.g. "$length".
/// @param value Its value, evaluated.
/// @param at Its place, for errors.
/// @return The result.
/// @throws DocumentError If the value doesn't suit the operator.
json applyToValue(const std::string& name, const json& value, const std::string& at) {
	if(name == "$not") return !truthy(value);
	if(name == "$empty") return !truthy(value) && !value.is_boolean() && !value.is_number();
	if(name == "$round") return static_cast<std::int64_t>(std::llround(numberOf(value, at)));
	if(name == "$floor") return static_cast<std::int64_t>(std::floor(numberOf(value, at)));
	if(name == "$ceil") return static_cast<std::int64_t>(std::ceil(numberOf(value, at)));
	if(name == "$abs") return numeric(std::abs(numberOf(value, at)), value.is_number_integer());
	if(name == "$text") return toText(value);
	if(name == "$type")
		return value.is_null()      ? "null"
		       : value.is_boolean() ? "boolean"
		       : value.is_number()  ? "number"
		       : value.is_string()  ? "string"
		       : value.is_array()   ? "list"
		                            : "object";
	if(name == "$entries") { // {a: 1} -> [{key: "a", value: 1}], to repeat over an object
		json result = json::array();
		if(value.is_object())
			for(const auto& [key, item] : value.items()) result.push_back({{"key", key}, {"value", item}});
		return result;
	}
	if(name == "$json") return value.dump();
	if(name == "$parseJson") {
		try {
			return parseUntrusted(toText(value), true);
		} catch(const std::exception&) {
			return nullptr; // not JSON (or nested too deeply)
		}
	}
	if(name == "$number") {
		if(value.is_number()) return value;
		try {
			std::size_t used = 0;
			const std::string text = toText(value);
			const double n = std::stod(text, &used);
			return used == text.size() ? numeric(n, text.find_first_of(".eE") == std::string::npos) : json();
		} catch(const std::exception&) {
			return nullptr;
		}
	}
	if(name == "$upper" || name == "$lower" || name == "$trim") {
		std::string text = toText(value);
		if(name == "$trim") return std::string(trim(text));
		for(char& c : text)
			c = static_cast<char>(name == "$upper" ? std::toupper(static_cast<unsigned char>(c))
			                                       : std::tolower(static_cast<unsigned char>(c)));
		return text;
	}
	if(name == "$keys" || name == "$values") {
		json result = json::array();
		if(value.is_object())
			for(const auto& [key, item] : value.items()) result.push_back(name == "$keys" ? json(key) : item);
		return result;
	}
	if(name == "$reverse") {
		if(value.is_string()) {
			const std::string& text = value.get_ref<const std::string&>();
			return std::string(text.rbegin(), text.rend());
		}
		if(!value.is_array()) fail(at, "expected a list");
		json result = json::array();
		for(auto it = value.rbegin(); it != value.rend(); ++it) result.push_back(*it);
		return result;
	}
	if(name == "$range") {
		const auto n = static_cast<std::int64_t>(numberOf(value, at));
		if(n > 10000) fail(at, "range too long");
		json result = json::array();
		for(std::int64_t i = 0; i < n; ++i) result.push_back(i);
		return result;
	}
	// $length
	if(value.is_string()) return value.get<std::string>().size();
	if(value.is_array() || value.is_object()) return value.size();
	if(value.is_null()) return 0;
	fail(at, "expected a string, a list or an object");
}

/// @brief An operator's evaluated arguments, with the checks every operator needs.
struct Arguments {
	json values;           ///< The arguments, an array.
	const std::string& at; ///< The operator's place, for errors.

	/// @brief The i-th argument.
	/// @param i Its position.
	/// @return It.
	const json& operator[](std::size_t i) const { return values[i]; }
	/// @brief How many there are.
	/// @return The count.
	[[nodiscard]] std::size_t size() const { return values.size(); }
	/// @brief Requires exactly n arguments.
	/// @param n The count.
	/// @throws DocumentError If there are more or fewer.
	void count(std::size_t n) const {
		if(size() != n) fail(at, "expected " + std::to_string(n) + " arguments");
	}
	/// @brief Requires low to high arguments.
	/// @param low The fewest.
	/// @param high The most.
	/// @throws DocumentError If there are more or fewer.
	void between(std::size_t low, std::size_t high) const {
		if(size() < low || size() > high)
			fail(at, "expected " + std::to_string(low) + " to " + std::to_string(high) + " arguments");
	}
	/// @brief The i-th argument as a number.
	/// @param i Its position.
	/// @return The number.
	/// @throws DocumentError If it is not one.
	[[nodiscard]] double number(std::size_t i) const { return numberOf(values[i], at); }
};

/* Operator groups, tried in order by applyToArguments(): each returns std::nullopt for an operator that isn't one
   of its own. */

/// @brief ==, !=, <, >, <=, >=: numbers by size, text alphabetically.
/// @param name The operator.
/// @param args Its arguments.
/// @return The result, or std::nullopt if it isn't a comparison.
std::optional<json> comparison(const std::string& name, const Arguments& args) {
	auto compare = [&](auto test) {
		args.count(2);
		if(args[0].is_string() && args[1].is_string())
			return test(args[0].get<std::string>().compare(args[1].get<std::string>()));
		const double a = args.number(0), b = args.number(1);
		return test(a < b ? -1 : a > b ? 1 : 0);
	};

	if(name == "$equals") return args.count(2), args[0] == args[1];
	if(name == "$notEquals") return args.count(2), args[0] != args[1];
	if(name == "$greater") return compare([](int c) { return c > 0; });
	if(name == "$less") return compare([](int c) { return c < 0; });
	if(name == "$greaterOrEqual") return compare([](int c) { return c >= 0; });
	if(name == "$lessOrEqual") return compare([](int c) { return c <= 0; });
	return std::nullopt;
}

/// @brief +, *, min, max, -, /, %, clamp; integers in, integers out. A + with text joins it.
/// @param name The operator.
/// @param args Its arguments.
/// @return The result, or std::nullopt if it isn't arithmetic.
std::optional<json> arithmetic(const std::string& name, const Arguments& args) {
	const std::string& at = args.at;

	// Text joins: "{{n + ' cards'}}"
	if(name == "$add" && std::ranges::any_of(args.values, [](const json& v) { return v.is_string(); })) {
		std::string result;
		for(const json& part : args.values) result += toText(part);
		return result;
	}
	if(name == "$add" || name == "$multiply" || name == "$min" || name == "$max") {
		if(args.size() == 0) fail(at, "expected at least one argument");
		double result = args.number(0);
		for(std::size_t i = 1; i < args.size(); ++i) {
			const double v = args.number(i);
			if(name == "$add") result += v;
			else if(name == "$multiply") result *= v;
			else if(name == "$min") result = std::min(result, v);
			else result = std::max(result, v);
		}
		return numeric(result, allIntegers(args.values));
	}
	if(name == "$subtract") return args.count(2), numeric(args.number(0) - args.number(1), allIntegers(args.values));
	if(name == "$divide" || name == "$mod") {
		args.count(2);
		const double divisor = args.number(1);
		if(divisor == 0.0) fail(at, "division by zero");
		if(name == "$mod") return numeric(std::fmod(args.number(0), divisor), allIntegers(args.values));
		return args.number(0) / divisor;
	}
	if(name == "$clamp") {
		args.count(3);
		const double low = args.number(1), high = args.number(2);
		return numeric(std::clamp(args.number(0), low, std::max(low, high)), allIntegers(args.values));
	}
	return std::nullopt;
}

/// @brief concat, join, split, slice, contains, indexOf, merge: lists, text and objects.
/// @param name The operator.
/// @param args Its arguments.
/// @return The result, or std::nullopt if it isn't one of these.
std::optional<json> listOrText(const std::string& name, const Arguments& args) {
	const std::string& at = args.at;

	if(name == "$concat") {
		if(std::ranges::all_of(args.values, [](const json& v) { return v.is_array(); })) {
			json result = json::array();
			for(const json& part : args.values) result.insert(result.end(), part.begin(), part.end());
			return result;
		}
		std::string result;
		for(const json& part : args.values) result += toText(part);
		return result;
	}
	// {"$join": [list, separator]}: the items as text, e.g. "#board, /state/cards"
	if(name == "$join") {
		args.count(2);
		if(!args[0].is_array()) fail(at, "expected a list first");
		std::string result;
		for(const json& item : args[0]) result += (result.empty() ? "" : toText(args[1])) + toText(item);
		return result;
	}
	if(name == "$split") {
		args.count(2);
		const std::string text = toText(args[0]), separator = toText(args[1]);
		json result = json::array();
		if(separator.empty()) {
			for(char c : text) result.push_back(std::string(1, c));
			return result;
		}
		std::size_t start = 0;
		for(std::size_t end; (end = text.find(separator, start)) != std::string::npos; start = end + separator.size())
			result.push_back(text.substr(start, end - start));
		result.push_back(text.substr(start));
		return result;
	}
	if(name == "$slice") {
		args.between(2, 3);
		const auto size = static_cast<std::int64_t>(args[0].is_string() ? args[0].get<std::string>().size()
		                                            : args[0].is_array() ? args[0].size()
		                                                                 : 0);
		auto clampIndex = [&](std::size_t arg) {
			auto i = static_cast<std::int64_t>(args.number(arg));
			if(i < 0) i += size;
			return std::clamp<std::int64_t>(i, 0, size);
		};
		const std::int64_t from = clampIndex(1), to = args.size() > 2 ? clampIndex(2) : size;
		if(args[0].is_string())
			return to > from ? args[0].get<std::string>().substr(static_cast<std::size_t>(from),
			                                                     static_cast<std::size_t>(to - from))
			                 : std::string();
		if(!args[0].is_array()) fail(at, "expected a list or a text first");
		json result = json::array();
		for(std::int64_t i = from; i < to; ++i) result.push_back(args[0][static_cast<std::size_t>(i)]);
		return result;
	}
	if(name == "$contains" || name == "$indexOf") {
		args.count(2);
		std::int64_t found = -1;
		if(args[0].is_string()) {
			const std::size_t i = args[0].get<std::string>().find(toText(args[1]));
			found = i == std::string::npos ? -1 : static_cast<std::int64_t>(i);
		} else if(args[0].is_array()) {
			const auto it = std::ranges::find(args[0], args[1]);
			found = it == args[0].end() ? -1 : static_cast<std::int64_t>(std::distance(args[0].begin(), it));
		} else if(args[0].is_object()) {
			found = args[0].contains(toText(args[1])) ? 0 : -1;
		}
		return name == "$contains" ? json(found >= 0) : json(found);
	}
	if(name == "$merge") { // objects, later keys win
		json result = json::object();
		for(const json& part : args.values) {
			if(part.is_null()) continue;
			if(!part.is_object()) fail(at, "expected objects");
			result.update(part);
		}
		return result;
	}
	return std::nullopt;
}

/// @brief fixed, pad, plural, now, time: values as text, and the clock.
/// @param name The operator.
/// @param args Its arguments.
/// @return The result, or std::nullopt if it isn't one of these.
std::optional<json> formatting(const std::string& name, const Arguments& args) {
	if(name == "$fixed") {
		args.count(2);
		const int digits = std::clamp(static_cast<int>(args.number(1)), 0, 12);
		char buffer[64];
		std::snprintf(buffer, sizeof(buffer), "%.*f", digits, args.number(0));
		return std::string(buffer);
	}
	if(name == "$pad") {
		args.between(2, 3);
		std::string text = toText(args[0]);
		const auto width = static_cast<std::size_t>(std::max(args.number(1), 0.0));
		if(width > 10000) fail(args.at, "width too large");
		const std::string fill = args.size() > 2 ? toText(args[2]) : "0";
		while(text.size() < width && !fill.empty()) text = fill + text;
		return text;
	}
	if(name == "$plural") {
		args.between(2, 3);
		const double n = args.number(0);
		const std::string word = n == 1.0 ? toText(args[1]) : args.size() > 2 ? toText(args[2]) : toText(args[1]) + "s";
		return toText(args[0]) + " " + word;
	}
	if(name == "$now") {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
		           std::chrono::system_clock::now().time_since_epoch())
		    .count();
	}
	if(name == "$time") {
		args.count(2);
		return formatTime(args.number(0), toText(args[1]));
	}
	return std::nullopt;
}

/// @brief Evaluates an operator that takes a list of arguments: `{"$max": [a, b]}`, `max(a, b)`.
/// @param name The operator, e.g. "$max".
/// @param values Its arguments, evaluated.
/// @param at Its place, for errors.
/// @return The result.
/// @throws DocumentError If the operator is unknown or its arguments don't suit it.
json applyToArguments(const std::string& name, json values, const std::string& at) {
	if((name == "$min" || name == "$max") && values.size() == 1 && values[0].is_array()) {
		json items = std::move(values[0]); // max(list): its items
		values = std::move(items);
	}
	const Arguments args{std::move(values), at};
	for(auto group : {comparison, arithmetic, listOrText, formatting})
		if(std::optional<json> result = group(name, args)) return std::move(*result);
	fail(at, "unknown operator '" + name + "'");
}

/// @brief Evaluates one `$` operator.
/// @param name The operator, e.g. "$add".
/// @param raw Its arguments, unevaluated.
/// @param context What paths can see.
/// @param where Its place, for errors.
/// @return The result.
/// @throws DocumentError If the operator is unknown or its arguments are invalid.
json applyOperator(const std::string& name, const json& raw, const Context& context, const std::string& where) {
	const std::string at = where + "." + name;

	// Compiled expressions: literals and paths
	if(name == "$value") return raw;
	if(name == "$object") {
		json result = json::object();
		for(const auto& [key, value] : raw.items()) result[key] = evaluate(value, context, at + "." + key);
		return result;
	}
	if(name == "$get") {
		if(!raw.is_array() || raw.empty()) fail(at, "expected [variable, key, ...]");
		std::vector<json> keys;
		for(std::size_t i = 1; i < raw.size(); ++i)
			keys.push_back(raw[i].is_string() || raw[i].is_number_integer() ? raw[i] : evaluate(raw[i], context, at));
		if(raw[0].is_string()) return walk(raw[0].get<std::string>(), std::move(keys), context, where);

		// A path from a computed value, e.g. (a ?? b).name
		const Scope scope{&context.locals, nullptr, {{"$base", evaluate(raw[0], context, at)}}};
		const Context inner{context.state,   context.window, context.revision, context.event,
		                    scope,           context.globals, context.nodes,   context.computed};
		return walk("$base", std::move(keys), inner, where);
	}
	if(name == "$path") return lookup(raw.get<std::string>(), context, where); // a path from before expressions

	// Lazy: only what is needed is evaluated
	if(name == "$if") {
		if(!raw.is_array() || raw.size() != 3) fail(at, "expected [condition, then, else]");
		return truthy(evaluate(raw[0], context, at)) ? evaluate(raw[1], context, at) : evaluate(raw[2], context, at);
	}
	if(name == "$and" || name == "$or") {
		if(!raw.is_array()) fail(at, "expected an array of conditions");
		const bool isAnd = name == "$and";
		json last = isAnd;
		for(const json& item : raw) {
			last = evaluate(item, context, at);
			if(truthy(last) != isAnd) return !isAnd;
		}
		return isAnd;
	}
	if(name == "$coalesce") {
		if(!raw.is_array()) fail(at, "expected an array of values");
		for(const json& item : raw)
			if(json value = evaluate(item, context, at); !value.is_null()) return value;
		return nullptr;
	}
	if(name == "$filter" || name == "$map" || name == "$count" || name == "$sum" || name == "$find" ||
	   name == "$some" || name == "$every" || name == "$sort")
		return applyToItems(name, raw, context, at);

	// One-argument operators take their value directly: {"$length": "{{state.items}}"}, {"$length": [1, 2, 3]}
	if(takes(name) == Takes::One) return applyToValue(name, evaluate(raw, context, at), at);
	return applyToArguments(name, arguments(raw, context, at), at);
}

/* Templates */

/// @brief Finds the `}}` that closes a placeholder, skipping quoted text and object literals inside it.
/// @param text The string.
/// @param from Just after the `{{`.
/// @return Its position, or npos.
std::size_t closing(std::string_view text, std::size_t from) {
	char quote = 0;
	int braces = 0;
	for(std::size_t i = from; i + 1 < text.size(); ++i) {
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
			return i;
		}
	}
	return std::string_view::npos;
}

/// @brief Evaluates a string with `{{expression}}` placeholders. A lone placeholder keeps the value's own type.
/// @param text The string.
/// @param context What paths can see.
/// @param where Its place, for errors.
/// @return The value, or the interpolated string.
/// @throws DocumentError If a placeholder is unclosed or its expression invalid.
json evaluateString(const std::string& text, const Context& context, const std::string& where) {
	const std::size_t open = text.find("{{");
	if(open == std::string::npos) return text;

	// Exactly one placeholder and nothing else: keep the value's own type
	const std::string_view whole = trim(text);
	if(whole.starts_with("{{") && closing(whole, 2) == whole.size() - 2)
		return evaluate(compile(whole.substr(2, whole.size() - 4), where), context, where);

	std::string result;
	std::size_t position = 0;
	for(std::size_t start; (start = text.find("{{", position)) != std::string::npos;) {
		const std::size_t end = closing(text, start + 2);
		if(end == std::string::npos) fail(where, "unclosed '{{' in \"" + text + "\"");
		result += text.substr(position, start - position);
		result += toText(
		    evaluate(compile(std::string_view(text).substr(start + 2, end - start - 2), where), context, where));
		position = end + 2;
	}
	return result + text.substr(position);
}

} // namespace

const json& compile(std::string_view text, const std::string& where) {
	static std::unordered_map<std::string, json> cache; // the main thread's; expressions repeat on every update
	const std::string key(trim(text));
	if(const auto it = cache.find(key); it != cache.end()) return it->second;
	if(cache.size() > 20000) cache.clear();

	json compiled;
	try {
		compiled = Parser(key, where).parse();
	} catch(const DocumentError&) {
		// Text that isn't an expression but looks like a path is read as one (paths written before expressions). Few
		// names with '-' get here: "state.my-list" parses, as state.my - list, so such names go in brackets:
		// state['my-list'] (SCREEN_FORMAT.md, "Expressions")
		const bool path = !key.empty() && std::ranges::all_of(key, [](char c) {
			return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == '[' ||
			       c == ']';
		});
		if(!path) throw;
		compiled = node("$path", key);
	}
	return cache.emplace(key, std::move(compiled)).first->second;
}

json evaluate(const json& value, const Context& context, const std::string& where) {
	if(value.is_string()) return evaluateString(value.get_ref<const std::string&>(), context, where);

	if(value.is_object()) {
		if(value.size() == 1 && value.begin().key().starts_with('$'))
			return applyOperator(value.begin().key(), value.begin().value(), context, where);

		json result = json::object();
		for(const auto& [key, item] : value.items()) result[key] = evaluate(item, context, where + "." + key);
		return result;
	}

	if(value.is_array()) {
		json result = json::array();
		for(std::size_t i = 0; i < value.size(); ++i)
			result.push_back(evaluate(value[i], context, where + "[" + std::to_string(i) + "]"));
		return result;
	}

	return value;
}

json lookup(std::string_view path, const Context& context, const std::string& where) {
	std::vector<json> keys = segments(path, context, where);
	const std::string root = keys.front().get<std::string>();
	keys.erase(keys.begin());
	return walk(root, std::move(keys), context, where);
}

json::json_pointer statePointer(std::string_view path, const Context& context, const std::string& where) {
	const std::vector<json> keys = segments(path, context, where);
	if(keys.front() != "state") fail(where, "can only change values under 'state', not '" + std::string(path) + "'");

	json::json_pointer pointer;
	for(std::size_t i = 1; i < keys.size(); ++i) {
		if(keys[i].is_string()) pointer /= keys[i].get<std::string>();
		else pointer /= static_cast<std::size_t>(keys[i].get<std::int64_t>());
	}
	return pointer;
}

bool truthy(const json& value) noexcept {
	if(value.is_boolean()) return value.get<bool>();
	if(value.is_null()) return false;
	if(value.is_number()) return value.get<double>() != 0.0;
	if(value.is_string()) return !value.get_ref<const std::string&>().empty();
	return !value.empty();
}

bool isDynamic(const json& value) noexcept {
	if(value.is_string()) return value.get_ref<const std::string&>().find("{{") != std::string::npos;
	return value.is_object() && value.size() == 1 && value.begin().key().starts_with('$');
}

json operatorNames() {
	json names = json::array();
	for(const auto& [name, kind] : operators) names.push_back(std::string(name));
	return names;
}

} // namespace affresco::format
