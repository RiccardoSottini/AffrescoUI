#pragma once

/// @file
/// @brief Settings: the "settings" section (a type with a "default" per setting), the values kept flat in
/// `state.settings`, and the `settings` and `settingsList` variables that read them.

#include "affresco/core/Json.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace affresco::format {

struct Engine;

/// @brief Checks the "settings" section: `"editor.fontSize" → type`, each type one StateTypes reads, with a
/// "default" that matches it, and no key that is another's group ("editor" beside "editor.fontSize").
/// @param section The section.
/// @param name "settings".
/// @throws DocumentError If a setting is malformed.
void checkSettings(const json& section, const std::string& name);

/// @brief The setting a written path names: "settings.editor.fontSize" → "editor.fontSize".
/// @param path The path, as a step writes it.
/// @return The setting's key, or std::nullopt if the path isn't under "settings.".
[[nodiscard]] std::optional<std::string> settingKey(std::string_view path);

/// @brief Where a written path lands when it names a setting: "settings.editor.fontSize" → /settings/editor.fontSize
/// in the state.
/// @param document The document; its "settings" are read.
/// @param path The path, as a step or a "bind" writes it.
/// @param where Its place, for errors.
/// @return The state pointer, or std::nullopt if the path isn't a setting's (or the document has no "settings").
/// @throws DocumentError If the path is under "settings." but there is no such setting.
[[nodiscard]] std::optional<json::json_pointer> settingPointer(const json& document, std::string_view path,
                                                               const std::string& where);

/// @brief Checks a value written to a setting.
/// @param document The document; its "settings" are read.
/// @param key The setting.
/// @param value The value, or nullptr when it is removed (back to its default).
/// @param where Its place, for errors.
/// @throws DocumentError If there is no such setting, or the value doesn't match its type.
void checkSettingValue(const json& document, const std::string& key, const json* value, const std::string& where);

/// @brief Adds the `settings` (nested, each value or its default) and `settingsList` variables.
/// @param engine The runtime; outlives the variables.
void addSettingsVariables(Engine& engine);

} // namespace affresco::format
