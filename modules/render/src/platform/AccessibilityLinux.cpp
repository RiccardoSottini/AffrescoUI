/// @file
/// @brief The accessibility adapter on Linux and the BSDs: AccessKit serves AT-SPI over D-Bus (Orca) from the tree
/// AccessKitTree builds. AccessKit asks for the tree and runs actions on its own thread: the tree then hands over the
/// last one read and queues the actions, and the main thread reads it again at its next poll().

#include "platform/Accessibility.hpp"
#include "platform/AccessKitTree.hpp"

#include "affresco/render/Window.hpp"

#include <GLFW/glfw3.h>

namespace affresco::platform {

namespace {

/// @brief A window's adapter on Linux.
class LinuxAccessibility final : public AccessibilityAdapter {
public:
	/// @brief Creates AccessKit's adapter.
	/// @param window The window, shown in an OS window.
	explicit LinuxAccessibility(Window& window) : window_(&window), tree_(window, window.contentScale()) {
		adapter_ = accesskit_unix_adapter_new(AccessKitTree::activate, &tree_, AccessKitTree::act, &tree_,
		                                      AccessKitTree::release, &tree_);
	}
	/// @brief Frees the adapter first: its callbacks, which hold the tree, end with it.
	~LinuxAccessibility() override { accesskit_unix_adapter_free(adapter_); }
	LinuxAccessibility(const LinuxAccessibility&) = delete;
	LinuxAccessibility& operator=(const LinuxAccessibility&) = delete;

	/// @brief Runs the actions asked for, follows the window's focus and its place on the screen.
	void poll() override {
		tree_.runActions();
		GLFWwindow* handle = window_->nativeHandle();
		const bool focused = glfwGetWindowAttrib(handle, GLFW_FOCUSED) != 0;
		if(focused != focused_) {
			focused_ = focused;
			accesskit_unix_adapter_update_window_focus_state(adapter_, focused);
		}
		int x = 0, y = 0, width = 0, height = 0;
		glfwGetWindowPos(handle, &x, &y);
		glfwGetFramebufferSize(handle, &width, &height);
		const accesskit_rect bounds{static_cast<double>(x), static_cast<double>(y), static_cast<double>(x + width),
		                            static_cast<double>(y + height)};
		if(bounds.x0 != bounds_.x0 || bounds.y0 != bounds_.y0 || bounds.x1 != bounds_.x1 || bounds.y1 != bounds_.y1) {
			bounds_ = bounds;
			accesskit_unix_adapter_set_root_window_bounds(adapter_, bounds, bounds); // GLFW doesn't give the frame
		}
		if(tree_.takeStale()) frameDrawn();
	}
	/// @brief Sends what changed in the tree, while Orca (or another client) reads it.
	void frameDrawn() override {
		tree_.setScale(window_->contentScale());
		if(tree_.readChanges()) accesskit_unix_adapter_update_if_active(adapter_, AccessKitTree::changesOf, &tree_);
	}
	/// @brief Sends the new focus now.
	void focusChanged() override { frameDrawn(); }

private:
	Window* window_;                  ///< The window.
	AccessKitTree tree_;              ///< Its tree; the adapter's callbacks point at it.
	accesskit_unix_adapter* adapter_; ///< AccessKit's adapter.
	bool focused_ = false;            ///< Whether the window had the focus at the last poll().
	accesskit_rect bounds_{};         ///< The window's place on the screen, as last sent.
};

} // namespace

std::unique_ptr<AccessibilityAdapter> createAccessibilityAdapter(Window& window) {
	if(!window.nativeHandle()) return nullptr;
	return std::make_unique<LinuxAccessibility>(window);
}

} // namespace affresco::platform
