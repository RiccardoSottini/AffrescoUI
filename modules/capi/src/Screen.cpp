/// @file
/// @brief The C API's screens: creating and showing them, their document and state, the host's callbacks, and peers.

#include "Handle.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <system_error>

using affresco::capi::checkedString;
using affresco::capi::checkedText;
using affresco::capi::Failure;
using affresco::capi::fromC;
using affresco::capi::guarded;
using affresco::capi::require;

namespace {

/// @brief The largest window, in window units or pixels.
constexpr int largestSize = affresco::Embedding::maximumSize;

/// @brief The most names options.native_actions may list.
constexpr std::size_t maximumNativeActions = 4096;

/// @brief Checks the options' mode.
/// @param mode The mode, as the host gave it: by reference, as loading a value no afr_mode has is undefined.
/// @throws Failure If it isn't an afr_mode.
void checkMode(const afr_mode& mode) {
	if(const int value = fromC(mode);
	   value != AFR_MODE_WINDOW && value != AFR_MODE_HEADLESS && value != AFR_MODE_EMBEDDED)
		throw Failure{AFR_ERROR_ARGUMENT, "options.mode is not an afr_mode"};
}

/// @brief Checks the options' window size and scale.
/// @param options The options.
/// @throws Failure If one is out of range.
void checkArea(const afr_options& options) {
	if(options.width < 0 || options.height < 0 || options.width > largestSize || options.height > largestSize)
		throw Failure{AFR_ERROR_ARGUMENT, "options.width and options.height must be 0 to 16384"};
	if(!std::isfinite(options.scale) || options.scale < 0.25f || options.scale > 16.0f)
		throw Failure{AFR_ERROR_ARGUMENT, "options.scale must be 0.25 to 16"};
}

/// @brief Reads the host's options, of this version or an older one (a smaller size), checking each.
/// @param given The host's, or NULL.
/// @return The options.
/// @throws Failure If one is out of range.
afr_options readOptions(const afr_options* given) {
	afr_options options{};
	static_cast<void>(afr_options_init(&options));
	if(given == nullptr) return options;
	if(given->size < offsetof(afr_options, mode) + sizeof(afr_mode) || given->size > sizeof(afr_options))
		throw Failure{AFR_ERROR_ARGUMENT, "options.size is not one this library knows (use afr_options_init())"};
	std::memcpy(&options, given, given->size);
	options.size = sizeof(afr_options);
	checkMode(options.mode);
	checkArea(options);
	if(options.mode == AFR_MODE_EMBEDDED && options.gl_loader == nullptr)
		throw Failure{AFR_ERROR_ARGUMENT, "options.gl_loader is required to embed"};
	return options;
}

/// @brief The window section's title and size, as the host's Runtime reads them, the options' size first.
struct WindowSettings {
	std::string title = "AffrescoUI"; ///< The title.
	int width = 800;                  ///< In window units.
	int height = 600;                 ///< In window units.
};

/// @brief Reads the document's "window" section.
/// @param screen The screen.
/// @param options The options, whose size wins.
/// @return The settings.
WindowSettings windowSettings(const affresco::Screen& screen, const afr_options& options) {
	WindowSettings settings;
	const affresco::json document = affresco::parseUntrusted(screen.documentJson());
	const affresco::json window =
	    document.is_object() ? document.value("window", affresco::json::object()) : affresco::json();
	if(window.is_object()) {
		if(window.contains("title") && window["title"].is_string()) settings.title = window["title"].get<std::string>();
		if(window.contains("width") && window["width"].is_number_integer())
			settings.width = std::clamp(window["width"].get<int>(), 1, largestSize);
		if(window.contains("height") && window["height"].is_number_integer())
			settings.height = std::clamp(window["height"].get<int>(), 1, largestSize);
	}
	if(options.width > 0) settings.width = options.width;
	if(options.height > 0) settings.height = options.height;
	return settings;
}

/// @brief Builds the screen into a window of the options' mode, and starts its channels.
/// @param handle The screen, its document loaded.
/// @param options The options.
void show(afr_screen& handle, const afr_options& options) {
	affresco::Screen& screen = *handle.screen; // NOLINT(bugprone-unchecked-optional-access): its document is loaded
	handle.mode = options.mode;
	handle.scale = options.scale;
	// GLFW on macOS moves the current folder into an app bundle's resources; a host's relative paths must keep theirs
	if(options.mode != AFR_MODE_EMBEDDED) glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
	if(options.mode == AFR_MODE_WINDOW) {
		handle.application.emplace(affresco::ApplicationOptions{nullptr, options.hidden != 0});
		handle.window = &screen.createWindow(*handle.application);
		return;
	}
	const WindowSettings settings = windowSettings(screen, options);
	if(options.mode == AFR_MODE_HEADLESS) {
		handle.application.emplace(affresco::ApplicationOptions{&handle.manualClock, true});
		handle.window = &handle.application->createWindow(settings.title, settings.width, settings.height);
		screen.setExecutor(&handle.tasks);
		screen.build(*handle.window);
		screen.startChannels(handle.tasks);
		return;
	}
	handle.embedding = std::make_unique<affresco::Embedding>(options.gl_loader, handle.steadyClock, settings.title,
	                                                         settings.width, settings.height);
	screen.embed(*handle.embedding);
	handle.window = &handle.embedding->window();
}

/// @brief Finishes a screen whose document is loaded: the socket folder, the event listener, the window.
/// @param handle The screen.
/// @param options The options.
/// @param out Receives it.
/// @return AFR_OK.
afr_status finish(std::unique_ptr<afr_screen> handle, const afr_options& options, afr_screen** out) {
	affresco::Screen& screen = *handle->screen; // NOLINT(bugprone-unchecked-optional-access): its document is loaded
	if(options.socket_directory != nullptr) {
		handle->socketDirectory = checkedString(options.socket_directory, "options.socket_directory", 4096);
		screen.setSocketDirectory(handle->socketDirectory);
	} else {
		handle->socketDirectory = affresco::Screen::defaultSocketDirectory();
	}
	if(options.native_actions != nullptr) // known from now on; afr_screen_on_action() gives each its callback
		for(std::size_t i = 0; options.native_actions[i] != nullptr; ++i) {
			if(i == maximumNativeActions)
				throw Failure{AFR_ERROR_ARGUMENT, "options.native_actions has too many names"};
			const std::string_view name = checkedString(options.native_actions[i], "options.native_actions", 256);
			if(name.empty()) throw Failure{AFR_ERROR_ARGUMENT, "options.native_actions has an empty name"};
			screen.onAction(std::string(name), [](affresco::Panel&) {});
		}
	const afr_screen* self = handle.get();
	screen.onEvent([self](const std::string& eventJson) {
		if(self->onEvent != nullptr) self->onEvent(self->onEventUser, eventJson.c_str());
	});
	show(*handle, options);
	*out = handle.release(); // the host's from now on, until afr_screen_destroy()
	return AFR_OK;
}

/// @brief Turns a path of the state, dotted or a JSON Pointer, into a JSON Pointer.
/// @param path "a.b.0", "/a/b/0" or "".
/// @return The pointer, e.g. "/a/b/0".
std::string pointerOf(std::string_view path) {
	if(path.empty() || path.front() == '/') return std::string(path);
	std::string pointer = "/";
	for(const char c : path) {
		if(c == '.') pointer += '/';
		else if(c == '~') pointer += "~0";
		else if(c == '/') pointer += "~1";
		else pointer += c;
	}
	return pointer;
}

/// @brief Applies a revision as the application, refused only by the document's deny rules.
/// @param handle The screen.
/// @param revision The revision.
/// @return AFR_OK.
/// @throws Failure If it is refused.
afr_status applyAsApplication(afr_screen& handle, const affresco::json& revision) {
	std::string error;
	// NOLINTNEXTLINE(bugprone-unchecked-optional-access): engaged from the handle's creation on
	if(!handle.screen->applyRevision(revision.dump(), &error)) throw Failure{AFR_ERROR_REFUSED, error};
	return AFR_OK;
}

/// @brief Checks a screen handle.
/// @param screen The handle.
/// @return It.
afr_screen& checked(afr_screen* screen) {
	require(screen, "screen");
	return *screen;
}

} // namespace

