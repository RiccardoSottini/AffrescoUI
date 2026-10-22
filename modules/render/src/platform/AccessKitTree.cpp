/// @file
/// @brief The window's tree as AccessKit sees it (platform/AccessKitTree.hpp): the flat table, the updates built from
/// it, the roles mapped to AccessKit's, and the queued actions.

#include "platform/AccessKitTree.hpp"

#include "affresco/render/Window.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace affresco::platform {

namespace {

using Table = std::unordered_map<std::uint64_t, FlatNode>;

/// @brief The AccessKit role of each role name (ARIA's names, and the window's).
constexpr std::array<std::pair<std::string_view, accesskit_role>, 50> roles{{
    {"window", ACCESSKIT_ROLE_WINDOW},
    {"button", ACCESSKIT_ROLE_BUTTON},
    {"checkbox", ACCESSKIT_ROLE_CHECK_BOX},
    {"switch", ACCESSKIT_ROLE_SWITCH},
    {"slider", ACCESSKIT_ROLE_SLIDER},
    {"textbox", ACCESSKIT_ROLE_TEXT_INPUT},
    {"searchbox", ACCESSKIT_ROLE_SEARCH_INPUT},
    {"text", ACCESSKIT_ROLE_LABEL},
    {"img", ACCESSKIT_ROLE_IMAGE},
    {"image", ACCESSKIT_ROLE_IMAGE},
    {"tab", ACCESSKIT_ROLE_TAB},
    {"tablist", ACCESSKIT_ROLE_TAB_LIST},
    {"tabpanel", ACCESSKIT_ROLE_TAB_PANEL},
    {"tree", ACCESSKIT_ROLE_TREE},
    {"treeitem", ACCESSKIT_ROLE_TREE_ITEM},
    {"list", ACCESSKIT_ROLE_LIST},
    {"listitem", ACCESSKIT_ROLE_LIST_ITEM},
    {"listbox", ACCESSKIT_ROLE_LIST_BOX},
    {"option", ACCESSKIT_ROLE_LIST_BOX_OPTION},
    {"menu", ACCESSKIT_ROLE_MENU},
    {"menuitem", ACCESSKIT_ROLE_MENU_ITEM},
    {"menubar", ACCESSKIT_ROLE_MENU_BAR},
    {"toolbar", ACCESSKIT_ROLE_TOOLBAR},
    {"status", ACCESSKIT_ROLE_STATUS},
    {"dialog", ACCESSKIT_ROLE_DIALOG},
    {"alert", ACCESSKIT_ROLE_ALERT},
    {"alertdialog", ACCESSKIT_ROLE_ALERT_DIALOG},
    {"navigation", ACCESSKIT_ROLE_NAVIGATION},
    {"main", ACCESSKIT_ROLE_MAIN},
    {"region", ACCESSKIT_ROLE_REGION},
    {"group", ACCESSKIT_ROLE_GROUP},
    {"link", ACCESSKIT_ROLE_LINK},
    {"radio", ACCESSKIT_ROLE_RADIO_BUTTON},
    {"combobox", ACCESSKIT_ROLE_COMBO_BOX},
    {"heading", ACCESSKIT_ROLE_HEADING},
    {"separator", ACCESSKIT_ROLE_SPLITTER},
    {"tooltip", ACCESSKIT_ROLE_TOOLTIP},
    {"log", ACCESSKIT_ROLE_LOG},
    {"progressbar", ACCESSKIT_ROLE_PROGRESS_INDICATOR},
    {"terminal", ACCESSKIT_ROLE_TERMINAL},
    {"document", ACCESSKIT_ROLE_DOCUMENT},
    {"grid", ACCESSKIT_ROLE_GRID},
    {"row", ACCESSKIT_ROLE_ROW},
    {"cell", ACCESSKIT_ROLE_CELL},
    {"gridcell", ACCESSKIT_ROLE_CELL},
    {"table", ACCESSKIT_ROLE_TABLE},
    {"complementary", ACCESSKIT_ROLE_COMPLEMENTARY},
    {"banner", ACCESSKIT_ROLE_BANNER},
    {"search", ACCESSKIT_ROLE_SEARCH},
    {"form", ACCESSKIT_ROLE_FORM},
}};

/// @brief Our actions and AccessKit's.
constexpr std::array<std::pair<AccessibleAction, accesskit_action>, 6> actions{{
    {AccessibleAction::Press, ACCESSKIT_ACTION_CLICK},
    {AccessibleAction::Focus, ACCESSKIT_ACTION_FOCUS},
    {AccessibleAction::Increment, ACCESSKIT_ACTION_INCREMENT},
    {AccessibleAction::Decrement, ACCESSKIT_ACTION_DECREMENT},
    {AccessibleAction::Expand, ACCESSKIT_ACTION_EXPAND},
    {AccessibleAction::Collapse, ACCESSKIT_ACTION_COLLAPSE},
}};

/// @brief The AccessKit role of a role name.
/// @param role The name.
/// @return The role; a generic container for an unknown name.
accesskit_role roleOf(std::string_view role) {
	const auto* found = std::ranges::find(roles, role, &std::pair<std::string_view, accesskit_role>::first);
	return found != roles.end() ? found->second : ACCESSKIT_ROLE_GENERIC_CONTAINER;
}

/// @brief Whether two elements of the table read the same.
/// @param a One.
/// @param b The other.
/// @return Whether they do.
bool same(const FlatNode& a, const FlatNode& b) {
	const AccessibleNode& x = a.node;
	const AccessibleNode& y = b.node;
	return a.children == b.children && a.textRun == b.textRun && x.role == y.role && x.name == y.name &&
	       x.description == y.description && x.value == y.value && x.range == y.range && x.checked == y.checked &&
	       x.state == y.state && x.selection == y.selection && x.bounds == y.bounds && x.actions == y.actions;
}

/// @brief The UTF-8 length of each code point of a text, as AccessKit counts a text run's characters.
/// @param text The text.
/// @return The lengths.
std::vector<std::uint8_t> characterLengths(std::string_view text) {
	std::vector<std::uint8_t> lengths;
	for(std::size_t index = 0; index < text.size();) {
		const auto lead = static_cast<unsigned char>(text[index]);
		std::size_t length = lead < 0x80u ? 1 : lead < 0xE0u ? 2 : lead < 0xF0u ? 3 : 4;
		length = std::min(length, text.size() - index);
		lengths.push_back(static_cast<std::uint8_t>(length));
		index += length;
	}
	return lengths;
}

/// @brief Sets a node's states: checked, range, expanded, selected, disabled, live.
/// @param node The AccessKit node.
/// @param element The element.
void setStates(accesskit_node* node, const AccessibleNode& element) {
	if(element.range) {
		accesskit_node_set_numeric_value(node, element.range->value);
		accesskit_node_set_min_numeric_value(node, element.range->minimum);
		accesskit_node_set_max_numeric_value(node, element.range->maximum);
		if(element.range->step > 0.0) accesskit_node_set_numeric_value_step(node, element.range->step);
	}
	if(element.checked)
		accesskit_node_set_toggled(node, *element.checked ? ACCESSKIT_TOGGLED_TRUE : ACCESSKIT_TOGGLED_FALSE);
	if(element.state.expanded) accesskit_node_set_expanded(node, *element.state.expanded);
	if(element.state.selected) accesskit_node_set_selected(node, *element.state.selected);
	if(element.state.disabled) accesskit_node_set_disabled(node);
	if(element.state.live == LiveRegion::Polite) accesskit_node_set_live(node, ACCESSKIT_LIVE_POLITE);
	if(element.state.live == LiveRegion::Assertive) accesskit_node_set_live(node, ACCESSKIT_LIVE_ASSERTIVE);
	for(const auto& [ours, theirs] : actions)
		if((element.actions & static_cast<std::uint8_t>(ours)) != 0) accesskit_node_add_action(node, theirs);
}

} // namespace

