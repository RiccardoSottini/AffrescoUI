/// @file
/// @brief gl::Surfaces: making, drawing and releasing GPU surfaces, each in its own framebuffer, behind a guard that
/// gives the window its state back.

#include "gl/Surfaces.hpp"

#include "gl/SurfaceGuard.hpp"
#include "gl/SurfaceState.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace affresco::gl {

namespace {

constexpr int errorDrain = 32; ///< At most this many earlier errors are cleared before a surface draws.

/// @brief A size in window units as whole pixels, at least one when not empty, at most SurfaceFrame::maxPassSide.
/// @param units The size.
/// @param scale Pixels per unit.
/// @return The pixels; 0 when empty.
int pixels(float units, float scale) noexcept {
	const float value = units * scale;
	if(!(value > 0.0f)) return 0;
	return static_cast<int>(std::min(std::ceil(value), static_cast<float>(SurfaceFrame::maxPassSide)));
}

/// @brief Clears the errors left by earlier drawing, so that the surface is judged by its own.
void drainErrors() noexcept {
	for(int index = 0; index < errorDrain && glGetError() != GL_NO_ERROR; ++index) {
	}
}

/// @brief The documented start state: the framebuffer bound and cleared, the tests off, nothing bound.
/// @param framebuffer The surface's framebuffer.
/// @param width Its width.
/// @param height Its height.
void startState(GLuint framebuffer, int width, int height) {
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glViewport(0, 0, width, height);
	for(const GLenum capability :
	    std::array<GLenum, 5>{GL_SCISSOR_TEST, GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE})
		glDisable(capability);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glStencilMask(0xFFu);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0);
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glActiveTexture(GL_TEXTURE0);
}

} // namespace

/// @brief A render()'s frame: its passes are the entry's.
class Surfaces::Frame : public SurfaceFrame {
public:
	/// @brief A frame for an entry.
	/// @param entry The entry.
	/// @param params Its parameters.
	Frame(Entry& entry, const json& params) noexcept : SurfaceFrame(params), entry_(entry) {}

	/// @brief See SurfaceFrame::pass().
	SurfacePass pass(std::string_view name, const PassSpec& spec) override {
		if(spec.width <= 0 || spec.height <= 0) throw std::invalid_argument("a surface pass needs a size");
		if(spec.width > maxPassSide || spec.height > maxPassSide)
			throw std::length_error("a surface pass is over " + std::to_string(maxPassSide) + " pixels a side");
		const auto found = std::ranges::find(entry_.passes, name, &Pass::name);
		const bool same = found != entry_.passes.end() && found->target.width == spec.width &&
		                  found->target.height == spec.height && found->target.format == spec.format;
		const Target& target = same ? found->target : remake(found, name, spec);
		return {target.texture.id(), target.framebuffer.id(), target.width, target.height};
	}

private:
	/// @brief Makes a pass, or remakes one with another size or format, within the limits.
	/// @param found The pass, or the end.
	/// @param name Its name.
	/// @param spec Its size and format.
	/// @return Its texture.
	/// @throws std::length_error Over the limits (the framebuffer counts twice: colour, and depth if any).
	Target& remake(std::vector<Pass>::iterator found, std::string_view name, const PassSpec& spec) {
		if(found == entry_.passes.end() && entry_.passes.size() >= maxPasses)
			throw std::length_error("a surface has at most " + std::to_string(maxPasses) + " passes");
		std::size_t bytes = 2 * bytesOf(entry_.color.width, entry_.color.height, PassFormat::rgba8);
		for(const Pass& pass : entry_.passes)
			if(pass.name != name) bytes += bytesOf(pass.target.width, pass.target.height, pass.target.format);
		if(bytes + bytesOf(spec.width, spec.height, spec.format) > maxBytes)
			throw std::length_error("a surface's passes are over " + std::to_string(maxBytes >> 20) + " MiB");
		Target& target =
		    found != entry_.passes.end() ? found->target : entry_.passes.emplace_back(std::string(name)).target;
		makeTarget(target, spec.width, spec.height, spec.format);
		return target;
	}

	Entry& entry_; ///< The entry.
};

Surfaces::~Surfaces() {
	for(auto& [state, entry] : entries_) release(entry);
}

std::size_t Surfaces::bytesOf(int width, int height, PassFormat format) noexcept {
	const std::size_t texel = format == PassFormat::rgba16f ? 8 : 4;
	return static_cast<std::size_t>(std::max(width, 0)) * static_cast<std::size_t>(std::max(height, 0)) * texel;
}

