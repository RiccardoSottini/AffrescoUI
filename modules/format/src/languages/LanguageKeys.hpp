#pragma once

/// @file
/// @brief The "editor" keys of code editing (LanguageEditor.hpp): "commands", "popup", "goTo", "insert" and
/// "tokens", and the code events ("onType", "onTextHover", "onPopupKey").

#include "LanguageEditor.hpp"

#include "TypeRegistry.hpp"

#include <string>

namespace affresco::format {

/// @brief Applies the code keys to an editor; an editor that isn't a LanguageEditor takes only "tokens".
/// @param editor The editor.
/// @param values The node's values.
/// @param path The node's place, for errors.
/// @throws DocumentError For a value of the wrong kind, or an unknown command.
void applyCodeKeys(TextEditor& editor, const json& values, const std::string& path);

/// @brief Runs the code events' handlers.
/// @param editor The editor.
/// @param sink Runs a handler.
/// @return The connection, or 0 for an editor that isn't a LanguageEditor.
Connection listenToCodeEvents(TextEditor& editor, const EventSink& sink);

/// @brief Disconnects listenToCodeEvents().
/// @param editor The editor.
/// @param connection The connection.
void stopListeningToCodeEvents(TextEditor& editor, Connection connection);

} // namespace affresco::format
