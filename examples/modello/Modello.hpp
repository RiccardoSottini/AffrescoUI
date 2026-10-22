#pragma once

/// @file
/// @brief Modello's native functions and its viewport: what its screen (modello.screen.json) can't do in JSON. The
/// scene itself is in the document (`computed.scene`, `computed.animations`), so every edit is a revision that undo
/// takes back; the natives only compute: a picked node, a camera, a gizmo drag's new transforms, a patch.

#include <affresco/affresco.hpp>
#include <affresco/scene3d/Mesh.hpp>
#include <affresco/scene3d/Renderer.hpp>
#include <affresco/scene3d/Scene.hpp>

#include <filesystem>
#include <memory>

namespace modello {

/// @brief What Modello's natives and its viewport share.
struct Model {
	/// @brief The store with the primitives' meshes, and the folders imports are read from and exports written to.
	/// @param samples The bundled samples' folder. @param data The data folder (`imports/` and `exports/` in it).
	Model(std::filesystem::path samples, std::filesystem::path data);

	std::shared_ptr<affresco::scene3d::Assets> assets; ///< Meshes and textures: primitives, then imports.
	std::shared_ptr<affresco::scene3d::ViewportProbe> probe =
	    std::make_shared<affresco::scene3d::ViewportProbe>(); ///< What the viewport drew and picked.
	std::filesystem::path samples;                            ///< examples/modello/samples.
	std::filesystem::path data;                               ///< The screen's data folder.
};

/// @brief The mesh id of a primitive in the store. @param primitive It. @return `"primitive:<name>"`.
[[nodiscard]] std::string primitiveMesh(affresco::scene3d::Primitive primitive);

/// @brief Registers the `scene` surface (the renderer, with a depth buffer) and Modello's natives (README.md):
/// `modello.camera`, `modello.pick`, `modello.boxSelect`, `modello.gizmoHit`, `modello.gizmoDrag`, `modello.numeric`,
/// `modello.add`, `modello.remove`, `modello.duplicate`, `modello.samples`, `modello.import`, `modello.load`,
/// `modello.export`, `modello.materialNode`, `modello.checkGraph`, `modello.tracks`, `modello.setKey`,
/// `modello.moveKey` and `modello.stats`.
/// @param screen The screen. @param model What they share.
void registerModello(affresco::Screen& screen, const std::shared_ptr<Model>& model);

} // namespace modello
