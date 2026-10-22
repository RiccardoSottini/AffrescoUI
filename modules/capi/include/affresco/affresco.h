/**
 * @file
 * @brief The AffrescoUI C API: load a screen document, read and change its state, answer its actions and calls,
 * and show it in a window of its own, headless, or inside the host application's OpenGL context. C11; usable from
 * C++ and from any language with a C foreign function interface (the Python binding uses ctypes).
 *
 * Conventions:
 * - Every function returns an afr_status (or a value documented otherwise); on failure, afr_last_error() says why.
 * - Strings are UTF-8. A string given with a length need not end with a NUL; one given without must.
 * - A string or buffer the library returns through a `char**` belongs to the caller, who frees it with afr_free().
 * - Pointers and lengths are checked: NULL where a value is required gives AFR_ERROR_ARGUMENT, never a crash.
 * - No C++ exception crosses this boundary.
 *
 * Threads: a screen belongs to the thread that created it, and every function taking it must be called on that thread,
 * except afr_screen_post(), which is safe from any thread. A screen with its own window (AFR_MODE_WINDOW) or a headless
 * one (AFR_MODE_HEADLESS) uses the windowing system, which is process-wide: create it on the main thread, and only one
 * such screen may exist at a time. Embedded screens (AFR_MODE_EMBEDDED) need no windowing system and have no limit:
 * each is used on the thread whose OpenGL context it draws into.
 *
 * Permissions: the C API adds no way around them. A native call a document reaches still needs the host's grant
 * (afr_screen_on_call()), and revisions follow the document's permission rules like any peer's.
 *
 * Embedding (AFR_MODE_EMBEDDED): the host makes its OpenGL 4.1 core (or later) context current and calls
 * afr_screen_frame() or afr_screen_render_texture(). The library saves the OpenGL state it changes and restores it
 * before returning: the draw and read framebuffer bindings, the renderbuffer binding, viewport, scissor box and test,
 * blending (enable, functions, equations), colour mask, clear colour, depth and stencil tests, face culling,
 * GL_FRAMEBUFFER_SRGB, the program, the vertex array, the array buffer, the active texture unit and the 2D textures of
 * units 0 and 1, the pixel pack and unpack buffers, and GL_PACK_ALIGNMENT, GL_UNPACK_ALIGNMENT and
 * GL_UNPACK_ROW_LENGTH. It draws with depth, stencil, culling and sRGB off whatever the host set. Its own objects
 * (a texture, a framebuffer, shaders, buffers) live until afr_screen_destroy(), which must run with the context
 * current.
 */

#ifndef AFFRESCO_AFFRESCO_H
#define AFFRESCO_AFFRESCO_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(AFR_SHARED)
#ifdef AFR_BUILDING
#define AFR_API __declspec(dllexport)
#else
#define AFR_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(AFR_SHARED)
#define AFR_API __attribute__((visibility("default")))
#else
#define AFR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The version of this header: a library with the same major version and at least this minor one fits. */
#define AFR_VERSION_MAJOR 0
#define AFR_VERSION_MINOR 1 /**< See AFR_VERSION_MAJOR. */
#define AFR_VERSION_PATCH 0 /**< See AFR_VERSION_MAJOR. */
/** @brief The version packed as afr_version() returns it. */
#define AFR_VERSION ((AFR_VERSION_MAJOR << 16) | (AFR_VERSION_MINOR << 8) | AFR_VERSION_PATCH)

/** @brief What a function did. */
/* NOLINTNEXTLINE(cppcoreguidelines-use-enum-class): a C API */
typedef enum afr_status {
	AFR_OK = 0,              /**< It succeeded. */
	AFR_ERROR_ARGUMENT = 1,  /**< An argument is missing, too long, out of range or not valid UTF-8 or JSON. */
	AFR_ERROR_DOCUMENT = 2,  /**< The document can't be read or built. */
	AFR_ERROR_STATE = 3,     /**< The call doesn't fit the screen's mode, or comes at the wrong time. */
	AFR_ERROR_NOT_FOUND = 4, /**< The path names nothing. */
	AFR_ERROR_REFUSED = 5,   /**< The document's rules refused a revision. */
	AFR_ERROR_INTERNAL = 6   /**< Anything else (out of memory, OpenGL); see afr_last_error(). */
} afr_status;

