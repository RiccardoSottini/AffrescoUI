#pragma once

/// @file
/// @brief Literal text: the text a person reads that a document writes in place instead of naming a key of "strings"
/// (SCREEN_FORMAT.md, "Strings and locales"), for `affrescoui strings`.

#include "affresco/core/Json.hpp"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief What says which values a person reads: the marks of the catalog, and how components pass them on.
struct TextVocabulary {
	std::map<std::string, std::set<std::string, std::less<>>, std::less<>> types;      ///< Each type's own properties.
	std::map<std::string, std::string, std::less<>> componentTypes;                    ///< Each component's node type.
	std::map<std::string, std::set<std::string, std::less<>>, std::less<>> parameters; ///< Each component's text.

	/// @brief Whether a person reads a property.
	/// @param type The node's type.
	/// @param property The property.
	/// @return Whether they do: it is the type's, or every node's ("panel").
	[[nodiscard]] bool shows(std::string_view type, std::string_view property) const;
};

/// @brief Reads the vocabulary of a screen.
/// @param catalog The screen's catalog (Screen::catalogJson()), whose types say what they show.
/// @param document The screen, its includes merged, whose components pass text to what they show.
/// @return The vocabulary. A component's parameter is text if it reaches a property a person reads.
[[nodiscard]] TextVocabulary textVocabulary(const json& catalog, const json& document);

/// @brief A text found written in place.
struct LiteralText {
	std::string pointer; ///< Where, as a JSON Pointer into the file's document.
	std::string key;     ///< The key it would get, from where it is rather than what it says, so it survives rewording.
	std::string text;    ///< What it says.
	bool built = false;  ///< It is text around an expression ("Saved {{name}}"), which a person has to rewrite.
};

/// @brief Finds the literal text of one file: its nodes, commands, menus, settings and window. Ids, code, the state and
/// the steps' own values are left alone, as are texts without a letter and those "strings.keep" lists.
/// @param document The file's document, before its includes are merged.
/// @param vocabulary What says which values a person reads.
/// @param keep The texts to leave, from the merged document's "strings.keep".
/// @return What it found, in document order.
[[nodiscard]] std::vector<LiteralText> literalTexts(const json& document, const TextVocabulary& vocabulary,
                                                    const json& keep);

/// @brief The message that shows a text as written: quotes and braces are quoted, as messages require.
/// @param text The text.
/// @return The message.
[[nodiscard]] std::string messageOfText(std::string_view text);

} // namespace affresco::format
