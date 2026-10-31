/// @file
/// @brief Which file each entry of a merged screen came from: includes are merged when a screen loads and revisions
/// patch the result, so this is the only record of what a change means in terms of files (Screen::fileChanges()).

#pragma once

#include "affresco/core/Json.hpp"

#include <map>
#include <string>
#include <string_view>

namespace affresco::detail {

/// @brief Entries of a merged screen by JSON Pointer, each with the file that holds it. An entry is what merging
/// handles as one: a section's member, a member of a member for "menus" and "strings", or a section that isn't an
/// object. A deeper path belongs to its nearest entry. It is kept beside the document, never in it, so it changes
/// neither the document nor its hashes.
class SourceMap {
public:
	/// @brief Claims every entry of a file's own document. Entries already claimed stay: a screen's entries win over
	/// what it includes, and an earlier include over a later one, as merging decides.
	/// @param document The file's document, before its includes are merged.
	/// @param file The file: its absolute path, or empty for the screen itself.
	void claimDocument(const json& document, const std::string& file);

	/// @brief Claims what an included file's own map holds; entries already claimed stay.
	/// @param part The included file's map, its own includes in it.
	void absorb(const SourceMap& part);

	/// @brief Says a path is held by a file whatever claimed it before: a seed file's records, given to a model.
	/// @param pointer The path.
	/// @param file The file.
	void assign(std::string pointer, std::string file);

	/// @brief The file that holds a path: its nearest entry's. A path no entry covers is new, and is the screen's.
	/// @param pointer The path.
	/// @return The file: its absolute path, or empty for the screen itself.
	[[nodiscard]] std::string ownerOf(std::string_view pointer) const;

	/// @brief Whether entries lie below a path, so its members may be held by different files.
	/// @param pointer The path.
	/// @return Whether they do.
	[[nodiscard]] bool hasEntriesBelow(std::string_view pointer) const;

	/// @brief A pointer to a member.
	/// @param pointer The object's pointer.
	/// @param key The member's key.
	/// @return The member's pointer, its key escaped as RFC 6901 says.
	[[nodiscard]] static std::string child(std::string_view pointer, std::string_view key);

private:
	std::map<std::string, std::string, std::less<>> entries_; ///< The file of each entry, by pointer.
};

} // namespace affresco::detail
