#pragma once

/// @file
/// @brief Arranging a slide's objects: align, distribute, z-order, group and ungroup, and nudge. Locked objects stay
/// where they are (the canvas snaps and shows smart guides).

#include "Deck.hpp"

#include <span>
#include <string>
#include <vector>

namespace sinopia {

/// @brief Aligns objects: to the box around them, or to the slide when there is one, or `toSlide` is set.
/// @param objects The slide's objects.
/// @param ids The ones to align.
/// @param edge "left", "center", "right", "top", "middle" or "bottom".
/// @param toSlide Align to the slide instead of the selection.
/// @param slide The slide's width and height.
/// @throws std::runtime_error For another edge.
void alignObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, const std::string& edge,
                  bool toSlide, std::pair<float, float> slide);

/// @brief Spaces objects evenly between the first and the last, across or down: the gaps between them are made equal.
/// @param objects The slide's objects.
/// @param ids The ones to space (three or more, else nothing happens).
/// @param across Across (left to right), else down.
void distributeObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, bool across);

/// @brief Moves objects in the z-order.
/// @param objects The slide's objects, back to front.
/// @param ids The ones to move.
/// @param how "front", "back", "forward" (one step) or "backward".
/// @throws std::runtime_error For another way.
void reorderObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, const std::string& how);

/// @brief Moves objects by an offset.
/// @param objects The slide's objects.
/// @param ids The ones to move.
/// @param x Right.
/// @param y Down.
void nudgeObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, float x, float y);

/// @brief Groups objects: one group, where the frontmost was, around them, holding them relative to its corner.
/// @param objects The slide's objects.
/// @param ids The ones to group (two or more, none a group, else nothing happens).
/// @param id The group's id.
/// @return Whether a group was made.
bool groupObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, const std::string& id);

/// @brief Ungroups groups: their objects take their place, on the slide's coordinates.
/// @param objects The slide's objects.
/// @param ids The groups to ungroup (other objects are left alone).
void ungroupObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids);

} // namespace sinopia
