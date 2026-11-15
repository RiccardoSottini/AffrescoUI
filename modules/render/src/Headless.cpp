/// @file
/// @brief detail::createHeadlessWindow: a Window without a platform window or a renderer.

#include "Headless.hpp"
#include "paint/Renderer.hpp"
#include "platform/Platform.hpp"

#include <cmath>
#include <limits>
#include <optional>
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

	/// @brief Renders a frame, without presenting it.
	/// @param window The window.
	static void renderOnly(Window& window) {
		window.runTimers();
		window.runDeferred();
		if(!window.redrawDirty_) return;
		if(window.renderer_) window.draw();
		else window.updateLayout();
		window.redrawDirty_ = false;
	}

	/// @brief A window's next frame, its damage handed over.
	/// @param window The window.
	/// @return The frame.
	static Frame take(Window& window) {
		window.updateLayout();
		Frame frame = window.frame();
		window.clearDamage();
		return frame;
	}

	/// @brief Draws a window's frame from nothing.
	/// @param window The window.
	/// @param scale Pixels per window unit.
	/// @return The pixels; empty for a headless window.
	static std::vector<std::uint8_t> fromScratch(Window& window, float scale) {
		const std::optional<platform::PixelSize> size = scaledSize(window.width_, window.height_, scale);
		if(!window.renderer_ || !size) return {};
		window.runDeferred();
		window.native_->makeCurrent();
		window.updateLayout();
		window.refreshSnapshots();
		return window.renderer_->drawFromScratch(window.frame(), *size);
	}
};

std::unique_ptr<Window> createHeadlessWindow(int width, int height, const Clock& clock, std::string title) {
	return WindowFactory::headless(width, height, clock, std::move(title));
}

std::optional<platform::PixelSize> scaledSize(int width, int height, float scale) noexcept {
	if(!std::isfinite(scale) || scale <= 0.0f) return std::nullopt;
	// In double, so a whole-number scale gives exactly width * scale, as it did when the scale was an int
	const double pixelsWide = std::round(static_cast<double>(width) * static_cast<double>(scale));
	const double pixelsHigh = std::round(static_cast<double>(height) * static_cast<double>(scale));
	constexpr double largest = static_cast<double>(std::numeric_limits<int>::max());
	if(pixelsWide > largest || pixelsHigh > largest) return std::nullopt;
	return platform::PixelSize{static_cast<int>(pixelsWide), static_cast<int>(pixelsHigh)};
}

std::vector<std::uint8_t> drawFromScratch(Window& window, float scale) {
	return WindowFactory::fromScratch(window, scale);
}

Frame takeFrame(Window& window) { return WindowFactory::take(window); }

void renderWithoutPresenting(Window& window) { WindowFactory::renderOnly(window); }

} // namespace affresco::detail
