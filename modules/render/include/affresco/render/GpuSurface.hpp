#pragma once

/// @file
/// @brief GpuSurface: native GPU drawing inside a panel. A host-registered surface draws with OpenGL into a
/// framebuffer of its own; the window draws that as a quad, clipped, transformed and damage-tracked like an image.
///
/// The contract:
/// - prepare() runs with the window's context current before the first render(), and again whenever the size in
///   pixels or the scale changed; release() runs once, before the surface is destroyed or after it failed.
/// - render() starts in a documented state: the surface's framebuffer bound for drawing and reading (colour, plus
///   depth and stencil if SurfaceOptions::depth), the viewport its whole size, cleared to transparent (depth 1,
///   stencil 0); blending, depth, stencil, scissor and culling off; colour and depth writes on; no program, vertex
///   array or buffer bound; texture unit 0 active. It may change any of that: the window restores its own state.
/// - It draws premultiplied alpha, y up as OpenGL has it: its framebuffer's bottom row is the panel's bottom.
/// - A GL error after render() (or an exception from it) disables that surface: one error is logged, it draws
///   nothing from then on, and the rest of the frame is drawn as without it.
/// - Shaders use the common subset of GLSL 4.10 and GLSL ES 3.00, after surfaceShaderPrelude().
/// Documents only name a surface the host registered (Screen::registerSurface) and pass it parameters: they never
/// supply shader source.

#include "affresco/core/Json.hpp"
#include "affresco/render/Input.hpp"
#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace affresco {

namespace detail {
struct SurfaceState;
} // namespace detail

/// @brief The texel format of a surface's pass.
enum class PassFormat : std::uint8_t {
	rgba8,   ///< 8 bits per channel, linearly filtered.
	rgba16f, ///< Half floats per channel, linearly filtered: for HDR and blur chains.
	depth24, ///< A 24-bit depth texture (with 8 bits of stencil), for shadow maps and depth reads.
};

/// @brief What a pass is: its size in pixels and its format.
struct PassSpec {
	int width = 0;                         ///< Width in pixels; 1..maxPassSide.
	int height = 0;                        ///< Height in pixels; 1..maxPassSide.
	PassFormat format = PassFormat::rgba8; ///< Its format.
};

/// @brief An offscreen pass a surface draws into and reads from: a texture and a framebuffer holding it.
struct SurfacePass {
	unsigned texture = 0;     ///< The texture (GL_TEXTURE_2D).
	unsigned framebuffer = 0; ///< A framebuffer with it attached (as colour, or as depth and stencil).
	int width = 0;            ///< Its width, in pixels.
	int height = 0;           ///< Its height, in pixels.
};

/// @brief What prepare() learns.
struct SurfaceSetup {
	int width = 0;            ///< The framebuffer's width, in pixels.
	int height = 0;           ///< The framebuffer's height, in pixels.
	float scale = 1.0f;       ///< Pixels per window unit.
	std::string_view prelude; ///< surfaceShaderPrelude(): the first line of each of its shaders.
};

/// @brief What a surface needs of its framebuffer.
struct SurfaceOptions {
	bool depth = false; ///< A depth and stencil buffer (24 + 8 bits), cleared before each render().
};

/// @brief What the pointer did over a surface.
enum class PointerKind : std::uint8_t {
	move,    ///< It moved.
	press,   ///< A button was pressed.
	release, ///< A button was released.
	scroll,  ///< The wheel or trackpad scrolled.
};

/// @brief One pointer event, in the surface's pixels: (0, 0) its top-left corner, y down.
struct SurfacePointer {
	PointerKind kind = PointerKind::move;   ///< What happened.
	Vec2 position;                          ///< Where, in pixels from the top-left corner.
	MouseButton button = MouseButton::Left; ///< press, release: the button.
	Vec2 scroll;                            ///< scroll: the amount per axis.
	Mods mods;                              ///< press, release: the modifier keys held.
};

/// @brief One render(): the size, the time, the parameters, the input since the last one, and offscreen passes.
class SurfaceFrame {
public:
	static constexpr std::size_t maxPasses = 8;         ///< Passes per surface.
	static constexpr int maxPassSide = 4096;            ///< Pixels per side of a pass, and of the surface.
	static constexpr std::size_t maxBytes = 256u << 20; ///< Bytes of its framebuffer and passes together.

	/// @brief A frame.
	/// @param params The document's parameters.
	explicit SurfaceFrame(const json& params) noexcept : params_(params) {}
	virtual ~SurfaceFrame() = default;
	SurfaceFrame(const SurfaceFrame&) = delete;
	SurfaceFrame& operator=(const SurfaceFrame&) = delete;

	/// @brief The document's "params" (an object; empty if it gave none).
	[[nodiscard]] const json& params() const noexcept { return params_; }

