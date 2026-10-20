#pragma once

/// @file
/// @brief gl::QuadRenderer: draws a panel tree as batched, textured quads, from the panels' retained geometry.

#include "GeometryCache.hpp"
#include "Renderer.hpp"
#include "RetainedGeometry.hpp"
#include "gl/GlObjects.hpp"

#include "affresco/render/Font.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace affresco {
class Panel;
}

namespace affresco::gl {

/// @brief Draws a panel tree as textured quads: one buffer upload per frame, one draw call per texture switch.
///
/// Each panel's drawing is recorded once (detail::PaintRecorder) and kept with the panel until it changes
/// (detail::GeometryCache); a frame replays the records in tree order into batches. Replaying a record changes the
/// batches exactly as drawing directly did, so a frame has the batches, vertices and pixels it always had. A frame
/// may be drawn only within a region (the damage): records and quads wholly outside it are left out, the batches
/// still switch where they would, and the caller clips the drawing to the region (scissor).
///
/// Solid rectangles sample a white texel of the current texture, so rectangles and text share a batch: a frame using
/// a single font is a single draw call, while its glyphs fit the font's first atlas page (Latin-1 always does). Colour
/// glyphs (emoji) sample a second texture, the font's colour page, bound next to it: they share the batch too.
///
/// Rounded rectangles, outlines and shadows are white-texel quads too, in the same batch: each vertex also carries a
/// signed-distance description of its shape, from which the fragment shader computes an anti-aliased (or blurred)
/// coverage. Other quads are marked as not shapes and skip that work. A frame without shapes is drawn with a shader
/// that has no shape code at all, so it keeps exactly the pixels it had before shapes existed, on every driver; the
/// same goes for colour glyphs. Which of them a frame has is known for the whole frame, even one drawn in a region.
///
/// The textures of a font's atlas pages are kept while the font lives, and freed by the first frame after it is gone.
class QuadRenderer final {
public:
	/// @brief Compiles the shaders and sets up the vertex layout, in the current context.
	/// @throws std::runtime_error If the shader fails.
	QuadRenderer();

	/// @brief Brings the geometry of a frame's panels up to date (detail::GeometryCache::refresh()).
	/// @param frame The frame, laid out.
	/// @return The damage found, and the shaders the frame needs.
	[[nodiscard]] detail::Refreshed refresh(const detail::Frame& frame) { return cache_.refresh(frame); }

	/// @brief Draws a refreshed frame into the current framebuffer, whose viewport, blending and clearing are set.
	/// @param frame The frame.
	/// @param region Only what lies in it, in window units; std::nullopt for everything.
	/// @param shapes Whether the frame has shapes (from refresh()).
	/// @param color Whether the frame has colour glyphs (from refresh()).
	void draw(const detail::Frame& frame, const std::optional<Rect>& region, bool shapes, bool color);

	/// @brief Draws a frame from nothing: records every shown panel afresh into records of its own, and keeps nothing.
	/// What a frame drawn from the retained geometry must look like.
	/// @param frame The frame, laid out.
	void drawFromScratch(const detail::Frame& frame);

	/// @brief Refreshes and draws a whole frame: a tree, its overlays and the tooltip.
	/// @param root The tree, laid out.
	/// @param overlays Overlay panels, in drawing order (they are skipped inside the tree).
	/// @param tooltip The tooltip to draw last, or nullptr.
	/// @param width Window width, in window units.
	/// @param height Window height, in window units.
	void draw(Panel& root, std::span<Panel* const> overlays, Panel* tooltip, int width, int height);

	/// @brief How many draw calls the last frame made.
	[[nodiscard]] std::size_t drawCalls() const noexcept { return drawCalls_; }
	/// @brief How many quads the last frame drew.
	[[nodiscard]] std::size_t quadsDrawn() const noexcept { return vertices_.size() / detail::verticesPerQuad; }
	/// @brief How many fonts have textures on the GPU now.
	[[nodiscard]] std::size_t fontsOnGpu() const noexcept { return fontTextures_.size(); }

private:
	/// @brief A run of quads drawn with the same texture.
	struct Batch {
		GLuint texture;              ///< The texture.
		Vec2 white;                  ///< Its white texel.
		std::size_t firstQuad;       ///< First quad of the run.
		std::size_t quadCount;       ///< Quads in the run.
		const Font* font = nullptr;  ///< The font whose atlas page the texture holds, if any; alive for the frame.
		std::uint32_t page = 0;      ///< That page.
		GLuint colorTexture = 0;     ///< The font's colour page its colour glyphs sample; 0 while it has none.
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

