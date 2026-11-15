#pragma once

/// @file
/// @brief The ProgramConsole interface: evaluating Lisp into a screen's running program, and looking its names up,
/// for an editor connected to the app (an nREPL client such as CIDER, Calva or Conjure).

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief A text to evaluate in the program, and where it comes from.
struct ConsoleRequest {
	std::string text;       ///< The forms, as the editor sent them.
	std::string file;       ///< One of the program's files, where a new definition lands; "" for none.
	std::string space;      ///< The editor session's namespace name; "" for the program's own.
	std::string origin;     ///< A word for the commit's message (e.g. "editor"); "" for none.
	std::size_t line = 1;   ///< The line @p text starts on in the editor's buffer, from 1.
	std::size_t column = 1; ///< The column @p text starts on in the editor's buffer, from 1.
	bool wholeFile = false; ///< @p text is the whole of @p file (an editor's "load file"), not a few forms of it.
};

/// @brief What evaluating a text gave.
struct ConsoleResult {
	std::vector<std::string> values;  ///< One text per form, in the text's order: a definition's name, any other
	                                  ///< form's value printed as Lisp.
	std::optional<std::string> error; ///< What went wrong (names and kinds, never a value of the state), or nothing.
	std::size_t line = 0;             ///< Where the error is, in the editor's buffer; 0 when it has no place.
	std::size_t column = 0;           ///< Where the error is, in the editor's buffer; 0 when it has no place.
	std::optional<int> proposal;      ///< The proposal the text became, when it wasn't applied as it is.
};

/// @brief What is known of a name the program can call or read, for completion and documentation.
struct ConsoleName {
	std::string name;                   ///< The name.
	std::string kind;                   ///< `defn`, `defcommand`, `macro`, `function`, `special form`...
	std::string doc;                    ///< Its docstring, or "".
	std::string file;                   ///< The program's file it is defined in, or "" for the language's own.
	std::size_t line = 0;               ///< The line it is defined on in @p file, from 1; 0 when it has no place.
	std::vector<std::string> arguments; ///< Its argument lists as written (e.g. "[text]"), or none.
};

/// @brief A screen's running program, as an editor connected to the app sees it.
///
/// Implemented by the format module's runtime (format::Runtime::console()); used by the editor server in the net
/// module, which can't depend on the format module. Main thread only. A runtime without a program answers
/// hasProgram() false, and nothing else.
class ProgramConsole {
public:
	virtual ~ProgramConsole() = default;

	/// @brief Tells whether the screen has a program to evaluate into. @return true if it has.
	[[nodiscard]] virtual bool hasProgram() const = 0;

	/// @brief Evaluates a text as the person: only the host's own code calls it, after the person said yes. It applies
	/// as their change, under every rule that holds for them, never more.
	/// @param request The text and where it comes from.
	/// @return What it gave.
	[[nodiscard]] virtual ConsoleResult evaluate(const ConsoleRequest& request) = 0;

	/// @brief The names code can call or read now that start with a prefix, for completion.
	/// @param prefix The start of the names; "" for every name.
	/// @param most The most names to give.
	/// @return Them, sorted by name.
	[[nodiscard]] virtual std::vector<ConsoleName> names(std::string_view prefix, std::size_t most) = 0;

	/// @brief What is known of one name, for documentation and "go to definition".
	/// @param name The name.
	/// @return It, or std::nullopt if code can't call or read it.
	[[nodiscard]] virtual std::optional<ConsoleName> describe(std::string_view name) = 0;

	/// @brief The program's files. @return Their names, as the program writes them.
	[[nodiscard]] virtual std::vector<std::string> files() const = 0;

	/// @brief Tells why a text can't be shown to the person as it is, before they are asked about it: a character
	/// that draws nothing, for instance, would let a question show less than what runs.
	/// @param text The text.
	/// @return Why, or std::nullopt if it can be shown as it is.
	[[nodiscard]] virtual std::optional<std::string> unshowable(std::string_view text) const = 0;
};

} // namespace affresco
