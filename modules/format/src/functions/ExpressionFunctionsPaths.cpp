/// @file
/// @brief Expression functions of paths: pathTree, which nests a flat list of paths into a tree's items, as a file
/// explorer shows a folder.

#include "ExpressionFunctions.hpp"

#include "Locale.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::format::detail {
namespace {

constexpr std::size_t mostItems = 1'000'000; ///< The most items nested (a tree's most rows).
constexpr std::size_t mostDepth = 256;       ///< The deepest a path nests (a tree's deepest).

/// @brief One node of the tree being made.
struct PathNode {
	std::string id;                    ///< Its path, without empty parts: "src/app".
	std::string label;                 ///< Its last part: "app".
	std::optional<Value> item;         ///< The item for it, or none if only its descendants were given.
	std::vector<std::size_t> children; ///< Its children, by index.
};

/// @brief The path an item gives.
/// @param item The item: a text, or an object with a text under the key.
/// @param key The key.
/// @return The path, or std::nullopt if it gives none.
std::optional<std::string> pathOf(const Value& item, std::string_view key) {
	if(item.isText()) return std::string(item.text());
	if(!item.isObject()) return std::nullopt;
	const std::optional<Value> path = item.member(key);
	if(!path || !path->isText()) return std::nullopt;
	return std::string(path->text());
}

/// @brief Whether a node is a folder: it has children, or its item says it may (`children` or `expandable`).
/// @param node The node.
/// @return true if it is.
bool isFolder(const PathNode& node) {
	if(!node.children.empty()) return true;
	if(!node.item || !node.item->isObject()) return false;
	const std::optional<Value> children = node.item->member("children");
	const std::optional<Value> expandable = node.item->member("expandable");
	return (children && children->isList()) || (expandable && truthy(*expandable));
}

/// @brief Makes the tree's items from the nodes.
class TreeMaker {
public:
	/// @brief Prepares the nodes.
	/// @param nodes The nodes.
	explicit TreeMaker(std::vector<PathNode>& nodes) noexcept : nodes_(nodes) {}

	/// @brief Sorts nodes as a file explorer does: folders first, then by name in any case.
	/// @param indices The nodes' indices.
	void sort(std::vector<std::size_t>& indices) const {
		std::ranges::stable_sort(indices, [this](std::size_t a, std::size_t b) {
			const PathNode &left = nodes_[a], &right = nodes_[b];
			if(const bool folder = isFolder(left); folder != isFolder(right)) return folder;
			if(const int order = compareText(left.label, right.label); order != 0) return order < 0;
			return left.label < right.label;
		});
	}

	/// @brief The items of nodes, and of their descendants.
	/// @param indices The nodes' indices, in order.
	/// @return The items.
	Value items(std::vector<std::size_t>& indices) const {
		sort(indices);
		std::vector<Value> items;
		items.reserve(indices.size());
		for(const std::size_t index : indices) items.push_back(item(nodes_[index]));
		return Value::fromList(std::move(items));
	}

private:
	/// @brief A node's item: its id, label and children, and the other fields of the item given for it.
	/// @param node The node.
	/// @return The item.
	Value item(PathNode& node) const {
		std::vector<Value::Field> fields = {{"id", Value::fromText(node.id)}, {"label", Value::fromText(node.label)}};
		if(node.item && node.item->isObject())
			for(Value::Field& field : node.item->fields()) {
				if(field.key == "id" || (field.key == "children" && !node.children.empty())) continue;
				if(field.key == "label") fields[1].value = std::move(field.value);
				else fields.push_back(std::move(field));
			}
		if(!node.children.empty()) fields.push_back({"children", items(node.children)});
		return Value::fromObject(std::move(fields));
	}

	std::vector<PathNode>& nodes_; ///< The nodes.
};

/// @brief `pathTree(list, key?)`.
/// @param call [list, key?].
/// @return The tree's items: { id, label, children, ...the item's other fields }, folders first, then by name.
Value pathTree(const Call& call) {
	const Value& list = call[0];
	if(list.isNull()) return Value::fromList({});
	if(!list.isList()) call.fail("expected a list, got " + list.dump());
	if(list.size() > mostItems) call.fail("more than 1000000 items");
	const std::string key = call.size() > 1 && !call[1].isNull() ? call[1].toText() : std::string("path");

	std::vector<PathNode> nodes;
	std::unordered_map<std::string, std::size_t> byPath; // a node's index by its id
	std::vector<std::size_t> roots;
	for(std::size_t i = 0; i < list.size(); ++i) {
		const Value item = list.item(i);
		const std::optional<std::string> path = pathOf(item, key);
		if(!path) continue;
		std::string id;
		std::size_t parent = SIZE_MAX, depth = 0, node = SIZE_MAX;
		for(std::size_t from = 0; from <= path->size();) {
			const std::size_t slash = std::min(path->find('/', from), path->size());
			const std::string_view part = std::string_view(*path).substr(from, slash - from);
			from = slash + 1;
			if(part.empty()) continue;
			if(++depth > mostDepth) call.fail("a path nests more than 256 deep: " + *path);
			id += id.empty() ? std::string(part) : "/" + std::string(part);
			const auto [found, added] = byPath.try_emplace(id, nodes.size());
			if(added) {
				nodes.push_back({id, std::string(part), std::nullopt, {}});
				(parent == SIZE_MAX ? roots : nodes[parent].children).push_back(found->second);
			}
			parent = node = found->second;
		}
		if(node != SIZE_MAX) nodes[node].item = item; // a later item for the same path wins
	}
	return TreeMaker(nodes).items(roots);
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "pathTree",
	 .arguments = "list, key?",
	 .about = "Paths nested into a tree's items by their parts: each item (a path, or an object with its path under "
	          "key, \"path\" by default) as { id, label, children, ...its fields }, the folders a path names made, "
	          "folders first, then by name",
	 .fewest = 1,
	 .most = 2,
	 .apply = pathTree},
});

} // namespace

std::span<const Function> pathFunctions() { return table; }

} // namespace affresco::format::detail
