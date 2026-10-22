#pragma once

/// @file
/// @brief Blend modes: how a layer combines with what is below it, on the CPU (the reference) and in GLSL (the same
/// formulas, for the GPU compositor). The formulas are those of the W3C Compositing and Blending Level 1.

#include "affresco/imaging/Pixels.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace affresco::imaging {

/// @brief A blend mode: the twelve the editor offers.
enum class BlendMode : std::uint8_t {
	normal,     ///< The layer over what is below.
	multiply,   ///< Darkens: the product.
	screen,     ///< Lightens: the inverse of the product of the inverses.
	overlay,    ///< Multiply or screen, by what is below.
	softLight,  ///< A gentle overlay, by the layer.
	darken,     ///< The darker of the two, per channel.
	lighten,    ///< The lighter of the two, per channel.
	difference, ///< The absolute difference.
	hue,        ///< The layer's hue, with the saturation and luminosity below.
	saturation, ///< The layer's saturation, with the hue and luminosity below.
	color,      ///< The layer's hue and saturation, with the luminosity below.
	luminosity, ///< The layer's luminosity, with the hue and saturation below.
};

/// @brief How many blend modes there are.
inline constexpr std::size_t blendModeCount = 12;

/// @brief The modes' names, in order: "normal", "multiply", "screen", "overlay", "soft-light", ... (the file format's
/// and the menus' names).
[[nodiscard]] const std::array<std::string_view, blendModeCount>& blendModeNames() noexcept;
/// @brief A mode's name.
/// @param mode The mode.
[[nodiscard]] std::string_view blendModeName(BlendMode mode) noexcept;
/// @brief The mode of a name.
/// @param name The name.
/// @return The mode, or nothing for an unknown name.
[[nodiscard]] std::optional<BlendMode> blendModeOf(std::string_view name) noexcept;

/// @brief The blend function B(backdrop, source) of a mode, per colour (straight, 0 to 1, opaque).
/// @param mode The mode.
/// @param backdrop The colour below.
/// @param source The layer's colour.
/// @return The blended colour.
[[nodiscard]] Pixel blendColors(BlendMode mode, const Pixel& backdrop, const Pixel& source) noexcept;

/// @brief Composites a pixel over another: source-over with the mode's colour, straight alpha in and out.
/// @param backdrop What is below.
/// @param source The layer's pixel.
/// @param mode The mode.
/// @param opacity The layer's opacity and mask, 0 to 1, which scale the source's alpha.
/// @return The result.
[[nodiscard]] Pixel blendPixels(const Pixel& backdrop, const Pixel& source, BlendMode mode, float opacity) noexcept;

/// @brief The GLSL of the same: `vec4 blendPixels(vec4 backdrop, vec4 source, int mode, float opacity)`, straight
/// alpha in and out, mode the enum's value. Placed after surfaceShaderPrelude() and before the shader that calls it.
[[nodiscard]] std::string_view blendShaderSource() noexcept;

} // namespace affresco::imaging
