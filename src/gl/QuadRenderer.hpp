#pragma once

#include "gl/GlObjects.hpp"

#include "sgl/Painter.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace sgl {
class Panel;
}

namespace sgl::gl {

// Draws a panel tree as textured quads: one buffer upload per frame, and one draw call per texture switch.
// Solid rectangles sample a white texel of the current texture, so rectangles and text share a batch:
// a frame using a single font is a single draw call.
class QuadRenderer final : public Painter {
public:
	QuadRenderer();

	// `width`/`height` are the window size in layout units (not framebuffer pixels)
	void draw(const Panel& root, int width, int height);

	/* Painter */
	void fillRect(const Rect& rect, Color color) override;
	void drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) override;

private:
	struct Vertex {
		float x, y;
		std::uint16_t u, v;      // normalized by the GPU to [0, 1]
		std::uint8_t r, g, b, a; // normalized by the GPU to [0, 1]
	};
	static_assert(sizeof(Vertex) == 16);

	// A run of quads drawn with the same texture
	struct Batch {
		GLuint texture;
		Vec2 white; // white texel of that texture
		std::size_t firstQuad;
		std::size_t quadCount;
	};

	void collect(const Panel& panel);
	void useTexture(GLuint texture, Vec2 white);
	void pushQuad(const Rect& rect, const Rect& uv, Color color);
	GLuint fontTexture(const Font& font);
	void reserveQuads(std::size_t quads);

	static Texture createTexture(int size, std::span<const std::uint8_t> pixels);

	Program program_;
	VertexArray vertexArray_;
	Buffer vertexBuffer_;
	Buffer indexBuffer_;
	Texture whiteTexture_;                                    // 1x1, for frames without text
	std::unordered_map<std::uint64_t, Texture> fontTextures_; // by Font::id(), uploaded on first use
	GLint scaleLocation_ = -1;

	// Per frame, reused so steady-state frames don't allocate
	std::vector<Vertex> vertices_;
	std::vector<Batch> batches_;
	Rect clip_; // bounds of the panel being painted

	std::size_t quadCapacity_ = 0; // quads the index buffer can address
	int width_ = -1, height_ = -1; // size the scale uniform was last set for
};

} // namespace sgl::gl
