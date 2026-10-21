/// @file
/// @brief The "views" workloads: a tree of 100,000 items opened and closed, a list of 100,000 rows scrolled and
/// drawn, and a table of 100,000 rows sorted and scrolled, on a headless window (no display needed).

#include "Bench.hpp"
#include "Headless.hpp"

#include <affresco/render/ListView.hpp>
#include <affresco/render/TableView.hpp>
#include <affresco/render/TreeView.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

constexpr std::size_t itemCount = 100'000; ///< Items in the tree's folder and in the list.

/// @brief A headless window and its clock, kept as long as the workloads that use them.
struct HeadlessView {
	detail::ManualClock clock;      ///< The window's clock.
	std::unique_ptr<Window> window; ///< 800 by 600.

	/// @brief Opens the window.
	HeadlessView() : window(detail::createHeadlessWindow(800, 600, clock)) {}
};

/// @brief Items: with a folder, one root holding the rest; without, a flat list.
/// @param folder Whether the first item holds the others.
/// @return The model.
std::shared_ptr<const ItemList> manyItems(bool folder) {
	std::vector<ListItem> items;
	items.reserve(itemCount + 1);
	if(folder) items.push_back(ListItem{"root", {"root"}, 0, itemCount + 1, true});
	const std::uint32_t depth = folder ? 1 : 0;
	for(std::size_t i = 0; i < itemCount; ++i) {
		std::string id = "item " + std::to_string(i);
		items.push_back(ListItem{id, {std::move(id)}, depth, items.size() + 1, false});
	}
	return std::make_shared<const ItemList>(std::move(items));
}

/// @brief A flat table model: each item a name and a number in a shuffled order, so sorting moves every row.
/// @return The model.
std::shared_ptr<const ItemList> manyRows() {
	std::vector<ListItem> items;
	items.reserve(itemCount);
	for(std::size_t i = 0; i < itemCount; ++i) {
		std::string id = "row " + std::to_string(i);
		std::string size = std::to_string(i * 7919 % itemCount);
		items.push_back(ListItem{id, {std::move(id), std::move(size)}, 0, items.size() + 1, false});
	}
	return std::make_shared<const ItemList>(std::move(items));
}

/// @brief Adds the table workloads: a sort by numbers reversed and back, and a scroll drawn each time.
/// @param workloads Where they go.
void addTableWorkloads(std::vector<Workload>& workloads) {
	auto tableView = std::make_shared<HeadlessView>();
	auto& table = tableView->window->root().add<TableView>();
	table.setSize(px(800), px(600));
	table.setModel(manyRows());
	table.setColumns({{"name", "Name"}, {"size", "Size"}});
	table.setSort({"size", false});
	tableView->window->render();
	auto next = std::make_shared<std::size_t>(0);

	workloads.push_back({.name = "views/table-sort-100k", .runs = 11, .operations = 2, .step = [tableView, &table] {
		                     table.setSort({"size", true});
		                     table.setSort({"size", false});
	                     }});
	workloads.push_back(
	    {.name = "views/table-scroll-100k", .runs = 21, .operations = 1, .step = [tableView, &table, next] {
		     *next = (*next + 7919) % itemCount; // a prime stride: a new place every time
		     table.scrollToRow(*next);
		     tableView->window->render();
	     }});
}

} // namespace

std::vector<Workload> viewWorkloads(Context&) {
	auto treeView = std::make_shared<HeadlessView>();
	auto& tree = treeView->window->root().add<TreeView>();
	tree.setSize(px(800), px(600));
	tree.setModel(manyItems(true));
	treeView->window->render();

	auto listView = std::make_shared<HeadlessView>();
	auto& list = listView->window->root().add<ListView>();
	list.setSize(px(800), px(600));
	list.setModel(manyItems(false));
	listView->window->render();
	auto next = std::make_shared<std::size_t>(0);

	std::vector<Workload> workloads;
	workloads.push_back({.name = "views/tree-expand-100k", .runs = 21, .operations = 2, .step = [treeView, &tree] {
		                     tree.expand(0, true);
		                     tree.expand(0, false);
	                     }});
	workloads.push_back(
	    {.name = "views/list-scroll-100k", .runs = 21, .operations = 1, .step = [listView, &list, next] {
		     *next = (*next + 7919) % itemCount; // a prime stride: a new place every time
		     list.scrollToRow(*next);
		     listView->window->render();
	     }});
	addTableWorkloads(workloads);
	return workloads;
}

} // namespace affresco::bench
