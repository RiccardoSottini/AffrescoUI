#pragma once

/// @file
/// @brief The runtime's ProgramConsole (affresco/core/ProgramConsole.hpp): the screen's program, as an editor
/// connected to the app sees it (Runtime::console()).

#include "affresco/core/ProgramConsole.hpp"

#include <optional>
#include <string>

namespace affresco::format {

struct Engine;

/// @brief The screen's program, for the editor server: evaluating as the person, and looking names up.
///
/// Invariants:
/// - evaluate() is the one way an editor changes the program: as the person, through Programs::evaluateConfirmed()
///   or Programs::evaluateFile(), so every rule that holds for the person's own input holds (deny rules, protected
///   sections, a text keeps its first author). Only the host calls it, for a connection the person paired; nothing a
///   document, a step, a peer or a primitive runs reaches it.
/// - It refuses what the step `evaluate` refuses: no program, a proposal previewed, an agent's reply being decided.
/// - It refuses a definition named like a control or a review step (`accept`, `call`...), as an agent's revision is
///   refused one: its text comes from an editor's buffer, which someone else may have written, not from keystrokes
///   the app saw.
/// - names() and describe() read: they run nothing and change nothing.
///
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
	/// @brief The file a new definition goes in: the request's, if the program has it; else the namespace's, if the
	/// program has it; else Programs::defaultFile. A definition some file holds stays in it whatever this says.
	/// @param request The request.
	/// @return The file.
	[[nodiscard]] std::string fileFor(const ConsoleRequest& request) const;
	/// @brief What is known of a name, as the console gives it.
	/// @param name The name.
	/// @param callable Whether code can call or read it (Interpreter::callableNames()).
	/// @return It; std::nullopt if it is neither a definition, an action nor callable.
	[[nodiscard]] std::optional<ConsoleName> nameOf(const std::string& name, bool callable) const;

	Engine& engine_; ///< The runtime.
};

} // namespace affresco::format
