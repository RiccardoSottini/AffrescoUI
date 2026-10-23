#pragma once

/// @file
/// @brief PathView: a panel that draws a vector path (filled, stroked or both), fitted to it through a view box.

#include "affresco/render/GradientFill.hpp"
#include "affresco/render/ImageView.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/VectorPath.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief A panel that draws a vector path: its inside in one colour, its outline in another.
///
/// The path's coordinates are the panel's own (from its top-left corner) unless a view box is set: then the box is
/// fitted to the panel like an image (contain, cover or fill). With contain and cover the stroke's width and dashes
/// are in the view box's units, scaled with it; with fill (stretched unevenly) they are in window units. The meshes are
/// built when the panel's size or the path changes, not every frame. Transparent by default, like ImageView.
class PathView : public Panel {
public:
	/// @brief An empty view: no path, a transparent fill and no stroke.
	PathView();

	/// @brief Sets the path.
	/// @param path The path.
	void setPath(VectorPath path);
	/// @brief The path.
	[[nodiscard]] const VectorPath& path() const noexcept { return path_; }
	/// @brief Sets the path from SVG path data, parsing it only when it differs from the data set last.
	/// @param data The data, within VectorPath's default Limits.
	/// @throws std::invalid_argument If it can't be read; the path is kept.
	void setPathData(std::string_view data);

	/// @brief Sets the area of the path's coordinates fitted to the panel.
	/// @param viewBox The area, or nullopt to draw the path in the panel's own units.
	void setViewBox(std::optional<Rect> viewBox);
	/// @brief The view box, if any.
	[[nodiscard]] const std::optional<Rect>& viewBox() const noexcept { return viewBox_; }
	/// @brief Sets how the view box fills the panel (default: contain).
	/// @param fit The fit.
	void setFit(ImageFit fit);
	/// @brief How the view box fills the panel.
	[[nodiscard]] ImageFit fit() const noexcept { return fit_; }

	/// @brief Sets the inside's colour (transparent: not filled).
	/// @param color The colour.
	void setFillColor(Color color);
	/// @brief The inside's colour.
	[[nodiscard]] Color fillColor() const noexcept { return fillColor_; }
	/// @brief Sets which points are inside.
	/// @param rule The rule (default: non-zero).
	void setFillRule(FillRule rule);
	/// @brief Which points are inside.
	[[nodiscard]] FillRule fillRule() const noexcept { return fillRule_; }
	/// @brief Sets a gradient to fill the inside with, in place of the fill colour, spread over the panel.
	/// @param gradient The gradient (normalised, see normalizeGradient()), or nullopt for the fill colour.
	void setFillGradient(std::optional<Gradient> gradient);
	/// @brief The inside's gradient, if any.
	[[nodiscard]] const std::optional<Gradient>& fillGradient() const noexcept { return fillGradient_; }

	/// @brief Sets the outline's colour (transparent: no outline).
	/// @param color The colour.
	void setStrokeColor(Color color);
	/// @brief The outline's colour.
	[[nodiscard]] Color strokeColor() const noexcept { return strokeColor_; }
	/// @brief Sets how the outline is drawn.
	/// @param style The width, joins, caps and dashes.
	void setStrokeStyle(StrokeStyle style);
	/// @brief How the outline is drawn.
	[[nodiscard]] const StrokeStyle& strokeStyle() const noexcept { return strokeStyle_; }

protected:
	void paint(Painter& painter) const override;

private:
	/// @brief Forgets the meshes and redraws.
	void changed();
	/// @brief Builds the meshes for a size of the panel, unless they are built for it already.
	/// @param size The panel's width and height.
	void build(Vec2 size) const;
	/// @brief How the path's coordinates map to the panel's, for a size.
	/// @param size The panel's width and height.
	/// @return The transform.
	[[nodiscard]] PathTransform transformFor(Vec2 size) const noexcept;

	VectorPath path_;                         ///< See path().
	std::string pathData_;                    ///< The data path_ was parsed from by setPathData(), else empty.
	std::optional<Rect> viewBox_;             ///< See viewBox().
	ImageFit fit_ = ImageFit::contain;        ///< See fit().
	Color fillColor_ = colors::transparent;   ///< See fillColor().
	FillRule fillRule_ = FillRule::nonZero;   ///< See fillRule().
	std::optional<Gradient> fillGradient_;    ///< See fillGradient().
	Color strokeColor_ = colors::transparent; ///< See strokeColor().
	StrokeStyle strokeStyle_;                 ///< See strokeStyle().

	mutable std::vector<MeshVertex> fillMesh_;        ///< The inside, from the panel's top-left corner.
	mutable std::vector<MeshVertex> strokeMesh_;      ///< The outline, likewise.
	mutable std::vector<GradientBand> gradientBands_; ///< The inside cut into the gradient's colours, if it has one.
	mutable std::optional<Vec2> builtFor_;            ///< The size the meshes are built for; nullopt when stale.
};

} // namespace affresco