extern "C" {

afr_status afr_screen_create_from_file(const char* path, const afr_options* options, afr_screen** screen) {
	return guarded([&] {
		require(screen, "screen");
		*screen = nullptr;
		const std::filesystem::path file = std::filesystem::absolute(checkedString(path, "path", 4096));
		const afr_options settings = readOptions(options);
		auto handle = std::make_unique<afr_screen>();
		const auto history =
		    settings.history_in_memory != 0 ? affresco::Screen::History::InMemory : affresco::Screen::History::OnDisk;
		if(settings.data_directory != nullptr) {
			const std::filesystem::path data(checkedString(settings.data_directory, "options.data_directory", 4096));
			handle->screen.emplace(affresco::Screen::fromFile(file, data, history));
		} else if(settings.history_in_memory != 0) {
			handle->screen.emplace(affresco::Screen::fromFile(file, affresco::Screen::defaultDataDirectory(), history));
		} else {
			handle->screen.emplace(affresco::Screen::fromFile(file));
		}
		return finish(std::move(handle), settings, screen);
	});
}

afr_status afr_screen_create_from_string(const char* json, size_t length, const char* base_directory,
                                         const afr_options* options, afr_screen** screen) {
	return guarded([&] {
		require(screen, "screen");
		*screen = nullptr;
		const std::string_view text = checkedText(json, length, "json");
		std::filesystem::path base;
		if(base_directory != nullptr)
			base = std::filesystem::absolute(checkedString(base_directory, "base_directory", 4096));
		const afr_options settings = readOptions(options);
		auto handle = std::make_unique<afr_screen>();
		handle->screen.emplace(affresco::Screen::fromString(text, base));
		return finish(std::move(handle), settings, screen);
	});
}

void afr_screen_destroy(afr_screen* screen) {
	static_cast<void>(guarded([&] {
		const std::unique_ptr<afr_screen> owned(screen); // its members go in reverse: the screen before its window
		return AFR_OK;
	}));
}

afr_status afr_screen_replace_document(afr_screen* screen, const char* json, size_t length) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		affresco::json document = affresco::parseUntrusted(checkedText(json, length, "json"));
		if(!document.is_object()) throw Failure{AFR_ERROR_ARGUMENT, "the document must be a JSON object"};
		const affresco::json revision = {
		    {"author", "application"},
		    {"message", "Replace the document"},
		    {"patch", affresco::json::array({{{"op", "replace"}, {"path", ""}, {"value", std::move(document)}}})}};
		return applyAsApplication(handle, revision);
	});
}

