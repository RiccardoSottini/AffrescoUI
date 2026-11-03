#pragma once

/// @file
/// @brief A document that holds a program (RFC 0005, stage 2): reading a file into a document, the tree its `program`
/// section builds beside its other sections, and the edit of one keyword's value inside an entry's text.

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Program.hpp>

#include <cstddef>
#include <filesystem>
#include <map>
#include <set>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::lisp {

inline constexpr std::size_t maxEntryBytes = std::size_t{1} << 20U;  ///< Longest text of one definition: 1 MiB.
inline constexpr std::size_t maxEntryNameBytes = 1024;               ///< Longest address of a file or a definition.
inline constexpr std::size_t maxProgramEntries = 16384;              ///< Most definitions in a document.
inline constexpr std::size_t maxBuildOperations = 2'000'000;         ///< What building a document may spend.
inline constexpr std::size_t maxBuildBytes = std::size_t{16} << 20U; ///< What building a document may allocate.

/// @brief What a document's program built, kept per program so that judging a revision doesn't build twice.
struct BuiltProgram {
	json tree;                            ///< The tree: `format` and `version`, `root`, and the sections it defines.
	std::vector<UndefinedName> undefined; ///< What its definitions use that nothing defines.
	std::vector<Extension> extensions;    ///< Its hooks and advice.
	std::map<std::string, std::vector<std::string>>
	    namesUsed;         ///< What each definition's code uses (Program::namesUsed()).
	NamedCalls namedCalls; ///< What each calls with a text written out first (Program::namedCalls()).
	std::set<std::pair<std::string, std::string>> defaulted; ///< Members a `defdefaults` gave (Program::defaulted()).
};

/// @brief Tells whether a document holds a program.
/// @param document The document.
/// @return true if it has a `program` section.
[[nodiscard]] bool hasProgram(const json& document);

/// @brief Checks a `program` section's shape and limits; not what its text means, which is the build's to say.
/// @param section The section.
/// @throws DocumentError If it isn't `{file: {name: text}}`, or is too large.
void checkProgramShape(const json& section);

/// @brief A program's file as a document that holds it: `{"format", "version", "program": {file: {name: text}}}`.
/// @param text The file's text.
/// @param file What the program calls it.
/// @return The document.
/// @throws LispError If it can't be read, a form names no definition, or a name is taken.
[[nodiscard]] json sourceDocument(std::string_view text, const std::string& file);

/// @brief Reads a document's file in whatever it is written in: a JSON file as parseUntrusted() reads it; a program
/// (isProgram()) as sourceDocument() gives it; a data file with `~` as buildText() builds it; any other data file as
/// the notation's reader reads it. The one function a loader of documents on disk goes through.
/// @param text The file's text.
/// @param file The file.
/// @param name What a program calls this file; "" for its file name.
/// @return The value.
/// @throws json::parse_error If JSON text isn't JSON.
/// @throws DocumentError If it can't be read: a NotationError or a LispError names the place.
[[nodiscard]] json readDocument(std::string_view text, const std::filesystem::path& file, const std::string& name = "");

/// @brief readDocument() as JSON text, for the readers that take JSON text (Screen::fromString()): JSON text as it is.
/// @param text The file's text.
/// @param file The file.
/// @return The JSON text.
/// @throws DocumentError If it can't be read.
[[nodiscard]] std::string jsonText(std::string_view text, const std::filesystem::path& file);

/// @brief A document's `program` section as a Program, ready to Program::load() into a live interpreter.
/// @param section The section: `{file: {name: text}}`.
/// @param authors Who wrote each: `{file: {name: author}}` (VersionControl::programAuthors()); an entry not there has
/// no author.
/// @return The program.
/// @throws LispError If an entry isn't one form with its address, a name is taken, or the section is too large.
/// @throws DocumentError If it isn't a section of that shape.
[[nodiscard]] Program programOf(const json& section, const json& authors = json());

/// @brief What a `program` section builds, under the limits above (a build is the cost of a revision, so it is bounded
/// tighter than running). The same section gives the same result, kept for a few sections on this thread.
/// @param section The section.
/// @return What it built.
/// @throws LispError If it can't be built or a limit runs out, naming the definition.
[[nodiscard]] std::shared_ptr<const BuiltProgram> builtProgram(const json& section);

/// @brief A document as everything but version control reads it: what its program builds and its other sections
/// together, with its `program` section kept. A member both give must be the same; so the result, read as a document
/// again, gives itself.
/// @param source The document as versioned. It has a `program` section.
/// @return The tree.
/// @throws LispError If it can't be built, a section can't be built by a program (`include`, `assets`, `features`,
/// `program`), or a member is both built and written differently, naming it.
[[nodiscard]] json buildDocument(const json& source);

/// @brief The text of an entry with one keyword's value replaced, and every other byte as it was.
///
/// The keywords are those of a definition form's own options: after the name and the docstring of `defcommand` and
/// `defscreen`, after the value and the docstring of `defcustom`. Others have none. A further keyword looks inside the
/// map that is the previous one's value (`:window`, `:title`).
/// @param text The entry's text.
/// @param keywords The keywords, without their ':'; at least one.
/// @param value The new value's text, exactly one form with nothing around it.
/// @param where The entry's address, for errors.
/// @return The new text: one form with the same address.
/// @throws DocumentError If it has no such keyword, has it twice, or the value isn't one form.
[[nodiscard]] std::string replaceKeyword(std::string_view text, std::span<const std::string> keywords,
                                         std::string_view value, const std::string& where);

/// @brief Where a keyword's value is in a text of Lisp, read by tokens, with nothing run: in the first top-level form
/// that has it, a map or the options of `defcommand`, `defscreen` or `defcustom` (as replaceKeyword() reads them), a
/// further keyword looking inside the map that is the previous one's value (`:window`, `:title`).
/// @param text The text: a data file or a program.
/// @param keywords The keywords, without their ':'.
/// @return Where the value starts and ends in the text, or std::nullopt if no form has it.
/// @throws LispError If the text doesn't read as forms, or a form has a keyword twice.
[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> findKeyword(std::string_view text,
                                                                             std::span<const std::string> keywords);

/// @brief A text of Lisp with one keyword's value replaced (findKeyword() finds it), and every other byte as it was.
/// @param text The text: a data file or a program.
/// @param keywords The keywords, without their ':'.
/// @param value The new value's text, exactly one form with nothing around it.
/// @return The new text, or std::nullopt if no form has that keyword.
/// @throws LispError If the text doesn't read as forms, a form has a keyword twice, or the value isn't one form.
[[nodiscard]] std::optional<std::string> withKeyword(std::string_view text, std::span<const std::string> keywords,
                                                     std::string_view value);

/// @brief The form of a value for replaceKeyword(): a string as a string, `{"$form": "text"}` as that text, any other
/// value as the notation writes it.
/// @param value The value, from a revision.
/// @return The text.
/// @throws DocumentError If it can't be written.
[[nodiscard]] std::string keywordValueText(const json& value);

} // namespace affresco::lisp
