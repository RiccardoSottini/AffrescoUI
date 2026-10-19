#pragma once

/// @file
/// @brief gl::QuadRenderer: draws a panel tree as batched, textured quads.

#include "gl/GlObjects.hpp"

#include "affresco/render/Painter.hpp"

#include "affresco/render/Font.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
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
/// a single font is a single draw call, while its glyphs fit the font's first atlas page (Latin-1 always does). Colour
/// glyphs (emoji) sample a second texture, the font's colour page, bound next to it: they share the batch too. Quads
/// are clipped on the CPU to the panel being painted.
///
/// Rounded rectangles, outlines and shadows are white-texel quads too, in the same batch: each vertex also carries a
/// signed-distance description of its shape, from which the fragment shader computes an anti-aliased (or blurred)
/// coverage. Other quads are marked as not shapes and skip that work. A frame without shapes is drawn with a shader
/// that has no shape code at all, so it keeps exactly the pixels it had before shapes existed, on every driver; the
/// same goes for colour glyphs.
///
/// The textures of a font's atlas pages are kept while the font lives, and freed by the first frame after it is gone.
class QuadRenderer final : public Painter {
public:
	/// @brief Compiles the shaders and sets up the vertex layout, in the current context.
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

	/// @brief How many draw calls the last frame made.
	[[nodiscard]] std::size_t drawCalls() const noexcept { return drawCalls_; }
	/// @brief How many fonts have textures on the GPU now.
	[[nodiscard]] std::size_t fontsOnGpu() const noexcept { return fontTextures_.size(); }

private:
	/// @brief What a quad is, for the fragment shader (the vertex's last field).
	enum class QuadKind : std::uint16_t {
		plain = 0,      ///< A rectangle or a glyph of coverage.
		shape = 1,      ///< A rounded rectangle, an outline or a shadow.
		colorGlyph = 2, ///< A glyph from a colour page: its texels are its colours.
	};

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
		std::uint16_t kind; ///< A QuadKind: plain quads skip the shape's work and ignore the shape's fields.
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
		float radius = 0.0f;            ///< The corner's radius.
		bool left = false, top = false; ///< Which edges are nearest: left or right, top or bottom.
	};

	/// @brief A run of quads drawn with the same texture.
	struct Batch {
		GLuint texture;             ///< The texture.
		Vec2 white;                 ///< Its white texel.
		std::size_t firstQuad;      ///< First quad of the run.
		std::size_t quadCount;      ///< Quads in the run.
		const Font* font = nullptr; ///< The font whose atlas page the texture holds, if any; alive for the frame.
		std::uint32_t page = 0;     ///< That page.
		GLuint colorTexture = 0;    ///< The font's colour page its colour glyphs sample; 0 while it has none.
		std::uint32_t colorPage = 0; ///< That colour page.
	};

	/// @brief A font's atlas page on the GPU.
	struct PageTexture {
		Texture texture;       ///< The texture.
		std::uint64_t version; ///< The page's Font::atlasVersion() it holds.
	};

	/// @brief A font's atlas pages on the GPU.
	struct FontTextures {
		std::weak_ptr<const Font> font;                ///< The font: once it is gone, so are its textures.
		std::array<std::vector<PageTexture>, 2> pages; ///< By AtlasKind, then page.
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
	/// @param kind What it is: a plain quad or a colour glyph.
	void pushQuad(const Rect& rect, const Rect& uv, Color color, QuadKind kind = QuadKind::plain);
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
	/// @brief Starts a new batch unless the current one draws from the same atlas page.
	/// @param font The font.
	/// @param page Its atlas page.
	void useFontPage(const Font& font, std::uint32_t page);
	/// @brief Gives the current batch a font's colour page, starting a new batch if it has another one.
	/// @param font The font; the current batch draws from one of its coverage pages.
	/// @param page Its colour page.
	void useColorPage(const Font& font, std::uint32_t page);
	/// @brief The texture of a font's atlas page, uploaded on first use.
	/// @param font The font.
	/// @param page The page.
	/// @param kind The page's kind.
	/// @return The texture name.
	GLuint fontTexture(const Font& font, std::uint32_t page, AtlasKind kind);
	/// @brief Uploads the glyphs a font rasterised into an atlas page since its texture was last updated.
	///
	/// Runs after the frame's quads are collected: drawing text rasterises the glyphs it meets for the first time.
	/// @param font The font.
	/// @param page The page; fontTexture() made its texture.
	/// @param kind The page's kind.
	void updateFontTexture(const Font& font, std::uint32_t page, AtlasKind kind);
	/// @brief Draws each batch with its textures: the uploaded vertices, one draw call per batch.
	/// @pre The frame's program, vertex array and vertex buffer are bound.
	void drawBatches();
	/// @brief Grows the index buffer to address enough quads; it never shrinks.
	/// @param quads Quads in this frame.
	/// @pre The vertex array is bound.
	void reserveQuads(std::size_t quads);

	/// @brief Uploads a square, linearly filtered texture.
	/// @param size Width and height, in texels.
	/// @param pixels Its texels, top row first: one byte of coverage each, or four of RGBA for a colour page.
	/// @param kind Which of the two.
	/// @return The texture.
	static Texture createTexture(int size, std::span<const std::uint8_t> pixels, AtlasKind kind = AtlasKind::coverage);

	/// @brief A linked shader program, and the size its scale uniform was last set for.
	struct Shader {
		Program program;          ///< The program.
		GLint scaleLocation = -1; ///< Its uScale uniform.
		int width = -1;           ///< The width uScale was last set for.
		int height = -1;          ///< The height uScale was last set for.
	};

	/// @brief By what a frame has: plain, shapes, colour glyphs, both. A frame uses the first that draws everything it
	/// has, so frames without shapes or colour glyphs keep their pixels.
	std::array<Shader, 4> shaders_;
	VertexArray vertexArray_;                                 ///< Vertex layout and index buffer binding.
	Buffer vertexBuffer_;                                     ///< Re-specified every frame (orphaning).
	Buffer indexBuffer_;                                      ///< 0 1 2, 2 3 0 per quad.
	Texture whiteTexture_;                                    ///< 1x1, for frames without text.
	std::unordered_map<std::uint64_t, FontTextures> fontTextures_; ///< By Font::id().

	// Per frame, reused so steady-state frames don't allocate
	std::vector<Vertex> vertices_; ///< 4 per quad.
	std::vector<Batch> batches_;   ///< In drawing order.
	Rect clip_;                    ///< Bounds of the panel being painted.
	Rect outerClip_;               ///< What clips the panel being painted: where its shadow may fall.
	bool hasShapes_ = false;       ///< Whether a shape was appended: the frame then uses a shader with shapes.
	bool hasColor_ = false;        ///< Whether a colour glyph was appended: the frame then uses one with colour.
	std::size_t drawCalls_ = 0;    ///< See drawCalls().

	std::size_t quadCapacity_ = 0; ///< Quads the index buffer can address.
};

} // namespace affresco::gl
