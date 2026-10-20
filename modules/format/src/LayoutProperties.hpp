#pragma once

/// @file
/// @brief The flexible and grid layout keys: those every node has (maxWidth, maxHeight, aspectRatio, shrink, basis,
/// alignSelf, cell), the property sets of rows, columns and grids (justify, alignItems, rowGap, columnGap, rows,
/// justifyItems), and the readers of their values (grid tracks, cells, justifications, alignments).

#include "TypeRegistry.hpp"

#include "affresco/core/Json.hpp"
#include "affresco/render/Layout.hpp"
#include "affresco/render/Panel.hpp"

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief The layout keys every node has, for the catalog ("panel" lists them).
/// @return The specs, in catalog order.
[[nodiscard]] std::vector<PropertySpec> layoutItemProperties();

/// @brief Applies one of the layout keys every node has.
/// @param panel The panel.
/// @param key The key.
/// @param value Its evaluated value.
/// @param at Its place, for errors.
/// @return false if @p key isn't one of them.
/// @throws DocumentError If the value isn't valid for the key.
bool layoutItemProperty(Panel& panel, std::string_view key, const json& value, const std::string& at);

/// @brief Sets the layout keys every node has back to their defaults, before an update applies the node's own.
/// @param panel The panel.
void resetLayoutItem(Panel& panel);

/// @brief The property set of a row, a column or a stack: the keys the runtime applies itself (gap, from, ...), listed
/// first, then justify, alignItems, rowGap and columnGap, which the set applies.
/// @param applied The keys the runtime applies itself, in catalog order.
/// @return The set.
[[nodiscard]] std::shared_ptr<const PropertySet> stackProperties(std::initializer_list<PropertySpec> applied);

/// @brief The property set of a grid: columns and gap (which the runtime applies), then rows, rowGap, columnGap,
/// justifyItems and alignItems.
/// @return The set.
[[nodiscard]] std::shared_ptr<const PropertySet> gridProperties();

/// @brief Reads a grid's columns or rows: a number of equal columns, or a list of tracks (`120`, `"25%"`, `"1fr"`,
/// `"auto"`, `"minmax(100, 1fr)"`).
/// @param value The value.
/// @param at Its place, for errors.
/// @return The tracks, at least one for a number, at most maxGridTracks.
/// @throws DocumentError If it is neither, or a track is malformed.
[[nodiscard]] std::vector<Track> gridTracks(const json& value, const std::string& at);

} // namespace affresco::format
