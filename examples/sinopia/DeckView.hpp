#pragma once

/// @file
/// @brief What the workspace draws of a deck: each slide's objects with the theme's colours resolved, the master and
/// layout objects under them, and the outline, as JSON for `state.render`.

#include "Deck.hpp"

#include <affresco/core/Json.hpp>

#include <string>

namespace sinopia {

/// @brief A deck as the workspace draws it: `{ width, height, page, title, slides: [...] }` (`page`: the page in
/// points), each slide
/// `{ id, index, skip, section, notes, transition, background, title, body, builds, under, objects }`.
///
/// Objects are as writeDeck() writes them, with every colour a "#rrggbb(aa)" or "transparent" (theme tokens looked
/// up, empty ones given their defaults), `runs` always set (plain text as one run), `fontSize` the text's size at its
/// fit, and `viewBox` a pen path's view as "0 0 w h".
/// @param deck The deck.
/// @return It.
[[nodiscard]] affresco::json renderDeck(const Deck& deck);

/// @brief A colour as the deck writes it, resolved: a theme token's value, or the fallback for an empty one.
/// @param deck The deck (its theme).
/// @param color "", "transparent", "@token" or "#hex".
/// @param fallback What an empty colour (or a token the theme lacks) becomes.
/// @return The colour.
[[nodiscard]] std::string resolveDeckColor(const Deck& deck, const std::string& color, const std::string& fallback);

} // namespace sinopia
