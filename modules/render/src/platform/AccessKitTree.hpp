#pragma once

/// @file
/// @brief What the AccessKit adapters of every platform share: the window's tree read into a flat table, the
/// AccessKit updates built from it (the whole tree, or what changed since the last one), and the actions queued from
/// the platform's threads and run on the main one.

#include "affresco/render/Accessibility.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX // windows.h's min and max macros would break std::min and std::max in every file including this one
#endif
#include <windows.h> // accesskit.h names HWND, WPARAM and LPARAM
#endif
#include <accesskit.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace affresco::platform {

/// @brief One element of the flat table: an element without its children, and their ids.
struct FlatNode {
	AccessibleNode node;                 ///< The element, its children left out.
	std::vector<std::uint64_t> children; ///< Their ids, in order.
	bool textRun = false;                ///< The text run inside a text field, which holds its value.
};

/// @brief One window's tree as AccessKit sees it. Read on the main thread (refresh()); the updates may be asked for
/// from another, so the table is guarded by a mutex. AccessKit's callbacks take a pointer to it as their user data.
class AccessKitTree {
public:
	/// @brief The id of the window's own element, the root of the tree.
	static constexpr std::uint64_t windowId = 1;

	/// @brief A tree of a window, not read yet.
	/// @param window The window; it outlives the tree.
	/// @param scale What a window unit is in the platform's coordinates: 1 on macOS (points), the content scale
	/// elsewhere (pixels).
	AccessKitTree(Window& window, double scale);

	/// @brief Changes what a window unit is in the platform's coordinates (the window moved to another display).
	/// @param scale The factor.
	void setScale(double scale) noexcept { scale_.store(scale); }
	/// @brief Whether an assistive technology has asked for the tree, and not let go of it since.
	[[nodiscard]] bool active() const noexcept { return active_.load(); }
	/// @brief Records that the assistive technology let go of the tree: it is not read until asked again.
	void deactivate() noexcept { active_.store(false); }
	/// @brief Whether the tree was asked for from another thread since the last call: the main thread then reads it.
	[[nodiscard]] bool takeStale() noexcept { return stale_.exchange(false); }
	/// @brief While active, reads the window's tree (main thread).
	/// @return Whether it changed since the last update sent.
	[[nodiscard]] bool readChanges();

	/// @brief Reads the window's tree into the table (main thread).
	void refresh();
	/// @brief Whether the table differs from what was last sent.
	[[nodiscard]] bool changed();
	/// @brief An update with the whole table; what it holds counts as sent. Any thread.
	/// @return The update; AccessKit takes it.
	[[nodiscard]] accesskit_tree_update* fullUpdate();
	/// @brief An update with the elements that changed since the last one sent, and the focus (main thread).
	/// @return The update; AccessKit takes it.
	[[nodiscard]] accesskit_tree_update* changes();

	/// @brief Queues an action asked by the assistive technology, and wakes the event loop. Any thread.
	/// @param request The request; freed here.
	void queue(accesskit_action_request* request);
	/// @brief Runs the actions queued (main thread).
	void runActions();

	/// @brief AccessKit's activation callback: the whole tree, read now on the main thread, else the last one read.
	/// @param tree The AccessKitTree.
	/// @return The update.
	static accesskit_tree_update* activate(void* tree);
	/// @brief AccessKit's update factory: the changes since the last update.
	/// @param tree The AccessKitTree.
	/// @return The update.
	static accesskit_tree_update* changesOf(void* tree);
	/// @brief AccessKit's action callback: queues the request.
	/// @param request The request; freed.
	/// @param tree The AccessKitTree.
	static void act(accesskit_action_request* request, void* tree);
	/// @brief AccessKit's deactivation callback (Linux).
	/// @param tree The AccessKitTree.
	static void release(void* tree);

private:
	/// @brief One queued action.
	struct Request {
		AccessibleAction action; ///< What to do.
		std::uint64_t target;    ///< To which element.
	};

	/// @brief Adds an element and those under it to a table.
	/// @param node The element.
	/// @param table The table.
	/// @return Its id.
	std::uint64_t flatten(AccessibleNode& node, std::unordered_map<std::uint64_t, FlatNode>& table) const;
	/// @brief The AccessKit node of an element of the table.
	/// @param flat The element.
	/// @return The node; the update takes it.
	[[nodiscard]] accesskit_node* build(const FlatNode& flat) const;

	Window* window_;                                    ///< The window.
	std::atomic<double> scale_;                         ///< See setScale().
	std::atomic<bool> active_{false};                   ///< See active().
	std::atomic<bool> stale_{false};                    ///< See takeStale().
	std::thread::id mainThread_;                        ///< The thread the window runs on.
	std::mutex mutex_;                                  ///< Guards what follows.
	std::unordered_map<std::uint64_t, FlatNode> nodes_; ///< The table, as last read.
	std::unordered_map<std::uint64_t, FlatNode> sent_;  ///< The table, as last sent.
	std::uint64_t focus_ = windowId;                    ///< The focus, as last read.
	std::uint64_t sentFocus_ = windowId;                ///< The focus, as last sent.
	std::vector<Request> requests_;                     ///< The actions queued.
};

} // namespace affresco::platform
