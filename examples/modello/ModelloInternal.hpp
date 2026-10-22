#pragma once

/// @file
/// @brief What Modello's native files share (ModelloView.cpp, ModelloEdit.cpp, ModelloFiles.cpp): reading a call's
/// arguments and writing the JSON patches that its `patch` steps apply to `computed.scene`.

#include "Modello.hpp"

#include <affresco/scene3d/Viewport.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace modello::detail {

using affresco::json;

/// @brief Where the scene is in the document, for patches.
inline constexpr std::string_view scenePath = "/computed/scene";
/// @brief Where the animations are in the document, for patches.
inline constexpr std::string_view animationsPath = "/computed/animations";

/// @brief What a viewport call is about: the scene at the viewport's time and the viewport's settings.
struct ViewCall {
	affresco::scene3d::Scene scene;               ///< At the time (`view.time`) of the animations.
	affresco::scene3d::ViewportSettings settings; ///< From `view`: the camera, the selection, the gizmo, the time.
	float width = 1.0f;                           ///< The viewport's, in window units.
	float height = 1.0f;                          ///< The viewport's, in window units.
};

/// @brief Reads `scene`, `animations`, `view`, `width` and `height`. @param with The arguments.
/// @return The call. @throws std::exception If the scene or the animations are malformed.
[[nodiscard]] ViewCall readView(const json& with);

/// @brief A number argument. @param with The arguments. @param key Its name. @param fallback If it is missing.
/// @return It.
[[nodiscard]] float numberIn(const json& with, std::string_view key, float fallback) noexcept;

/// @brief A text argument. @param with The arguments. @param key Its name. @return It, or empty.
[[nodiscard]] std::string textIn(const json& with, std::string_view key);

/// @brief The `selection` argument's ids. @param with The arguments. @return Them.
[[nodiscard]] std::vector<std::string> selectionIn(const json& with);

/// @brief A node's index in the scene. @param scene The scene. @param id Its id.
/// @return It, or the node count if there is none.
[[nodiscard]] std::size_t indexOf(const affresco::scene3d::Scene& scene, std::string_view id) noexcept;

/// @brief The patch that sets nodes' transforms. @param scene The scene, with the new transforms.
/// @param ids The nodes changed. @return JSON Patch operations on the document.
[[nodiscard]] json transformPatch(const affresco::scene3d::Scene& scene, const std::vector<std::string>& ids);

/// @brief The selected nodes that no other selected node is an ancestor of: the ones a transform moves.
/// @param scene The scene. @param selection The ids. @return Them, in selection order.
[[nodiscard]] std::vector<std::string> movedRoots(const affresco::scene3d::Scene& scene,
                                                  const std::vector<std::string>& selection);

/// @brief Adds the operations that append values to an array of the document, or that make it if there is none.
/// @param patch Receives them. @param array The array as the document holds it (or null). @param path Its path.
/// @param values A JSON array of what to append.
void appendTo(json& patch, const json& array, const std::string& path, const json& values);

/// @brief Registers a native that takes and returns JSON; what it throws fails the call with its message.
/// @param screen The screen. @param name Its name. @param call It.
void define(affresco::Screen& screen, const std::string& name, std::function<json(const json&)> call);

/// @brief Registers the viewport's natives: camera, pick, boxSelect, gizmoHit, gizmoDrag, numeric.
/// @param screen The screen. @param model What they share.
void registerViewNatives(affresco::Screen& screen, const std::shared_ptr<Model>& model);

/// @brief Registers the editing natives: add, remove, duplicate, materialNode, checkGraph, tracks, setKey, moveKey,
/// stats. @param screen The screen. @param model What they share.
void registerEditNatives(affresco::Screen& screen, const std::shared_ptr<Model>& model);

/// @brief Registers the file natives: samples, import, load, export. @param screen The screen.
/// @param model What they share.
void registerFileNatives(affresco::Screen& screen, const std::shared_ptr<Model>& model);

} // namespace modello::detail