afr_status afr_screen_get_state(afr_screen* screen, const char* path, char** json) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		require(json, "json");
		*json = nullptr;
		const affresco::json::json_pointer pointer(pointerOf(checkedString(path, "path", affresco::capi::maximumPath)));
		const affresco::json state = affresco::parseUntrusted(handle.screen->stateJson());
		if(!state.contains(pointer)) throw Failure{AFR_ERROR_NOT_FOUND, "the state has nothing at that path"};
		affresco::capi::copyOut(state.at(pointer).dump(), json);
		return AFR_OK;
	});
}

afr_status afr_screen_set_state(afr_screen* screen, const char* path, const char* json, size_t length) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		const std::string pointer = pointerOf(checkedString(path, "path", affresco::capi::maximumPath));
		if(pointer.empty()) throw Failure{AFR_ERROR_ARGUMENT, "path must name a value of the state"};
		affresco::json value = affresco::parseUntrusted(checkedText(json, length, "json"));
		const affresco::json revision = {
		    {"author", "application"}, {"message", "Set state"}, {"set", {{"/state" + pointer, std::move(value)}}}};
		return applyAsApplication(handle, revision);
	});
}

afr_status afr_screen_submit_revision(afr_screen* screen, const char* json, size_t length, char** result_json) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		if(result_json != nullptr) *result_json = nullptr;
		const std::string_view revision = checkedText(json, length, "json");
		const affresco::RevisionResult result = handle.screen->submitRevision(revision);
		const char* status = result.status == affresco::RevisionStatus::Applied    ? "applied"
		                     : result.status == affresco::RevisionStatus::Proposed ? "proposed"
		                                                                           : "rejected";
		if(result_json != nullptr) {
			const affresco::json report = {{"status", status},
			                               {"revision", result.revision},
			                               {"commit", result.commit},
			                               {"proposal", result.proposal},
			                               {"error", result.error}};
			affresco::capi::copyOut(report.dump(), result_json);
		}
		if(result.status == affresco::RevisionStatus::Rejected) throw Failure{AFR_ERROR_REFUSED, result.error};
		return AFR_OK;
	});
}

