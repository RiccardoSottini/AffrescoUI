#pragma once

/// @file
/// @brief platform::EmbeddedPlatform and platform::EmbeddedWindow: a windowing system with no windows of its own, for a
/// Window drawn into the host application's OpenGL context (affresco::Embedding); and gl::EmbeddedTarget, the texture
/// it draws into, with what saves and restores the host's OpenGL state.

#include "platform/Platform.hpp"

#include "gl/GlObjects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace affresco {

namespace platform {

/// @brief A NativeWindow that is a region of the host's context: its size and scale are what the host says, its
/// clipboard is its own, it never closes, and it neither makes a context current nor presents.
class EmbeddedWindow final : public NativeWindow {
public:
	/// @brief Creates it.
	/// @param loader The host's function loader.
	/// @param scale Pixels per window unit at first.
	EmbeddedWindow(ProcAddressLoader loader, float scale) noexcept : loader_(loader), scale_(scale) {}

	/// @brief Gives the window the host's size and scale, telling it what changed.
	/// @param width In pixels.
	/// @param height In pixels.
	/// @param scale Pixels per window unit.
	void resize(int width, int height, float scale);

	/// @brief Tells the window the host's appearance.
	/// @param scheme Light or dark.
	/// @param reducedMotion Whether the host asks for less motion.
	void setAppearance(ColorScheme scheme, bool reducedMotion);

	void connect(Window& window) override { window_ = &window; }
	void setTitle(const std::string& title) override { static_cast<void>(title); }
	void setIcon(const Bitmap* icon) override { static_cast<void>(icon); }
	void setSize(int width, int height) override;
	void setMinSize(int width, int height) override {
		static_cast<void>(width);
		static_cast<void>(height);
	}
	[[nodiscard]] bool isVisible() const override { return true; }
	void setVisible(bool visible) override { static_cast<void>(visible); }
	[[nodiscard]] bool shouldClose() const override { return false; }
	[[nodiscard]] std::string clipboardText() const override { return clipboard_; }
	void setClipboardText(const std::string& utf8) override { clipboard_ = utf8; }
	void setCursor(Cursor cursor) override { static_cast<void>(cursor); }
	[[nodiscard]] PixelSize framebufferSize() const override { return size_; }
	void makeCurrent() override {}
	void present() override {}
	[[nodiscard]] ProcAddressLoader procAddressLoader() const override { return loader_; }
	void wake() override {}
	[[nodiscard]] void* handle() const noexcept override { return nullptr; }
	[[nodiscard]] float contentScale() const override { return scale_; }
	bool setMenuBar(const std::vector<MenuItem>& menus) override {
		static_cast<void>(menus);
		return false;
	}
	[[nodiscard]] bool fileDialogs() const override { return false; }
	[[nodiscard]] std::vector<std::string> chooseFiles(const FileDialog& dialog) override;

private:
	ProcAddressLoader loader_; ///< See procAddressLoader().
	Window* window_ = nullptr; ///< The window it belongs to, once connected.
	PixelSize size_{};         ///< The host's pixels.
	float scale_ = 1.0f;       ///< Pixels per window unit.
	std::string clipboard_;    ///< Its own clipboard (the host's isn't reachable).
};

/// @brief The windowing system of an embedding: it opens EmbeddedWindows, never waits, and reports the appearance the
/// host gave it.
class EmbeddedPlatform final : public Platform {
public:
	/// @brief Creates it.
	/// @param loader The host's function loader, given to each window.
	explicit EmbeddedPlatform(ProcAddressLoader loader) noexcept : loader_(loader) {}

	[[nodiscard]] std::unique_ptr<NativeWindow> createWindow(const std::string& title, int width, int height) override;
	void waitEvents(std::optional<double> timeoutSeconds) override { static_cast<void>(timeoutSeconds); }
	void wake() override {}
	[[nodiscard]] ColorScheme colorScheme() const override { return scheme_; }
	[[nodiscard]] bool reducedMotion() const override { return reducedMotion_; }

