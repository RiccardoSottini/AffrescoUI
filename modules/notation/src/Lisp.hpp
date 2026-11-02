#pragma once

/// @file
/// @brief The Lisp notation's reader and writer (README.md), which Notation.cpp offers through read(), write() and
/// format().

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::notation::detail {

struct CommentTree;

/// @brief The comments around one member of a list, an object or a node.
struct MemberComments {
	std::vector<std::string> leading;   ///< On lines of their own before it.
	std::string trailing;               ///< At the end of its line, or empty.
	std::unique_ptr<CommentTree> inner; ///< Those inside its value, if any.
};

/// @brief The comments inside a list, an object or a node, by member: an index in the list, a key's place in the
/// object. A node's children are the members of the `children` member's own tree.
struct CommentTree {
	std::map<std::size_t, MemberComments> members; ///< By member.
	std::vector<std::string> closing;              ///< On lines of their own before the closing bracket.

	/// @brief Whether it holds nothing. @return true if so.
	[[nodiscard]] bool empty() const noexcept { return members.empty() && closing.empty(); }
};

/// @brief The comments of a whole text.
struct DocumentComments {
	MemberComments root;            ///< Those before the value, at the end of its last line, and inside it.
	std::vector<std::string> after; ///< On lines of their own after it.
};

/// @brief Reads a value written in the Lisp notation.
/// @param text The text.
/// @return The value, its keys in the order written.
/// @throws NotationError If the text isn't one value, nests deeper than maxNesting or is longer than maxLispBytes.
[[nodiscard]] json readLisp(std::string_view text);

/// @brief Reads a value written in the Lisp notation, and its comments and discarded forms.
/// @param text The text.
/// @param comments Gets its comments.
/// @return The value, its keys in the order written.
/// @throws NotationError As readLisp() throws it.
[[nodiscard]] json readLisp(std::string_view text, DocumentComments& comments);

/// @brief Writes a value in the Lisp notation, canonically: lines of at most 120 columns where the values allow it,
/// a node's children one per line, a line break at the end.
/// @param value The value, at most maxNesting deep.
/// @param comments Its comments, or nullptr: a container that holds some is written over several lines.
/// @return The text.
[[nodiscard]] std::string writeLisp(const json& value, const DocumentComments* comments = nullptr);

} // namespace affresco::notation::detail
