/// @file
/// @brief The accessibility adapter on macOS (Cocoa, ARC): AccessKit subclasses the window's content view, which then
/// answers NSAccessibility (VoiceOver) from the tree AccessKitTree builds.

#include "platform/Accessibility.hpp"
#include "platform/AccessKitTree.hpp"

#include "affresco/render/Window.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

namespace affresco::platform {

namespace {

/// @brief Raises the events AccessKit queued, if any.
/// @param events What an update returned; may be null.
void raise(accesskit_macos_queued_events* events) {
	if(events) accesskit_macos_queued_events_raise(events);
}

/// @brief A window's adapter on macOS.
class MacAccessibility final : public AccessibilityAdapter {
public:
	/// @brief Subclasses the window's content view.
	/// @param window The window, shown in an OS window.
	explicit MacAccessibility(Window& window)
	    : window_(&window), tree_(window, window.contentScale()) { // AccessKit takes backing pixels, as elsewhere
		NSView* view = glfwGetCocoaView(window.nativeHandle());
		adapter_ = accesskit_macos_subclassing_adapter_new((__bridge void*)view, AccessKitTree::activate, &tree_,
		                                                   AccessKitTree::act, &tree_);
	}
	/// @brief Frees the adapter first: its callbacks, which hold the tree, end with it.
	~MacAccessibility() override { accesskit_macos_subclassing_adapter_free(adapter_); }
	MacAccessibility(const MacAccessibility&) = delete;
	MacAccessibility& operator=(const MacAccessibility&) = delete;

	/// @brief Runs the actions asked for, and tells AccessKit when the window gains or loses the focus.
	void poll() override {
		tree_.runActions();
		const bool focused = glfwGetWindowAttrib(window_->nativeHandle(), GLFW_FOCUSED) != 0;
		if(focused != focused_) {
			focused_ = focused;
			raise(accesskit_macos_subclassing_adapter_update_view_focus_state(adapter_, focused));
		}
		if(tree_.takeStale()) frameDrawn();
	}
	/// @brief Sends what changed in the tree, while VoiceOver (or another client) reads it.
	void frameDrawn() override {
		tree_.setScale(window_->contentScale());
		if(tree_.readChanges())
			raise(accesskit_macos_subclassing_adapter_update_if_active(adapter_, AccessKitTree::changesOf, &tree_));
	}
	/// @brief Sends the new focus now.
	void focusChanged() override { frameDrawn(); }

private:
	Window* window_;                               ///< The window.
	AccessKitTree tree_;                           ///< Its tree; the adapter's callbacks point at it.
	accesskit_macos_subclassing_adapter* adapter_; ///< AccessKit's adapter.
	bool focused_ = false;                         ///< Whether the window had the focus at the last poll().
};

} // namespace

std::unique_ptr<AccessibilityAdapter> createAccessibilityAdapter(Window& window) {
	if(!window.nativeHandle()) return nullptr;
	return std::make_unique<MacAccessibility>(window);
}

} // namespace affresco::platform
