/// @file
/// @brief The C API's running and drawing: the window's loop, the headless step, the host's input, and the pictures
/// (frames over the host's framebuffer, its texture, RGBA pixels).

#include "Handle.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

using affresco::capi::Failure;
using affresco::capi::fromC;
using affresco::capi::guarded;
using affresco::capi::require;

namespace {

/// @brief GLFW_KEY_LAST: the largest key code.
constexpr int lastKey = 348;

/// @brief The modifier bits GLFW sends: Shift, Control, Alt, Super, Caps Lock and Num Lock.
constexpr int allModifiers = 0x3F;

/// @brief The longest text afr_screen_text() takes, in bytes.
constexpr std::size_t longestText = 65536;

/// @brief The widest and highest picture afr_screen_render_rgba() draws, in pixels: an embedded screen's limit.
constexpr int largestPicture = affresco::Embedding::maximumSize;

/// @brief Checks a screen handle, and that it is in one of the modes given.
/// @param screen The handle.
/// @param first A mode it may be in.
/// @param second Another, or the same.
/// @return It.
/// @throws Failure If it is NULL or in another mode.
afr_screen& inMode(afr_screen* screen, afr_mode first, afr_mode second) {
	require(screen, "screen");
	if(screen->mode != first && screen->mode != second)
		throw Failure{AFR_ERROR_STATE, "this call doesn't fit the screen's mode"};
	return *screen;
}

/// @brief Checks a screen handle, in any mode.
/// @param screen The handle.
/// @return It.
afr_screen& anyMode(afr_screen* screen) {
	require(screen, "screen");
	return *screen;
}

/// @brief Throws if a coordinate isn't a finite number.
/// @param value It.
/// @param what Its name.
void requireFinite(double value, const char* what) {
	if(!std::isfinite(value)) throw Failure{AFR_ERROR_ARGUMENT, std::string(what) + " is not a finite number"};
}

/// @brief How many continuation bytes follow a UTF-8 lead byte.
/// @param lead The byte, 0x80 or more.
/// @return 1 to 3, or 0 for a byte no well-formed sequence starts with: a continuation byte, C0, C1, or F5 and up.
int continuationBytes(unsigned char lead) noexcept {
	if(lead > 0xF4) return 0;
	return lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC2 ? 1 : 0;
}

/// @brief Whether a decoded value is a Unicode scalar value in its shortest form.
/// @param value The value.
/// @param extra The continuation bytes it took: 1 to 3.
/// @return False for an overlong form, a surrogate, or a value past U+10FFFF.
bool isShortestScalar(char32_t value, int extra) noexcept {
	const char32_t smallest = extra == 1 ? 0x80 : extra == 2 ? 0x800 : 0x10000;
	return value >= smallest && value <= 0x10FFFF && (value < 0xD800 || value > 0xDFFF);
}

/// @brief Decodes one UTF-8 code point, rejecting overlong forms, surrogates and values past U+10FFFF.
/// @param text The text.
/// @param at Where it starts, before the end; moved past it.
/// @return The code point, or 0xFFFFFFFF if it is malformed.
char32_t decode(std::string_view text, std::size_t& at) noexcept {
	const auto lead = static_cast<unsigned char>(text[at++]);
	if(lead < 0x80) return lead;
	const int extra = continuationBytes(lead);
	if(extra == 0 || at + static_cast<std::size_t>(extra) > text.size()) return 0xFFFFFFFF;
	char32_t value = lead & (0x3F >> extra);
	for(int i = 0; i < extra; ++i) {
		const auto next = static_cast<unsigned char>(text[at++]);
		if((next & 0xC0) != 0x80) return 0xFFFFFFFF;
		value = (value << 6) | (next & 0x3F);
	}
	return isShortestScalar(value, extra) ? value : 0xFFFFFFFF;
}

/// @brief Draws a headless or window screen from scratch, and widens its RGB to opaque RGBA.
/// @param handle The screen.
/// @param scale Whole pixels per window unit.
/// @return RGBA, top row first.
std::vector<std::uint8_t> windowPixels(const afr_screen& handle, int scale) {
	const std::vector<std::uint8_t> rgb = affresco::detail::drawFromScratch(*handle.window, scale);
	std::vector<std::uint8_t> rgba(rgb.size() / 3 * 4);
	for(std::size_t from = 0, to = 0; from + 2 < rgb.size(); from += 3, to += 4) {
		std::memcpy(&rgba[to], &rgb[from], 3);
		rgba[to + 3] = 255;
	}
	return rgba;
}

} // namespace

