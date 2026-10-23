#pragma once

/// @file
/// @brief Scenes and animation clips as JSON, the form they take in a document's state.
///
/// A node: `{"id", "name", "parent", "translation": [x, y, z], "rotation": [x, y, z, w], "scale": [x, y, z],
/// "mesh", "material", "light": {...}, "camera": {...}, "visible"}`, where `"rotationDegrees": [x, y, z]` (Euler
/// angles, applied x, then y, then z) may stand for the rotation and wins over it; a material: `{"id", "name", "baseColor":
/// [r, g, b, a], "metallic", "roughness", "emissive": [r, g, b], "alphaMode", "alphaCutoff", "doubleSided",
/// "textures": {"baseColor", "metallicRoughness", "normal", "emissive"}}`. Members left out take their defaults;
/// members this file doesn't know (a material's "graph") are ignored here and kept by the document.

#include "affresco/core/Json.hpp"
#include "affresco/scene3d/Animation.hpp"
#include "affresco/scene3d/Scene.hpp"

#include <vector>

namespace affresco::scene3d {

/// @brief A scene as JSON. @param scene It. @return `{"nodes": [...], "materials": [...]}`.
[[nodiscard]] json toJson(const Scene& scene);
/// @brief A scene from JSON, checked.
/// @param value `{"nodes": [...], "materials": [...]}`.
/// @param limits What it may hold.
/// @return The scene.
/// @throws DocumentError Naming the place, if a member has the wrong type, a number isn't finite or the scene is
/// invalid (Scene::validate()).
[[nodiscard]] Scene sceneFromJson(const json& value, const SceneLimits& limits = {});

/// @brief A node as JSON. @param node It. @return The object.
[[nodiscard]] json toJson(const Node& node);
/// @brief A material as JSON. @param material It. @return The object.
[[nodiscard]] json toJson(const Material& material);

/// @brief Clips as JSON. @param clips Them. @return `[{"name", "tracks": [{"node", "path", "interpolation",
/// "times", "values"}]}]`.
[[nodiscard]] json toJson(const std::vector<AnimationClip>& clips);
/// @brief Clips from JSON, checked.
/// @param value The array.
/// @param maxKeys The most keys across all tracks.
/// @return The clips.
/// @throws DocumentError If malformed, or a track is invalid (Track::validate()).
[[nodiscard]] std::vector<AnimationClip> clipsFromJson(const json& value, std::size_t maxKeys = 100000);

/// @brief A Vec3 as a JSON array. @param v It. @return `[x, y, z]`.
[[nodiscard]] json toJson(Vec3 v);
/// @brief A Vec3 from a JSON array of 3 finite numbers. @param value It. @param where Names it in errors.
/// @return The vector. @throws DocumentError If malformed.
[[nodiscard]] Vec3 vec3FromJson(const json& value, std::string_view where);
/// @brief A Quat as a JSON array. @param q It. @return `[x, y, z, w]`.
[[nodiscard]] json toJson(Quat q);
/// @brief A rotation from a JSON array of 4 finite numbers, normalised. @param value It. @param where Names it in
/// errors. @return The rotation. @throws DocumentError If malformed.
[[nodiscard]] Quat quatFromJson(const json& value, std::string_view where);

} // namespace affresco::scene3d
