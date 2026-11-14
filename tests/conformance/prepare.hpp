/// @file
/// @brief The conformance run's token pass (README.md, "Preparing the text"): splits a test file of the Clojure suite
/// into its top-level forms, puts `(%out "<reason>")` in place of each literal or call the language has no value for,
/// writes each assertion's line into it, and reads the file's `ns` form for its aliases. Strings and comments are left
/// alone. It reads untrusted text: every count is bounded by the text's length, and nesting by a fixed depth.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::conformance {

/// @brief What the pass needs to know of the language it prepares for.
struct Options {
	bool markOut = true; ///< Whether forms out of scope are marked: the everyday probes are run as written.
};

/// @brief A top-level form of a prepared file.
struct TopForm {
	std::string text;          ///< Its prepared text.
	std::size_t line = 1;      ///< The line it starts on, from 1.
	std::string head;          ///< Its first word, without a namespace (`deftest`, `when-var-exists`), or "".
	std::string name;          ///< The word after the head (a test's name, a function's), or "".
	std::vector<TopForm> body; ///< For `when-var-exists`: the forms of its body, each prepared alone.
	std::size_t assertions =
	    0; ///< The assertions written in it (an `is`, or one row of an `are`), as the language reads it.
};

/// @brief A library the file's `ns` form requires, and the alias it gives it.
struct Requirement {
	std::string library; ///< `clojure.string`, `clojure.core-test.portability`.
	std::string alias;   ///< `str`, `p`, or "".
};

/// @brief A test file, prepared.
struct PreparedFile {
	std::vector<TopForm> forms;            ///< Its top-level forms but `ns`, in order.
	std::vector<Requirement> requirements; ///< What its `ns` form requires.
	std::vector<std::string> tests;        ///< The names of its `deftest`s, in order.
	std::string tested;                    ///< The function it tests (its first `when-var-exists`), or "".
	std::size_t assertions = 0;            ///< The assertions written in it.
	std::string error;                     ///< Why its text couldn't be split into forms, or "".
};

/// @brief The reasons an assertion is out of scope (README.md and modules/lisp/reference/differences.md), in order.
/// @return Them.
[[nodiscard]] const std::vector<std::string_view>& outReasons();

/// @brief Prepares a test file's text.
/// @param text The text.
/// @param options What the language reads.
/// @return The file, prepared; PreparedFile::error says why when the text doesn't split (an unclosed form).
[[nodiscard]] PreparedFile prepare(std::string_view text, const Options& options);

} // namespace affresco::conformance
