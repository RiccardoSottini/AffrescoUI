/// @file
/// @brief The desktop helpers on Windows (Win32): the colour scheme from the user's personalisation settings, and a
/// menu bar at the top of the window. The window's procedure is subclassed to hear WM_COMMAND, as long as the bar
/// lives. A shortcut is shown after a tab, and runs through the window's key handling, so it never runs twice.

#include "platform/Desktop.hpp"

#include "affresco/render/Window.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstddef>
#include <string>
#include <utility>

namespace affresco::platform::desktop {

namespace {

constexpr const wchar_t* propertyName = L"AffrescoUI.menu"; ///< Where the window keeps its MenuBar.

/// @brief UTF-16 from UTF-8.
/// @param text The text.
/// @return It.
std::wstring wide(const std::string& text) {
	if(text.empty()) return {};
	const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring result(static_cast<std::size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
	return result;
}

/// @brief A menu bar on Windows.
class Win32MenuBar final : public MenuBar {
public:
	/// @brief An empty bar; starts hearing the window's commands.
	/// @param window The GLFW window.
	/// @param chosen Runs chosen items.
	Win32MenuBar(GLFWwindow* window, MenuChosen chosen)
	    : hwnd_(glfwGetWin32Window(window)), chosen_(std::move(chosen)) {
		SetPropW(hwnd_, propertyName, this);
		previous_ = reinterpret_cast<WNDPROC>(
		    SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Win32MenuBar::procedure)));
	}

	/// @brief Removes the bar and gives the window its procedure back.
	~Win32MenuBar() override {
		SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous_));
		RemovePropW(hwnd_, propertyName);
		replace(nullptr);
	}

	Win32MenuBar(const Win32MenuBar&) = delete;
	Win32MenuBar& operator=(const Win32MenuBar&) = delete;
	Win32MenuBar(Win32MenuBar&&) = delete;
	Win32MenuBar& operator=(Win32MenuBar&&) = delete;

	void show(const std::vector<MenuItem>& menus) override {
		actions_.clear();
		HMENU bar = nullptr;
		if(!menus.empty()) {
			bar = CreateMenu();
			for(const MenuItem& menu : menus) add(bar, menu);
		}
		replace(bar);
	}

	void activate() override {} // the bar belongs to its window

private:
	/// @brief Shows a new bar, destroying the old one.
	/// @param bar The new bar, or nullptr for none.
	void replace(HMENU bar) {
		HMENU old = GetMenu(hwnd_);
		SetMenu(hwnd_, bar);
		if(old) DestroyMenu(old);
	}

	/// @brief Adds an item, and its submenu, to a menu.
	/// @param menu The menu.
	/// @param item The item.
	void add(HMENU menu, const MenuItem& item) {
		if(item.separator) {
			AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
			return;
		}
		std::wstring title = wide(item.title);
		if(!item.keyLabel.empty()) title += L"\t" + wide(item.keyLabel);
		const UINT state = item.enabled ? MF_ENABLED : MF_GRAYED;
		if(!item.submenu.empty()) {
			HMENU submenu = CreatePopupMenu();
			for(const MenuItem& child : item.submenu) add(submenu, child);
			AppendMenuW(menu, MF_POPUP | MF_STRING | state, reinterpret_cast<UINT_PTR>(submenu), title.c_str());
			return;
		}
		if(actions_.size() >= 0xFFFF) return; // WM_COMMAND carries 16-bit ids
		actions_.push_back(item.run);
		AppendMenuW(menu, MF_STRING | state, actions_.size(), title.c_str()); // ids from 1: 0 is no command
	}

	/// @brief The window's procedure while the bar lives: runs chosen items, passes the rest on.
	/// @param hwnd The window.
	/// @param message The message.
	/// @param wParam Its first parameter.
	/// @param lParam Its second parameter.
	/// @return What the message returns.
	static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
		auto* self = static_cast<Win32MenuBar*>(GetPropW(hwnd, propertyName));
		if(!self) return DefWindowProcW(hwnd, message, wParam, lParam);
		if(message == WM_COMMAND && HIWORD(wParam) == 0 && lParam == 0) {
			const std::size_t id = LOWORD(wParam);
			if(id >= 1 && id <= self->actions_.size() && self->chosen_) {
				self->chosen_(self->actions_[id - 1]); // a copy: running it may replace the menus
				return 0;
			}
		}
		return CallWindowProcW(self->previous_, hwnd, message, wParam, lParam);
	}

	HWND hwnd_;                                  ///< Its window.
	MenuChosen chosen_;                          ///< Runs chosen items.
	WNDPROC previous_ = nullptr;                 ///< GLFW's procedure.
	std::vector<std::function<void()>> actions_; ///< What each item does, by id - 1.
};

} // namespace

ColorScheme colorScheme() {
	DWORD light = 1;
	DWORD size = sizeof(light);
	const LSTATUS status =
	    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		             L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
	return status == ERROR_SUCCESS && light == 0 ? ColorScheme::Dark : ColorScheme::Light;
}

bool reducedMotion() {
	BOOL animations = TRUE;
	return SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0) && !animations;
}

void watchColorScheme() {} // WM_SETTINGCHANGE reaches the window and wakes the event loop, which reads it again

void setApplicationIcon(const Bitmap* /*icon*/) {} // each window has its own: glfwSetWindowIcon

std::unique_ptr<MenuBar> createMenuBar(GLFWwindow* window, MenuChosen chosen) {
	return std::make_unique<Win32MenuBar>(window, std::move(chosen));
}

} // namespace affresco::platform::desktop
