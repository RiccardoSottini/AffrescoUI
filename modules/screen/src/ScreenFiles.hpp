#pragma once

/// @file
/// @brief How a screen's files are read and where its data is kept: shared by Screen::fromFile() and
/// Screen::fromApp().

#include "affresco/core/Json.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace affresco::detail {

/// @brief Merges a screen's "include" files into it (recursively, at most 8 deep): each section of an included file
/// comes first and the including file's entries win; root and window come from the including file.
/// @param document The screen.
/// @param baseDirectory Where its include paths are resolved from.
/// @param migrated Set if one of them was written for version 1, and migrated (Migrate.hpp).
/// @param depth Includes inside includes so far.
/// @throws ScreenError If an include can't be read.
void resolveIncludes(json& document, const std::filesystem::path& baseDirectory, bool& migrated, int depth = 0);

/// @brief The name of a place's data in the data folder: a name the place chose (an app's id, a screen file's name),
/// bound to the first place that uses it, so that another place with the same name never gets its data or the
/// person's decisions. The record holds the place, absolute with its links followed; one without a record (new, or
/// kept before names were bound) is recorded as this place's.
/// @param recordOf Where a name's record is.
/// @param name The name the place chose.
/// @param location The place: an app's folder or a screen file.
/// @return @p name if it is this place's, else `<name>~<hash of the place>`, this place's own.
/// @throws ScreenError If both are recorded as other places'.
std::string boundDataName(const std::function<std::filesystem::path(const std::string&)>& recordOf,
                          const std::string& name, const std::filesystem::path& location);

/// @brief Where a screen file's data on disk is kept: its "persist" state and its history.
/// @param dataDirectory The data folder (Screen::defaultDataDirectory()).
/// @param screenFile The screen file.
/// @param suffix What is kept: ".state.json" or ".history".
/// @return In the data folder, the file's name without ".screen.json", then @p suffix: "habits.screen.json" gives
/// "habits.history". A folder's index screen is named after its folder: "flow/index.screen.json" gives "flow.history".
std::filesystem::path stateLocation(const std::filesystem::path& dataDirectory, const std::filesystem::path& screenFile,
                                    std::string_view suffix);

/// @brief The name a change set records as its template: the screen file's name, with its folder for an index screen
/// ("flow/index.screen.json"), whose own name says nothing.
/// @param screenFile The screen file.
/// @return The name.
std::string templateName(const std::filesystem::path& screenFile);

} // namespace affresco::detail
