#pragma once

/// @file
/// @brief Bencode, the encoding of nREPL's messages: integers, byte strings, lists and dictionaries. decode() reads one
/// value from untrusted bytes, within limits; encode() writes one.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace affresco::net::bencode {

/// @brief One bencoded value: a 64-bit integer, a byte string, a list or a dictionary.
class Value {
public:
	using List = std::vector<Value>;                              ///< A list's items, in order.
	using Dictionary = std::map<std::string, Value, std::less<>>; ///< A dictionary's entries, by key (sorted, as sent).

	/// @brief An empty byte string.
	Value() = default;
	/// @brief An integer. @param integer It.
	explicit Value(std::int64_t integer) : data_(integer) {}
	/// @brief A byte string. @param bytes Its bytes.
	explicit Value(std::string bytes) : data_(std::move(bytes)) {}
	/// @brief A list. @param items Its items.
	explicit Value(List items) : data_(std::move(items)) {}
	/// @brief A dictionary. @param entries Its entries.
	explicit Value(Dictionary entries) : data_(std::move(entries)) {}

	/// @brief Tells whether it is an integer. @return true if it is.
	[[nodiscard]] bool isInteger() const noexcept { return std::holds_alternative<std::int64_t>(data_); }
	/// @brief Tells whether it is a byte string. @return true if it is.
	[[nodiscard]] bool isString() const noexcept { return std::holds_alternative<std::string>(data_); }
	/// @brief Tells whether it is a list. @return true if it is.
	[[nodiscard]] bool isList() const noexcept { return std::holds_alternative<List>(data_); }
	/// @brief Tells whether it is a dictionary. @return true if it is.
	[[nodiscard]] bool isDictionary() const noexcept { return std::holds_alternative<Dictionary>(data_); }

	/// @brief Its integer. @return It. @throws std::bad_variant_access If it isn't one.
	[[nodiscard]] std::int64_t integer() const { return std::get<std::int64_t>(data_); }
	/// @brief Its bytes. @return Them. @throws std::bad_variant_access If it isn't a byte string.
	[[nodiscard]] const std::string& string() const { return std::get<std::string>(data_); }
	/// @brief Its items. @return Them. @throws std::bad_variant_access If it isn't a list.
	[[nodiscard]] const List& list() const { return std::get<List>(data_); }
	/// @brief Its items, to change. @return Them. @throws std::bad_variant_access If it isn't a list.
	[[nodiscard]] List& list() { return std::get<List>(data_); }
	/// @brief Its entries. @return Them. @throws std::bad_variant_access If it isn't a dictionary.
	[[nodiscard]] const Dictionary& dictionary() const { return std::get<Dictionary>(data_); }
	/// @brief Its entries, to change. @return Them. @throws std::bad_variant_access If it isn't a dictionary.
	[[nodiscard]] Dictionary& dictionary() { return std::get<Dictionary>(data_); }

	/// @brief One entry of a dictionary.
	/// @param key The key.
	/// @return Its value, or nullptr if it isn't a dictionary or has no such key; valid while this value is unchanged.
	[[nodiscard]] const Value* find(std::string_view key) const noexcept {
		const auto* entries = std::get_if<Dictionary>(&data_);
		if(entries == nullptr) return nullptr;
		const auto found = entries->find(key);
		return found == entries->end() ? nullptr : &found->second;
	}

private:
	std::variant<std::string, std::int64_t, List, Dictionary> data_; ///< What it holds.
};

/// @brief What decode() accepts: an editor's bytes are untrusted.
struct Limits {
	std::size_t maxBytes = std::size_t{8} << 20; ///< The longest value, in bytes; a longer one is Malformed.
	std::size_t maxDepth = 16;                   ///< Lists and dictionaries inside each other, at most.
	std::size_t maxKeys = 64;                    ///< Entries in one dictionary, at most.
};

/// @brief How reading a value went.
enum class Read {
	Value,    ///< A whole value was read.
	NeedMore, ///< The bytes so far start a value that isn't whole yet, within the limits.
	Malformed ///< The bytes can't start a value within the limits: the connection should close.
};

/// @brief What decode() read.
struct Decoded {
	Read status = Read::Malformed; ///< How it went.
	Value value;                   ///< The value, when status is Read::Value.
	std::size_t used = 0;          ///< The bytes it took, when status is Read::Value.
	std::string why;               ///< What is wrong, when status is Read::Malformed (never the bytes themselves).
};

/// @brief Reads the first value of some bytes.
/// @param bytes The bytes received so far; what follows the value is left for the next call.
/// @param limits The limits.
/// @return What it read.
[[nodiscard]] Decoded decode(std::string_view bytes, const Limits& limits);

/// @brief Writes a value.
/// @param value The value.
/// @return Its bytes.
[[nodiscard]] std::string encode(const Value& value);

} // namespace affresco::net::bencode
