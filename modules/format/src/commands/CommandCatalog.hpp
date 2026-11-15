#pragma once

/// @file
/// @brief format::CommandCatalog: main's commands, the entries of `actions` that describe themselves, as tools.

#include "affresco/core/Commands.hpp"
#include "affresco/core/Json.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace affresco::format {

struct Engine;

/// @brief The commands of the document a runtime shows: an action with a title, arguments and a category (a Lisp
/// `defcommand`, or a JSON action written so); plain actions are never listed.
///
/// Not built yet (f4c fills it in): it lists no command. Callers: Runtime::commandList(), command(), commandReach(),
/// checkCommandArguments(), and CommandRuns. Main thread only.
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

	/// @brief The commands, in the order the document writes them.
	/// @return `[{name, title, category, description, arguments, enabled, key?}]`; not built yet: empty.
	[[nodiscard]] json list() const;
	/// @brief One command.
	/// @param name Its name.
	/// @return It, as in list(), or std::nullopt; not built yet: std::nullopt.
	[[nodiscard]] std::optional<json> find(std::string_view name) const;
	/// @brief What a command can reach, from its steps and the actions they run (CommandReach.hpp).
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
	[[maybe_unused]] Engine& engine_; ///< The runtime (f4c reads its document and program).
};

} // namespace affresco::format
