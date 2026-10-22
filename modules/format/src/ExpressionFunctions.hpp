#pragma once

/// @file
/// @brief The registry of expression functions: each one's name, arguments, one line of documentation and
/// implementation. Every function is also an operator: `clamp(v, 0, 9)` is `{"$clamp": [v, 0, 9]}`.
///
/// A new function is a new row in the table of its group's file (ExpressionFunctions<Group>.cpp); a new group is a
/// new file whose table functions() lists. The catalog's operators, the format guide's function list
/// (SCREEN_FORMAT.md, which a unit test checks against functionGuide()) and the parser all read this registry.

#include "Value.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace affresco::format::detail {

class Machine;
struct Node;
struct Place;

/// @brief One call of a function: its arguments, and what the function may ask of the machine that runs it.
///
/// Functions that take their arguments evaluated (Function::apply) read them with operator[] and number(). The others
/// (Function::decide: `if`, `and`, `filter`, ...) evaluate the ones they need, when they need them. Lives for the
/// call only.
class Call {
public:
	/// @brief Describes a call.
	/// @param machine The machine running it.
	/// @param node The call in the typed tree.
	/// @param at The function's place, for errors ("<where>.$clamp").
	/// @param values The evaluated arguments (empty for a function that evaluates its own).
	Call(Machine& machine, const Node& node, const Place& at, std::span<const Value> values) noexcept
	    : machine_(machine), node_(node), at_(at), values_(values) {}

	/// @brief How many arguments were given.
	[[nodiscard]] std::size_t size() const noexcept;
	/// @brief An evaluated argument.
	/// @param index Its position, below size().
	/// @return It.
	[[nodiscard]] const Value& operator[](std::size_t index) const noexcept { return values_[index]; }
	/// @brief The evaluated arguments.
	[[nodiscard]] std::span<const Value> values() const noexcept { return values_; }
	/// @brief An evaluated argument that must be a number.
	/// @param index Its position.
	/// @return The number.
	/// @throws DocumentError If it is not one: "expected a number, got ...".
	[[nodiscard]] double number(std::size_t index) const;
	/// @brief Tells whether every evaluated argument is an integer: integers in, integers out.
	[[nodiscard]] bool allIntegers() const noexcept;
	/// @brief Evaluates an argument (for functions that evaluate their own).
	/// @param index Its position, below size().
	/// @return Its value.
	/// @throws DocumentError If it is invalid.
	[[nodiscard]] Value evaluate(std::size_t index) const;
	/// @brief Evaluates an argument once per item of a list, which it sees as `it`, and its position as `index`.
	/// @param index The argument's position.
	/// @param item The item.
	/// @param position The item's position.
	/// @return Its value.
	/// @throws DocumentError If it is invalid.
	[[nodiscard]] Value evaluateFor(std::size_t index, const Value& item, std::size_t position) const;
	/// @brief Reports a problem at the function's place.
	/// @param message What is wrong.
	/// @throws DocumentError Always.
	[[noreturn]] void fail(const std::string& message) const;

private:
	Machine& machine_;              ///< Runs the arguments.
	const Node& node_;              ///< The call.
	const Place& at_;               ///< Its place.
	std::span<const Value> values_; ///< The evaluated arguments.
};

/// How a function takes its arguments, as an operator.
enum class Takes : std::uint8_t {
	One,  ///< One value directly: `{"$length": x}`, `len(x)`.
	List, ///< An array of arguments: `{"$max": [a, b]}`, `max(a, b)`.
};

/// Where the format guide lists a function.
enum class Group : std::uint8_t {
	Syntax,    ///< Written as an operator (`+`, `&&`, `c ? a : b`); the guide lists the syntax instead.
	Functions, ///< "functions:".
	Items,     ///< "per item": an argument evaluated once per item of a list.
};

/// No limit on the number of arguments.
inline constexpr std::size_t unlimited = std::numeric_limits<std::size_t>::max();

