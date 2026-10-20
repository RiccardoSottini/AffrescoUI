/// @file
/// @brief The expression API: the compile cache, and evaluate(), lookup() and statePointer() on the machine
/// (ExpressionMachine.hpp). The parser is in ExpressionParser.cpp, the functions in ExpressionFunctions*.cpp.

#include "Expression.hpp"

#include "ExpressionMachine.hpp"
#include "ExpressionParser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <functional>
#include <new>
#include <optional>
#include <unordered_map>

namespace affresco::format {

using expect::fail;

namespace {

/// @brief Tells whether text that isn't an expression may be a path written before expressions: "state.items[0]".
/// @param text The text.
/// @return true for names, dots and brackets, starting with a variable's name (never a digit: "99999999999999999999"
/// is a number too large).
bool looksLikePath(std::string_view text) {
	if(text.empty() || std::isdigit(static_cast<unsigned char>(text.front()))) return false;
	return std::ranges::all_of(text, [](char c) {
		return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == '[' || c == ']';
	});
}

/// @brief Hashes texts and views of them alike, so the cache is searched without copying the text.
struct TextHash {
	using is_transparent = void; ///< Lets find() take a std::string_view.
	/// @brief Hashes a text.
	/// @param text The text.
	/// @return Its hash.
	std::size_t operator()(std::string_view text) const noexcept { return std::hash<std::string_view>{}(text); }
};

/// @brief Things made from texts, by text, searched without copying the text.
/// @tparam T What is made.
template <typename T>
using TextCache = std::unordered_map<std::string, std::shared_ptr<const T>, TextHash, std::equal_to<>>;

/// @brief The compiled expressions and the split templates: the main thread's, because they repeat on every update.
struct Caches {
	TextCache<detail::Compiled> expressions; ///< By the expression's text, trimmed.
	TextCache<detail::Template> templates;   ///< By the whole text of the template.
};

/// @brief The caches.
/// @return Them.
Caches& caches() {
	static Caches cache;
	return cache;
}

/// @brief Parses and lowers an expression.
/// @param text The expression, trimmed.
/// @param where Its place, for errors.
/// @return It compiled.
/// @throws DocumentError If it isn't a valid expression.
std::shared_ptr<const detail::Compiled> build(const std::string& text, const std::string& where) {
	auto result = std::make_shared<detail::Compiled>();
	try {
		result->tree = detail::parse(text, where);
	} catch(const DocumentError&) {
		// Text that isn't an expression but looks like a path is read as one (paths written before expressions). Few
		// names with '-' get here: "state.my-list" parses, as state.my - list, so such names go in brackets:
		// state['my-list'] (SCREEN_FORMAT.md, "Expressions")
		if(!looksLikePath(text)) throw;
		result->tree = {{"$path", text}};
	}
	result->program = detail::lower(result->tree);
	return result;
}

/// @brief The keys of a JSON Pointer, outermost first.
/// @param pointer The pointer.
/// @return Its keys, unescaped.
std::vector<std::string> keysOf(const json::json_pointer& pointer) {
	std::vector<std::string> keys;
	for(json::json_pointer rest = pointer; !rest.empty(); rest = rest.parent_pointer()) keys.push_back(rest.back());
	std::ranges::reverse(keys);
	return keys;
}

/// @brief Reads a key into a list as a position, as JSON Pointers write them: digits, without a leading zero.
/// @param key The key.
/// @return The position, or std::nullopt if the key isn't one.
std::optional<std::size_t> positionOf(std::string_view key) noexcept {
	if(key.empty() || (key.size() > 1 && key.front() == '0')) return std::nullopt;
	std::size_t position = 0;
	const auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), position);
	if(error != std::errc() || end != key.data() + key.size()) return std::nullopt;
	return position;
}

/// @brief The outermost place: what a caller gave.
/// @param where Its text.
/// @return The place.
detail::Place outermost(const std::string& where) noexcept { return detail::Place(where); }

} // namespace

