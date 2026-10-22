#pragma once

/// @file
/// @brief detail::PaintRecorder: the Painter a panel draws with; it records clipped quads and the steps that batch
/// them into a PaintRecord, which the renderer keeps and replays.

#include "RetainedGeometry.hpp"

#include "affresco/render/Painter.hpp"

namespace affresco::detail {

/// @brief Records what a panel draws: quads clipped to the panel (shadows to what clips the panel), their colours and
/// texture coordinates, and the batch steps between them. No OpenGL: glyphs are looked up (and rasterised into their
/// font's atlas), not uploaded.
///
/// Transparent quads and quads clipped to nothing are dropped, but the steps around them are kept, so replaying the
/// record changes the frame's batches exactly as drawing directly did.
class PaintRecorder final : public Painter {
public:
	/// @brief Records into a record, from its current end.
	/// @param record The record; it must outlive the recorder.
	/// @param clip The panel's bounds: what clips its drawing.
	/// @param outerClip What clips the panel: where its shadow may fall.
	PaintRecorder(PaintRecord& record, const Rect& clip, const Rect& outerClip) noexcept
	    : record_(record), clip_(clip), outerClip_(outerClip), shapeClip_(clip) {}

	/// @brief Keeps edges smooth for a panel drawn turned or skewed: rectangles become shapes (anti-aliased by
	/// distance, which a transform keeps), and shapes may reach a unit past the panel's own edges for their outer ramp
	/// (not past an edge something else clipped).
	/// @param frame The panel's frame.
	void smoothEdges(const Rect& frame) noexcept;

	void fillRect(const Rect& rect, Color color) override;
	void drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) override;
	void fillRoundedRect(const Rect& rect, const CornerRadii& radii, Color color) override;
	void strokeRoundedRect(const Rect& rect, const CornerRadii& radii, float width, Color color) override;
	void drawShadow(const Rect& rect, const CornerRadii& radii, const Shadow& shadow) override;
	void drawImage(const Bitmap& bitmap, const Rect& rect) override;
	void drawSurface(const SurfaceView& view, const Rect& rect) override;
	void fillMesh(std::span<const MeshVertex> quads, Vec2 offset, Color color) override;

private:
	/// @brief How a shape is drawn.
	struct ShapeStyle {
		Color color;          ///< Its colour.
		float outline = 0.0f; ///< The width of its outline, inside its edges; 0 fills it.
		float blur = 0.0f;    ///< How soft its edges are (see Shadow::blur); 0 anti-aliases them.
	};

	/// @brief A quarter of a shape: the part nearest one of its corners.
	struct Quarter {
		Rect piece;                     ///< The area it covers, from the shape's centre to past its corner.
		float radius = 0.0f;            ///< The corner's radius.
		bool left = false, top = false; ///< Which edges are nearest: left or right, top or bottom.
	};

	/// @brief Starts a step.
	/// @param step What it does.
	/// @param font Its font, for text steps.
	/// @param page Its atlas page, for glyph steps.
	void beginStep(PaintStep step, const Font* font = nullptr, std::uint32_t page = 0);
	/// @brief Clips a quad to the panel (adjusting its texture coordinates) and adds it to the current step.
	/// @param rect The quad.
	/// @param uv Its texture coordinates (fills get theirs when replayed).
	/// @param color Its colour; transparent quads are dropped.
	/// @param kind What it is: a plain quad or a colour glyph.
	void pushQuad(const Rect& rect, const Rect& uv, Color color, QuadKind kind = QuadKind::plain);
	/// @brief Adds a rounded rectangle, an outline or a shadow, as four quarters.
	/// @param rect The shape.
	/// @param radii Its corners; each is clamped to half the shorter side.
	/// @param style How it is drawn; transparent shapes are dropped.
	/// @param clip What it is clipped to.
	void pushShape(const Rect& rect, const CornerRadii& radii, const ShapeStyle& style, const Rect& clip);
	/// @brief Clips one quarter of a shape and adds it to the current step.
	/// @param rect The whole shape.
	/// @param quarter The quarter.
	/// @param style How it is drawn, with the outline already clamped.
	/// @param clip What it is clipped to.
	void pushQuarter(const Rect& rect, const Quarter& quarter, const ShapeStyle& style, const Rect& clip);
	/// @brief Clips a mesh's quad to the panel and adds what is left, as one quad or a fan of quads.
	/// @param corners Its 4 corners, already moved to window units.
	/// @param color Its colour, before the corners' coverage.
	void pushMeshQuad(const std::array<MeshVertex, 4>& corners, Color color);
	/// @brief Adds four vertices to the current step and its quad to the extent.
	/// @param corners Top-left, top-right, bottom-right, bottom-left.
	void addQuad(const std::array<Vertex, 4>& corners);

	PaintRecord& record_; ///< Not owning.
	Rect clip_;           ///< The panel's bounds.
	Rect outerClip_;      ///< What clips the panel.
	Rect shapeClip_;      ///< What clips its shapes: its bounds, or a unit more where smoothEdges() allows.
	bool smooth_ = false; ///< smoothEdges() was called: rectangles are drawn as shapes.
};

} // namespace affresco::detail
