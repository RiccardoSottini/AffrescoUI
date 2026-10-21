#pragma once

/// @file
/// @brief Reading the items of lists, trees and tables from a node, with the limits untrusted input needs.

#include "affresco/core/Json.hpp"
#include "affresco/render/ListView.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

inline constexpr std::size_t maxItems = 1'000'000; ///< The most items a list, a tree or a table reads.
inline constexpr std::size_t maxDepth = 256;       ///< The deepest a tree nests.
inline constexpr std::size_t maxCells = 256;       ///< The most cells a table's item has.

/// @brief Items read from a node, with the JSON they came from, so an update that gives the same items keeps them.
class SourcedItems final : public ItemList {
public:
	/// @brief Items and their source.
	/// @param items The items.
	/// @param source The JSON they were read from.
	SourcedItems(std::vector<ListItem> items, json source) : ItemList(std::move(items)), source_(std::move(source)) {}
	/// @brief The JSON the items were read from.
	[[nodiscard]] const json& source() const noexcept { return source_; }

private:
	json source_; ///< See source().
};

/// @brief Reads a list's or a tree's items: strings (each its id and label) or {id, label, children, expandable}.
/// @param value The items.
/// @param tree Whether "children" nest; a tree is flattened in pre-order.
/// @param path Their place, for errors.
/// @return The items.
[[nodiscard]] std::vector<ListItem> readItems(const json& value, bool tree, const std::string& path);

/// @brief Reads a list of ids: strings or numbers.
/// @param value The list.
/// @param path Its place, for errors.
/// @return The ids.
[[nodiscard]] std::vector<std::string> readIds(const json& value, const std::string& path);

} // namespace affresco::format
