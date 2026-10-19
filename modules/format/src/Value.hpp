#pragma once

/// @file
/// @brief format::Value: the values expressions compute with. JSON appears only at the edges: a Value reads the state
/// in place, and becomes JSON again when evaluate() returns.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief A value of an expression: null, a boolean, a number, a text, a list or an object.
///
/// Cheap to copy: numbers and texts of up to 16 bytes are stored inline; longer texts, lists and objects share
/// immutable storage. A Value can also refer to a JSON value without copying it (fromJson()): reading
/// `state.cards` costs a pointer. Such a Value is valid while that JSON is, which for the state, the window, the event
/// and the locals is the evaluation. Everything else a Value holds, it owns. Used on one thread at a time.
class Value {
public:
	/// What a value is, as `type()` names it in expressions.
	enum class Type : std::uint8_t {
		Null,    ///< null.
		Boolean, ///< true or false.
		Number,  ///< An integer (64 bits) or a double.
		Text,    ///< UTF-8 text.
		List,    ///< Values in order.
		Object,  ///< Values by key, in the order the keys were set (like the documents' ordered JSON).
	};

	/// @brief One field of an object.
	struct Field;

	/// @brief Makes null.
	Value() noexcept = default;

	/// @brief Makes a boolean.
	/// @param value The boolean.
	/// @return The value.
	[[nodiscard]] static Value fromBoolean(bool value) noexcept;
	/// @brief Makes an integer.
	/// @param value The integer.
	/// @return The value.
	[[nodiscard]] static Value fromInteger(std::int64_t value) noexcept;
	/// @brief Makes a number that is a double, even when it is whole (`8 / 2` is 4.0).
	/// @param value The number.
	/// @return The value.
	[[nodiscard]] static Value fromNumber(double value) noexcept;
	/// @brief Makes a text, inline when it is short.
	/// @param text UTF-8 text.
	/// @return The value.
	[[nodiscard]] static Value fromText(std::string_view text);
	/// @brief Makes a text from a literal.
	/// @param text UTF-8 text, ending with a zero.
	/// @return The value.
	[[nodiscard]] static Value fromText(const char* text) { return fromText(std::string_view(text)); }
	/// @brief Makes a text from a string it takes over.
	/// @param text UTF-8 text.
	/// @return The value.
	[[nodiscard]] static Value fromText(std::string&& text);
	/// @brief Makes a list.
	/// @param items Its items.
	/// @return The value.
	[[nodiscard]] static Value fromList(std::vector<Value> items);
	/// @brief Makes an object.
	/// @param fields Its fields, in order; a key given twice keeps its first place and its last value, as JSON does.
	/// @return The value.
	[[nodiscard]] static Value fromObject(std::vector<Field> fields);
	/// @brief Refers to a JSON value without copying it: numbers, booleans and null are copied, the rest is read in
	/// place.
	/// @param value The JSON; it must outlive the Value and every Value read from it.
	/// @return The value.
	[[nodiscard]] static Value fromJson(const json& value) noexcept;
	/// @brief Takes a JSON value over, for one that lives no longer than the call that made it (a computed value).
	/// @param value The JSON.
	/// @return The value.
	[[nodiscard]] static Value ownJson(json value);
	/// @brief Copies a JSON value into Values of its own, so that it doesn't depend on the JSON (literals).
	/// @param value The JSON.
	/// @return The value.
	[[nodiscard]] static Value copyJson(const json& value);

	/// @brief What the value is.
	[[nodiscard]] Type type() const noexcept;
	/// @brief Tells whether it is null.
	[[nodiscard]] bool isNull() const noexcept { return type() == Type::Null; }
	/// @brief Tells whether it is a boolean.
	[[nodiscard]] bool isBoolean() const noexcept { return type() == Type::Boolean; }
	/// @brief Tells whether it is a number.
	[[nodiscard]] bool isNumber() const noexcept { return type() == Type::Number; }
	/// @brief Tells whether it is a number held as an integer, as JSON's integers are (1, not 1.0).
	[[nodiscard]] bool isInteger() const noexcept;
	/// @brief Tells whether it is a text.
	[[nodiscard]] bool isText() const noexcept { return type() == Type::Text; }
	/// @brief Tells whether it is a list.
	[[nodiscard]] bool isList() const noexcept { return type() == Type::List; }
	/// @brief Tells whether it is an object.
	[[nodiscard]] bool isObject() const noexcept { return type() == Type::Object; }

