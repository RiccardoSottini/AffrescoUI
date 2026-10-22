#pragma once

/// @file
/// @brief The "sinopiaShape" type: a path panel that draws one of Sinopia's shapes at its size, with a solid or
/// gradient fill, a stroke and a shadow that follows the outline.

#include <affresco/render/PathView.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace affresco {
class Screen;
}

namespace sinopia {

/// @brief A linear gradient, resolved to colours.
struct ShapeGradient {
	affresco::Color from; ///< Where it starts.
	affresco::Color to;   ///< Where it ends.
	float angle = 90.0f;  ///< Its direction in degrees: 0 left to right, 90 top to bottom.

	bool operator==(const ShapeGradient&) const = default; ///< Field by field.
};

/// @brief A shape's shadow, resolved.
struct ShapeShadow {
	affresco::Vec2 offset{0.0f, 4.0f}; ///< How far it is moved.
	float blur = 8.0f;                 ///< How soft its edge is.
	affresco::Color color;             ///< Its colour.

	bool operator==(const ShapeShadow&) const = default; ///< Field by field.
};

/// @brief A shape. The path type's properties give its fill colour, stroke and (for kind "path") its data and view
/// box; its own give the kind and what shapes it. Its meshes are built for its size and kept until either changes.
class ShapePanel : public affresco::PathView {
public:
	/// @brief Applies the type's own properties: "shape", "sides", "inner", "cornerRadius", "gradient", "outlineShadow".
	/// @param screen Resolves colours.
	/// @param properties The JSON object of them.
	/// @throws std::exception For a colour that isn't one.
	void apply(const affresco::Screen& screen, const std::string& properties);

	/// @brief The polygon of a closed contour clipped to one side of a line: the points p with p·normal >= at.
	/// @param points The contour.
	/// @param normal The line's normal.
	/// @param at Where the line is along it.
	/// @return What is left (fewer than three points: nothing).
	[[nodiscard]] static std::vector<affresco::Vec2> clipToHalfPlane(const std::vector<affresco::Vec2>& points,
	                                                                 affresco::Vec2 normal, float at);

protected:
	/// @brief Draws the panel's own look, then the shadow, the fill and the stroke.
	/// @param painter Where.
	void paint(affresco::Painter& painter) const override;

private:
	/// @brief Whether the meshes are built for a size and the look the panel has now.
	/// @param size The size.
	/// @return Whether they are.
	[[nodiscard]] bool isBuiltFor(affresco::Vec2 size) const;
	/// @brief Builds the meshes for a size, unless they are built for it.
	/// @param size The size.
	void build(affresco::Vec2 size) const;
	/// @brief Builds the shadow and the fill (a colour or a gradient's bands) of a closed shape.
	/// @param outline The outline.
	/// @param size The size.
	void buildFill(const std::vector<affresco::VectorPath::Contour>& outline, affresco::Vec2 size) const;
	/// @brief The shape's contours at a size.
	/// @param size The size.
	/// @return They.
	[[nodiscard]] std::vector<affresco::VectorPath::Contour> contours(affresco::Vec2 size) const;
	/// @brief Builds the gradient's bands: the whole fill in the first colour, then each band beyond a line across it.
	/// @param gradient The gradient.
	/// @param outline The outline.
	/// @param size The size.
	void buildGradient(const ShapeGradient& gradient, const std::vector<affresco::VectorPath::Contour>& outline,
	                   affresco::Vec2 size) const;

	std::string kind_ = "rect";             ///< rect, ellipse, polygon, star, line, arrow or path.
	int sides_ = 5;                         ///< A polygon's sides, a star's points.
	float inner_ = 0.5f;                    ///< A star's inner radius.
	float cornerRadius_ = 0.0f;             ///< A rectangle's corner radius.
	std::optional<ShapeGradient> gradient_; ///< A gradient fill instead of the fill colour.
	std::optional<ShapeShadow> shadow_;     ///< Its shadow.

	mutable std::vector<affresco::MeshVertex> shadowMesh_; ///< The shadow, from the top-left corner.
	mutable std::vector<std::pair<affresco::Color, std::vector<affresco::MeshVertex>>> fills_; ///< Fill layers.
	mutable std::vector<affresco::MeshVertex> strokeMesh_;                                     ///< The outline.
	mutable std::optional<affresco::Vec2> builtFor_; ///< The size they are built for; nullopt when stale.
	mutable affresco::Color builtFill_;              ///< The fill colour they were built with.
	mutable affresco::Color builtStroke_;            ///< The stroke colour they were built with.
	mutable affresco::StrokeStyle builtStyle_;       ///< The stroke style they were built with.
	mutable affresco::VectorPath builtPath_;         ///< The path they were built from (kind "path").
};

/// @brief Registers the "sinopiaShape" type on a screen.
/// @param screen The screen.
void registerShape(affresco::Screen& screen);

} // namespace sinopia
