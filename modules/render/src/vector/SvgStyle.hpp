#pragma once

/// @file
/// @brief vector::SvgStyle: the inherited presentation of SVG elements (paint, stroke, opacity, transform) and the
/// readers of the attribute values it is made from.

#include "SvgXml.hpp"

#include "affresco/render/Color.hpp"
#include "affresco/render/VectorPath.hpp"

#include <optional>
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
	PathTransform transform;                                   ///< From the element's units to pixels.

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

/// @brief Reads a transform list: translate, scale and matrix (without rotation or skew) compose; the others are
/// skipped.
/// @param text The list, e.g. "translate(4 4) scale(2)".
/// @return The transform it makes.
[[nodiscard]] PathTransform svgTransform(std::string_view text);

/// @brief Composes transforms.
/// @param outer Applied last.
/// @param inner Applied first.
/// @return The composition.
[[nodiscard]] constexpr PathTransform compose(const PathTransform& outer, const PathTransform& inner) noexcept {
	return {{outer.scale.x * inner.scale.x, outer.scale.y * inner.scale.y}, outer.apply(inner.offset)};
}

} // namespace affresco::vector
