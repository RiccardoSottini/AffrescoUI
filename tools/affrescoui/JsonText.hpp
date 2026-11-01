#pragma once

/// @file
/// @brief JSON as written: a tree that keeps key order, the text of every number and string, and the layout choices
/// a person makes, so that `affrescoui fmt` and `affrescoui ids` change only what they must.

#include <string>
#include <string_view>
#include <vector>

namespace affresco::cli {

/// @brief A JSON value as written in a file.
struct TextValue {
	/// @brief What the value is.
	enum class Kind {
		Object, ///< `{ ... }`: members with keys.
		Array,  ///< `[ ... ]`: members without keys.
		Scalar  ///< A string, number, true, false or null: its text.
	};

	struct Member;

	Kind kind = Kind::Scalar;    ///< What it is.
	std::string text;            ///< A scalar's text as written ("1.50", "\"caf\\u00e9\"").
	std::vector<Member> members; ///< An object's members or an array's items, in order.
	bool expanded = false;       ///< An object written with a line break after its '{': it stays on several lines.
	std::size_t start = 0;       ///< Where it starts in the text it was read from: an object's '{', a string's quote.
	std::size_t end = 0;         ///< Where it ends there: just after its last character.
	std::string lead;            ///< The spaces and line breaks between an object's '{' and its first key.
};

/// @brief A member of an object or an item of an array.
struct TextValue::Member {
	std::string key;          ///< An object member's key as written, quotes included; empty for an array item.
	TextValue value;          ///< Its value.
	bool blankBefore = false; ///< An empty line comes before it (kept: it separates groups).
	std::size_t start = 0;    ///< Where it starts in the text it was read from: its key's quote, or an item's value.
};

/// @brief Reads JSON text into a tree that keeps how it was written.
/// @param text The text (UTF-8).
/// @return The value.
/// @throws std::runtime_error If it isn't JSON; what() says where ("line 3, column 8: expected ':'").
[[nodiscard]] TextValue parseText(std::string_view text);

/// @brief Writes a tree as the AffrescoUI files are written: tabs, one member per line for an expanded object,
/// `{ "key": value }` and `[a, b]` on one line when they fit in 120 columns (a tab counting 4) and hold no expanded
/// object, empty lines kept, a line break at the end.
///
/// The same tree always gives the same text, and the text read again gives the same tree, so formatting twice
/// changes nothing.
/// @param value The value.
/// @return The text.
[[nodiscard]] std::string printText(const TextValue& value);

/// @brief Finds the member of an object with a key.
/// @param object The object.
/// @param key The key, unquoted.
/// @return The member's value, or nullptr.
[[nodiscard]] TextValue* memberOf(TextValue& object, std::string_view key);

/// @brief Finds the value at a JSON Pointer.
/// @param root The tree.
/// @param pointer The pointer ("" is the root).
/// @return The value, or nullptr.
[[nodiscard]] TextValue* valueAt(TextValue& root, std::string_view pointer);

} // namespace affresco::cli