	/// @brief Sets what colorScheme() and reducedMotion() report.
	/// @param scheme Light or dark.
	/// @param reducedMotion Whether the host asks for less motion.
	void setAppearance(ColorScheme scheme, bool reducedMotion) noexcept {
		scheme_ = scheme;
		reducedMotion_ = reducedMotion;
	}

private:
	ProcAddressLoader loader_;                ///< See the constructor.
	ColorScheme scheme_ = ColorScheme::Light; ///< See colorScheme().
	bool reducedMotion_ = false;              ///< See reducedMotion().
};

} // namespace platform

namespace gl {

/// @brief Saves the OpenGL state an embedding changes when it is created, and restores it when it is destroyed:
/// see affresco::Embedding::frame() for the list.
class SavedState {
public:
	/// @brief Reads the state, then sets what the library draws with: no depth or stencil test, no culling, no sRGB
	/// conversion, every colour written, additive blending, no pixel buffers bound.
	SavedState() noexcept;
	/// @brief Restores it.
	~SavedState();

	SavedState(const SavedState&) = delete;
	SavedState& operator=(const SavedState&) = delete;
	SavedState(SavedState&&) = delete;
	SavedState& operator=(SavedState&&) = delete;

private:
	GLint drawFramebuffer_ = 0, readFramebuffer_ = 0, renderbuffer_ = 0; ///< The framebuffer and renderbuffer bindings.
	std::array<GLint, 4> viewport_{}, scissorBox_{};                     ///< The viewport and the scissor box.
	GLint blendSourceRgb_ = 0, blendDestinationRgb_ = 0;                 ///< The colour's blend factors.
	GLint blendSourceAlpha_ = 0, blendDestinationAlpha_ = 0;             ///< The alpha's blend factors.
	GLint blendEquationRgb_ = 0, blendEquationAlpha_ = 0;                ///< The blend equations.
	GLint program_ = 0, vertexArray_ = 0, arrayBuffer_ = 0;              ///< The program, vertex array and buffer.
	GLint activeTexture_ = 0;                                            ///< The active texture unit.
	std::array<GLint, 2> textures_{};                                    ///< Units 0 and 1's 2D textures.
	GLint packAlignment_ = 0, unpackAlignment_ = 0, unpackRowLength_ = 0; ///< The pixel store.
	GLint packBuffer_ = 0, unpackBuffer_ = 0;                             ///< The pixel buffer bindings.
	std::array<GLfloat, 4> clearColor_{};                                 ///< The clear colour.
	std::array<GLboolean, 4> colorMask_{};                                ///< The colour mask.
	GLboolean blend_ = 0, scissor_ = 0, depth_ = 0, stencil_ = 0;         ///< Blending and the tests, enabled or not.
	GLboolean cull_ = 0, srgb_ = 0;                                       ///< Culling and sRGB writes, enabled or not.
};

/// @brief The texture an embedded window's frames are copied into, and what draws it over the host's framebuffer.
/// Created and destroyed in the host's context.
class EmbeddedTarget {
public:
	/// @brief Compiles the compositing shaders.
	EmbeddedTarget();

	/// @brief Gives the texture a size, keeping it if it has that size.
	/// @param width In pixels.
	/// @param height In pixels.
	void resize(int width, int height);

	/// @brief The framebuffer whose colour is the texture.
	[[nodiscard]] GLuint framebuffer() const noexcept { return framebuffer_.id(); }
	/// @brief The texture.
	[[nodiscard]] GLuint texture() const noexcept { return texture_.id(); }

	/// @brief Draws the texture over a framebuffer's whole viewport, premultiplied alpha blending.
	/// @param framebuffer The host's framebuffer.
	/// @param width Its width, in pixels.
	/// @param height Its height, in pixels.
	void composite(GLuint framebuffer, int width, int height) const;

	/// @brief Reads the texture.
	/// @return Tightly packed 8-bit RGBA, top row first.
	[[nodiscard]] std::vector<std::uint8_t> pixels() const;

private:
	Texture texture_;         ///< RGBA8, what the window draws.
	Framebuffer framebuffer_; ///< Its framebuffer.
	Program program_;         ///< Samples the texture.
	VertexArray vertexArray_; ///< Empty: the triangle's corners come from gl_VertexID.
	int width_ = 0;           ///< In pixels.
	int height_ = 0;          ///< In pixels.
};

} // namespace gl

} // namespace affresco
