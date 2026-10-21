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

/// @brief Reads one item, without its children.
/// @param value A string, or {id, label, cells, expandable}.
/// @param level The list it is in.
/// @param index Its place there.
/// @return The item, its depth and end not set.
ListItem readItem(const json& value, const Level& level, std::size_t index) {
	const auto place = [&] { return concatenate(level.path, "[", std::to_string(index) + "]"); };
	if(value.is_string() || value.is_number()) {
		std::string id = text(value);
		return ListItem{id, {std::move(id)}};
	}
	if(!value.is_object()) fail(place(), "expected an item: text, or {\"id\", \"label\"}");
	const auto id = value.find("id");
	if(id == value.end() || !(id->is_string() || id->is_number())) fail(place() + ".id", "an item needs an id");
	ListItem item{text(*id), {}};
	if(const auto cells = value.find("cells"); cells != value.end()) {
		if(!cells->is_array() || cells->empty()) fail(place() + ".cells", "expected a list of texts");
		if(cells->size() > maxCells) fail(place() + ".cells", "too many cells (at most 256)");
		item.cells.reserve(cells->size());
		for(const json& cell : *cells) item.cells.push_back(cell.is_null() ? std::string() : text(cell));
	} else {
		const auto label = value.find("label");
		item.cells.push_back(label != value.end() ? text(*label) : item.id);
	}
	if(const auto expandable = value.find("expandable"); expandable != value.end())
		item.expandable = boolean(*expandable, place() + ".expandable");
	return item;
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
		const auto children = tree && entry.is_object() ? entry.find("children") : entry.end();
		const bool nests = tree && entry.is_object() && children != entry.end();
		if(nests && !children->is_array())
			fail(concatenate(level.path, "[", std::to_string(index) + "].children"), "expected a list of items");
		if(nests && !children->empty()) item.expandable = true;
		items.push_back(std::move(item));
		if(!nests || children->empty()) continue;
		if(levels.size() > maxDepth)
			fail(concatenate(level.path, "[", std::to_string(index) + "]"), "items nest too deep (at most 256 levels)");
		std::string childPath = concatenate(level.path, "[", std::to_string(index) + "].children");
		levels.push_back({&*children, 0, items.size() - 1, std::move(childPath)}); // level is not used after this
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
