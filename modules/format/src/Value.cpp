/// @file
/// @brief format::Value: holding values inline or in shared storage, reading JSON in place, and converting back.

#include "Value.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <variant>

namespace affresco::format {

/// @brief What a Value shares with its copies: a long text, a list, an object, or JSON it owns. Never changed once
/// made.
struct Value::Storage {
	std::variant<std::string, std::vector<Value>, std::vector<Field>, json> data; ///< The content.
};

namespace {

/// The longest text a Value holds inline.
constexpr std::size_t inlineCapacity = 16;

} // namespace

Value Value::fromBoolean(bool value) noexcept {
	Value result;
	result.holding_ = Holding::Boolean;
	result.inline_.boolean = value;
	return result;
}

Value Value::fromInteger(std::int64_t value) noexcept {
	Value result;
	result.holding_ = Holding::Integer;
	result.inline_.integer = value;
	return result;
}

Value Value::fromNumber(double value) noexcept {
	Value result;
	result.holding_ = Holding::Double;
	result.inline_.number = value;
	return result;
}

Value Value::fromText(std::string_view text) {
	if(text.size() > inlineCapacity) return fromText(std::string(text));
	Value result;
	result.holding_ = Holding::ShortText;
	result.shortSize_ = static_cast<std::uint8_t>(text.size());
	if(!text.empty()) std::memcpy(result.inline_.characters.data(), text.data(), text.size());
	return result;
}

Value Value::fromText(std::string&& text) {
	if(text.size() <= inlineCapacity) return fromText(std::string_view(text));
	Value result;
	result.holding_ = Holding::Text;
	result.storage_ = std::make_shared<const Storage>(Storage{std::move(text)});
	return result;
}

Value Value::fromList(std::vector<Value> items) {
	Value result;
	result.holding_ = Holding::List;
	result.storage_ = std::make_shared<const Storage>(Storage{std::move(items)});
	return result;
}

Value Value::fromObject(std::vector<Field> fields) {
	// As a JSON object (ordered_json) sets keys: in the order they come, a key set again keeping its first place
	std::vector<Field> unique;
	unique.reserve(fields.size());
	for(Field& field : fields) {
		const auto same = std::ranges::find(unique, field.key, &Field::key);
		if(same != unique.end()) same->value = std::move(field.value);
		else unique.push_back(std::move(field));
	}
	Value result;
	result.holding_ = Holding::Object;
	result.storage_ = std::make_shared<const Storage>(Storage{std::move(unique)});
	return result;
}

Value Value::fromJson(const json& value) noexcept { return fromJson(value, nullptr); }

Value Value::fromJson(const json& value, std::shared_ptr<const Storage> owner) noexcept {
	// get_ptr() never throws: it gives nullptr for another type
	switch(value.type()) {
	case json::value_t::boolean:
		return fromBoolean(*value.get_ptr<const json::boolean_t*>());
	case json::value_t::number_integer:
		return fromInteger(*value.get_ptr<const json::number_integer_t*>());
	case json::value_t::number_float:
		return fromNumber(*value.get_ptr<const json::number_float_t*>());
	case json::value_t::number_unsigned:
		if(const auto n = *value.get_ptr<const json::number_unsigned_t*>();
		   n <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
			return fromInteger(static_cast<std::int64_t>(n));
		break; // larger than an integer: read in place, so it stays exact
	case json::value_t::string:
	case json::value_t::array:
	case json::value_t::object:
		break;
	default:
		return {}; // null, and what parsed JSON never holds (binary, discarded)
	}
	Value result;
	result.holding_ = Holding::Json;
	result.inline_.data = &value;
	result.storage_ = std::move(owner);
	return result;
}

Value Value::ownJson(json value) {
	auto storage = std::make_shared<const Storage>(Storage{std::move(value)});
	const json& kept = std::get<json>(storage->data);
	return fromJson(kept, std::move(storage));
}

Value Value::copyJson(const json& value) {
	if(value.is_string()) return fromText(value.get_ref<const std::string&>());
	if(value.is_array()) {
		std::vector<Value> items;
		items.reserve(value.size());
		for(const json& item : value) items.push_back(copyJson(item));
		return fromList(std::move(items));
	}
	if(value.is_object()) {
		std::vector<Field> fields;
		fields.reserve(value.size());
		for(const auto& [key, item] : value.items()) fields.push_back({key, copyJson(item)});
		return fromObject(std::move(fields));
	}
	const Value scalar = fromJson(value);
	return scalar.holding_ == Holding::Json ? ownJson(value) : scalar; // a large unsigned number
}

Value::Type Value::type() const noexcept {
	switch(holding_) {
	case Holding::Null:
		return Type::Null;
	case Holding::Boolean:
		return Type::Boolean;
	case Holding::Integer:
	case Holding::Double:
		return Type::Number;
	case Holding::ShortText:
	case Holding::Text:
		return Type::Text;
	case Holding::List:
		return Type::List;
	case Holding::Object:
		return Type::Object;
	case Holding::Json:
		break;
	}
	switch(inline_.data->type()) {
	case json::value_t::string:
		return Type::Text;
	case json::value_t::array:
		return Type::List;
	case json::value_t::object:
		return Type::Object;
	default:
		return Type::Number; // fromJson() keeps only large unsigned numbers besides those
	}
}

bool Value::isInteger() const noexcept {
	return holding_ == Holding::Integer || (holding_ == Holding::Json && inline_.data->is_number_integer());
}

bool Value::boolean() const noexcept { return holding_ == Holding::Boolean && inline_.boolean; }

double Value::number() const noexcept {
	if(holding_ == Holding::Integer) return static_cast<double>(inline_.integer);
	if(holding_ == Holding::Double) return inline_.number;
	if(holding_ != Holding::Json) return 0.0;
	const auto* large = inline_.data->get_ptr<const json::number_unsigned_t*>(); // fromJson() keeps only these
	return large ? static_cast<double>(*large) : 0.0;
}

std::int64_t Value::integer() const noexcept {
	if(holding_ == Holding::Integer) return inline_.integer;
	if(holding_ != Holding::Json) return 0;
	const auto* large = inline_.data->get_ptr<const json::number_unsigned_t*>();
	return large ? static_cast<std::int64_t>(*large) : 0; // past 2^63 it wraps, as JSON's get<std::int64_t>() does
}

std::string_view Value::text() const noexcept {
	if(holding_ == Holding::ShortText) return {inline_.characters.data(), shortSize_};
	if(holding_ == Holding::Text)
		if(const auto* text = std::get_if<std::string>(&storage_->data)) return *text;
	if(holding_ == Holding::Json)
		if(const auto* text = inline_.data->get_ptr<const json::string_t*>()) return *text;
	return {};
}

std::size_t Value::size() const noexcept {
	if(holding_ == Holding::List)
		if(const auto* items = std::get_if<std::vector<Value>>(&storage_->data)) return items->size();
	if(holding_ == Holding::Object)
		if(const auto* fields = std::get_if<std::vector<Field>>(&storage_->data)) return fields->size();
	if(holding_ == Holding::Json) {
		if(const auto* list = inline_.data->get_ptr<const json::array_t*>()) return list->size();
		if(const auto* object = inline_.data->get_ptr<const json::object_t*>()) return object->size();
	}
	return 0;
}

Value Value::item(std::size_t index) const {
	if(holding_ == Holding::List) return std::get<std::vector<Value>>(storage_->data)[index];
	return fromJson((*inline_.data)[index], storage_);
}

std::optional<Value> Value::member(std::string_view key) const {
	if(holding_ == Holding::Object) {
		const auto& fields = std::get<std::vector<Field>>(storage_->data);
		const auto it = std::ranges::find(fields, key, &Field::key);
		if(it != fields.end()) return it->value;
		return std::nullopt;
	}
	if(holding_ != Holding::Json || !inline_.data->is_object()) return std::nullopt;
	const auto it = inline_.data->find(key);
	if(it == inline_.data->end()) return std::nullopt;
	return fromJson(*it, storage_);
}

std::vector<Value::Field> Value::fields() const {
	if(holding_ == Holding::Object) return std::get<std::vector<Field>>(storage_->data);
	std::vector<Field> result;
	if(holding_ == Holding::Json && inline_.data->is_object()) {
		result.reserve(inline_.data->size());
		for(const auto& [key, item] : inline_.data->items()) result.push_back({key, fromJson(item, storage_)});
	}
	return result;
}

json Value::toJson() const {
	switch(holding_) {
	case Holding::Null:
		return nullptr;
	case Holding::Boolean:
		return inline_.boolean;
	case Holding::Integer:
		return inline_.integer;
	case Holding::Double:
		return inline_.number;
	case Holding::ShortText:
	case Holding::Text:
		return std::string(text());
	case Holding::List: {
		json result = json::array();
		const auto& items = std::get<std::vector<Value>>(storage_->data);
		result.get_ref<json::array_t&>().reserve(items.size());
		for(const Value& item : items) result.push_back(item.toJson());
		return result;
	}
	case Holding::Object: {
		json result = json::object();
		for(const Field& field : std::get<std::vector<Field>>(storage_->data)) result[field.key] = field.value.toJson();
		return result;
	}
	case Holding::Json:
		break;
	}
	return *inline_.data;
}

std::string Value::toText() const {
	switch(holding_) {
	case Holding::Null:
		return {};
	case Holding::Integer:
		return std::to_string(inline_.integer);
	case Holding::ShortText:
	case Holding::Text:
		return std::string(text());
	case Holding::Double:
		if(std::trunc(inline_.number) == inline_.number && std::abs(inline_.number) < 1e15)
			return std::to_string(static_cast<std::int64_t>(inline_.number));
		break;
	default:
		if(isText()) return std::string(text());
		break;
	}
	return dump();
}

std::string Value::dump() const { return toJson().dump(-1, ' ', false, json::error_handler_t::replace); }

namespace {

/// @brief Compares two lists item by item.
/// @param left A list.
/// @param right Another.
/// @return true if they are equal.
bool sameItems(const Value& left, const Value& right) {
	if(left.size() != right.size()) return false;
	for(std::size_t i = 0; i < left.size(); ++i)
		if(!(left.item(i) == right.item(i))) return false;
	return true;
}

/// @brief Compares two objects field by field, in order, as ordered_json compares them.
/// @param left An object.
/// @param right Another.
/// @return true if they have the same fields in the same order.
bool sameFields(const Value& left, const Value& right) {
	if(left.size() != right.size()) return false;
	const std::vector<Value::Field> a = left.fields(), b = right.fields();
	for(std::size_t i = 0; i < a.size(); ++i)
		if(a[i].key != b[i].key || !(a[i].value == b[i].value)) return false;
	return true;
}

} // namespace

bool Value::sameNumber(const Value& left, const Value& right) {
	if(left.holding_ == Holding::Json || right.holding_ == Holding::Json) return left.toJson() == right.toJson();
	if(left.holding_ == Holding::Integer && right.holding_ == Holding::Integer)
		return left.inline_.integer == right.inline_.integer;
	return left.number() == right.number(); // as JSON compares an integer with a double
}

bool operator==(const Value& left, const Value& right) {
	if(left.holding_ == Value::Holding::Json && right.holding_ == Value::Holding::Json)
		return *left.inline_.data == *right.inline_.data;
	const Value::Type type = left.type();
	if(type != right.type()) return false;
	switch(type) {
	case Value::Type::Null:
		return true;
	case Value::Type::Boolean:
		return left.boolean() == right.boolean();
	case Value::Type::Number:
		return Value::sameNumber(left, right);
	case Value::Type::Text:
		return left.text() == right.text();
	case Value::Type::List:
		return sameItems(left, right);
	case Value::Type::Object:
		break;
	}
	return sameFields(left, right);
}

bool truthy(const Value& value) noexcept {
	switch(value.type()) {
	case Value::Type::Null:
		return false;
	case Value::Type::Boolean:
		return value.boolean();
	case Value::Type::Number:
		return value.number() != 0.0;
	case Value::Type::Text:
		return !value.text().empty();
	case Value::Type::List:
	case Value::Type::Object:
		return value.size() > 0;
	}
	return false;
}

} // namespace affresco::format