afr_status afr_screen_on_action(afr_screen* screen, const char* name, afr_action_callback callback, void* user) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		if(callback == nullptr) throw Failure{AFR_ERROR_ARGUMENT, "callback is NULL"};
		const std::string action(checkedString(name, "name", 256));
		handle.screen->onAction(action, [callback, user, name = action](affresco::Panel& sender) {
			callback(user, name.c_str(), sender.id().c_str());
		});
		return AFR_OK;
	});
}

afr_status afr_screen_on_event(afr_screen* screen, afr_event_callback callback, void* user) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		handle.onEvent = callback;
		handle.onEventUser = user;
		return AFR_OK;
	});
}

afr_status afr_screen_on_call(afr_screen* screen, const char* name, afr_call_callback callback, void* user) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		if(callback == nullptr) throw Failure{AFR_ERROR_ARGUMENT, "callback is NULL"};
		handle.screen->onCall(std::string(checkedString(name, "name", 256)),
		                      [callback, user](const std::string& argumentsJson) {
			                      afr_call call;
			                      callback(user, argumentsJson.c_str(), &call);
			                      if(call.error) throw std::runtime_error(*call.error);
			                      return call.result;
		                      });
		return AFR_OK;
	});
}

afr_status afr_call_return(afr_call* call, const char* json, size_t length) {
	return guarded([&] {
		require(call, "call");
		const std::string_view text = checkedText(json, length, "json");
		static_cast<void>(affresco::parseUntrusted(text)); // a JSON value, or an AFR_ERROR_ARGUMENT
		call->result.assign(text);
		call->error.reset();
		return AFR_OK;
	});
}

afr_status afr_call_fail(afr_call* call, const char* message) {
	return guarded([&] {
		require(call, "call");
		call->error.emplace(checkedString(message, "message", 65536));
		return AFR_OK;
	});
}

afr_status afr_screen_post(afr_screen* screen, afr_task task, void* user) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		if(task == nullptr) throw Failure{AFR_ERROR_ARGUMENT, "task is NULL"};
		handle.executor().post([task, user] { task(user); });
		return AFR_OK;
	});
}

afr_status afr_screen_listen(afr_screen* screen, const char* name, const char* author) {
	return guarded([&] {
		afr_screen& handle = checked(screen);
		const std::string_view file = checkedString(name, "name", 255);
		const std::string_view peer = checkedString(author, "author", 256);
		if(file.empty() || file == "." || file == ".." || file.find_first_of("/\\") != std::string_view::npos)
			throw Failure{AFR_ERROR_ARGUMENT, "name must be a file name, without folders"};
		if(peer.empty()) throw Failure{AFR_ERROR_ARGUMENT, "author is empty"};
		std::error_code error;
		std::filesystem::create_directories(handle.socketDirectory, error);
		handle.screen->listen(handle.executor(), handle.socketDirectory / file, std::string(peer));
		return AFR_OK;
	});
}

} // extern "C"