// Drawn only when its state's version or its size changed; a failure disables it for good
GLuint Surfaces::place(const std::shared_ptr<detail::SurfaceState>& shared) {
	detail::SurfaceState& state = *shared;
	if(state.failed || !state.factory) return 0;
	const int width = pixels(state.size.x, state.scale), height = pixels(state.size.y, state.scale);
	if(width == 0 || height == 0) return 0;

	Entry& entry = entries_[&state];
	if(entry.state.lock() != shared || entry.surface != state.surface) { // a new panel, or another surface
		release(entry);
		entry = Entry{};
		entry.state = shared;
		entry.surface = state.surface;
	}
	const bool resized = entry.color.width != width || entry.color.height != height || entry.scale != state.scale;
	if(entry.instance && !resized && entry.version == state.version) return entry.color.texture.id();

	try {
		if(!entry.instance) entry.instance = state.factory();
		if(!entry.instance) throw std::runtime_error("its factory made no surface");
		const SurfaceGuard guard;
		if(fit(entry, state, width, height)) entry.prepared = false;
		if(!render(entry, state)) {
			fail(entry, state, "an OpenGL error");
			return 0;
		}
	} catch(const std::exception& error) {
		fail(entry, state, error.what());
		return 0;
	}
	entry.version = state.version;
	entry.scale = state.scale;
	state.input.clear();
	return entry.color.texture.id();
}

void Surfaces::collect() {
	std::erase_if(entries_, [](auto& item) {
		if(!item.second.state.expired()) return false;
		release(item.second);
		return true;
	});
}

bool Surfaces::fit(Entry& entry, const detail::SurfaceState& state, int width, int height) {
	if(entry.color.width == width && entry.color.height == height && entry.scale == state.scale) return false;
	makeTarget(entry.color, width, height, PassFormat::rgba8);
	if(state.options.depth) {
		if(entry.depth.id() == 0) entry.depth = Renderbuffer::create();
		glBindRenderbuffer(GL_RENDERBUFFER, entry.depth.id());
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		glBindFramebuffer(GL_FRAMEBUFFER, entry.color.framebuffer.id());
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, entry.depth.id());
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	return true;
}

// Inside the caller's guard: whatever the surface leaves bound or enabled, the window gets its own state back
bool Surfaces::render(Entry& entry, detail::SurfaceState& state) {
	const int width = entry.color.width, height = entry.color.height;
	drainErrors();
	startState(entry.color.framebuffer.id(), width, height);
	if(!entry.prepared) {
		entry.instance->prepare({width, height, state.scale, surfaceShaderPrelude()});
		entry.prepared = true;
	}
	Frame frame(entry, state.params);
	frame.width = width;
	frame.height = height;
	frame.scale = state.scale;
	frame.time = state.time;
	frame.input = state.input;
	frame.framebuffer = entry.color.framebuffer.id();
	entry.instance->render(frame);
	return glGetError() == GL_NO_ERROR;
}

void Surfaces::fail(Entry& entry, detail::SurfaceState& state, const std::string& reason) {
	log(LogLevel::Error, "AffrescoUI: surface '" + state.name + "' disabled: " + reason);
	state.failed = true;
	state.input.clear();
	release(entry);
	entry.color = {};
	entry.depth.reset();
}

void Surfaces::release(Entry& entry) noexcept {
	if(entry.instance) {
		try {
			const SurfaceGuard guard;
			entry.instance->release();
		} catch(const std::exception& error) {
			log(LogLevel::Error, std::string("AffrescoUI: a surface's release() failed: ") + error.what());
		} catch(...) {
			log(LogLevel::Error, "AffrescoUI: a surface's release() failed");
		}
		entry.instance.reset();
	}
	entry.passes.clear();
	entry.prepared = false;
}

// Binds what it needs and then puts the framebuffer and texture bindings back: a surface may make passes mid-draw
void makeTarget(Target& target, int width, int height, PassFormat format) {
	GLint framebuffer = 0, texture = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
	if(target.texture.id() == 0) target.texture = Texture::create();
	if(target.framebuffer.id() == 0) target.framebuffer = Framebuffer::create();
	glBindTexture(GL_TEXTURE_2D, target.texture.id());
	const bool depth = format == PassFormat::depth24;
	if(depth)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
		             nullptr);
	else if(format == PassFormat::rgba16f)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
	else glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	const GLint filter = depth ? GL_NEAREST : GL_LINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer.id());
	glFramebufferTexture2D(GL_FRAMEBUFFER, depth ? GL_DEPTH_STENCIL_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                       target.texture.id(), 0);
	const GLenum none = GL_NONE, color = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, depth ? &none : &color);
	glReadBuffer(depth ? GL_NONE : GL_COLOR_ATTACHMENT0);
	target.width = width;
	target.height = height;
	target.format = format;
	glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
	glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
}

} // namespace affresco::gl