extern "C" {

afr_status afr_screen_run(afr_screen* screen) {
	return guarded([&] {
		afr_screen& handle = inMode(screen, AFR_MODE_WINDOW, AFR_MODE_WINDOW);
		handle.application->run();
		return AFR_OK;
	});
}

afr_status afr_screen_quit(afr_screen* screen) {
	return guarded([&] {
		inMode(screen, AFR_MODE_WINDOW, AFR_MODE_WINDOW).application->quit();
		return AFR_OK;
	});
}

afr_status afr_screen_step(afr_screen* screen, double milliseconds) {
	return guarded([&] {
		afr_screen& handle = inMode(screen, AFR_MODE_HEADLESS, AFR_MODE_EMBEDDED);
		if(handle.embedding) {
			handle.embedding->runTasks();
			return AFR_OK;
		}
		if(!std::isfinite(milliseconds) || milliseconds < 0.0 || milliseconds > 1e9)
			throw Failure{AFR_ERROR_ARGUMENT, "milliseconds must be 0 to 1e9"};
		handle.manualClock.advance(milliseconds);
		handle.tasks.run();
		affresco::detail::renderWithoutPresenting(*handle.window);
		return AFR_OK;
	});
}

afr_status afr_screen_mouse(afr_screen* screen, afr_mouse_action action, double x, double y, int button,
                            int modifiers) {
	return guarded([&] {
		affresco::Window& window = *anyMode(screen).window;
		requireFinite(x, "x");
		requireFinite(y, "y");
		if(modifiers < 0 || modifiers > allModifiers) throw Failure{AFR_ERROR_ARGUMENT, "modifiers has unknown bits"};
		const affresco::Vec2 position{static_cast<float>(x), static_cast<float>(y)};
		switch(fromC(action)) {
		case AFR_MOUSE_MOVE:
			window.routeMouseMove(position);
			return AFR_OK;
		case AFR_MOUSE_ENTER:
			window.routeCursorEnter(true);
			return AFR_OK;
		case AFR_MOUSE_LEAVE:
			window.routeCursorEnter(false);
			return AFR_OK;
		case AFR_MOUSE_PRESS:
		case AFR_MOUSE_RELEASE:
			break;
		default:
			throw Failure{AFR_ERROR_ARGUMENT, "action is not an afr_mouse_action"};
		}
		if(button < 0 || button > 7) throw Failure{AFR_ERROR_ARGUMENT, "button must be 0 to 7"};
		window.routeMouseMove(position);
		window.routeMouseButton({static_cast<affresco::MouseButton>(button),
		                         action == AFR_MOUSE_PRESS ? affresco::Action::Press : affresco::Action::Release,
		                         affresco::Mods{modifiers}, position});
		return AFR_OK;
	});
}

afr_status afr_screen_key(afr_screen* screen, int key, afr_key_action action, int modifiers) {
	return guarded([&] {
		affresco::Window& window = *anyMode(screen).window;
		if(key < -1 || key > lastKey) throw Failure{AFR_ERROR_ARGUMENT, "key is not a GLFW key code"};
		if(const int value = fromC(action);
		   value != AFR_KEY_RELEASE && value != AFR_KEY_PRESS && value != AFR_KEY_REPEAT)
			throw Failure{AFR_ERROR_ARGUMENT, "action is not an afr_key_action"};
		if(modifiers < 0 || modifiers > allModifiers) throw Failure{AFR_ERROR_ARGUMENT, "modifiers has unknown bits"};
		window.routeKey(
		    {static_cast<affresco::Key>(key), 0, static_cast<affresco::Action>(action), affresco::Mods{modifiers}});
		return AFR_OK;
	});
}

afr_status afr_screen_text(afr_screen* screen, const char* utf8, size_t length) {
	return guarded([&] {
		affresco::Window& window = *anyMode(screen).window;
		if(length > longestText) throw Failure{AFR_ERROR_ARGUMENT, "the text is longer than 65536 bytes"};
		const std::string_view text = affresco::capi::checkedText(utf8, length, "utf8");
		for(std::size_t at = 0; at < text.size();) // all of it checked before any of it is typed
			if(decode(text, at) == 0xFFFFFFFF) throw Failure{AFR_ERROR_ARGUMENT, "the text is not valid UTF-8"};
		for(std::size_t at = 0; at < text.size();) window.routeChar({decode(text, at)});
		return AFR_OK;
	});
}

afr_status afr_screen_scroll(afr_screen* screen, double x, double y, double dx, double dy) {
	return guarded([&] {
		affresco::Window& window = *anyMode(screen).window;
		requireFinite(x, "x");
		requireFinite(y, "y");
		requireFinite(dx, "dx");
		requireFinite(dy, "dy");
		const affresco::Vec2 position{static_cast<float>(x), static_cast<float>(y)};
		window.routeMouseMove(position);
		window.routeScroll({{static_cast<float>(dx), static_cast<float>(dy)}, position});
		return AFR_OK;
	});
}

afr_status afr_screen_frame(afr_screen* screen, int width, int height, float scale, uint32_t framebuffer) {
	return guarded([&] {
		afr_screen& handle = inMode(screen, AFR_MODE_EMBEDDED, AFR_MODE_EMBEDDED);
		if(!handle.embedding->frame(width, height, scale, framebuffer))
			throw Failure{AFR_ERROR_ARGUMENT, "width and height must be 1 to 16384, scale 0.25 to 16"};
		handle.scale = scale;
		return AFR_OK;
	});
}

afr_status afr_screen_render_texture(afr_screen* screen, int width, int height, float scale, uint32_t* texture) {
	return guarded([&] {
		afr_screen& handle = inMode(screen, AFR_MODE_EMBEDDED, AFR_MODE_EMBEDDED);
		require(texture, "texture");
		*texture = handle.embedding->renderTexture(width, height, scale);
		if(*texture == 0) throw Failure{AFR_ERROR_ARGUMENT, "width and height must be 1 to 16384, scale 0.25 to 16"};
		handle.scale = scale;
		return AFR_OK;
	});
}

afr_status afr_screen_render_rgba(afr_screen* screen, int* width, int* height, uint8_t** rgba) {
	return guarded([&] {
		afr_screen& handle = anyMode(screen);
		require(width, "width");
		require(height, "height");
		require(static_cast<const void*>(rgba), "rgba");
		*rgba = nullptr;
		const affresco::Window& window = *handle.window;
		const int whole = std::max(1, static_cast<int>(std::lround(handle.scale)));
		const float scale = handle.embedding ? handle.scale : static_cast<float>(whole);
		const int wide = static_cast<int>(std::lround(static_cast<float>(window.width()) * scale));
		const int high = static_cast<int>(std::lround(static_cast<float>(window.height()) * scale));
		// Headless, the window may be 16384 units wide and the scale 16: a picture of terabytes, which the host
		// would get as a failed allocation at best
		if(wide > largestPicture || high > largestPicture)
			throw Failure{AFR_ERROR_ARGUMENT, "the picture would be over 16384 pixels wide or high: lower the scale"};
		const std::vector<std::uint8_t> pixels =
		    handle.embedding ? handle.embedding->renderPixels(wide, high, scale) : windowPixels(handle, whole);
		if(pixels.size() != static_cast<std::size_t>(wide) * static_cast<std::size_t>(high) * 4)
			throw Failure{AFR_ERROR_INTERNAL, "the screen could not be drawn"};
		// NOLINTNEXTLINE(cppcoreguidelines-no-malloc): handed to C, which frees it with afr_free()
		auto* copy = static_cast<uint8_t*>(std::malloc(pixels.size()));
		if(copy == nullptr) throw std::bad_alloc();
		std::memcpy(copy, pixels.data(), pixels.size());
		*rgba = copy;
		*width = wide;
		*height = high;
		return AFR_OK;
	});
}

} // extern "C"
