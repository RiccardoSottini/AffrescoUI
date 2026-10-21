#pragma once

/// @file
/// @brief checkSchema(): the small part of JSON Schema that providers use for configurations and method arguments.

#include "affresco/core/Json.hpp"

#include <optional>
#include <string>

namespace affresco::services {

/// @brief Checks a value against a schema. Understood: `type` (a name or a list of names: object, array, string,
/// number, integer, boolean, null), `enum`, `properties`, `required`, `additionalProperties` (false or a schema),
/// `items`, `maxItems`, `minLength`, `maxLength`, `minimum` and `maximum`. Other keywords are ignored.
/// @param schema The schema (trusted: a provider's); null or {} takes anything.
/// @param value The value (untrusted).
/// @param path The value's place, for the message, e.g. "with".
/// @return What is wrong, naming the place ("with.path: must be a string"), or std::nullopt.
[[nodiscard]] std::optional<std::string> checkSchema(const json& schema, const json& value, const std::string& path);

} // namespace affresco::services
