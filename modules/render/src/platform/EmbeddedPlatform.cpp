/// @file
/// @brief platform::EmbeddedPlatform, platform::EmbeddedWindow and affresco::Embedding: a Window inside the host's
/// OpenGL context, with no window or event loop of its own.

#include "platform/EmbeddedPlatform.hpp"

#include "Renderer.hpp"
#include "gl/OpenGlRenderer.hpp"

#include "affresco/core/Executor.hpp"
#include "affresco/render/Embedding.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>

namespace affresco {

namespace platform {

void EmbeddedWindow::resize(int width, int height, float scale) {
	const bool pixelsChanged = width != size_.width || height != size_.height;
	size_ = {width, height};
	if(window_ == nullptr) return;
	if(scale != scale_) {
		scale_ = scale;
		displayChanged(*window_, scale);
	}
	const int unitsWide = std::max(1, static_cast<int>(std::lround(static_cast<float>(width) / scale)));
	const int unitsHigh = std::max(1, static_cast<int>(std::lround(static_cast<float>(height) / scale)));
	if(unitsWide != window_->width() || unitsHigh != window_->height()) resized(*window_, unitsWide, unitsHigh);
	if(pixelsChanged) framebufferResized(*window_);
}

void EmbeddedWindow::setAppearance(ColorScheme scheme, bool reducedMotion) {
	if(window_ != nullptr) window_->routeDisplay({scale_, scheme, reducedMotion});
}

void EmbeddedWindow::setSize(int width, int height) {
	// The host decides the size: the window takes this one until the next frame gives the host's
	if(window_ != nullptr) resized(*window_, width, height);
}

std::vector<std::string> EmbeddedWindow::chooseFiles(const FileDialog& dialog) {
	static_cast<void>(dialog);
	return {}; // the host shows its own dialogs (Window::setFileDialogHandler())
}

std::unique_ptr<NativeWindow> EmbeddedPlatform::createWindow(const std::string& title, int width, int height) {
	static_cast<void>(title);
	static_cast<void>(width);
	static_cast<void>(height);
	return std::make_unique<EmbeddedWindow>(loader_, 1.0f);
}

} // namespace platform

namespace detail {

/// @brief Creates the Window of an embedding (its constructor is private).
struct EmbeddedWindowFactory {
	/// @brief Creates it.
	/// @param native The embedded native window.
	/// @param renderer Its renderer.
	/// @param clock Its clock.
	/// @param title Its title.
	/// @param width In window units.
	/// @param height In window units.
	/// @return The window.
	static std::unique_ptr<Window> create(std::unique_ptr<platform::NativeWindow> native,
	                                      std::unique_ptr<Renderer> renderer, const Clock& clock, std::string title,
	                                      int width, int height) {
		return std::unique_ptr<Window>(
		    new Window(std::move(native), std::move(renderer), clock, std::move(title), width, height));
	}
};

} // namespace detail

/// @brief What an Embedding owns. The window comes first: its renderer loads the OpenGL functions the target needs.
struct Embedding::Impl {
	/// @brief Creates the window, its renderer and the target, in the current context.
	/// @param loader The host's loader.
	/// @param clock The clock.
	/// @param title The title.
	/// @param width In window units.
	/// @param height In window units.
	Impl(GlFunctionLoader loader, const Clock& clock, std::string title, int width, int height)
	    : platform(loader), native(createNative(title, width, height)),
	      window(createWindow(loader, clock, std::move(title), width, height)) {
		const gl::SavedState saved;
		target.emplace();
	}

	/// @brief The native window, kept for resizing after the Window owns it.
	/// @param title The title.
	/// @param width In window units.
	/// @param height In window units.
	/// @return It.
	std::unique_ptr<platform::NativeWindow> createNative(const std::string& title, int width, int height) {
		auto created = platform.createWindow(title, width, height);
		embedded = static_cast<platform::EmbeddedWindow*>(created.get());
		return created;
	}