	/// @brief An offscreen pass by name, made on first use and kept from frame to frame (ping-pong between two).
	///
	/// Asking again with another size or format remakes it, with undefined contents.
	/// @param name Its name.
	/// @param spec Its size and format.
	/// @return Its texture and framebuffer, owned by the window.
	/// @throws std::length_error Over maxPasses, maxPassSide or maxBytes; std::invalid_argument for an empty size.
	[[nodiscard]] virtual SurfacePass pass(std::string_view name, const PassSpec& spec) = 0;

	int width = 0;                         ///< The framebuffer's width, in pixels.
	int height = 0;                        ///< The framebuffer's height, in pixels.
	float scale = 1.0f;                    ///< Pixels per window unit.
	double time = 0.0;                     ///< The window clock's seconds when the panel last asked for a redraw.
	std::span<const SurfacePointer> input; ///< The pointer events since the last render(), oldest first.
	unsigned framebuffer = 0;              ///< The surface's own framebuffer: bind it again after drawing passes.

private:
	const json& params_; ///< See params().
};

/// @brief Native drawing in a panel: a host implements it and registers a factory.
class GpuSurface {
public:
	virtual ~GpuSurface() = default;
	GpuSurface() = default;
	GpuSurface(const GpuSurface&) = delete;
	GpuSurface& operator=(const GpuSurface&) = delete;

	/// @brief Makes or remakes what render() needs: programs, buffers, size-dependent textures.
	/// @param setup The size and scale, and the shader prelude.
	virtual void prepare(const SurfaceSetup& setup) { static_cast<void>(setup); }
	/// @brief Draws one frame into frame.framebuffer.
	/// @param frame The size, time, parameters, input and passes.
	virtual void render(SurfaceFrame& frame) = 0;
	/// @brief Frees its GL objects; the window's context is current. The passes are the window's.
	virtual void release() {}
};

/// @brief Makes a surface; called once per panel, the first time it is drawn.
using SurfaceFactory = std::function<std::unique_ptr<GpuSurface>()>;

/// @brief The first lines of a surface's shaders: the GLSL version this window's context takes.
/// @return "#version 410 core\n" on desktop OpenGL; on OpenGL ES (WebGL 2), "#version 300 es\n" and the precision.
[[nodiscard]] std::string_view surfaceShaderPrelude() noexcept;

/// @brief A panel that shows a GpuSurface, drawn at its size times the window's content scale.
///
/// It redraws when its parameters change, on pointer input, or every frame while continuous. Pointer events still
/// reach its signals, and reach the surface as SurfacePointer, in its pixels.
class SurfaceView : public Panel {
public:
	/// @brief Starts transparent, with no surface.
	SurfaceView();
	/// @brief Stops its redraw timer; the renderer releases its surface at the next frame.
	~SurfaceView() override;

	/// @brief Shows a surface; another name makes a new one (the old one is released).
	/// @param name The registered name.
	/// @param factory Makes it.
	/// @param options What its framebuffer needs.
	void setSurface(const std::string& name, SurfaceFactory factory, SurfaceOptions options = {});
	/// @brief The registered name shown, or empty.
	[[nodiscard]] const std::string& surfaceName() const noexcept;

	/// @brief Sets its parameters; a change redraws it.
	/// @param params An object.
	void setParams(const json& params);
	/// @brief Its parameters.
	[[nodiscard]] const json& params() const noexcept;

	/// @brief Whether it redraws every frame (animation), or only on changes and input.
	/// @param continuous true to redraw every frame.
	void setContinuous(bool continuous);
	/// @brief See setContinuous().
	[[nodiscard]] bool continuous() const noexcept;

	/// @brief Whether its surface failed (a GL error or an exception) and draws nothing.
	[[nodiscard]] bool failed() const noexcept;

	/// @brief Its state, shared with the renderer.
	[[nodiscard]] const std::shared_ptr<detail::SurfaceState>& state() const noexcept { return state_; }

protected:
	void paint(Painter& painter) const override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void scrollEvent(const ScrollEvent& event) override;

private:
	/// @brief Queues a pointer event for the next render() and asks for one.
	/// @param pointer The event, with its position still in window units.
	void push(SurfacePointer pointer);
	/// @brief Asks for a redraw: the next render() gets the window clock's time.
	void redraw() noexcept;
	/// @brief While continuous, asks for the next frame's redraw on a timer (a frame asked for while drawing is lost).
	void scheduleNext() const;

	std::shared_ptr<detail::SurfaceState> state_; ///< Shared with the renderer, which holds it weakly.
	mutable std::uint64_t timer_ = 0;             ///< The continuous redraw's timer on window(), or 0.
};

} // namespace affresco
