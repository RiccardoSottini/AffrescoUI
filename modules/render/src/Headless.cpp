/// @file
/// @brief detail::createHeadlessWindow: a Window without a platform window or a renderer.

#include "Headless.hpp"
#include "Renderer.hpp"
#include "platform/Platform.hpp"

#include <utility>

namespace affresco::detail {

/// @brief Reaches Window's private constructor, for windows not made by an Application.
struct WindowFactory {
	/// @brief Creates a headless window.
	/// @param width Width in window units.
	/// @param height Height in window units.
	/// @param clock Its clock.
	/// @param title Its title.
	/// @return The window.
	static std::unique_ptr<Window> headless(int width, int height, const Clock& clock, std::string title) {
		return std::unique_ptr<Window>(new Window(nullptr, nullptr, clock, std::move(title), width, height));
	}
};

std::unique_ptr<Window> createHeadlessWindow(int width, int height, const Clock& clock, std::string title) {
	return WindowFactory::headless(width, height, clock, std::move(title));
}

} // namespace affresco::detail
