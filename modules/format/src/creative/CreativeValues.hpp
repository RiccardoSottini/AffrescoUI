#pragma once

/// @file
/// @brief The JSON values of the creative controls (colorPicker, gradient, curves), shared by their node types and by
/// the expression functions curve() and gradientColor(). Reading is lenient: what isn't understood is left out.

#include "affresco/core/Json.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/CurvesEditor.hpp"
#include "affresco/render/GradientEditor.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief Formats a colour as the creative controls write it.
/// @param color The colour.
/// @return "#rrggbbaa", always eight digits.
[[nodiscard]] std::string colorToJson(Color color);

/// @brief Reads a colour a creative control's value holds.
/// @param value A hex text ("#rgb", "#rrggbb" or "#rrggbbaa").
/// @return The colour, or std::nullopt when it isn't one.
[[nodiscard]] std::optional<Color> colorFromJson(const json& value);

/// @brief Reads a gradient: { "kind": "linear" or "radial", "angle": degrees, "stops": [{ "at", "color" }] }.
/// @param value The value; anything else gives the default (black to white, linear, 90 degrees).
/// @return The gradient, normalised (see normalizeGradient()).
[[nodiscard]] Gradient gradientFromJson(const json& value);

/// @brief Writes a gradient as gradientFromJson() reads it.
/// @param gradient The gradient.
/// @return The value.
[[nodiscard]] json gradientToJson(const Gradient& gradient);

/// @brief Reads a curve's points: a list of [x, y] pairs, at most maxCurvePoints.
/// @param value The list; anything else gives no points (normalizeCurve() makes the diagonal of that).
/// @return The points, not normalised.
[[nodiscard]] std::vector<CurvePoint> curvePointsFromJson(const json& value);

/// @brief Writes a curve's points as [[x, y], ...].
/// @param points The points.
/// @return The list.
[[nodiscard]] json curvePointsToJson(std::span<const CurvePoint> points);

/// @brief Reads an interpolation's name.
/// @param name "monotone", "linear" or "bezier".
/// @return The interpolation, or std::nullopt for another name.
[[nodiscard]] std::optional<CurveInterpolation> curveInterpolationFromName(std::string_view name) noexcept;

/// @brief The key of a channel in a curves value.
/// @param channel The channel.
/// @return "points" (RGB), "red", "green" or "blue".
[[nodiscard]] std::string_view curveChannelKey(CurveChannel channel) noexcept;

} // namespace affresco::format