	/// @brief The window, its renderer loading the OpenGL functions.
	/// @param loader The host's loader.
	/// @param clock The clock.
	/// @param title The title.
	/// @param width In window units.
	/// @param height In window units.
	/// @return It.
	std::unique_ptr<Window> createWindow(GlFunctionLoader loader, const Clock& clock, std::string title, int width,
	                                     int height) {
		std::unique_ptr<detail::Renderer> renderer = gl::createRenderer(loader);
		rendererOf = renderer.get();
		return detail::EmbeddedWindowFactory::create(std::move(native), std::move(renderer), clock, std::move(title),
		                                             width, height);
	}

	/// @brief Runs the posted tasks, the ones they post waiting for the next call.
	void runPosted() {
		{
			const std::lock_guard lock(mutex);
			running.swap(posted);
		}
		affresco::runTasks(running); // keeps its capacity (no allocation per frame), and is emptied even if one throws
	}

	/// @brief Sizes the window and the target, runs the tasks and draws what changed into the target.
	/// @param width In pixels.
	/// @param height In pixels.
	/// @param scale Pixels per window unit.
	void draw(int width, int height, float scale) {
		embedded->resize(width, height, scale);
		target->resize(width, height);
		rendererOf->setOutputFramebuffer(target->framebuffer());
		runPosted();
		window->render();
	}

	platform::EmbeddedPlatform platform;                   ///< Opens the native window.
	platform::EmbeddedWindow* embedded = nullptr;          ///< The native window, owned by the Window.
	std::unique_ptr<platform::NativeWindow> native;        ///< Until the Window takes it.
	detail::Renderer* rendererOf = nullptr;                ///< The window's renderer, owned by the Window.
	std::unique_ptr<Window> window;                        ///< The window.
	std::optional<gl::EmbeddedTarget> target;             ///< Its frames; after the window, which loads OpenGL.
	std::mutex mutex;                                      ///< Guards posted.
	std::vector<std::function<void()>> posted;             ///< Tasks posted from any thread.
	std::vector<std::function<void()>> running;            ///< The tasks being run.
};

namespace {

/// @brief Whether a size and scale can be drawn.
/// @param width In pixels.
/// @param height In pixels.
/// @param scale Pixels per window unit.
/// @return true if they can.
bool drawable(int width, int height, float scale) noexcept {
	return width > 0 && height > 0 && width <= Embedding::maximumSize && height <= Embedding::maximumSize &&
	       std::isfinite(scale) && scale >= 0.25f && scale <= 16.0f;
}

} // namespace

Embedding::Embedding(GlFunctionLoader loader, const Clock& clock, std::string title, int width, int height)
    : impl_(std::make_unique<Impl>(loader, clock, std::move(title), width, height)) {}

Embedding::~Embedding() {
	const gl::SavedState saved;
	impl_->target.reset();
	impl_->window.reset();
}

Window& Embedding::window() noexcept { return *impl_->window; }

void Embedding::post(std::function<void()> task) {
	const std::lock_guard lock(impl_->mutex);
	impl_->posted.push_back(std::move(task));
}

void Embedding::runTasks() {
	impl_->runPosted();
	impl_->window->runDeferredTasks();
}

bool Embedding::resize(int width, int height, float scale) {
	if(!drawable(width, height, scale)) return false;
	impl_->embedded->resize(width, height, scale);
	return true;
}

void Embedding::setAppearance(ColorScheme scheme, bool reducedMotion) {
	impl_->platform.setAppearance(scheme, reducedMotion);
	impl_->embedded->setAppearance(scheme, reducedMotion);
}

bool Embedding::frame(int width, int height, float scale, unsigned framebuffer) {
	if(!drawable(width, height, scale)) return false;
	const gl::SavedState saved;
	impl_->draw(width, height, scale);
	impl_->target->composite(framebuffer, width, height);
	return true;
}

unsigned Embedding::renderTexture(int width, int height, float scale) {
	if(!drawable(width, height, scale)) return 0;
	const gl::SavedState saved;
	impl_->draw(width, height, scale);
	return impl_->target->texture();
}

std::vector<std::uint8_t> Embedding::renderPixels(int width, int height, float scale) {
	if(!drawable(width, height, scale)) return {};
	const gl::SavedState saved;
	impl_->draw(width, height, scale);
	return impl_->target->pixels();
}

} // namespace affresco