/// @brief One row of the registry.
struct Function {
	std::string_view name;                  ///< As written in expressions: "clamp". Its operator is "$clamp".
	std::string_view arguments;             ///< Its arguments, for the guide and the catalog: "value, low, high".
	std::string_view about;                 ///< One line: what it gives.
	Group group = Group::Functions;         ///< Where the guide lists it.
	Takes takes = Takes::List;              ///< One value, or a list of arguments.
	std::size_t fewest = 0;                 ///< Fewest arguments (checked once they are evaluated).
	std::size_t most = unlimited;           ///< Most arguments.
	Value (*apply)(const Call&) = nullptr;  ///< Its implementation, given evaluated arguments; or
	Value (*decide)(const Call&) = nullptr; ///< its implementation, evaluating the arguments it needs.
	std::string_view shape = {};  ///< For `decide`: the error when its arguments aren't a list of the right size.
	bool spreads = false;         ///< A single list argument stands for its items: `max(list)`.
	std::string_view alias = {};  ///< Another name it is written with ("len").
	std::string_view remark = {}; ///< A note the guide adds: "(n up to 10000)".
	bool changes = false;         ///< Gives another value with nothing read changed (the clock): read on every update.
};

/// @brief Every function: the rows of every group's table, in order.
/// @return The registry.
[[nodiscard]] std::span<const Function> functions();
/// @brief Finds a function by the name expressions write.
/// @param name "clamp" or "len".
/// @return Its row, or nullptr.
[[nodiscard]] const Function* findFunction(std::string_view name);
/// @brief Finds a function by its operator.
/// @param name "$clamp".
/// @return Its row, or nullptr.
[[nodiscard]] const Function* findOperator(std::string_view name);

/// @brief The tables of the groups (one per file).
/// @return Their rows.
[[nodiscard]] std::span<const Function> logicFunctions();
/// @copydoc logicFunctions
[[nodiscard]] std::span<const Function> numberFunctions();
/// @copydoc logicFunctions
[[nodiscard]] std::span<const Function> textFunctions();
/// @copydoc logicFunctions
[[nodiscard]] std::span<const Function> listFunctions();
/// @copydoc logicFunctions
[[nodiscard]] std::span<const Function> itemFunctions();
/// @copydoc logicFunctions
[[nodiscard]] std::span<const Function> timeFunctions();
/// @copydoc logicFunctions
[[nodiscard]] std::span<const Function> fuzzyFunctions();
/// @copydoc logicFunctions
[[nodiscard]] std::span<const Function> keyframeFunctions();

/* Helpers the tables share */

/// 2^63: doubles from -2^63 up to, not including, this convert to a 64-bit integer. Casting any other is undefined.
inline constexpr double integerLimit = 9223372036854775808.0;

/// @brief Converts a number to a whole number for a count, an index or a width, saturating at the 64-bit range.
/// @param value The number.
/// @return It without its fraction, clamped to the 64-bit range; 0 for NaN.
[[nodiscard]] inline std::int64_t saturated(double value) noexcept {
	if(std::isnan(value)) return 0;
	if(value >= integerLimit) return std::numeric_limits<std::int64_t>::max();
	if(value < -integerLimit) return std::numeric_limits<std::int64_t>::min();
	return static_cast<std::int64_t>(value);
}

/// @brief Makes a numeric result; integer in, integer out, so indices and counters stay integers.
/// @param value The result.
/// @param integral Whether every argument was an integer.
/// @return An integer when possible (whole, and within 64 bits), else a double.
[[nodiscard]] inline Value numeric(double value, bool integral) noexcept {
	if(integral && std::trunc(value) == value && value >= -integerLimit && value < integerLimit)
		return Value::fromInteger(static_cast<std::int64_t>(value));
	return Value::fromNumber(value);
}

/// @brief Orders two values: numbers by size, text alphabetically, anything else by its JSON text.
/// @param a A value.
/// @param b Another.
/// @return Negative, zero or positive.
[[nodiscard]] int order(const Value& a, const Value& b);

/// @brief How many bytes the UTF-8 character starting with a byte takes.
/// @param lead The first byte.
/// @return 1 to 4 (1 for a byte that can't start one).
[[nodiscard]] inline std::size_t utf8CharacterSize(unsigned char lead) noexcept {
	if((lead & 0xE0) == 0xC0) return 2;
	if((lead & 0xF0) == 0xE0) return 3;
	if((lead & 0xF8) == 0xF0) return 4;
	return 1;
}

/// @brief Removes surrounding whitespace.
/// @param text The text.
/// @return A view without it.
[[nodiscard]] std::string_view trim(std::string_view text) noexcept;

} // namespace affresco::format::detail
