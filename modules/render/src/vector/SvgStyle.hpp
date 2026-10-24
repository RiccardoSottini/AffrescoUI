#pragma once

/// @file
/// @brief vector::SvgStyle: the inherited presentation of SVG elements (paint, stroke, opacity, transform) and the
/// readers of the attribute values it is made from.

#include "SvgXml.hpp"

#include "affresco/render/Color.hpp"
#include "affresco/render/Transform.hpp"
#include "affresco/render/VectorPath.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vector {

/// @brief How an element and what it holds are drawn; children start from their parent's.
struct SvgStyle {
	std::optional<Color> fill = Color{0.0f, 0.0f, 0.0f, 1.0f}; ///< The fill, or nullopt for none.
	FillRule fillRule = FillRule::nonZero;                     ///< The fill rule.
	std::optional<Color> stroke;                               ///< The stroke, or nullopt for none.
	StrokeStyle line;                                          ///< The stroke's shape, in the element's units.
	float opacity = 1.0f;                                      ///< Opacity, multiplied down the tree.
	float fillOpacity = 1.0f;                                  ///< The fill's opacity.
	float strokeOpacity = 1.0f;                                ///< The stroke's opacity.
	Affine2 transform;                                         ///< From the element's units to pixels.
	std::string fillGradient;   ///< The gradient the fill names ("url(#sky)" gives "sky"), or "" for a colour.
	std::string strokeGradient; ///< The gradient the stroke names, or "".

	/// @brief Applies an element's attributes: presentation attributes, then its style attribute, then its transform.
	/// The opacity attribute multiplies into the inherited one.
	/// @param tag The element.
	void apply(const XmlTag& tag);
	/// @brief Applies one property.
	/// @param key Its name, e.g. "stroke-width".
	/// @param value Its value.
	void set(std::string_view key, std::string_view value);
};

/// @brief Reads a length or a number (spaces and a "px" unit allowed).
/// @param text The text.
/// @return The number, or nullopt if it isn't one or is beyond a million either sign.
[[nodiscard]] std::optional<float> svgNumber(std::string_view text);

/// @brief Reads a list of numbers separated by spaces or commas, at most 64.
/// @param text The text.
/// @return The numbers read up to the first that isn't one.
[[nodiscard]] std::vector<float> svgNumbers(std::string_view text);

/// @brief Reads a transform list: translate, scale, rotate (with or without a centre), skewX, skewY and matrix, at
/// most 64 functions; one that isn't read is skipped.
/// @param text The list, e.g. "translate(4 4) rotate(30 10 10) scale(2)".
/// @return The transform it makes.
[[nodiscard]] Affine2 svgTransform(std::string_view text);

/// @brief Reads a gradient a paint names: "url(#id)".
/// @param text The paint.
/// @return The id, or nullopt if it names none.
[[nodiscard]] std::optional<std::string> svgPaintUrl(std::string_view text);

/// @brief Reads a colour as SVG writes it: "#rgb", "#rrggbb", a few names, currentColor.
/// @param text The value.
/// @return The colour, or nullopt.
[[nodiscard]] std::optional<Color> svgColor(std::string_view text);

} // namespace affresco::vector