AccessKitTree::AccessKitTree(Window& window, double scale)
    : window_(&window), scale_(scale), mainThread_(std::this_thread::get_id()) {
	FlatNode root;
	root.node.role = "window";
	nodes_.emplace(windowId, std::move(root)); // until the tree is first read
}

std::uint64_t AccessKitTree::flatten(AccessibleNode& node, Table& table) const {
	FlatNode flat;
	if(node.role == "textbox" || node.role == "searchbox") { // its text is a run inside it, where the caret is
		FlatNode run;
		run.textRun = true;
		run.node.value = node.value;
		run.node.bounds = node.bounds;
		flat.children.push_back(node.id + 1); // ids are addresses: id + 1 is no other panel's
		table.insert_or_assign(node.id + 1, std::move(run));
	}
	for(auto& child : node.children)
		if(table.size() < maxAccessibleNodes) flat.children.push_back(flatten(child, table));
	node.children.clear();
	const std::uint64_t id = node.id;
	flat.node = std::move(node);
	table.insert_or_assign(id, std::move(flat));
	return id;
}

void AccessKitTree::refresh() {
	AccessibleNode tree = accessibilityTree(window_->root());
	Table table;
	FlatNode root;
	root.node.role = "window";
	root.node.name = window_->title();
	root.node.bounds = Rect{0.0f, 0.0f, static_cast<float>(window_->width()), static_cast<float>(window_->height())};
	if(tree.panel) root.children.push_back(flatten(tree, table));
	else
		for(auto& child : tree.children) root.children.push_back(flatten(child, table));
	table.insert_or_assign(windowId, std::move(root));
	Panel* focused = window_->focusedPanel();
	const std::uint64_t focus = focused && table.contains(accessibleId(*focused)) ? accessibleId(*focused) : windowId;
	const std::scoped_lock lock(mutex_);
	nodes_ = std::move(table);
	focus_ = focus;
}

