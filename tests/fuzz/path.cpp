/// @file
/// @brief Fuzzes vector paths (render's VectorPath): a revision or a peer can give a path node any path data, and the
/// screen parses it, flattens it and builds the meshes that fill and stroke it.
///
/// The input is path data after one byte that chooses the drawing: bit 0 the even-odd rule, bits 1 and 2 the join,
/// bits 3 and 4 the cap, bit 5 a dash, bits 6 and 7 the scale. Data the parser refuses is fine; what it accepts must
/// flatten and mesh within the limits, with finite vertices.

#include "Fuzz.hpp"

#include <affresco/render/VectorPath.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

/// @brief Checks a mesh: whole quads, within the limit, every vertex finite and its coverage between 0 and 1.
/// @param mesh The mesh.
void checkMesh(const std::vector<affresco::MeshVertex>& mesh) {
	if(mesh.size() % 4 != 0) affresco::fuzz::broken("whole quads");
	if(mesh.size() / 4 > affresco::meshQuadLimit) affresco::fuzz::broken("quads within the limit");
	for(const affresco::MeshVertex& vertex : mesh)
		if(!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) || !(vertex.coverage >= 0.0f) ||
		   vertex.coverage > 1.0f)
			affresco::fuzz::broken("vertices are finite, coverage within 0 and 1");
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const unsigned first = size > 0 ? data[0] : 0u;
	const std::string_view text = affresco::fuzz::text(data, size);
	affresco::VectorPath path;
	try {
		path = affresco::VectorPath::parse(size > 0 ? text.substr(1) : text);
	} catch(const std::invalid_argument&) {
		return 0; // broken or past the limits: refused, as it should be
	}
	const float scale = 0.01f * std::pow(20.0f, static_cast<float>(first >> 6));
	const std::vector<affresco::VectorPath::Contour> contours = path.flatten({{scale, scale}, {3.0f, 5.0f}});
	std::size_t points = 0;
	for(const affresco::VectorPath::Contour& contour : contours) points += contour.points.size();
	if(points > affresco::flattenPointLimit) affresco::fuzz::broken("points within the limit");

	std::vector<affresco::MeshVertex> fill;
	affresco::appendFill(contours, (first & 1u) != 0 ? affresco::FillRule::evenOdd : affresco::FillRule::nonZero, fill);
	checkMesh(fill);
	affresco::StrokeStyle style;
	style.width = 0.5f + static_cast<float>(first & 7u);
	style.join = static_cast<affresco::LineJoin>(((first >> 1) & 3u) % 3u);
	style.cap = static_cast<affresco::LineCap>(((first >> 3) & 3u) % 3u);
	if((first & 32u) != 0) style.dash = {3.0f, 0.5f, 0.0f};
	std::vector<affresco::MeshVertex> stroke;
	affresco::appendStroke(contours, style, stroke);
	checkMesh(stroke);
	return 0;
}
