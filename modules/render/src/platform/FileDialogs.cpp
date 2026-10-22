/// @file
/// @brief File dialogs (open, save, choose a folder), shown by the OS through nativefiledialog-extended: Cocoa on
/// macOS, the shell's dialogs on Windows, the desktop portal over D-Bus elsewhere. Built without them (Linux without
/// D-Bus), there are none.

#include "platform/Desktop.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Window.hpp"

#include <string>
#include <vector>

#ifndef AFFRESCOUI_NO_FILE_DIALOGS
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <nfd.h>
#endif

namespace affresco::platform::desktop {

#ifdef AFFRESCOUI_NO_FILE_DIALOGS

bool fileDialogs() noexcept { return false; }

std::vector<std::string> chooseFiles(GLFWwindow*, const FileDialog&) { return {}; }

#else

namespace {

/// @brief The window a dialog belongs to, so that it shows over it (on Windows and macOS; none elsewhere).
/// @param window The GLFW window.
/// @return Its native handle.
nfdwindowhandle_t parentOf([[maybe_unused]] GLFWwindow* window) {
#if defined(_WIN32)
	return {NFD_WINDOW_HANDLE_TYPE_WINDOWS, glfwGetWin32Window(window)};
#elif defined(__APPLE__)
	return {NFD_WINDOW_HANDLE_TYPE_COCOA, glfwGetCocoaWindow(window)};
#else
	return {NFD_WINDOW_HANDLE_TYPE_UNSET, nullptr};
#endif
}

/// @brief A dialog's filters as nativefiledialog-extended takes them, with the strings they point to.
struct Filters {
	std::vector<std::string> specs;       ///< Each filter's extensions, comma-separated ("png,jpg").
	std::vector<nfdu8filteritem_t> items; ///< Name and spec of each, pointing into the dialog and specs.

	/// @brief Converts a dialog's filters, leaving out those without extensions.
	/// @param dialog The dialog.
	explicit Filters(const FileDialog& dialog) {
		specs.reserve(dialog.filters.size()); // never moved: items point into them
		for(const FileFilter& filter : dialog.filters) {
			std::string spec;
			for(const std::string& extension : filter.extensions) {
				if(extension.empty()) continue;
				if(!spec.empty()) spec += ',';
				spec += extension;
			}
			if(spec.empty()) continue;
			specs.push_back(std::move(spec));
			items.push_back({filter.name.c_str(), specs.back().c_str()});
		}
	}

	/// @brief How many there are.
	[[nodiscard]] nfdfiltersize_t count() const { return static_cast<nfdfiltersize_t>(items.size()); }
	/// @brief The first, or nullptr.
	[[nodiscard]] const nfdu8filteritem_t* list() const { return items.empty() ? nullptr : items.data(); }
};

/// @brief A string for nativefiledialog-extended: nullptr when empty (its default).
/// @param text The string.
/// @return It, or nullptr.
const char* orNull(const std::string& text) { return text.empty() ? nullptr : text.c_str(); }

/// @brief The paths of an open dialog with several files chosen; frees them.
/// @param set The set nativefiledialog-extended returned.
/// @return The paths.
std::vector<std::string> takePaths(const nfdpathset_t* set) {
	std::vector<std::string> paths;
	nfdpathsetsize_t count = 0;
	if(NFD_PathSet_GetCount(set, &count) == NFD_OKAY)
		for(nfdpathsetsize_t i = 0; i < count; ++i) {
			nfdu8char_t* path = nullptr;
			if(NFD_PathSet_GetPathU8(set, i, &path) != NFD_OKAY || !path) continue;
			paths.emplace_back(path);
			NFD_PathSet_FreePathU8(path);
		}
	NFD_PathSet_Free(set);
	return paths;
}

/// @brief Shows a dialog, nativefiledialog-extended being started.
/// @param window The GLFW window.
/// @param dialog What it asks for.
/// @return The paths chosen; none if cancelled.
/// @throws Nothing; a failure is logged and returns none.
std::vector<std::string> show(GLFWwindow* window, const FileDialog& dialog) {
	const Filters filters(dialog);
	const nfdwindowhandle_t parent = parentOf(window);
	nfdresult_t result = NFD_ERROR;
	nfdu8char_t* path = nullptr;
	switch(dialog.kind) {
	case FileDialogKind::Open:
		if(dialog.multiple) {
			const nfdpathset_t* set = nullptr;
			const nfdopendialogu8args_t args{
			    filters.list(), filters.count(), orNull(dialog.folder), parent, orNull(dialog.title), nullptr, nullptr};
			result = NFD_OpenDialogMultipleU8_With(&set, &args);
			if(result == NFD_OKAY) return takePaths(set);
		} else {
			const nfdopendialogu8args_t args{
			    filters.list(), filters.count(), orNull(dialog.folder), parent, orNull(dialog.title), nullptr, nullptr};
			result = NFD_OpenDialogU8_With(&path, &args);
		}
		break;
	case FileDialogKind::Save: {
		const nfdsavedialogu8args_t args{
		    filters.list(), filters.count(), orNull(dialog.folder), orNull(dialog.name), parent, orNull(dialog.title),
		    nullptr,        nullptr};
		result = NFD_SaveDialogU8_With(&path, &args);
		break;
	}
	case FileDialogKind::Folder: {
		nfdpickfolderu8args_t args{orNull(dialog.folder), parent, orNull(dialog.title), nullptr, nullptr};
		result = NFD_PickFolderU8_With(&path, &args);
		break;
	}
	}
	if(result == NFD_ERROR) {
		const char* error = NFD_GetError();
		log(LogLevel::Warning, std::string("AffrescoUI: the file dialog failed: ") + (error ? error : "unknown error"));
		return {};
	}
	if(result != NFD_OKAY || !path) return {};
	std::vector<std::string> paths{path};
	NFD_FreePathU8(path);
	return paths;
}

} // namespace

bool fileDialogs() noexcept { return true; }

std::vector<std::string> chooseFiles(GLFWwindow* window, const FileDialog& dialog) {
	if(NFD_Init() != NFD_OKAY) {
		const char* error = NFD_GetError();
		log(LogLevel::Warning,
		    std::string("AffrescoUI: file dialogs aren't available: ") + (error ? error : "unknown error"));
		return {};
	}
	std::vector<std::string> paths = show(window, dialog);
	NFD_Quit();
	return paths;
}

#endif

} // namespace affresco::platform::desktop
