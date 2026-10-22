#pragma once

/// @file
/// @brief Keyframe animation: tracks of a node's translation, rotation or scale, with step, linear or cubic keys.

#include "affresco/scene3d/Scene.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::scene3d {

/// @brief How values between two keys are found (as glTF's samplers).
enum class Interpolation { Step, Linear, Cubic };

/// @brief Which part of a node's transform a track moves.
enum class TrackPath { Translation, Rotation, Scale };

/// @brief An interpolation's name. @param interpolation It. @return "step", "linear" or "cubic".
[[nodiscard]] const char* interpolationName(Interpolation interpolation) noexcept;
/// @brief An interpolation by name. @param name "step", "linear" or "cubic". @return It, or nothing.
[[nodiscard]] std::optional<Interpolation> interpolationNamed(std::string_view name) noexcept;
/// @brief A path's name. @param path It. @return "translation", "rotation" or "scale".
[[nodiscard]] const char* trackPathName(TrackPath path) noexcept;
/// @brief A path by name. @param name "translation", "rotation" or "scale". @return It, or nothing.
[[nodiscard]] std::optional<TrackPath> trackPathNamed(std::string_view name) noexcept;

/// @brief The keys of one part of one node.
///
/// `values` holds, per key, 3 floats (translation, scale) or 4 (a rotation's x, y, z, w). For Interpolation::Cubic
/// each key holds three of those in a row, as glTF does: the in-tangent, the value, the out-tangent.
struct Track {
	std::string node;                                    ///< The node's id.
	TrackPath path = TrackPath::Translation;             ///< What it moves.
	Interpolation interpolation = Interpolation::Linear; ///< Between keys.
	std::vector<float> times;                            ///< Seconds, ascending.
	std::vector<float> values;                           ///< See above.

	/// @brief The floats per value: 4 for a rotation, else 3. @return It.
	[[nodiscard]] std::size_t width() const noexcept { return path == TrackPath::Rotation ? 4 : 3; }
	/// @brief Whether the sizes agree, times ascend and everything is finite. @return An empty string, or the fault.
	[[nodiscard]] std::string validate() const;
	/// @brief The value at a time, held before the first key and after the last.
	/// @param time Seconds.
	/// @return width() floats; nothing if there are no keys.
	[[nodiscard]] std::optional<std::array<float, 4>> sample(float time) const;
};

/// @brief A named set of tracks played together.
struct AnimationClip {
	std::string name;          ///< Shown to people.
	std::vector<Track> tracks; ///< Each moves one part of one node.

	/// @brief The last key's time. @return Seconds, 0 if there are none.
	[[nodiscard]] float duration() const noexcept;
	/// @brief Moves the scene's nodes to where the tracks put them at a time; missing nodes are skipped.
	/// @param scene The scene. @param time Seconds.
	void apply(Scene& scene, float time) const;
};

/// @brief Adds or replaces a key on a track, keeping times in order (a new cubic key gets flat tangents).
/// @param track The track. @param time Seconds. @param value width() floats.
void setKey(Track& track, float time, const std::array<float, 4>& value);

/// @brief The value of a node's part, as a track holds it. @param node The node. @param path The part.
/// @return The floats.
[[nodiscard]] std::array<float, 4> partValue(const Node& node, TrackPath path) noexcept;

} // namespace affresco::scene3d
