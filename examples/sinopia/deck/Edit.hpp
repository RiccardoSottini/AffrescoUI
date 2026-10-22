#pragma once

/// @file
/// @brief The edits the workspace makes to a deck (`sinopia.edit`): arranging, adding and removing objects and slides,
/// and setting their properties. Each takes the deck as JSON and gives it back read again, so every limit holds.

#include "Deck.hpp"

#include <affresco/core/Json.hpp>

namespace sinopia {

/// @brief Applies one edit.
///
/// `edit` is `{ "deck", "slide" (its index), "op", "ids" (the selected objects), ... }`; the ops and their other keys:
/// - objects: `align` (`edge`, `toSlide`), `distribute` (`across`), `reorder` (`how`), `nudge` (`x`, `y`), `group`,
///   `ungroup`, `add` (`kind`, `x`, `y`, `w`, `h`, `image`, `text`), `delete`, `duplicate`, `transform` (`items`, as a
///   canvas's onTransform gives them), `set` (`props`: an object's keys, as writeDeck() writes them), `lock`
///   (`locked`);
/// - slides: `addSlide` (`layout`), `deleteSlide`, `duplicateSlide`, `moveSlide` (`from`, `to`), `setSlide` (`props`:
///   `notes`, `background`, `transition`, `section`, `skip`, `layout`), `addSection` (`title`: a section that starts at
///   the slide), `setDeck` (`props`: `title`, `size`, `page`, `theme`).
/// @param edit The edit.
/// @param limits What the deck may hold.
/// @return `{ "deck", "slide", "selection" }`: the deck, the slide to show and the objects to select.
/// @throws std::runtime_error For an unknown op, a slide or object that isn't there, or a deck past a limit.
[[nodiscard]] affresco::json editDeck(const affresco::json& edit, const DeckLimits& limits = {});

} // namespace sinopia