bool AccessKitTree::changed() {
	const std::scoped_lock lock(mutex_);
	if(focus_ != sentFocus_ || nodes_.size() != sent_.size()) return true;
	return std::ranges::any_of(nodes_, [this](const auto& entry) {
		const auto found = sent_.find(entry.first);
		return found == sent_.end() || !same(found->second, entry.second);
	});
}

bool AccessKitTree::readChanges() {
	if(!active()) return false;
	refresh();
	return changed();
}

accesskit_node* AccessKitTree::build(const FlatNode& flat) const {
	const AccessibleNode& element = flat.node;
	accesskit_node* node = accesskit_node_new(flat.textRun ? ACCESSKIT_ROLE_TEXT_RUN : roleOf(element.role));
	if(element.role == "text") accesskit_node_set_value(node, element.name.c_str()); // a label's text is its value
	else if(!element.name.empty()) accesskit_node_set_label(node, element.name.c_str());
	if(!element.description.empty()) accesskit_node_set_description(node, element.description.c_str());
	if(!element.value.empty() || flat.textRun) accesskit_node_set_value(node, element.value.c_str());
	if(flat.textRun) {
		const std::vector<std::uint8_t> lengths = characterLengths(element.value);
		accesskit_node_set_character_lengths(node, lengths.size(), lengths.data());
	}
	if(element.selection) {
		const accesskit_text_position anchor{element.id + 1, element.selection->first};
		const accesskit_text_position focus{element.id + 1, element.selection->second};
		accesskit_node_set_text_selection(node, accesskit_text_selection{anchor, focus});
	}
	setStates(node, element);
	const double scale = scale_.load();
	const Rect& box = element.bounds;
	accesskit_node_set_bounds(node,
	                          accesskit_rect{box.left * scale, box.top * scale, box.right * scale, box.bottom * scale});
	if(!flat.children.empty()) accesskit_node_set_children(node, flat.children.size(), flat.children.data());
	return node;
}

accesskit_tree_update* AccessKitTree::fullUpdate() {
	const std::scoped_lock lock(mutex_);
	accesskit_tree_update* update = accesskit_tree_update_with_capacity_and_focus(nodes_.size(), focus_);
	accesskit_tree_update_set_tree_info(update, accesskit_tree_info_new(windowId));
	for(const auto& [id, flat] : nodes_) accesskit_tree_update_push_node(update, id, build(flat));
	sent_ = nodes_;
	sentFocus_ = focus_;
	return update;
}

accesskit_tree_update* AccessKitTree::changes() {
	const std::scoped_lock lock(mutex_);
	accesskit_tree_update* update = accesskit_tree_update_with_focus(focus_);
	for(const auto& [id, flat] : nodes_) {
		const auto found = sent_.find(id);
		if(found == sent_.end() || !same(found->second, flat)) accesskit_tree_update_push_node(update, id, build(flat));
	}
	sent_ = nodes_;
	sentFocus_ = focus_;
	return update;
}

void AccessKitTree::queue(accesskit_action_request* request) {
	const auto* found =
	    std::ranges::find(actions, request->action, &std::pair<AccessibleAction, accesskit_action>::second);
	if(found != actions.end()) {
		const std::scoped_lock lock(mutex_);
		if(requests_.size() < maxAccessibleNodes) requests_.push_back({found->first, request->target_node});
	}
	accesskit_action_request_free(request);
	glfwPostEmptyEvent(); // wakes the main thread, which runs it (runActions())
}

void AccessKitTree::runActions() {
	std::vector<Request> requests;
	{
		const std::scoped_lock lock(mutex_);
		requests.swap(requests_);
	}
	for(const Request& request : requests) {
		Panel* panel = findByAccessibleId(window_->root(), request.target);
		if(!panel) panel = findByAccessibleId(window_->root(), request.target - 1); // a text field's run
		if(panel) static_cast<void>(performAccessibleAction(*window_, *panel, request.action));
	}
}

accesskit_tree_update* AccessKitTree::activate(void* tree) {
	auto* self = static_cast<AccessKitTree*>(tree);
	if(std::this_thread::get_id() == self->mainThread_) self->refresh();
	else {
		self->stale_.store(true); // the main thread reads the tree at its next poll, and sends what changed
		glfwPostEmptyEvent();
	}
	self->active_.store(true);
	return self->fullUpdate();
}

accesskit_tree_update* AccessKitTree::changesOf(void* tree) { return static_cast<AccessKitTree*>(tree)->changes(); }

void AccessKitTree::act(accesskit_action_request* request, void* tree) {
	static_cast<AccessKitTree*>(tree)->queue(request);
}

void AccessKitTree::release(void* tree) { static_cast<AccessKitTree*>(tree)->deactivate(); }

} // namespace affresco::platform
