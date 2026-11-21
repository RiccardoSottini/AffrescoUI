#pragma once

/// @file
/// @brief The typed `arguments` of a command: what a command may declare, and a run's arguments checked against it,
/// defaults filled in.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::format {

/// How many arguments a command may declare.
inline constexpr std::size_t maxCommandArguments = 32;
/// How long an argument's name may be, in bytes.
inline constexpr std::size_t maxArgumentNameBytes = 64;

/// @brief Tells whether a text may name an argument: 1 to 64 letters, digits, '_' and '-'.
/// @param name The text.
/// @return true if so.
[[nodiscard]] bool isArgumentName(std::string_view name) noexcept;

/// @brief Checks what a command declares as its arguments: a list of names (any value), or an object of types, one per
/// argument (the state's types: StateTypes.hpp), at most 32, each named as isArgumentName() says, each default
/// matching its type.
/// @param arguments The command's "arguments".
/// @param place Their place: "actions.<name>.arguments".
/// @throws DocumentError If they are malformed.
void checkArgumentDeclarations(const json& arguments, const std::string& place);

/// @brief Checks the arguments of every command of a document: checkArgumentDeclarations(), and for a command whose
/// `do` runs a function of the document's program, that each argument it types is one of the function's parameters.
/// @param document The document, built: with its program.
/// @throws DocumentError If one is wrong.
void checkCommandArgumentsOf(const json& document);

/// @brief What a command's arguments are, as an object of types: a list of names gives each the type that allows
/// anything (`{}`), and none gives none.
/// @param declared The command's "arguments", or null.
/// @return The object.
[[nodiscard]] json argumentTypes(const json& declared);

/// @brief Checks a run's arguments against what a command declares (an object of types, as argumentTypes() gives it),
/// and fills in what's missing: its default, else null when its type allows null. Messages name the command and the
/// argument and never quote a value sent: "move-card: arguments must be an object, like {"card": "…"}", "move-card:
/// no argument 'colum' (it takes card, to)", "move-card: to: expected one of todo, doing, done", "move-card: card is
/// missing".
/// @param command The command, as CommandCatalog::list() gives it.
/// @param arguments The arguments, changed in place.
/// @return Why they are refused, or std::nullopt.
std::optional<std::string> checkCommandArguments(const json& command, json& arguments);

} // namespace affresco::format