	/// @brief The boolean (false if it isn't one).
	[[nodiscard]] bool boolean() const noexcept;
	/// @brief The number as a double (0 if it isn't one).
	[[nodiscard]] double number() const noexcept;
	/// @brief The integer (0 if it isn't one).
	[[nodiscard]] std::int64_t integer() const noexcept;
	/// @brief The text, valid while this Value is (empty if it isn't one).
	[[nodiscard]] std::string_view text() const noexcept;
	/// @brief How many items a list has, or fields an object has (0 for anything else).
	[[nodiscard]] std::size_t size() const noexcept;
	/// @brief A list's item.
	/// @param index Its position, below size().
	/// @return It.
	[[nodiscard]] Value item(std::size_t index) const;
	/// @brief An object's field.
	/// @param key Its key.
	/// @return Its value, or std::nullopt if the object has no such field (or this isn't an object).
	[[nodiscard]] std::optional<Value> member(std::string_view key) const;
	/// @brief An object's fields, in order.
	/// @return Copies of them (empty if this isn't an object).
	[[nodiscard]] std::vector<Field> fields() const;

	/// @brief The value as JSON: what evaluate() returns.
	[[nodiscard]] json toJson() const;
	/// @brief The value as text, as format::toText() writes JSON: texts as they are, null as "", whole doubles below
	/// 10^15 without a fraction, the rest as JSON (bytes that aren't UTF-8 replaced, never an exception).
	[[nodiscard]] std::string toText() const;
	/// @brief The value as JSON text, as `json()` writes it.
	[[nodiscard]] std::string dump() const;

	/// @brief Compares two values as JSON compares them: numbers by value (1 == 1.0), lists item by item, objects
	/// field by field in order (the documents' JSON keeps the order of keys, so {a, b} differs from {b, a}).
	/// @param left A value.
	/// @param right Another.
	/// @return true if they are equal.
	friend bool operator==(const Value& left, const Value& right);

private:
	/// How a value is held.
	enum class Holding : std::uint8_t {
		Null,      ///< Nothing.
		Boolean,   ///< inline_.boolean.
		Integer,   ///< inline_.integer.
		Double,    ///< inline_.number.
		ShortText, ///< inline_.characters, shortSize_ bytes.
		Text,      ///< storage_: a string.
		List,      ///< storage_: Values.
		Object,    ///< storage_: Fields, in order, each key once.
		Json,      ///< inline_.data: a JSON text, list, object or large unsigned number; storage_ owns it if it is set.
	};

	/// What storage_ points at.
	struct Storage;

	/// @brief Refers to JSON that storage keeps (or nothing keeps, for borrowed JSON).
	/// @param value The JSON.
	/// @param owner What keeps it alive, or nullptr.
	/// @return The value.
	[[nodiscard]] static Value fromJson(const json& value, std::shared_ptr<const Storage> owner) noexcept;

	std::shared_ptr<const Storage> storage_; ///< Long texts, lists, objects and owned JSON.
	union Inline {
		bool boolean;                 ///< Holding::Boolean.
		std::int64_t integer;         ///< Holding::Integer.
		double number;                ///< Holding::Double.
		const json* data;             ///< Holding::Json.
		char characters[16];          ///< Holding::ShortText.
	} inline_{};                      ///< What isn't in storage_.
	Holding holding_ = Holding::Null; ///< How the value is held.
	std::uint8_t shortSize_ = 0;      ///< The length of a ShortText.
};

/// @brief One field of an object.
struct Value::Field {
	std::string key; ///< Its key.
	Value value;     ///< Its value.
};

/// @brief Tells whether a value counts as true.
/// @param value The value.
/// @return false for false, null, 0, "", [] and {}.
[[nodiscard]] bool truthy(const Value& value) noexcept;

} // namespace affresco::format
