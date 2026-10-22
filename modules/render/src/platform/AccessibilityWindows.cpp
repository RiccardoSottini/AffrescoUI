/// @file
/// @brief The accessibility adapter on Windows: the window procedure answers WM_GETOBJECT through AccessKit, which
/// then serves UI Automation (Narrator) from the tree AccessKitTree builds. The window is subclassed with comctl32's
/// SetWindowSubclass, beside GLFW's own procedure.

#include "platform/Accessibility.hpp"
#include "platform/AccessKitTree.hpp"

#include "affresco/render/Window.hpp"

#include <commctrl.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace affresco::platform {

namespace {

/// @brief Raises the events AccessKit queued, if any.
/// @param events What an update returned; may be null.
void raise(accesskit_windows_queued_events* events) {
	if(events) accesskit_windows_queued_events_raise(events);
}

/// @brief A window's adapter on Windows.
class WindowsAccessibility final : public AccessibilityAdapter {
public:
	/// @brief Creates AccessKit's adapter and subclasses the window.
	/// @param window The window, shown in an OS window.
	explicit WindowsAccessibility(Window& window)
	    : window_(&window), handle_(glfwGetWin32Window(window.nativeHandle())), tree_(window, window.contentScale()) {
		focused_ = glfwGetWindowAttrib(window.nativeHandle(), GLFW_FOCUSED) != 0;
		adapter_ = accesskit_windows_adapter_new(handle_, focused_, AccessKitTree::act, &tree_);
		SetWindowSubclass(handle_, procedure, subclassId, reinterpret_cast<DWORD_PTR>(this));
	}
	/// @brief Removes the subclass, then frees the adapter: no callback reaches the tree afterwards.
	~WindowsAccessibility() override {
		RemoveWindowSubclass(handle_, procedure, subclassId);
		accesskit_windows_adapter_free(adapter_);
	}
	WindowsAccessibility(const WindowsAccessibility&) = delete;
	WindowsAccessibility& operator=(const WindowsAccessibility&) = delete;

	/// @brief Runs the actions asked for, and tells AccessKit when the window gains or loses the focus.
	void poll() override {
		tree_.runActions();
		const bool focused = glfwGetWindowAttrib(window_->nativeHandle(), GLFW_FOCUSED) != 0;
		if(focused != focused_) {
			focused_ = focused;
			raise(accesskit_windows_adapter_update_window_focus_state(adapter_, focused));
		}
		if(tree_.takeStale()) frameDrawn();
	}
	/// @brief Sends what changed in the tree, while Narrator (or another client) reads it.
	void frameDrawn() override {
		tree_.setScale(window_->contentScale());
		if(tree_.readChanges())
			raise(accesskit_windows_adapter_update_if_active(adapter_, AccessKitTree::changesOf, &tree_));
	}
	/// @brief Sends the new focus now.
	void focusChanged() override { frameDrawn(); }

private:
	/// @brief The id of the subclass, among others of the window.
	static constexpr UINT_PTR subclassId = 0x41636365; // "Acce"

	/// @brief The subclass's window procedure: WM_GETOBJECT goes to AccessKit, the rest to GLFW.
	/// @param handle The window.
	/// @param message The message.
	/// @param wparam Its first parameter.
	/// @param lparam Its second parameter.
	/// @param id The subclass's id.
	/// @param data The WindowsAccessibility.
	/// @return The message's result.
	static LRESULT CALLBACK procedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id,
	                                  DWORD_PTR data) {
		static_cast<void>(id);
		if(message == WM_GETOBJECT) {
			auto* self = reinterpret_cast<WindowsAccessibility*>(data);
			const accesskit_opt_lresult result = accesskit_windows_adapter_handle_wm_getobject(
			    self->adapter_, wparam, lparam, AccessKitTree::activate, &self->tree_);
			if(result.has_value) return result.value;
		}
		return DefSubclassProc(handle, message, wparam, lparam);
	}

	Window* window_;                     ///< The window.
	HWND handle_;                        ///< Its OS window.
	AccessKitTree tree_;                 ///< Its tree; the adapter's callbacks point at it.
	accesskit_windows_adapter* adapter_; ///< AccessKit's adapter.
	bool focused_ = false;               ///< Whether the window had the focus at the last poll().
};

} // namespace

std::unique_ptr<AccessibilityAdapter> createAccessibilityAdapter(Window& window) {
	if(!window.nativeHandle()) return nullptr;
	return std::make_unique<WindowsAccessibility>(window);
}

} // namespace affresco::platform
