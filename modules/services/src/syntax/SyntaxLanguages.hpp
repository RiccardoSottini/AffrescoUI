#pragma once

/// @file
/// @brief The syntax engine's languages: each grammar, its names and file extensions, and its queries, compiled once
/// and shared by every document in that language (they are only read after).

#include "affresco/core/TextPattern.hpp"

#include <tree_sitter/api.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services::syntax {

/// @brief A predicate of a query pattern (e.g. `(#match? @constant "^[A-Z]")`).
struct Predicate {
	/// @brief What it checks.
	enum class Kind { Match, Eq, AnyOf };
	Kind kind = Kind::Match;        ///< See Kind.
	bool negated = false;           ///< The `not-` form.
	std::uint32_t capture = 0;      ///< The capture it reads.
	std::int64_t otherCapture = -1; ///< For #eq? against a capture: that capture; otherwise -1.
	std::vector<std::string> words; ///< The strings it compares with (one for #eq?).
	TextPattern pattern;            ///< For #match?: the expression.
};

/// @brief A compiled query and the predicates of each of its patterns.
struct Query {
	/// @brief Frees the query.
	~Query();
	Query() = default;
	Query(const Query&) = delete;
	Query& operator=(const Query&) = delete;

	TSQuery* query = nullptr;                   ///< The query, or nullptr if it has none (or didn't compile).
	std::vector<std::vector<Predicate>> checks; ///< Per pattern, the predicates it must satisfy.
	std::string error;                          ///< Why it didn't compile, if it didn't.
};

/// @brief One language.
struct Grammar {
	std::string name;                     ///< Its name (e.g. "cpp").
	const TSLanguage* language = nullptr; ///< Its grammar.
	Query highlights;                     ///< Captures named for styles.
	Query folds;                          ///< `@fold` captures.
	Query brackets;                       ///< `@open` and `@close` captures, one pair per match.
	std::vector<std::string> styles;      ///< The highlight capture names, by capture id.
	std::vector<bool> plain;              ///< By capture id: a plain `@variable` (painted under any other).
};

/// @brief The languages' names, in a fixed order.
[[nodiscard]] const std::vector<std::string>& names();
/// @brief A language's name from a name or alias.
/// @param name The name, any case.
/// @return Its name in names(), or "" if none.
[[nodiscard]] std::string canonical(std::string_view name);
/// @brief The language a file's extension suggests.
/// @param fileName The name, or a path.
/// @return Its name in names(), or "".
[[nodiscard]] std::string forFile(std::string_view fileName);
/// @brief A language's grammar and queries, compiled at the first call.
/// @param name A name from names().
/// @return It, or nullptr if no grammar has that name.
[[nodiscard]] const Grammar* grammar(std::string_view name);

} // namespace affresco::services::syntax
