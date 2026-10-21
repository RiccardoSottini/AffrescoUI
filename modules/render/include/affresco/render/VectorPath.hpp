#pragma once

/// @file
/// @brief VectorPath: SVG path data, parsed and flattened into contours, and the meshes of quads that fill and stroke
/// it (a fill rule, a stroke's width, joins, caps and dashes), anti-aliased with a fading fringe.

#include "affresco/render/Geometry.hpp"
#include "affresco/render/Painter.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief Which points a fill covers, like SVG's fill-rule.
enum class FillRule : std::uint8_t {
	nonZero, ///< Points the contours wind around at all.
	evenOdd, ///< Points the contours wind around an odd number of times: holes in holes are filled again.
};

/// @brief How a stroke turns a corner, like SVG's stroke-linejoin.
enum class LineJoin : std::uint8_t {
	miter, ///< A sharp corner, bevelled when it would reach past the miter limit.
	round, ///< A round corner.
	bevel, ///< A corner cut straight.
};

/// @brief How a stroke ends, like SVG's stroke-linecap.
enum class LineCap : std::uint8_t {
	butt,   ///< Square, at the end point.
	round,  ///< A half circle past the end point.
	square, ///< Square, half the width past the end point.
};

/// @brief How a path's outline is drawn.
struct StrokeStyle {
	float width = 1.0f;              ///< Its width.
	LineJoin join = LineJoin::miter; ///< Its corners.
	LineCap cap = LineCap::butt;     ///< Its ends.
	float miterLimit = 4.0f;         ///< How far a miter corner may reach, in widths, before it is bevelled.
	std::vector<float> dash;         ///< Dash and gap lengths, repeated (an odd list twice, like SVG); empty: solid.

	/// @brief Equal when every member is equal.
	bool operator==(const StrokeStyle&) const = default;
};

/// @brief How a path's units become window units: scaled, then moved.
struct PathTransform {
	Vec2 scale{1.0f, 1.0f}; ///< The scale on each axis.
	Vec2 offset{};          ///< Where the path's origin lands.

	/// @brief Transforms a point.
	/// @param point In the path's units.
	/// @return In window units.
	[[nodiscard]] constexpr Vec2 apply(Vec2 point) const noexcept {
		return {point.x * scale.x + offset.x, point.y * scale.y + offset.y};
	}
};

/// @brief A path of straight lines and cubic curves (quadratic curves and arcs become cubics), as SVG path data
/// describes it.
///
/// Path data is input (a revision or a peer can send any text), so parsing is bounded by Limits and refuses what it
/// can't read with the place of the problem. Flattening bounds the points it makes, and the meshes bound their quads
/// and their work (meshQuadLimit, meshWorkLimit): a path past them is drawn in part, never slowly.
class VectorPath {
public:
	/// @brief What parsing accepts.
	struct Limits {
		std::size_t dataBytes = std::size_t{64} << 10; ///< The longest path data.
		std::size_t commands = 16384;                  ///< The most commands, each repeated one counted.
		float magnitude = 1.0e6f;                      ///< The largest coordinate, either sign.
	};

	/// @brief A run of connected points, after flattening.
	struct Contour {
		std::vector<Vec2> points; ///< The points, no two in a row equal.
		bool closed = false;      ///< Whether the path closed it (fills close every contour anyway).
	};

	/// @brief Parses SVG path data: M, L, H, V, C, S, Q, T, A and Z, absolute (upper case) and relative.
	/// @param data The data, e.g. "M0 0 L10 0 L5 8 Z".
	/// @param limits What is accepted.
	/// @return The path.
	/// @throws std::invalid_argument If the data is broken or past the limits, naming the place.
	[[nodiscard]] static VectorPath parse(std::string_view data, const Limits& limits);
	/// @brief Parses SVG path data within the default Limits.
	/// @param data The data.
	/// @return The path.
	/// @throws std::invalid_argument If the data is broken or past the limits, naming the place.
	[[nodiscard]] static VectorPath parse(std::string_view data);

	/// @brief Starts a contour.
	/// @param point Where.
	void moveTo(Vec2 point);
	/// @brief Adds a straight line from the current point.
	/// @param point Its end.
	void lineTo(Vec2 point);
	/// @brief Adds a cubic Bézier curve from the current point.
	/// @param first Its first control point.
	/// @param second Its second control point.
	/// @param end Its end.
	void cubicTo(Vec2 first, Vec2 second, Vec2 end);
	/// @brief Closes the current contour.
	void close();

	/// @brief Tells whether it has no commands.
	[[nodiscard]] bool empty() const noexcept { return verbs_.empty(); }
	/// @brief The box of its points and control points (holding the curves), or nullopt if it is empty.
	[[nodiscard]] std::optional<Rect> bounds() const noexcept;
	/// @brief Flattens it into contours of points, at most flattenPointLimit of them.
	/// @param transform How its units become window units (curves are flattened for that size).
	/// @param tolerance How far a flattened curve may stray from the curve, in window units.
	/// @return The contours.
	[[nodiscard]] std::vector<Contour> flatten(const PathTransform& transform, float tolerance = 0.2f) const;

	/// @brief Equal when the commands and points are equal.
	bool operator==(const VectorPath&) const = default;

private:
	/// @brief A command.
	enum class Verb : std::uint8_t { move, line, cubic, close };

	std::vector<Verb> verbs_;  ///< The commands.
	std::vector<Vec2> points_; ///< Their points: one for move and line, three for cubic, none for close.
};

constexpr std::size_t flattenPointLimit = 65536; ///< The most points a flattened path has.
constexpr std::size_t meshQuadLimit = 65536;     ///< The most quads a fill or a stroke makes.
constexpr std::size_t meshWorkLimit = 1u << 24;  ///< The most edge visits a fill makes.

/// @brief Adds the quads that fill contours to a mesh: the area inside (by the rule), then a fringe @p feather wide
/// outside each contour that fades to nothing, which anti-aliases its edges.
/// @param contours The contours, in window units; each is closed.
/// @param rule Which points are inside.
/// @param mesh Where the quads go, 4 vertices each.
/// @param feather How wide the fringe is; 0 for none.
void appendFill(std::span<const VectorPath::Contour> contours, FillRule rule, std::vector<MeshVertex>& mesh,
                float feather = 1.0f);

/// @brief Adds the quads that stroke contours to a mesh: each segment, join and cap as a convex piece with a fringe
/// on its outer edges. The pieces overlap at the joins, so a translucent stroke is darker there.
/// @param contours The contours, in window units.
/// @param style The stroke; its width and dashes are in window units.
/// @param mesh Where the quads go, 4 vertices each.
/// @param feather How wide the fringe is; 0 for none.
void appendStroke(std::span<const VectorPath::Contour> contours, const StrokeStyle& style,
                  std::vector<MeshVertex>& mesh, float feather = 1.0f);

} // namespace affresco
