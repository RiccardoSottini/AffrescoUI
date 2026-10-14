#include "screen/Expression.hpp"

#include "sgl/Screen.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <vector>

namespace sgl::screen {

namespace {

[[noreturn]] void fail(const std::string& where, const std::string& message) {
	throw ScreenError("sGLGUI screen: " + where + ": " + message);
}

/* Paths */

// A path split into object keys (strings) and array indices (numbers)
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
			// [3] or [state.current]: the inner part can itself be a path, with brackets of its own
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
			json key = number ? json(std::stoll(std::string(inner))) : lookup(inner, context, where);
			if(!key.is_number_integer() && !key.is_string())
				fail(where, "index '" + std::string(inner) + "' is neither a number nor a key");
			result.push_back(std::move(key));
		} else {
			fail(where, "invalid path '" + std::string(path) + "'");
		}
	}

	return result;
}

std::string_view trim(std::string_view text) {
	while(!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
	while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
	return text;
}

/* Operators */

double numberOf(const json& value, const std::string& where) {
	if(!value.is_number()) fail(where, "expected a number, got " + value.dump());
	return value.get<double>();
}

// Integer in, integer out: indices and counters stay integers
json numeric(double value, bool integral) {
	if(integral && std::trunc(value) == value) return static_cast<std::int64_t>(value);
	return value;
}

bool allIntegers(const json& values) {
	return std::ranges::all_of(values, [](const json& v) { return v.is_number_integer(); });
}

// Operators with several arguments take a literal array of them: {"$add": [1, "{{state.n}}"]}
json arguments(const json& raw, const Context& context, const std::string& where) {
	if(!raw.is_array()) fail(where, "expected an array of arguments");
	json values = json::array();
	for(std::size_t i = 0; i < raw.size(); ++i)
		values.push_back(evaluate(raw[i], context, where + "[" + std::to_string(i) + "]"));
	return values;
}

json applyOperator(const std::string& name, const json& raw, const Context& context, const std::string& where) {
	const std::string at = where + "." + name;

	// Lazy: only the branch taken is evaluated
	if(name == "$if") {
		if(!raw.is_array() || raw.size() != 3) fail(at, "expected [condition, then, else]");
		return truthy(evaluate(raw[0], context, at)) ? evaluate(raw[1], context, at) : evaluate(raw[2], context, at);
	}
	if(name == "$and" || name == "$or") {
		if(!raw.is_array()) fail(at, "expected an array of conditions");
		const bool isAnd = name == "$and";
		for(const json& item : raw)
			if(truthy(evaluate(item, context, at)) != isAnd) return !isAnd;
		return isAnd;
	}

	// One-argument operators take their value directly: {"$length": "{{state.items}}"}, {"$length": [1, 2, 3]}
	if(name == "$not" || name == "$empty" || name == "$length" || name == "$round") {
		const json value = evaluate(raw, context, at);
		if(name == "$not") return !truthy(value);
		if(name == "$empty") return !truthy(value) && !value.is_boolean() && !value.is_number();
		if(name == "$round") return static_cast<std::int64_t>(std::llround(numberOf(value, at)));
		if(value.is_string()) return value.get<std::string>().size();
		if(value.is_array() || value.is_object()) return value.size();
		if(value.is_null()) return 0;
		fail(at, "expected a string, a list or an object");
	}

	const json args = arguments(raw, context, at);
	auto count = [&](std::size_t n) {
		if(args.size() != n) fail(at, "expected " + std::to_string(n) + " arguments");
	};

	if(name == "$equals") return count(2), args[0] == args[1];
	if(name == "$notEquals") return count(2), args[0] != args[1];
	if(name == "$greater") return count(2), numberOf(args[0], at) > numberOf(args[1], at);
	if(name == "$less") return count(2), numberOf(args[0], at) < numberOf(args[1], at);
	if(name == "$greaterOrEqual") return count(2), numberOf(args[0], at) >= numberOf(args[1], at);
	if(name == "$lessOrEqual") return count(2), numberOf(args[0], at) <= numberOf(args[1], at);

	if(name == "$add" || name == "$multiply" || name == "$min" || name == "$max") {
		if(args.empty()) fail(at, "expected at least one argument");
		double result = numberOf(args[0], at);
		for(std::size_t i = 1; i < args.size(); ++i) {
			const double v = numberOf(args[i], at);
			if(name == "$add") result += v;
			else if(name == "$multiply") result *= v;
			else if(name == "$min") result = std::min(result, v);
			else result = std::max(result, v);
		}
		return numeric(result, allIntegers(args));
	}
	if(name == "$subtract") return count(2), numeric(numberOf(args[0], at) - numberOf(args[1], at), allIntegers(args));
	if(name == "$divide") {
		count(2);
		const double divisor = numberOf(args[1], at);
		if(divisor == 0.0) fail(at, "division by zero");
		return numberOf(args[0], at) / divisor;
	}
	if(name == "$clamp") {
		count(3);
		const double low = numberOf(args[1], at), high = numberOf(args[2], at);
		return numeric(std::clamp(numberOf(args[0], at), low, std::max(low, high)), allIntegers(args));
	}

	if(name == "$concat") {
		if(std::ranges::all_of(args, [](const json& v) { return v.is_array(); })) {
			json result = json::array();
			for(const json& part : args) result.insert(result.end(), part.begin(), part.end());
			return result;
		}
		std::string result;
		for(const json& part : args) result += toText(part);
		return result;
	}

	fail(at, "unknown operator '" + name + "'");
}

/* Templates */

json evaluateString(const std::string& text, const Context& context, const std::string& where) {
	const std::size_t open = text.find("{{");
	if(open == std::string::npos) return text;

	// Exactly one placeholder and nothing else: keep the value's own type
	const std::string_view whole = trim(text);
	if(whole.starts_with("{{") && whole.ends_with("}}") && whole.find("{{", 2) == std::string_view::npos)
		return lookup(trim(whole.substr(2, whole.size() - 4)), context, where);

	std::string result;
	std::size_t position = 0;
	for(std::size_t start; (start = text.find("{{", position)) != std::string::npos;) {
		const std::size_t end = text.find("}}", start + 2);
		if(end == std::string::npos) fail(where, "unclosed '{{' in \"" + text + "\"");
		result += text.substr(position, start - position);
		result += toText(lookup(trim(std::string_view(text).substr(start + 2, end - start - 2)), context, where));
		position = end + 2;
	}
	return result + text.substr(position);
}

} // namespace

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
	const std::vector<json> keys = segments(path, context, where);
	const std::string& root = keys.front().get_ref<const std::string&>();

	const json* current = nullptr;
	if(root == "state") current = &context.state;
	else if(root == "window") current = &context.window;
	else if(root == "revision") current = &context.revision;
	else if(root == "event") current = &context.event;
	else if(context.locals.contains(root)) current = &context.locals[root];
	else if(context.globals.contains(root)) current = &context.globals[root];
	else fail(where, "unknown variable '" + root + "' (state, event, window, revision, proposals, history, preview, or a "
	                 "repeat/forEach name)");

	for(std::size_t i = 1; i < keys.size(); ++i) {
		const json& key = keys[i];
		if(key.is_string() && current->is_object() && current->contains(key.get<std::string>())) {
			current = &(*current)[key.get<std::string>()];
		} else if(key.is_number_integer() && current->is_array() && key.get<std::int64_t>() >= 0 &&
		          static_cast<std::size_t>(key.get<std::int64_t>()) < current->size()) {
			current = &(*current)[static_cast<std::size_t>(key.get<std::int64_t>())];
		} else {
			return nullptr;
		}
	}
	return *current;
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

std::string toText(const json& value) {
	if(value.is_string()) return value.get<std::string>();
	if(value.is_number_float()) {
		const double v = value.get<double>();
		if(std::trunc(v) == v && std::abs(v) < 1e15) return std::to_string(static_cast<std::int64_t>(v));
	}
	if(value.is_null()) return "";
	return value.dump();
}

} // namespace sgl::screen
