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
///
/// Rounded rectangles, outlines and shadows are white-texel quads too, in the same batch: each vertex also carries a
/// signed-distance description of its shape, from which the fragment shader computes an anti-aliased (or blurred)
/// coverage. Other quads are marked as not shapes and skip that work, so they draw exactly as before.
class QuadRenderer final : public Painter {
public:
	/// @brief Compiles the shader and sets up the vertex layout, in the current context.
	/// @throws std::runtime_error If the shader fails.
	QuadRenderer();

	/// @brief Draws a laid-out tree, then its overlays and the tooltip: collects the quads, uploads them once, draws
	/// each batch.
	/// @param root The tree.
	/// @param overlays Overlay panels, in drawing order (they are skipped inside the tree).
	/// @param tooltip The tooltip to draw last, or nullptr.
	/// @param width Window width, in window units.
	/// @param height Window height, in window units.
	void draw(const Panel& root, std::span<Panel* const> overlays, const Panel* tooltip, int width, int height);

	/* Painter */
	void fillRect(const Rect& rect, Color color) override;
	void drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) override;
	void fillRoundedRect(const Rect& rect, const CornerRadii& radii, Color color) override;
	void strokeRoundedRect(const Rect& rect, const CornerRadii& radii, float width, Color color) override;
	void drawShadow(const Rect& rect, const CornerRadii& radii, const Shadow& shadow) override;

private:
	/// @brief One vertex, 32 bytes.
	struct Vertex {
		float x, y;              ///< Position, in window units.
		std::uint16_t u, v;      ///< Texture coordinates, normalized by the GPU to [0, 1].
		std::uint8_t r, g, b, a; ///< Colour, normalized by the GPU to [0, 1].
		/// Shapes: how far inside the shape's nearest vertical and horizontal edges it is (negative outside).
		float insideX, insideY;
		/// Shapes: the corner's radius, the outline's width (0 for a filled shape) and the blur, in sixteenths of a
		/// window unit.
		std::uint16_t radius, outline, blur;
		std::uint16_t isShape; ///< 1 for shapes; 0 for other quads, which skip the shape's work and ignore the rest.
	};
	static_assert(sizeof(Vertex) == 32);

	/// @brief How a shape is drawn.
	struct ShapeStyle {
		Color color;          ///< Its colour.
		float outline = 0.0f; ///< The width of its outline, inside its edges; 0 fills it.
		float blur = 0.0f;    ///< How soft its edges are (see Shadow::blur); 0 anti-aliases them.
	};

	/// @brief A quarter of a shape: the part nearest one of its corners.
	struct Quarter {
		Rect piece;     ///< The area it covers, from the shape's centre to past its corner.
		float radius;   ///< The corner's radius.
		bool left, top; ///< Which edges are nearest: left or right, top or bottom.
	};

	/// @brief A run of quads drawn with the same texture.
	struct Batch {
		GLuint texture;        ///< The texture.
		Vec2 white;            ///< Its white texel.
		std::size_t firstQuad; ///< First quad of the run.
		std::size_t quadCount; ///< Quads in the run.
	};

	/// @brief Paints a panel, then its children (pre-order: later ones land on top). Hidden subtrees are skipped.
	/// @param panel The subtree's root.
	/// @param outerClip What clips the panel: its parent's bounds, or the window for roots and overlays.
	void collect(const Panel& panel, const Rect& outerClip);
	/// @brief Starts a new batch unless the current one uses the same texture.
	/// @param texture The texture.
	/// @param white Its white texel.
	void useTexture(GLuint texture, Vec2 white);
	/// @brief Clips a quad to the panel being painted (adjusting its texture coordinates) and appends it.
	/// @param rect The quad.
	/// @param uv Its texture coordinates.
	/// @param color Its colour; transparent quads are dropped.
	void pushQuad(const Rect& rect, const Rect& uv, Color color);
	/// @brief Appends a rounded rectangle, an outline or a shadow, as four quarters.
	/// @param rect The shape.
	/// @param radii Its corners; each is clamped to half the shorter side.
	/// @param style How it is drawn; transparent shapes are dropped.
	/// @param clip What it is clipped to.
	void pushShape(const Rect& rect, const CornerRadii& radii, const ShapeStyle& style, const Rect& clip);
	/// @brief Clips one quarter of a shape and appends it.
	/// @param rect The whole shape.
	/// @param quarter The quarter.
	/// @param style How it is drawn, with the outline already clamped.
	/// @param clip What it is clipped to.
	void pushQuarter(const Rect& rect, const Quarter& quarter, const ShapeStyle& style, const Rect& clip);
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
	Rect outerClip_;               ///< What clips the panel being painted: where its shadow may fall.

	std::size_t quadCapacity_ = 0; ///< Quads the index buffer can address.
	int width_ = -1, height_ = -1; ///< Size the scale uniform was last set for.
};

} // namespace affresco::gl
