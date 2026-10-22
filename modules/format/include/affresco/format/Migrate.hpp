#pragma once

/// @file
/// @brief Format versions: the one this library writes, and the migration of version 1 documents to it
/// (SCREEN_FORMAT.md, "Migrating from version 1"; COMPATIBILITY.md).
///
/// Version 2 keeps one form of each thing that version 1 wrote two ways: `justify` on rows, columns and stacks (not
/// `from`, nor `align` on a stack), `length()` in expressions (not `len()`), and a feature's `requires.format` of 2.
/// The same rewrites apply to every kind of document that holds screen content: screens and their include parts,
/// scenario files, change sets, feature manifests and catalogue indexes. Each is idempotent: a version 2 document
/// is left as it is.

#include "affresco/core/Json.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// The format version this library writes and the newest it reads; it also reads version 1, by migrating it.
inline constexpr int formatVersion = 2;

/// @brief What a document is, by its "format".
enum class DocumentKind {
	Screen,    ///< "affrescoui-screen", or a screen's include part (it has "root", "screens" or other sections).
	Scenarios, ///< "affrescoui-scenarios".
	Changes,   ///< "affrescoui-changes".
	Feature,   ///< "affrescoui-feature".
	Catalog,   ///< "affrescoui-catalog".
	Other      ///< Anything else: migrate() leaves it.
};

/// @brief One change a migration made, at one place of the document as it was.
struct Rewrite {
	/// @brief What it did there.
	enum class Kind {
		Rename,  ///< The member at `pointer` now has the key `key`.
		Replace, ///< The value at `pointer` is now `value`.
		Remove   ///< The member at `pointer` is gone (its new form was already there).
	};

	Kind kind = Kind::Replace; ///< What it did.
	std::string pointer;       ///< Where, as a JSON Pointer into the document before the migration.
	std::string key;           ///< Rename: the new key.
	json value;                ///< Replace: the new value.
	std::string message;       ///< One line for people: "root.children[2]: \"from\" is \"justify\" in version 2".
};

/// @brief Tells what a document is.
/// @param document The document.
/// @return Its kind; DocumentKind::Other if it isn't an object, or has a "format" this library doesn't migrate.
[[nodiscard]] DocumentKind documentKind(const json& document) noexcept;

/// @brief Tells the version a document declares.
/// @param document The document.
/// @return Its "version", or 0 if it has none (an include part may leave it out).
[[nodiscard]] int documentVersion(const json& document) noexcept;

/// @brief Migrates a version 1 document to version 2, in place: renames, rewrites expressions, then sets "version".
///
/// A document of version 2 (or one without a "version" that has nothing to rewrite) is left as it is; doing it twice
/// changes nothing the second time. Ids, key order and every other value are kept. A signed document's signature no
/// longer matches afterwards: readers migrate those after checking the signature, and `affrescoui migrate` leaves
/// their files as they are.
/// @param document The document; DocumentKind::Other is left alone.
/// @return What changed, in document order; empty if nothing did.
[[nodiscard]] std::vector<Rewrite> migrate(json& document);

/// @brief Migrates a revision's patch written for version 1 (in a version 1 change set or scenario), in place:
/// paths that end in a stack's "from" go to "justify", and the values it adds or replaces are migrated as
/// migrate() migrates a document's content.
/// @param patch The patch: a list of JSON Patch operations (anything else is left).
/// @param pointer Where the patch is in its file, for the rewrites' pointers ("" for a patch on its own).
/// @return What changed.
[[nodiscard]] std::vector<Rewrite> migratePatch(json& patch, const std::string& pointer = "");

/// @brief The hint for a key version 2 removed, for the error that refuses it.
/// @param type The node's type ("row"; "" if it isn't known).
/// @param key The key.
/// @return "'from' was removed in version 2: use 'justify'", or "" if version 2 didn't remove it.
[[nodiscard]] std::string removedKeyHint(std::string_view type, std::string_view key);

/// @brief The hint for an expression function version 2 removed.
/// @param name The function's name.
/// @return "'len' was removed in version 2: use 'length'", or "" if version 2 didn't remove it.
[[nodiscard]] std::string removedFunctionHint(std::string_view name);

} // namespace affresco::format