/** @brief Where a screen is shown. */
/* NOLINTNEXTLINE(cppcoreguidelines-use-enum-class): a C API */
typedef enum afr_mode {
	AFR_MODE_WINDOW = 0, /**< In a window of its own, run by afr_screen_run(). */
	AFR_MODE_HEADLESS =
	    1, /**< Offscreen, on a clock moved by afr_screen_step(); pictures by afr_screen_render_rgba(). */
	AFR_MODE_EMBEDDED = 2 /**< Inside the host's current OpenGL context: afr_screen_frame(). */
} afr_mode;

/** @brief A mouse event's kind. */
/* NOLINTNEXTLINE(cppcoreguidelines-use-enum-class): a C API */
typedef enum afr_mouse_action {
	AFR_MOUSE_MOVE = 0,    /**< The pointer moved. */
	AFR_MOUSE_PRESS = 1,   /**< A button went down. */
	AFR_MOUSE_RELEASE = 2, /**< A button went up. */
	AFR_MOUSE_ENTER = 3,   /**< The pointer entered the screen. */
	AFR_MOUSE_LEAVE = 4    /**< The pointer left the screen. */
} afr_mouse_action;

/** @brief A key event's kind (GLFW's values). */
/* NOLINTNEXTLINE(cppcoreguidelines-use-enum-class): a C API */
typedef enum afr_key_action {
	AFR_KEY_RELEASE = 0, /**< The key went up. */
	AFR_KEY_PRESS = 1,   /**< The key went down. */
	AFR_KEY_REPEAT = 2   /**< The key is held, and repeats. */
} afr_key_action;

/** @brief Modifier bits (GLFW's values). */
/* NOLINTNEXTLINE(cppcoreguidelines-use-enum-class): a C API */
enum {
	AFR_MOD_SHIFT = 0x1,   /**< Shift. */
	AFR_MOD_CONTROL = 0x2, /**< Control. */
	AFR_MOD_ALT = 0x4,     /**< Alt, or Option. */
	AFR_MOD_SUPER = 0x8    /**< Command, or the Windows key. */
};

/** @brief Looks an OpenGL function of the host's context up by name (glfwGetProcAddress and the like). */
typedef void* (*afr_gl_loader)(const char* name);

/** @brief How a screen is created. Fill it with afr_options_init(), then change what you need. */
typedef struct afr_options {
	size_t size;                  /**< sizeof(afr_options), set by afr_options_init(): lets the struct grow. */
	afr_mode mode;                /**< Where it is shown (AFR_MODE_WINDOW). */
	const char* socket_directory; /**< Where its live channels and listen() sockets go, or NULL for the default. */
	const char* data_directory;   /**< Where a file's history is kept (from_file only), or NULL beside the file. */
	int history_in_memory;        /**< Nonzero: keep the history in memory only (from_file only). */
	int width;                    /**< Headless: the width in window units; 0 (default): the document's, or 800. */
	int height;                   /**< Headless: the height in window units; 0 (default): the document's, or 600. */
	float scale;                  /**< Headless: pixels per window unit for afr_screen_render_rgba(), whole (1). */
	int hidden;                   /**< Window: nonzero opens it hidden (tests). */
	afr_gl_loader gl_loader;      /**< Embedded: the host's function loader (required). */
	/** The native actions the host will register with afr_screen_on_action(), NULL-terminated, or NULL: the document is
	 *  checked when the screen is created, and names it runs that aren't defined or listed here are reported. */
	const char* const* native_actions;
} afr_options;

/** @brief A screen: a document, its state and what shows it. Opaque. */
typedef struct afr_screen afr_screen;

/** @brief A native call in progress: answer it with afr_call_return() or afr_call_fail(). Opaque. */
typedef struct afr_call afr_call;

/**
 * @brief Runs when the document runs a native action the host registered.
 * @param user What was given with it.
 * @param action The action's name.
 * @param sender The id of the panel that ran it, or "" if it has none.
 */
typedef void (*afr_action_callback)(void* user, const char* action, const char* sender);

/**
 * @brief Runs for each event the screen reports: a revision applied, a proposal, an action run (as JSON text).
 * @param user What was given with it.
 * @param event_json The event, a JSON object.
 */
typedef void (*afr_event_callback)(void* user, const char* event_json);

/**
 * @brief Runs when the document calls a native function the host registered (its grant).
 * @param user What was given with it.
 * @param arguments_json The arguments, JSON text.
 * @param call Answer with afr_call_return() or afr_call_fail() before returning; no answer returns null.
 */
typedef void (*afr_call_callback)(void* user, const char* arguments_json, afr_call* call);

/**
 * @brief A task afr_screen_post() runs on the screen's thread.
 * @param user What was given with it.
 */
