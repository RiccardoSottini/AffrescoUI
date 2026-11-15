#pragma once

/// @file
/// @brief What the person pointed at, as the in-process agent tells a model of it: a request's "picks" (the records
/// the engine made when the person clicked an element), kept to their known keys and sizes, and the first message's
/// block that starts from the element and the definition that built it instead of the whole screen.

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <string>

namespace affresco::intelligence {

/// The most picks a request lists that the model is told of.
inline constexpr std::size_t maxPicks = 4;

/// The most bytes of one pick's block in the first message: its record, and its definition or node.
inline constexpr std::size_t maxPickBlock = std::size_t{12} * 1024;

/// @brief The picks of a request, as the model may be told of them: at most maxPicks records, each with only the
/// keys of the contract (id, revision, node, type, hitType, pointer, bounds, bindings, handlers, repeat, component,
/// exact, source, proposal), each of its type, texts cut to 200 bytes (an address or a pointer to 512), at most 16
/// bindings, 16 handlers and 4 repeats. A record without a "node" text, and anything that isn't a record, is left out.
/// Never throws.
/// @param in The request's "picks" (any JSON).
/// @return The records.
[[nodiscard]] affresco::json picksOf(const affresco::json& in);

/// @brief The block of the first message about the picks: each record on one line, then, in a program, the text of
/// the definition at its source.address (cut to 8,000 bytes), or, in a JSON document, its node to depth 2 (cut to
/// 4,000 bytes), read from @p document only (what the agent was allowed to read): what isn't there is said to be
/// unreadable. Each pick's part is at most maxPickBlock bytes. Never throws.
/// @param picks The records (picksOf()).
/// @param document The screen document the agent read.
/// @return The block, or "" without picks.
[[nodiscard]] std::string pickBlock(const affresco::json& picks, const affresco::json& document);

/// @brief One line about the rest of the screen, for a first message that doesn't hold it: the revision, how many
/// nodes and definitions it has, and how to read more.
/// @param document The screen document the agent read.
/// @param revision Its revision.
/// @param tools Whether the model has tools (else it reads by answering read, find and guide).
/// @return The line.
[[nodiscard]] std::string screenSummary(const affresco::json& document, int revision, bool tools);

} // namespace affresco::intelligence
