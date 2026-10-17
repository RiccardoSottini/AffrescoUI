#pragma once

/// @file
/// @brief gl::QuadRenderer: draws a panel tree as batched, textured quads.

#include "gl/GlObjects.hpp"

#include "affresco/render/Painter.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace affresco {
class Panel;
}

namespace affresco::gl {

/// @brief Draws a panel tree as textured quads: one buffer upload per frame, one draw call per texture switch.
///
/// Solid rectangles sample a white texel of the current texture, so rectangles and text share a batch: a frame using
/// a single font is a single draw call. Quads are clipped on the CPU to the panel being painted.
class QuadRenderer final : public Painter {
public:
	/// @brief Compiles the shader and sets up the vertex layout, in the current context.
	/// @throws std::runtime_error If the shader fails.
	QuadRenderer();

	/// @brief Draws a laid-out tree: collects its quads, uploads them once, draws each batch.
	/// @param root The tree.
	/// @param width Window width in layout units (not framebuffer pixels).
	/// @param height Window height in layout units.
	void draw(const Panel& root, int width, int height);

	/* Painter */
	void fillRect(const Rect& rect, Color color) override;
	void drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) override;

private:
	/// @brief One vertex, 16 bytes.
	struct Vertex {
		float x, y;              ///< Position, in window units.
		std::uint16_t u, v;      ///< Texture coordinates, normalized by the GPU to [0, 1].
		std::uint8_t r, g, b, a; ///< Colour, normalized by the GPU to [0, 1].
	};
	static_assert(sizeof(Vertex) == 16);

	/// @brief A run of quads drawn with the same texture.
	struct Batch {
		GLuint texture;        ///< The texture.
		Vec2 white;            ///< Its white texel.
		std::size_t firstQuad; ///< First quad of the run.
		std::size_t quadCount; ///< Quads in the run.
	};

	/// @brief Paints a panel, then its children (pre-order: later ones land on top). Hidden subtrees are skipped.
	/// @param panel The subtree's root.
	void collect(const Panel& panel);
	/// @brief Starts a new batch unless the current one uses the same texture.
	/// @param texture The texture.
	/// @param white Its white texel.
	void useTexture(GLuint texture, Vec2 white);
	/// @brief Clips a quad to the panel being painted (adjusting its texture coordinates) and appends it.
	/// @param rect The quad.
	/// @param uv Its texture coordinates.
	/// @param color Its colour; transparent quads are dropped.
	void pushQuad(const Rect& rect, const Rect& uv, Color color);
	/// @brief The texture of a font's atlas, uploaded on first use.
	/// @param font The font.
	/// @return The texture name.
	GLuint fontTexture(const Font& font);
	/// @brief Grows the index buffer to address enough quads; it never shrinks.
	/// @param quads Quads in this frame.
	/// @pre The vertex array is bound.
	void reserveQuads(std::size_t quads);

	/// @brief Uploads a square, single-channel, linearly filtered texture.
	/// @param size Width and height, in texels.
	/// @param pixels Coverage bytes, top row first.
	/// @return The texture.
	static Texture createTexture(int size, std::span<const std::uint8_t> pixels);

	Program program_;                                         ///< The quad shader.
	VertexArray vertexArray_;                                 ///< Vertex layout and index buffer binding.
	Buffer vertexBuffer_;                                     ///< Re-specified every frame (orphaning).
	Buffer indexBuffer_;                                      ///< 0 1 2, 2 3 0 per quad.
	Texture whiteTexture_;                                    ///< 1x1, for frames without text.
	std::unordered_map<std::uint64_t, Texture> fontTextures_; ///< By Font::id(), uploaded on first use.
	GLint scaleLocation_ = -1;                                ///< The uScale uniform.

	// Per frame, reused so steady-state frames don't allocate
	std::vector<Vertex> vertices_; ///< 4 per quad.
	std::vector<Batch> batches_;   ///< In drawing order.
	Rect clip_;                    ///< Bounds of the panel being painted.

	std::size_t quadCapacity_ = 0; ///< Quads the index buffer can address.
	int width_ = -1, height_ = -1; ///< Size the scale uniform was last set for.
};

} // namespace affresco::gl