typedef void (*afr_task)(void* user);

/** @brief The library's version: (major << 16) | (minor << 8) | patch. */
AFR_API uint32_t afr_version(void);

/** @brief Why the last call on this thread failed, or "" after a success. Valid until the next call on the thread. */
AFR_API const char* afr_last_error(void);

/** @brief Frees what the library returned through a `char**` or `uint8_t**`. NULL is ignored. */
AFR_API void afr_free(void* memory);

/** @brief Sets the defaults, and size. @param options The options. */
AFR_API afr_status afr_options_init(afr_options* options);

/**
 * @brief Loads a screen from a file, its history kept on disk beside it (or as the options say).
 * @param path The file's path, UTF-8.
 * @param options The options, or NULL for the defaults.
 * @param screen Receives the screen.
 */
AFR_API afr_status afr_screen_create_from_file(const char* path, const afr_options* options, afr_screen** screen);

/**
 * @brief Loads a screen from JSON text, its history in memory.
 * @param json The document.
 * @param length Its length in bytes.
 * @param base_directory Where relative paths in it are resolved (fonts, images), or NULL for the current directory.
 * @param options The options, or NULL for the defaults.
 * @param screen Receives the screen.
 */
AFR_API afr_status afr_screen_create_from_string(const char* json, size_t length, const char* base_directory,
                                                 const afr_options* options, afr_screen** screen);

/** @brief Closes a screen and frees it (embedded: with the host's context current). NULL is ignored. */
AFR_API void afr_screen_destroy(afr_screen* screen);

/**
 * @brief Replaces the whole document, as a revision by "application" (the rules decide, as for any revision).
 * @param screen The screen.
 * @param json The new document.
 * @param length Its length in bytes.
 */
AFR_API afr_status afr_screen_replace_document(afr_screen* screen, const char* json, size_t length);

/**
 * @brief Reads the state, or a part of it.
 * @param screen The screen.
 * @param path "items.0.title" (dots), "/items/0/title" (a JSON Pointer), or "" for the whole state.
 * @param json Receives the value as JSON text; free it with afr_free().
 */
AFR_API afr_status afr_screen_get_state(afr_screen* screen, const char* path, char** json);

/**
 * @brief Sets a value of the state, as a revision by "application".
 * @param screen The screen.
 * @param path As for afr_screen_get_state(); not "".
 * @param json The value, JSON text.
 * @param length Its length in bytes.
 */
AFR_API afr_status afr_screen_set_state(afr_screen* screen, const char* path, const char* json, size_t length);

/**
 * @brief Submits a revision: {"author", "message", "patch": [...]} or {"set": {"/state/a": 1}} (SCREEN_FORMAT.md).
 * @param screen The screen.
 * @param json The revision.
 * @param length Its length in bytes.
 * @param result_json Receives {"status": "applied"|"proposed"|"rejected", ...}, or NULL; free it with afr_free().
 * @return AFR_OK if applied or proposed, AFR_ERROR_REFUSED if rejected.
 */
AFR_API afr_status afr_screen_submit_revision(afr_screen* screen, const char* json, size_t length, char** result_json);

/**
 * @brief Answers an action the document runs by name ("onClick": "name", or a {"call": "name"} step) when the
 * document has no action of that name.
 * @param screen The screen.
 * @param name The action's name.
 * @param callback What runs, on the screen's thread (required).
 * @param user Given to it.
 */
AFR_API afr_status afr_screen_on_action(afr_screen* screen, const char* name, afr_action_callback callback, void* user);

/**
 * @brief Reports every event of the screen (one listener; another call replaces it, NULL removes it).
 * @param screen The screen.
 * @param callback What runs, on the screen's thread.
 * @param user Given to it.
 */
AFR_API afr_status afr_screen_on_event(afr_screen* screen, afr_event_callback callback, void* user);

/**
 * @brief Grants the document a native function by name, and answers it.
 * @param screen The screen.
 * @param name The function's name.
 * @param callback What runs, on the screen's thread.
 * @param user Given to it.
 */
AFR_API afr_status afr_screen_on_call(afr_screen* screen, const char* name, afr_call_callback callback, void* user);

/**
 * @brief Answers a native call with a value.
 * @param call The call, inside its callback.
 * @param json The value, JSON text.
 * @param length Its length in bytes.
 */
AFR_API afr_status afr_call_return(afr_call* call, const char* json, size_t length);

/**
 * @brief Answers a native call with an error, which the document's action sees.
 * @param call The call, inside its callback.
 * @param message Why, UTF-8.
 */
