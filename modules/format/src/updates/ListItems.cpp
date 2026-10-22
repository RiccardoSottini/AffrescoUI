/// @file
/// @brief Reading list, tree and table items: one pass, no recursion, so a deep or huge tree can't exhaust the stack.

#include "ListItems.hpp"
#include "NodeKeys.hpp"

#include <algorithm>
#include <cstdint>

namespace affresco::format {

namespace {

/// @brief A list of children being read.
struct Level {
	const json* children = nullptr; ///< The list.
	std::size_t next = 0;           ///< The next child to read.
	std::size_t owner = 0;          ///< The item they belong to; ignored for the roots.
	std::string path;               ///< The list's place, for errors.
};

/// @brief An item's place, for errors.
/// @param level The list it is in.
/// @param index Its place there.
/// @return "path[index]".
std::string placeOf(const Level& level, std::size_t index) {
	return concatenate(level.path, "[", std::to_string(index) + "]");
}

/// @brief Reads an item's cells: its "cells", else its "label", else its id.
/// @param value The item, an object.
/// @param place Its place, for errors.
/// @param item Gets them.
void readCells(const json& value, const auto& place, ListItem& item) {
	const auto cells = value.find("cells");
	if(cells == value.end()) {
		const auto label = value.find("label");
		item.cells.push_back(label != value.end() ? text(*label) : item.id);
		return;
	}
	if(!cells->is_array() || cells->empty()) fail(place() + ".cells", "expected a list of texts");
	if(cells->size() > maxCells) fail(place() + ".cells", "too many cells (at most 256)");
	item.cells.reserve(cells->size());
	for(const json& cell : *cells) item.cells.push_back(cell.is_null() ? std::string() : text(cell));
}

/// @brief Reads an item's "icon" and "iconColor".
/// @param value The item, an object.
/// @param place Its place, for errors.
/// @param item Gets them.
void readIcon(const json& value, const auto& place, ListItem& item) {
	if(const auto icon = value.find("icon"); icon != value.end() && icon->is_string())
		item.icon = icon->get<std::string>();
	const auto color = value.find("iconColor");
	if(color == value.end() || color->is_null()) return;
	if(color->is_string()) item.iconColor = Color::parseHex(color->get<std::string>());
	if(!item.iconColor) fail(place() + ".iconColor", "expected a hex colour: #rgb, #rrggbb or #rrggbbaa");
}

/// @brief Reads one item, without its children.
/// @param value A string, or {id, label, cells, expandable, icon, iconColor}.
/// @param level The list it is in.
/// @param index Its place there.
/// @return The item, its depth and end not set.
ListItem readItem(const json& value, const Level& level, std::size_t index) {
	// Made only on an error: most items are fine, and a list can hold a million
	const auto place = [&] { return placeOf(level, index); };
	if(value.is_string() || value.is_number()) {
		std::string id = text(value);
		ListItem item;
		item.cells.push_back(id);
		item.id = std::move(id);
		return item;
	}
	if(!value.is_object()) fail(place(), R"(expected an item: text, or {"id", "label"})");
	const auto id = value.find("id");
	if(id == value.end() || !(id->is_string() || id->is_number())) fail(place() + ".id", "an item needs an id");
	ListItem item;
	item.id = text(*id);
	readCells(value, place, item);
	if(const auto expandable = value.find("expandable"); expandable != value.end())
		item.expandable = boolean(*expandable, place() + ".expandable");
	readIcon(value, place, item);
	return item;
}

/// @brief A tree item's children.
/// @param entry The item.
/// @param tree Whether the items are a tree's.
/// @param level The list it is in.
/// @param index Its place there.
/// @return Its "children" list, or nullptr if it has none.
const json* childrenOf(const json& entry, bool tree, const Level& level, std::size_t index) {
	if(!tree || !entry.is_object()) return nullptr;
	const auto children = entry.find("children");
	if(children == entry.end()) return nullptr;
	if(!children->is_array()) fail(placeOf(level, index) + ".children", "expected a list of items");
	return &*children;
}

} // namespace

std::vector<ListItem> readItems(const json& value, bool tree, const std::string& path) {
	if(!value.is_array()) fail(path, "expected a list of items");
	std::vector<ListItem> items;
	items.reserve(std::min(value.size(), maxItems));
	std::vector<Level> levels;
	levels.push_back({&value, 0, 0, path});
	while(!levels.empty()) {
		Level& level = levels.back();
		if(level.next >= level.children->size()) {
			if(levels.size() > 1) items[level.owner].end = items.size();
			levels.pop_back();
			continue;
		}
		if(items.size() >= maxItems) fail(path, "too many items (at most 1,000,000)");
		const std::size_t index = level.next++;
		const json& entry = (*level.children)[index];
		ListItem item = readItem(entry, level, index);
		item.depth = static_cast<std::uint32_t>(levels.size() - 1);
		item.end = items.size() + 1;
		const json* children = childrenOf(entry, tree, level, index);
		const bool nests = children != nullptr && !children->empty();
		if(nests) item.expandable = true;
		items.push_back(std::move(item));
		if(!nests) continue;
		if(levels.size() > maxDepth) fail(placeOf(level, index), "items nest too deep (at most 256 levels)");
		levels.push_back(
		    {children, 0, items.size() - 1, placeOf(level, index) + ".children"}); // level isn't used after
	}
	return items;
}

std::vector<std::string> readIds(const json& value, const std::string& path) {
	std::vector<std::string> ids;
	if(value.is_null()) return ids;
	if(!value.is_array()) fail(path, "expected a list of ids");
	ids.reserve(std::min(value.size(), maxItems));
	for(std::size_t i = 0; i < value.size() && i < maxItems; ++i)
		if(value[i].is_string() || value[i].is_number()) ids.push_back(text(value[i]));
	return ids;
}

} // namespace affresco::format
