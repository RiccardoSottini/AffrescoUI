#pragma once

/// @file
/// @brief The runtime's ProgramConsole (affresco/core/ProgramConsole.hpp): the screen's program, as an editor
/// connected to the app sees it (Runtime::console()).

#include "affresco/core/ProgramConsole.hpp"

namespace affresco::format {

struct Engine;

/// @brief The screen's program, for the editor server: evaluating as the person, and looking names up.
///
/// A stub until the console's package (f2d) fills it: it tells whether there is a program, and does nothing else.
/// Owned by the runtime, after its engine (destroyed before it). Main thread only.
class RuntimeConsole final : public ProgramConsole {
public:
	/// @brief The console of an engine's program.
	/// @param engine The runtime; outlives this object.
	explicit RuntimeConsole(Engine& engine) noexcept : engine_(engine) {}

	/// @brief Tells whether the document has a program. @return true if it has.
	[[nodiscard]] bool hasProgram() const override;
	/// @brief Evaluates a text as the person. @param request The text. @return What it gave.
	[[nodiscard]] ConsoleResult evaluate(const ConsoleRequest& request) override;
	/// @brief The names starting with a prefix. @param prefix It. @param most The most to give. @return Them.
	[[nodiscard]] std::vector<ConsoleName> names(std::string_view prefix, std::size_t most) override;
	/// @brief What is known of a name. @param name It. @return It, or std::nullopt.
	[[nodiscard]] std::optional<ConsoleName> describe(std::string_view name) override;
	/// @brief The program's files. @return Their names.
	[[nodiscard]] std::vector<std::string> files() const override;
	/// @brief Why a text can't be shown as it is. @param text It. @return Why, or std::nullopt.
	[[nodiscard]] std::optional<std::string> unshowable(std::string_view text) const override;

private:
	Engine& engine_; ///< The runtime.
};

} // namespace affresco::format