AFR_API afr_status afr_call_fail(afr_call* call, const char* message);

/**
 * @brief Queues a task for the screen's thread: it runs inside afr_screen_run(), afr_screen_step() or a frame.
 * Safe to call from any thread.
 * @param screen The screen.
 * @param task The task.
 * @param user Given to it.
 */
AFR_API afr_status afr_screen_post(afr_screen* screen, afr_task task, void* user);

/** @brief Window: shows the window and runs until it is closed or afr_screen_quit(). @param screen The screen. */
AFR_API afr_status afr_screen_run(afr_screen* screen);

/** @brief Window: makes afr_screen_run() return. @param screen The screen. */
AFR_API afr_status afr_screen_quit(afr_screen* screen);

/**
 * @brief Headless or embedded: runs posted tasks, timers and deferred work. Headless moves its clock first.
 * @param screen The screen.
 * @param milliseconds How far to move the clock (0 to 1e9), headless; ignored when embedded.
 */
AFR_API afr_status afr_screen_step(afr_screen* screen, double milliseconds);

/**
 * @brief Sends a mouse event. Coordinates are in window units, from the top left.
 * @param screen The screen.
 * @param action What happened.
 * @param x Where, horizontally.
 * @param y Where, vertically.
 * @param button 0 left, 1 right, 2 middle, up to 7 (press and release).
 * @param modifiers AFR_MOD_* bits.
 */
AFR_API afr_status afr_screen_mouse(afr_screen* screen, afr_mouse_action action, double x, double y, int button,
                                    int modifiers);

/**
 * @brief Sends a key event.
 * @param screen The screen.
 * @param key A GLFW key code (GLFW_KEY_*; -1 for an unknown key).
 * @param action What happened.
 * @param modifiers AFR_MOD_* bits.
 */
AFR_API afr_status afr_screen_key(afr_screen* screen, int key, afr_key_action action, int modifiers);

/**
 * @brief Sends typed text, one character event per code point.
 * @param screen The screen.
 * @param utf8 The text.
 * @param length Its length in bytes (at most 65536).
 */
AFR_API afr_status afr_screen_text(afr_screen* screen, const char* utf8, size_t length);

/**
 * @brief Sends a scroll (wheel or trackpad) at a point.
 * @param screen The screen.
 * @param x Where, in window units.
 * @param y Where, in window units.
 * @param dx How far, horizontally.
 * @param dy How far, vertically.
 */
AFR_API afr_status afr_screen_scroll(afr_screen* screen, double x, double y, double dx, double dy);

/**
 * @brief Embedded: draws the screen over the host's framebuffer, its whole size, premultiplied alpha.
 * @param screen The screen.
 * @param width The framebuffer's width, in pixels.
 * @param height Its height, in pixels.
 * @param scale Pixels per window unit.
 * @param framebuffer The framebuffer (0: the default one).
 */
AFR_API afr_status afr_screen_frame(afr_screen* screen, int width, int height, float scale, uint32_t framebuffer);

/**
 * @brief Embedded: draws the screen into a texture the library owns (RGBA8, premultiplied alpha, first row at the
 * bottom), valid until the next call or afr_screen_destroy().
 * @param screen The screen.
 * @param width In pixels.
 * @param height In pixels.
 * @param scale Pixels per window unit.
 * @param texture Receives the texture's OpenGL name.
 */
AFR_API afr_status afr_screen_render_texture(afr_screen* screen, int width, int height, float scale, uint32_t* texture);

/**
 * @brief Renders a picture from scratch and reads it back. Headless and window: at the options' scale (whole);
 * embedded: at the last frame's scale.
 * @param screen The screen.
 * @param width Receives its width in pixels (window width times the scale).
 * @param height Receives its height in pixels.
 * @param rgba Receives width * height * 4 bytes, premultiplied RGBA, top row first; free it with afr_free().
 */
AFR_API afr_status afr_screen_render_rgba(afr_screen* screen, int* width, int* height, uint8_t** rgba);

/**
 * @brief Accepts peers (the Studio, the MCP server, other programs) on a local socket.
 * @param screen The screen.
 * @param name The socket's file name, inside the socket directory (no separators), e.g. "agent.sock".
 * @param author The name its peers' changes are authored as, which the document's "permissions" know them by, e.g.
 * "agent".
 */
AFR_API afr_status afr_screen_listen(afr_screen* screen, const char* name, const char* author);

#ifdef __cplusplus
}
#endif

#endif
