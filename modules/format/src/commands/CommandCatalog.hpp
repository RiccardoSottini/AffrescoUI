#pragma once

/// @file
/// @brief format::CommandCatalog: main's commands, the entries of `actions` that describe themselves, as tools.

#include "affresco/core/Commands.hpp"
#include "affresco/core/Json.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::format {

struct Engine;

/// @brief The commands of the document a runtime shows: an action with a title, arguments and a category (a Lisp
/// `defcommand`, or a JSON action written so); plain actions are never listed.
///
/// Callers: Runtime::commandList(), command(), commandReach(), checkCommandArguments(), and CommandRuns. Main thread
/// only.
class CommandCatalog {
public:
	/// @brief A catalog for an engine.
	/// @param engine The runtime; outlives this object.
	explicit CommandCatalog(Engine& engine) noexcept : engine_(engine) {}

	CommandCatalog(const CommandCatalog&) = delete;
	CommandCatalog& operator=(const CommandCatalog&) = delete;
	CommandCatalog(CommandCatalog&&) = delete;
	CommandCatalog& operator=(CommandCatalog&&) = delete;
	~CommandCatalog() = default;

	/// How many commands the catalog lists.
	static constexpr std::size_t maxCommands = 1024;
	/// How long a title may be, in bytes; a longer one is cut at a character.
	static constexpr std::size_t maxTitleBytes = 128;
	/// How long a description may be, in bytes; a longer one is cut at a character.
	static constexpr std::size_t maxDescriptionBytes = 512;

	/// @brief The commands, in the order of the `commands` variable (by category, then title), at most maxCommands.
	/// @return `[{name, title, category, description, arguments, enabled, key?}]`: titles and descriptions translated
	/// and cut, `arguments` always an object of types (a name the command doesn't type takes `{}`).
	[[nodiscard]] json list() const;
	/// @brief One command.
	/// @param name Its name.
	/// @return It, as in list(), or std::nullopt.
	[[nodiscard]] std::optional<json> find(std::string_view name) const;
	/// @brief What a command can reach, from its steps and the actions they run (CommandReach.hpp), kept until main's
	/// head, the proposal previewed or the services change.
	/// @param name Its name.
	/// @return What it reaches; for no such command, a control reach.
	[[nodiscard]] CommandReach reach(std::string_view name) const;
	/// @brief Checks a command's arguments against what it declares, and fills in their defaults
	/// (CommandArguments.hpp).
	/// @param name Its name.
	/// @param arguments The arguments: an object, changed in place.
	/// @return Why they are refused, or std::nullopt.
	std::optional<std::string> checkArguments(std::string_view name, json& arguments) const;

private:
	Engine& engine_; ///< The runtime.
	/// What reach() found, by command, for the head, preview and services in reachKey_.
	mutable std::map<std::string, CommandReach, std::less<>> reaches_;
	mutable std::string reachKey_; ///< What reaches_ was found for.
};

} // namespace affresco::format