	/// @brief Empties the frame's quads and batches, and frees the textures of fonts that are gone.
	void startFrame();
	/// @brief Replays a panel's record, then its children's (pre-order: later ones land on top), then the rest of its
	/// record. Hidden subtrees, and subtrees wholly outside the region, are left out.
	/// @param panel The subtree's root.
	void replayTree(const Panel& panel);
	/// @brief Records a panel afresh, replays it and its children the same way (drawFromScratch()).
	/// @param panel The subtree's root.
	/// @param outerClip What clips it: its parent's bounds, or the window.
	/// @param depth Its depth, which picks its scratch record.
	void scratchTree(const Panel& panel, const Rect& outerClip, std::size_t depth);
	/// @brief Replays steps of a record into the frame's batches.
	/// @param record The record.
	/// @param first Its first step to replay.
	/// @param last Past its last step to replay.
	void replay(const detail::PaintRecord& record, std::size_t first, std::size_t last);
	/// @brief Adds a step's quads to the current batch: those in the region; fills take its white texel.
	/// @param record The record.
	/// @param op The step.
	/// @param white Whether they take the white texel.
	void addQuads(const detail::PaintRecord& record, const detail::PaintOp& op, bool white);
	/// @brief Uploads the frame's quads and draws each batch, with the shader the frame needs.
	/// @param width Window width, in window units.
	/// @param height Window height, in window units.
	/// @param shapes Whether the frame has shapes.
	/// @param color Whether the frame has colour glyphs.
	void submit(int width, int height, bool shapes, bool color);
	/// @brief Starts a new batch unless the current one uses the same texture.
	/// @param texture The texture.
	/// @param white Its white texel.
	void useTexture(GLuint texture, Vec2 white);
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
	/// Runs after the frame's quads are collected: recording text rasterises the glyphs it meets for the first time.
	/// @param font The font.
	/// @param page The page; fontTexture() made its texture.
	/// @param kind The page's kind.
	void updateFontTexture(const Font& font, std::uint32_t page, AtlasKind kind);
	/// @brief Draws each batch with its textures: the uploaded vertices, one draw call per batch.
	/// @param color Whether the frame's shader samples colour pages, which each batch then binds.
	/// @pre The frame's program, vertex array and vertex buffer are bound.
	void drawBatches(bool color);
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
	VertexArray vertexArray_;                                      ///< Vertex layout and index buffer binding.
	Buffer vertexBuffer_;                                          ///< Re-specified every frame (orphaning).
	Buffer indexBuffer_;                                           ///< 0 1 2, 2 3 0 per quad.
	Texture whiteTexture_;                                         ///< 1x1, for frames without text.
	std::unordered_map<std::uint64_t, FontTextures> fontTextures_; ///< By Font::id().
	detail::GeometryCache cache_;                                  ///< The panels' geometry, frame after frame.

	// Per frame, reused so steady-state frames don't allocate
	std::vector<detail::Vertex> vertices_;    ///< 4 per quad.
	std::vector<Batch> batches_;              ///< In drawing order.
	std::optional<Rect> region_;              ///< The region drawn, if not everything.
	std::uint32_t textPage_ = 0;              ///< The atlas page the text being replayed draws from.
	std::deque<detail::PaintRecord> scratch_; ///< drawFromScratch()'s records, one per depth (deque: they stay put).
	bool scratchShapes_ = false;              ///< drawFromScratch() recorded a shape.
	bool scratchColor_ = false;               ///< drawFromScratch() recorded a colour glyph.
	std::size_t drawCalls_ = 0;               ///< See drawCalls().
	/// The paint generation of whole frames drawn by draw(root, ...), counted from the top so it never meets a
	/// window's.
	std::uint64_t ownGeneration_ = std::uint64_t{1} << 63;

	std::size_t quadCapacity_ = 0; ///< Quads the index buffer can address.
};

} // namespace affresco::gl
