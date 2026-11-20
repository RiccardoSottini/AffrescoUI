/// @file
/// @brief A glyph's outline drawn by HarfBuzz into contours of cubic segments.

#include "GlyphOutline.hpp"

#include <hb.h>

#include <memory>

namespace affresco::imaging::detail {

namespace {

/// @brief The contours being drawn, and the scale they are drawn at.
struct Drawing {
	float scale = 1.0f;                              ///< Pixels per font unit.
	std::vector<std::vector<PathPoint>> contours;    ///< The contours so far.

	/// @brief A point in pixels, y down, from one in font units, y up.
	/// @param x Across, in font units.
	/// @param y Up, in font units.
	[[nodiscard]] VectorPoint at(float x, float y) const noexcept { return {x * scale, -y * scale}; }

	/// @brief Adds an anchor reached by a cubic segment from the last one.
	/// @param out The last anchor's handle.
	/// @param in The new anchor's handle.
	/// @param to The new anchor.
	void curve(VectorPoint out, VectorPoint in, VectorPoint to) {
		if(contours.empty() || contours.back().empty()) contours.push_back({{to, to, to}});
		contours.back().back().out = out;
		contours.back().push_back({to, in, to});
	}
};

/// @brief HarfBuzz's move-to: starts a contour.
void moveTo(hb_draw_funcs_t*, void* data, hb_draw_state_t*, float x, float y, void*) {
	auto& drawing = *static_cast<Drawing*>(data);
	const VectorPoint to = drawing.at(x, y);
	drawing.contours.push_back({{to, to, to}});
}

/// @brief HarfBuzz's line-to: a straight segment.
void lineTo(hb_draw_funcs_t*, void* data, hb_draw_state_t* state, float x, float y, void*) {
	auto& drawing = *static_cast<Drawing*>(data);
	const VectorPoint from = drawing.at(state->current_x, state->current_y), to = drawing.at(x, y);
	drawing.curve(from, to, to);
}

/// @brief HarfBuzz's quadratic-to: raised to a cubic segment.
void quadraticTo(hb_draw_funcs_t*, void* data, hb_draw_state_t* state, float controlX, float controlY, float x,
                 float y, void*) {
	auto& drawing = *static_cast<Drawing*>(data);
	const VectorPoint from = drawing.at(state->current_x, state->current_y), control = drawing.at(controlX, controlY);
	const VectorPoint to = drawing.at(x, y);
	constexpr float third = 2.0f / 3.0f;
	drawing.curve({from.x + (control.x - from.x) * third, from.y + (control.y - from.y) * third},
	              {to.x + (control.x - to.x) * third, to.y + (control.y - to.y) * third}, to);
}

/// @brief HarfBuzz's cubic-to: a cubic segment.
void cubicTo(hb_draw_funcs_t*, void* data, hb_draw_state_t*, float firstX, float firstY, float secondX, float secondY,
             float x, float y, void*) {
	auto& drawing = *static_cast<Drawing*>(data);
	drawing.curve(drawing.at(firstX, firstY), drawing.at(secondX, secondY), drawing.at(x, y));
}

/// @brief HarfBuzz's close-path: the last anchor, back on the first, becomes the first's arriving handle.
void closePath(hb_draw_funcs_t*, void* data, hb_draw_state_t*, void*) {
	auto& contours = static_cast<Drawing*>(data)->contours;
	if(contours.empty() || contours.back().size() < 2) return;
	std::vector<PathPoint>& points = contours.back();
	const PathPoint last = points.back();
	if(last.anchor == points.front().anchor) {
		points.front().in = last.in;
		points.pop_back();
	}
}

/// @brief Destroys a HarfBuzz object.
template <auto destroy>
struct Release {
	/// @brief Destroys it.
	template <typename T>
	void operator()(T* object) const noexcept {
		destroy(object);
	}
};

} // namespace

std::vector<std::vector<PathPoint>> glyphContours(std::span<const unsigned char> file, unsigned glyph, float scale) {
	if(file.empty()) return {};
	const std::unique_ptr<hb_blob_t, Release<hb_blob_destroy>> blob(
	    hb_blob_create(reinterpret_cast<const char*>(file.data()), static_cast<unsigned>(file.size()),
	                   HB_MEMORY_MODE_READONLY, nullptr, nullptr));
	const std::unique_ptr<hb_face_t, Release<hb_face_destroy>> face(hb_face_create(blob.get(), 0));
	const std::unique_ptr<hb_font_t, Release<hb_font_destroy>> font(hb_font_create(face.get()));
	const std::unique_ptr<hb_draw_funcs_t, Release<hb_draw_funcs_destroy>> funcs(hb_draw_funcs_create());
	hb_draw_funcs_set_move_to_func(funcs.get(), moveTo, nullptr, nullptr);
	hb_draw_funcs_set_line_to_func(funcs.get(), lineTo, nullptr, nullptr);
	hb_draw_funcs_set_quadratic_to_func(funcs.get(), quadraticTo, nullptr, nullptr);
	hb_draw_funcs_set_cubic_to_func(funcs.get(), cubicTo, nullptr, nullptr);
	hb_draw_funcs_set_close_path_func(funcs.get(), closePath, nullptr, nullptr);
	Drawing drawing;
	drawing.scale = scale;
	hb_font_draw_glyph(font.get(), glyph, funcs.get(), &drawing);
	std::erase_if(drawing.contours, [](const std::vector<PathPoint>& contour) { return contour.size() < 2; });
	return std::move(drawing.contours);
}

} // namespace affresco::imaging::detail