namespace detail {

std::shared_ptr<const Compiled> compiled(std::string_view text, const Place& place) {
	TextCache<Compiled>& cache = caches().expressions;
	const std::string_view key = trim(text);
	if(const auto it = cache.find(key); it != cache.end()) return it->second;
	if(cache.size() >= compileCacheLimit) cache.clear(); // what still runs keeps its own (shared) copy

	std::string owned(key);
	std::shared_ptr<const Compiled> result = build(owned, place.text());
	cache.emplace(std::move(owned), result);
	return result;
}

std::shared_ptr<const Template> templateOf(std::string_view text, const Place& place) {
	TextCache<Template>& cache = caches().templates;
	if(const auto it = cache.find(text); it != cache.end()) return it->second;
	std::shared_ptr<const Template> result = splitTemplate(text, place);
	if(result->failure) return result; // its error names this place
	if(cache.size() >= compileCacheLimit) cache.clear();
	cache.emplace(std::string(text), result);
	return result;
}

} // namespace detail

const json& compile(std::string_view text, const std::string& where) {
	return detail::compiled(text, outermost(where))->tree; // the cache keeps it until it is cleared
}

void clearCompileCache() noexcept {
	caches().expressions.clear();
	caches().templates.clear();
}

json evaluate(const json& value, const Context& context, const std::string& where) {
	detail::Machine machine(context);
	return machine.evaluateDocument(value, outermost(where));
}

Placeholders placeholders(std::string_view text) {
	Placeholders result;
	const std::string_view whole = detail::trim(text);
	if(whole.starts_with("{{") && detail::closing(whole, 2) == whole.size() - 2) {
		result.expressions.push_back(whole.substr(2, whole.size() - 4));
		result.whole = true;
		return result;
	}
	for(std::size_t start; (start = text.find("{{")) != std::string_view::npos;) {
		const std::size_t end = detail::closing(text, start + 2);
		if(end == std::string_view::npos) {
			result.unclosed = true;
			break;
		}
		result.expressions.push_back(text.substr(start + 2, end - start - 2));
		text.remove_prefix(end + 2);
	}
	return result;
}

json lookup(std::string_view path, const Context& context, const std::string& where) {
	detail::Machine machine(context);
	return machine.lookup(path, outermost(where)).toJson();
}

json::json_pointer statePointer(std::string_view path, const Context& context, const std::string& where) {
	detail::Machine machine(context);
	const std::vector<Value> keys = machine.segments(path, outermost(where));
	if(keys.front().text() != "state")
		fail(where, "can only change values under 'state', not '" + std::string(path) + "'");
	if(keys.size() - 1 > static_cast<std::size_t>(maxDocumentNesting)) // writing there would build the levels
		fail(where, "a place in the state more than " + std::to_string(maxDocumentNesting) + " levels deep");

	json::json_pointer pointer;
	for(std::size_t i = 1; i < keys.size(); ++i) {
		if(keys[i].isText()) pointer /= std::string(keys[i].text());
		else pointer /= static_cast<std::size_t>(keys[i].integer());
	}
	return pointer;
}

json& statePlace(json& state, const json::json_pointer& pointer, const std::string& where) {
	json* at = &state;
	for(const std::string& key : keysOf(pointer)) {
		const std::optional<std::size_t> position = positionOf(key);
		if(at->is_null()) *at = position || key == "-" ? json::array() : json::object(); // as state[pointer] makes it
		if(at->is_object()) {
			at = &(*at)[key];
			continue;
		}
		if(!at->is_array())
			fail(where, "can't write '" + key + "' inside a " + std::string(at->type_name()) + " of the state");
		if(key == "-") {
			at = &at->emplace_back();
			continue;
		}
		if(!position) fail(where, "'" + key + "' isn't a position in a list of the state");
		if(*position > at->size() + maxListGap)
			fail(where, "position " + key + " is more than " + std::to_string(maxListGap) +
			                " past the end of a list of " + std::to_string(at->size()));
		at = &(*at)[*position]; // a position past the end fills the list up to it with null
	}
	return *at;
}

const json* findInState(const json& state, const json::json_pointer& pointer) noexcept {
	try {
		const json* at = &state;
		for(const std::string& key : keysOf(pointer)) {
			const std::optional<std::size_t> position = positionOf(key);
			if(at->is_object()) {
				const auto it = at->find(key);
				if(it == at->end()) return nullptr;
				at = &*it;
			} else if(at->is_array() && position && *position < at->size()) {
				at = &(*at)[*position];
			} else {
				return nullptr;
			}
		}
		return at;
	} catch(const std::bad_alloc&) { // copying the pointer's keys
		return nullptr;
	}
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

} // namespace affresco::format
