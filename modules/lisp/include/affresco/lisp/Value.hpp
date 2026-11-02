#pragma once

/// @file
/// @brief The language's values: JSON's (nil, booleans, numbers, strings, vectors, maps), and what a language needs
/// besides (symbols, keywords, lists, functions). Values are immutable from the language's view.

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace affresco::lisp {

/// @brief A place in source text. The file is owned by the interpreter (or the reader's heap) that made the value.
struct Place {
	const std::string* file = nullptr; ///< The file, or nullptr when there is none.
	std::uint32_t line = 0;            ///< The line, from 1; 0 when it isn't known.
	std::uint32_t column = 0;          ///< The column, from 1, in characters.
};

/// @brief What a value is.
enum class Kind : std::uint8_t {
	Nil,      ///< `nil`: JSON's null.
	Boolean,  ///< `true`, `false`.
	Integer,  ///< A number held as a signed 64-bit integer.
	Unsigned, ///< A number held as an unsigned 64-bit integer, above what Integer holds.
	Float,    ///< A number held as a double.
	String,   ///< A UTF-8 string.
	Symbol,   ///< A word in code, `kill-line`; in data it is the string of the same text.
	Keyword,  ///< `:title`; as a map's key it is the string of the same text.
	List,     ///< `(a b)`: code, or in data a node.
	Vector,   ///< `[a b]`: JSON's array.
	Map,      ///< `{:a 1}`: JSON's object, its keys strings, in the order written.
	Function, ///< A closure.
	Native,   ///< A function of the language's own or the host's (Interpreter::definePrimitive()).
	Splice    ///< Internal: a value `~@` spreads into the list being built. Never seen by a program.
};

/// @brief A value: small, and cheap to copy.
///
/// What a value points at lives as long as the Interpreter that made it (its heap), or as long as the JSON it views
/// (Interpreter::viewJson()). Nothing is converted when a JSON value is read: a vector or a map from JSON stays a view
/// of it, its items wrapped as they are read.
class Value {
public:
	/// @brief nil.
	constexpr Value() noexcept = default;

	/// @brief A boolean. @param value It. @return The value.
	[[nodiscard]] static Value boolean(bool value) noexcept;
	/// @brief An integer. @param value It. @return The value.
	[[nodiscard]] static Value integer(std::int64_t value) noexcept;
	/// @brief A number. @param value It. @return The value.
	[[nodiscard]] static Value real(double value) noexcept;
	/// @brief A view of a JSON value, which must outlive it: a scalar is copied, a string, an array or an object
	/// viewed.
	/// @param value The JSON value.
	/// @return The value.
	[[nodiscard]] static Value view(const json& value) noexcept;
	/// @brief A value pointing at one of the interpreter's objects (internal).
	/// @param kind What it is.
	/// @param object The object.
	/// @return The value.
	[[nodiscard]] static Value object(Kind kind, const void* object) noexcept;

	/// @brief What it is. @return The kind.
	[[nodiscard]] Kind kind() const noexcept { return kind_; }
	/// @brief Whether it is nil. @return true if so.
	[[nodiscard]] bool isNil() const noexcept { return kind_ == Kind::Nil; }
	/// @brief Whether it counts as true: everything but nil and false. @return true if so.
	[[nodiscard]] bool truthy() const noexcept {
		return kind_ != Kind::Nil && (kind_ != Kind::Boolean || bits_.boolean);
	}
	/// @brief Whether it is a number. @return true if so.
	[[nodiscard]] bool isNumber() const noexcept {
		return kind_ == Kind::Integer || kind_ == Kind::Unsigned || kind_ == Kind::Float;
	}
	/// @brief Whether it is a list or a vector. @return true if so.
	[[nodiscard]] bool isSequence() const noexcept { return kind_ == Kind::List || kind_ == Kind::Vector; }

	/// @brief A boolean's value. @return It.
	[[nodiscard]] bool asBoolean() const noexcept { return bits_.boolean; }
	/// @brief An Integer's value. @return It.
	[[nodiscard]] std::int64_t asInteger() const noexcept { return bits_.integer; }
	/// @brief An Unsigned's value. @return It.
	[[nodiscard]] std::uint64_t asUnsigned() const noexcept { return bits_.natural; }
	/// @brief Any number, as a double. @return It.
	[[nodiscard]] double asDouble() const noexcept;
	/// @brief A number's value in JSON's own kind (for a JSON value's exact spelling). @return It.
	[[nodiscard]] json numberAsJson() const;

	/// @brief The text of a string, a symbol or a keyword (without its ':').
	/// @return It; empty for any other value.
	[[nodiscard]] std::string_view text() const noexcept;

	/// @brief How many items a list, a vector or a map has.
	/// @return The count; 0 for any other value.
	[[nodiscard]] std::size_t count() const noexcept;
	/// @brief An item of a list or a vector.
	/// @param index Its index, below count().
	/// @return It.
	[[nodiscard]] Value at(std::size_t index) const;
	/// @brief A map's key.
	/// @param index Its index, below count().
	/// @return It.
	[[nodiscard]] std::string_view keyAt(std::size_t index) const;
	/// @brief A map's value.
	/// @param index Its index, below count().
	/// @return It.
	[[nodiscard]] Value valueAt(std::size_t index) const;
	/// @brief A map's value by key.
	/// @param key The key.
	/// @param found Set to whether the map has it, if given.
	/// @return The value, or nil.
	[[nodiscard]] Value lookup(std::string_view key, bool* found = nullptr) const;
	/// @brief Where a list, a vector, a map, a symbol or a keyword read from source text is.
	/// @return The place; an empty one for any other value.
	[[nodiscard]] Place place() const noexcept;

	/// @brief Whether it views JSON (view()) rather than the interpreter's own object. @return true if so.
	[[nodiscard]] bool isView() const noexcept { return view_; }
	/// @brief The JSON a view holds (internal). @return It.
	[[nodiscard]] const json& viewed() const noexcept { return *bits_.viewed; }
	/// @brief The interpreter's object it points at (internal). @return It.
	[[nodiscard]] const void* pointer() const noexcept { return bits_.pointer; }

private:
	/// The value's contents.
	union Bits {
		bool boolean;
		std::int64_t integer;
		std::uint64_t natural;
		double real;
		const json* viewed;
		const void* pointer;
	};
	Kind kind_ = Kind::Nil; ///< See kind().
	bool view_ = false;     ///< See isView().
	Bits bits_{.natural = 0};
};

/// @brief The name of what a value is, for an error: it never says the value itself, which may be anyone's.
/// @param value The value.
/// @return "a number", "a map"...
[[nodiscard]] std::string_view describeKind(const Value& value) noexcept;

} // namespace affresco::lisp
