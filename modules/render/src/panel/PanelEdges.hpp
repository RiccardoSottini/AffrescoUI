#pragma once

/// @file
/// @brief The edge arithmetic Panel.cpp (the anchors' setters) and PanelLayout.cpp (the layout) share.

#include <affresco/render/Panel.hpp>

#include <cstddef>
#include <cstdint>

namespace affresco::panel {

/// @brief The anchors_ slot of an edge.
/// @param edge The edge.
/// @return Its index.
constexpr std::size_t index(Edge edge) noexcept { return static_cast<std::size_t>(edge); }

/// @brief The bit of an edge in Panel::targetEdges_.
/// @param edge The edge.
/// @return Its bit.
constexpr std::uint8_t edgeBit(Edge edge) noexcept { return static_cast<std::uint8_t>(1u << index(edge)); }

} // namespace affresco::panel
