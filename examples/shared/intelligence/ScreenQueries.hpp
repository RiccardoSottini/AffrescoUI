#pragma once

/// @file
/// @brief Reading a screen document the way an agent needs it, as the MCP server does (mcp/affrescoui_mcp/document.py
/// and guide.py): the outline, one node by address, a search, and the format guide one topic at a time.

#include <affresco/core/Json.hpp>

#include <optional>
#include <string>

namespace examples::intelligence {

/// @brief The node tree, one line per node: its address ("#id"), type, text, bind, onClick, repeat and when.
/// @param document The screen document.
/// @param at A node's address ("#id") or a JSON Pointer to start at, or "" for every tree.
/// @param depth How many levels below to show (-1: all); a cut node says how many it hides ("+12").
/// @return The outline.
/// @throws std::runtime_error If @p at names nothing.
[[nodiscard]] std::string outline(const affresco::json& document, const std::string& at = {}, int depth = -1);

/// @brief The value at "#id", "#id/rest" or a JSON Pointer, with what is deeper than @p depth levels replaced by
/// its children's addresses.
/// @param document The screen document.
/// @param at The address.
/// @param depth Levels of children to keep whole (-1: all).
/// @return The value.
/// @throws std::runtime_error If it names nothing.
[[nodiscard]] affresco::json nodeAt(const affresco::json& document, const std::string& at, int depth = -1);

/// @brief The nodes matching every criterion given, one line each as in the outline, with the parent's address.
/// @param document The screen document.
/// @param type A node type or component name, or "".
/// @param text Text the node shows (any case), or "".
/// @param property A property it has, or "".
/// @param value That property's value, or a null value for any.
/// @param limit The most lines.
/// @return "N found" and the lines.
[[nodiscard]] std::string findNodes(const affresco::json& document, const std::string& type, const std::string& text,
                                    const std::string& property, const affresco::json& value, int limit = 20);

/// @brief A part of the format guide (SCREEN_FORMAT.md): without a topic, its introduction and the topics' list; a
/// topic (a heading, any case, or a word of one) gives that section, cut to about 6,000 characters.
/// @param guide The guide's text.
/// @param topic The topic, or "".
/// @return The part, or nullopt if no heading matches.
[[nodiscard]] std::optional<std::string> formatGuide(const std::string& guide, const std::string& topic = {});

/// @brief The format guide shipped with the examples: SCREEN_FORMAT.md beside the examples' folder (the source tree),
/// or in the release's assets.
/// @return Its text, or "" if it isn't found.
[[nodiscard]] std::string readFormatGuide();

/// @brief Minified JSON, invalid UTF-8 replaced: what the model is sent.
/// @param value The value.
/// @return The text.
[[nodiscard]] std::string compact(const affresco::json& value);

} // namespace examples::intelligence
