#pragma once

/// @file
/// @brief Material node graphs: a `graph` node's nodes and links, checked against a fixed set of operations (their
/// port kinds, one link per input, no cycles), then assembled into GLSL from fixed snippets. A document chooses
/// operations and numbers, never shader source: only the host's snippets and the numbers it formats reach the shader.

#include "affresco/core/Json.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco::scene3d {

/// @brief What a material graph may hold.
struct MaterialGraphLimits {
	std::size_t nodes = 256;  ///< Nodes at most.
	std::size_t links = 1024; ///< Links at most.
};

/// @brief A checked and assembled graph.
struct MaterialGraphResult {
	std::vector<std::string> errors; ///< What is wrong, one line each; empty if it compiled.
	std::string glsl;                ///< The `graphMaterial` function, if it compiled.
	/// @brief Whether it compiled. @return true if there are no errors.
	[[nodiscard]] bool ok() const noexcept { return errors.empty(); }
};

/// @brief The operations' names, in menu order: "output", "color", "number", "mix", "multiply", "add", "scale",
/// "invert", "fresnel", "checker", "noise", "height". @return Them.
[[nodiscard]] const std::vector<std::string>& materialOperations();

/// @brief A new graph node for an operation, with its title and typed ports, ready for a `graph` node's `nodes`.
/// @param operation Its name. @param id The node's id. @param x Where, across. @param y Where, down.
/// @return The object, or null if the operation is unknown.
[[nodiscard]] json makeMaterialNode(std::string_view operation, std::string_view id, double x, double y);

/// @brief Checks a graph and assembles its GLSL.
///
/// The graph is `{ nodes, links }` as a `graph` node takes them; each node names its operation in `op` and may hold
/// numbers in `value` (a "number": a number; a "color": three). The function it writes is
/// `void graphMaterial(vec3 position, vec3 normal, vec3 toEye, inout vec3 baseColor, inout float metallic,
/// inout float roughness, inout vec3 emissive)`; it sets the output node's linked inputs.
/// @param graph The graph. @param limits What it may hold.
/// @return The errors, or the GLSL.
[[nodiscard]] MaterialGraphResult compileMaterialGraph(const json& graph, const MaterialGraphLimits& limits = {});

} // namespace affresco::scene3d
