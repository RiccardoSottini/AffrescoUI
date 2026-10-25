/// @file
/// @brief The expression parser: precedence climbing over the text, building the operator tree.

#include "ExpressionParser.hpp"

#include "ExpressionFunctions.hpp"
#include "affresco/format/Migrate.hpp"
#include "affresco/core/Numbers.hpp"

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <unordered_map>
#include <utility>

namespace affresco::format::detail {

using expect::fail;

namespace {

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
			if(++parser_.depth_ > maxNesting) parser_.error("nested too deeply");
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
			if(value.is_object() && value.size() == 1 && value.contains("$get"))
				value["$get"].push_back(std::move(key));
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

	/// @brief A number literal: digits, maybe with a fraction (a '-' before it is an operator).
	/// @return The number: an integer, or a double with a fraction.
	json number() {
		const std::size_t start = position_;
		while(position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
		bool fraction = false;
		if(position_ + 1 < text_.size() && text_[position_] == '.' &&
		   std::isdigit(static_cast<unsigned char>(text_[position_ + 1]))) {
			fraction = true;
			++position_;
			while(position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
		}
		const std::string digits(text_.substr(start, position_ - start));
		if(!fraction) {
			if(const std::optional<std::int64_t> value = wholeNumber(digits)) return *value;
		} else if(const double value = numberLengthFits(digits) ? std::strtod(digits.c_str(), nullptr) : HUGE_VAL;
		          std::isfinite(value)) {
			return value; // std::stod would throw out_of_range, past evaluate()'s callers
		}
		error("number '" + digits + "' is too large");
	}

	/// @brief Literals, `( )`, `[ ]`, `{ }`, `@colors`, function calls and variables.
	/// @return The node.
	json primary() {
		skip();
		if(position_ >= text_.size()) error("expected a value");
		const char c = text_[position_];

		if(std::isdigit(static_cast<unsigned char>(c))) return number();
		if(c == '\'' || c == '"') return node("$value", quoted(c));
		if(c == '@') return colour();
		if(accept("(")) {
			json inner = ternary();
			expect(")");
			return inner;
		}
		if(accept("[")) return list();
		if(accept("{")) return object();

		const std::string identifier = name(false);
		if(identifier == "true") return true;
		if(identifier == "false") return false;
		if(identifier == "null") return nullptr;
		if(accept("(")) return call(identifier);
		return node("$get", json::array({identifier}));
	}

	/// @brief A colour name, such as `@accent`, which stays text.
	/// @return The node.
	json colour() {
		const std::size_t start = position_++;
		while(position_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[position_])) ||
		                                   text_[position_] == '_' || text_[position_] == '-'))
			++position_;
		return node("$value", std::string(text_.substr(start, position_ - start)));
	}

	/// @brief A list literal, after its '[': its items are evaluated one by one.
	/// @return The node.
	json list() {
		json items = json::array();
		if(!accept("]")) {
			do items.push_back(ternary());
			while(accept(","));
			expect("]");
		}
		return items;
	}

	/// @brief An object literal, after its '{': `{ name: value, 'any key': value }`.
	/// @return The node.
	json object() {
		json fields = json::object();
		json::object_t& entries = fields.get_ref<json::object_t&>();
		std::unordered_map<std::string, std::size_t> at; // a key's entry: finding it in the object costs its size
		if(!accept("}")) {
			do {
				skip();
				std::string key = position_ < text_.size() && (text_[position_] == '\'' || text_[position_] == '"')
				                      ? quoted(text_[position_])
				                      : name(true);
				expect(":");
				json value = ternary();
				if(const auto [it, added] = at.try_emplace(key, entries.size()); added)
					entries.emplace_back(std::move(key), std::move(value));
				else
					std::next(entries.begin(), static_cast<std::ptrdiff_t>(it->second))->second =
					    std::move(value); // a key written again keeps its place
			} while(accept(","));
			expect("}");
		}
		return node("$object", std::move(fields));
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

	/// @brief A function call, after its '(': `length(x)` is `{"$length": x}`, `max(a, b)` is `{"$max": [a, b]}` (the
	/// registry, ExpressionFunctions.hpp, says which takes one value).
	/// @param function Its name.
	/// @return The node.
	json call(const std::string& function) {
		json arguments = json::array();
		if(!accept(")")) {
			do arguments.push_back(ternary());
			while(accept(","));
			expect(")");
		}
		const Function* found = findFunction(function);
		if(!found) {
			const std::string removed = removedFunctionHint(function);
			error("unknown function '" + function + "'" + (removed.empty() ? "" : " (" + removed + ")"));
		}
		const std::string name = "$" + std::string(found->name);
		if(found->takes == Takes::One) {
			if(arguments.size() != 1) error("'" + function + "' takes one argument");
			return node(name, std::move(arguments[0]));
		}
		return node(name, std::move(arguments));
	}

	std::string_view text_;    ///< The expression.
	const std::string& where_; ///< Its place, for errors.
	std::size_t position_ = 0; ///< Where parsing is.
	int depth_ = 0;            ///< Nesting now (see Nesting).
};

} // namespace

json parse(std::string_view text, const std::string& where) { return Parser(text, where).parse(); }

std::optional<std::int64_t> wholeNumber(std::string_view digits) noexcept {
	std::int64_t value = 0;
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
	if(error != std::errc() || end != digits.data() + digits.size()) return std::nullopt;
	return value;
}

} // namespace affresco::format::detail
