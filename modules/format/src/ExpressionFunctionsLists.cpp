/// @file
/// @brief Expression functions: lists, objects and their parts (len, concat, join, slice, contains, keys, merge, ...).

#include "ExpressionFunctions.hpp"

#include <algorithm>
#include <array>

namespace affresco::format::detail {
namespace {

/// @brief `len(x)`: a text's bytes, a list's items, an object's fields; 0 for null.
/// @param call [x].
/// @return The length.
Value length(const Call& call) {
	const Value& value = call[0];
	if(value.isText()) return Value::fromInteger(static_cast<std::int64_t>(value.text().size()));
	if(value.isList() || value.isObject()) return Value::fromInteger(static_cast<std::int64_t>(value.size()));
	if(value.isNull()) return Value::fromInteger(0);
	call.fail("expected a string, a list or an object");
}

/// @brief `concat(a, b, ...)`: lists joined into one if all are lists, else texts joined.
/// @param call The parts.
/// @return The list or the text.
Value concat(const Call& call) {
	if(std::ranges::all_of(call.values(), &Value::isList)) {
		std::vector<Value> items;
		for(const Value& part : call.values())
			for(std::size_t i = 0; i < part.size(); ++i) items.push_back(part.item(i));
		return Value::fromList(std::move(items));
	}
	std::string result;
	for(const Value& part : call.values()) result += part.toText();
	return Value::fromText(std::move(result));
}

/// @brief `join(list, separator)`: the items as text, e.g. "#board, /state/cards". Separators go between texts
/// written so far and the next item, so empty items at the start add none.
/// @param call [list, separator].
/// @return The text.
Value join(const Call& call) {
	if(!call[0].isList()) call.fail("expected a list first");
	const std::string separator = call[1].toText();
	std::string result;
	for(std::size_t i = 0; i < call[0].size(); ++i) {
		if(!result.empty()) result += separator;
		result += call[0].item(i).toText();
	}
	return Value::fromText(std::move(result));
}

/// @brief `slice(list or text, from, to?)`: negative positions count from the end; a text is cut by bytes.
/// @param call [list or text, from, to?].
/// @return The part.
Value slice(const Call& call) {
	const Value& whole = call[0];
	const auto size = static_cast<std::int64_t>(whole.isText()   ? whole.text().size()
	                                            : whole.isList() ? whole.size()
	                                                             : 0);
	const auto position = [&](std::size_t argument) {
		std::int64_t i = saturated(call.number(argument));
		if(i < 0) i += size;
		return std::clamp<std::int64_t>(i, 0, size);
	};
	const std::int64_t from = position(1), to = call.size() > 2 ? position(2) : size;
	if(whole.isText())
		return to > from ? Value::fromText(
		                       whole.text().substr(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from)))
		                 : Value::fromText(std::string_view());
	if(!whole.isList()) call.fail("expected a list or a text first");
	std::vector<Value> items;
	for(std::int64_t i = from; i < to; ++i) items.push_back(whole.item(static_cast<std::size_t>(i)));
	return Value::fromList(std::move(items));
}

/// @brief Finds a part: a text in a text (its byte position), a value in a list, a key in an object (0).
/// @param call [where, what].
/// @return Its position, or -1.
std::int64_t position(const Call& call) {
	const Value& where = call[0];
	if(where.isText()) {
		const std::size_t i = where.text().find(call[1].toText());
		return i == std::string_view::npos ? -1 : static_cast<std::int64_t>(i);
	}
	if(where.isList()) {
		for(std::size_t i = 0; i < where.size(); ++i)
			if(where.item(i) == call[1]) return static_cast<std::int64_t>(i);
		return -1;
	}
	if(where.isObject()) return where.member(call[1].toText()) ? 0 : -1;
	return -1;
}

/// @brief `contains(list, text or object, value)`.
/// @param call [where, what].
/// @return Whether it is there.
Value contains(const Call& call) { return Value::fromBoolean(position(call) >= 0); }

/// @brief `indexOf(list or text, value)`.
/// @param call [where, what].
/// @return Its position, or -1.
Value indexOf(const Call& call) { return Value::fromInteger(position(call)); }

/// @brief `keys(object)`.
/// @param call [object].
/// @return Its keys in order ([] for anything else).
Value keys(const Call& call) {
	std::vector<Value> result;
	for(const Value::Field& field : call[0].fields()) result.push_back(Value::fromText(field.key));
	return Value::fromList(std::move(result));
}

/// @brief `values(object)`.
/// @param call [object].
/// @return Its values, by key ([] for anything else).
Value values(const Call& call) {
	std::vector<Value> result;
	for(Value::Field& field : call[0].fields()) result.push_back(std::move(field.value));
	return Value::fromList(std::move(result));
}

/// @brief `entries(object)`: `{a: 1}` gives `[{key: "a", value: 1}]`, to repeat over an object.
/// @param call [object].
/// @return The entries ([] for anything else).
Value entries(const Call& call) {
	std::vector<Value> result;
	for(Value::Field& field : call[0].fields())
		result.push_back(Value::fromObject({{"key", Value::fromText(field.key)}, {"value", std::move(field.value)}}));
	return Value::fromList(std::move(result));
}

/// @brief `merge(a, b, ...)`: objects, later keys win; null is skipped.
/// @param call The objects.
/// @return The merged object.
Value merge(const Call& call) {
	std::vector<Value::Field> fields;
	for(const Value& part : call.values()) {
		if(part.isNull()) continue;
		if(!part.isObject()) call.fail("expected objects");
		for(Value::Field& field : part.fields()) fields.push_back(std::move(field));
	}
	return Value::fromObject(std::move(fields)); // of equal keys, the later stays
}

/// @brief `reverse(list or text)`: a text by characters, so "é" stays whole.
/// @param call [list or text].
/// @return It reversed.
Value reverse(const Call& call) {
	const Value& value = call[0];
	if(value.isText()) {
		const std::string_view text = value.text();
		std::string result(text.size(), '\0');
		for(std::size_t i = 0; i < text.size();) {
			const std::size_t size = std::min(utf8CharacterSize(static_cast<unsigned char>(text[i])), text.size() - i);
			text.copy(result.data() + (text.size() - i - size), size, i);
			i += size;
		}
		return Value::fromText(std::move(result));
	}
	if(!value.isList()) call.fail("expected a list");
	std::vector<Value> items;
	items.reserve(value.size());
	for(std::size_t i = value.size(); i > 0; --i) items.push_back(value.item(i - 1));
	return Value::fromList(std::move(items));
}

/// @brief `range(n)`: 0, 1, ... n - 1.
/// @param call [n].
/// @return The list.
Value range(const Call& call) {
	const std::int64_t n = saturated(call.number(0));
	if(n > 10000) call.fail("range too long");
	std::vector<Value> items;
	items.reserve(static_cast<std::size_t>(std::max<std::int64_t>(n, 0)));
	for(std::int64_t i = 0; i < n; ++i) items.push_back(Value::fromInteger(i));
	return Value::fromList(std::move(items));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "length",
	 .arguments = "value",
	 .about = "A text's bytes, a list's items, an object's fields; 0 for null",
	 .takes = Takes::One,
	 .apply = length,
	 .alias = "len"},
    {.name = "concat",
	 .arguments = "a, b, ...",
	 .about = "Lists joined into one, or else texts joined",
	 .apply = concat},
    {.name = "join",
	 .arguments = "list, separator",
	 .about = "The items as text, with the separator between them",
	 .fewest = 2,
	 .most = 2,
	 .apply = join},
    {.name = "slice",
	 .arguments = "list or text, from, to?",
	 .about = "The part from one position to another (negative counts from the end)",
	 .fewest = 2,
	 .most = 3,
	 .apply = slice},
    {.name = "contains",
	 .arguments = "list or text or object, value",
	 .about = "Whether the list holds the value, the text holds the text, or the object has the key",
	 .fewest = 2,
	 .most = 2,
	 .apply = contains},
    {.name = "indexOf",
	 .arguments = "list or text, value",
	 .about = "Where the value is, or -1",
	 .fewest = 2,
	 .most = 2,
	 .apply = indexOf},
    {.name = "keys", .arguments = "object", .about = "The object's keys", .takes = Takes::One, .apply = keys},
    {.name = "values", .arguments = "object", .about = "The object's values", .takes = Takes::One, .apply = values},
    {.name = "entries",
	 .arguments = "object",
	 .about = "The object's fields as {key, value} objects, to repeat over",
	 .takes = Takes::One,
	 .apply = entries},
    {.name = "merge", .arguments = "a, b, ...", .about = "The objects merged; later keys win", .apply = merge},
    {.name = "reverse",
	 .arguments = "list or text",
	 .about = "The items, or the characters, in reverse order",
	 .takes = Takes::One,
	 .apply = reverse},
    {.name = "range",
	 .arguments = "count",
	 .about = "0, 1, ... count - 1",
	 .takes = Takes::One,
	 .apply = range,
	 .remark = "count up to 10000"},
});

} // namespace

std::span<const Function> listFunctions() { return table; }

} // namespace affresco::format::detail
