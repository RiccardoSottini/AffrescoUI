#pragma once

/// @file
/// @brief Parses the inside of a `{{ }}` (JS-style) into an operator tree: the JSON form of expressions, which the
/// Checker reads and the machine lowers (ExpressionMachine.hpp).

#include "affresco/core/Json.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::format::detail {

/// @brief How deep an expression may nest (real expressions use a handful of levels): a bracket takes two, a '!' one.
inline constexpr int maxNesting = 100;

/// @brief Parses an expression. Literals are `{"$value": x}` (numbers, booleans and null stay bare), paths
/// `{"$get": [root, key, ...]}`, where a string key is a name and anything else is evaluated; functions are their
/// operators.
/// @param text E.g. "a > 3 ? 'big' : 'small'".
/// @param where Its place in the document, for errors.
/// @return The operator tree.
/// @throws DocumentError If it isn't a valid expression (the message gives the column).
[[nodiscard]] json parse(std::string_view text, const std::string& where);

/// @brief Reads a whole number written in digits: a literal, or an index in a path (a '-' is an operator).
///
/// Unlike std::stoll, it doesn't throw std::out_of_range, which the callers of evaluate() would let through.
/// @param digits The digits.
/// @return The number, or std::nullopt if it doesn't fit 64 bits.
[[nodiscard]] std::optional<std::int64_t> wholeNumber(std::string_view digits) noexcept;

} // namespace affresco::format::detail
