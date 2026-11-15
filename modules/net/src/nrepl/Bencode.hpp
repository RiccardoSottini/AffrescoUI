#pragma once

/// @file
/// @brief Bencode, the encoding of nREPL's messages: integers, byte strings, lists and dictionaries. decode() reads one
/// value from untrusted bytes, within limits; encode() writes one.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
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
	/// Values in one message, at most (every integer, string, list, dictionary and key counts): a small value grows
	/// when it is decoded (`le`, two bytes, is a whole list), so this bounds the memory one message can take.
	std::size_t maxValues = std::size_t{1} << 16;
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

/// @brief Reads values from bytes that arrive in pieces, without reading the same bytes again for every piece: it
/// keeps the lists and dictionaries it is inside, and a string's length prefix tells it how long to wait. Used by one
/// thread at a time.
class Reader {
public:
	/// @brief A reader at the start of a value.
	/// @param limits What it accepts.
	explicit Reader(const Limits& limits) : limits_(limits) {}

	/// @brief Reads on from where the last call stopped.
	/// @param bytes Everything received since the last value ended: the bytes passed before, then any new ones.
	/// @return What it read. After Read::Value (the first `used` bytes were the value) it starts on the next value,
	/// from a buffer without them; after Read::Malformed it reads nothing more until reset().
	[[nodiscard]] Decoded read(std::string_view bytes);

	/// @brief Forgets what it read, to start on a new value.
	void reset() noexcept;

private:
	/// @brief A list or a dictionary being read.
	struct Open {
		Value value;         ///< What it holds so far.
		std::string key;     ///< A dictionary's key, read and waiting for its value.
		bool hasKey = false; ///< Whether key is waiting.
	};

	/// @brief Reads one token (a scalar, an opening or an `e`) at position_.
	/// @param bytes The bytes.
	/// @return Read::Value when a whole top-level value ended, NeedMore when the token isn't whole, Malformed, or
	/// nullopt to read on.
	[[nodiscard]] std::optional<Read> step(std::string_view bytes);
	/// @brief Reads a string token at position_.
	/// @param bytes The bytes.
	/// @return As step().
	[[nodiscard]] std::optional<Read> readString(std::string_view bytes);
	/// @brief Reads an integer token at position_.
	/// @param bytes The bytes.
	/// @return As step().
	[[nodiscard]] std::optional<Read> readInteger(std::string_view bytes);
	/// @brief Opens a list or a dictionary.
	/// @param dictionary Which.
	/// @return As step().
	[[nodiscard]] std::optional<Read> open(bool dictionary);
	/// @brief Closes the innermost list or dictionary (an `e`).
	/// @return As step().
	[[nodiscard]] std::optional<Read> close();
	/// @brief Puts a whole value where it belongs: in the innermost list or dictionary, or as the result.
	/// @param value The value.
	/// @return As step().
	[[nodiscard]] std::optional<Read> place(Value value);
	/// @brief Counts one more value, within maxValues.
	/// @return Whether it fits.
	[[nodiscard]] bool count();
	/// @brief Stops on a malformed value.
	/// @param why What is wrong.
	/// @return Read::Malformed.
	Read fail(std::string why);

	Limits limits_;            ///< What it accepts.
	std::vector<Open> open_;   ///< The lists and dictionaries it is inside, outermost first.
	std::size_t position_ = 0; ///< The bytes read and taken into the value.
	std::size_t wanted_ = 0;   ///< The bytes a string waits for: nothing is read until there are as many.
	std::size_t values_ = 0;   ///< Values read so far.
	Value result_;             ///< The value, once whole.
	std::string why_;          ///< Why it failed.
	bool failed_ = false;      ///< It read a malformed value.
};

/// @brief Writes a value.
/// @param value The value.
/// @return Its bytes.
[[nodiscard]] std::string encode(const Value& value);

} // namespace affresco::net::bencode
