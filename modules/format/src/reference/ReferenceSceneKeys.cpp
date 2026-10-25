/// @file
/// @brief format::sceneKeys(): the reference of the keys the scene node adds, its models, look, controls and events.
///
/// Part of nodeKeys() (ReferenceNodes.cpp), which puts it after nodeTypeKeys(); the keys it shares with other types
/// (bind, view, time, grid) take their schema from there.

#include "Reference.hpp"

#include <array>

namespace affresco::format {

namespace {

/// The keys, in the order of the table.
constexpr auto sceneTable = std::to_array<KeyReference>({
    {"bind", "", "A state path keeping the view (camera and selection), both ways.", "scene"},
    {"view", "",
	 "The view, if there is no `bind`: `{ \"camera\": { \"target\": [x, y, z], \"yaw\", \"pitch\", \"distance\", "
	 "\"fieldOfView\", \"orthographic\" }, \"selection\": [ids] }`, each part only if given. Angles are in radians; "
	 "the pitch stays within ±90°, the distance within 0.01 and 100,000, the field of view within 0.05 and 3. "
	 "Orbiting, panning, zooming and selecting write it back and run `onChange`.",
	 "scene"},
    {"scene", R"json({"anyOf": [{"type": "object", "properties": {
		"nodes": {"type": "array", "maxItems": 100000, "items": {"type": "object"}},
		"materials": {"type": "array", "maxItems": 10000, "items": {"type": "object"}}}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "scene: { nodes, materials }"})json",
	 "The scene, usually `{{state.scene}}`: `{ \"nodes\": [...], \"materials\": [...] }`. A node: `{ \"id\", "
	 "\"name\", \"parent\", \"translation\": [x, y, z], \"rotation\": [x, y, z, w] (or `\"rotationDegrees\": [x, y, "
	 "z]`), \"scale\", \"mesh\", \"material\", \"light\", \"camera\", \"visible\", \"asset\" }`; `mesh` is `\"cube\"`, "
	 "`\"sphere\"`, `\"cylinder\"`, `\"cone\"`, `\"torus\"`, `\"plane\"` or a mesh of an asset; `light` is `{ "
	 "\"type\": "
	 "\"directional\" | \"point\" | \"spot\", \"color\", \"intensity\", \"range\", \"innerCone\", \"outerCone\" }`. "
	 "A material (PBR): `{ \"id\", \"name\", \"baseColor\": [r, g, b, a], \"metallic\", \"roughness\", "
	 "\"emissive\", \"alphaMode\", \"alphaCutoff\", \"doubleSided\" }`. Read when it changes, checked and bounded.",
	 "scene"},
    {"assets", R"json({"anyOf": [{"type": "object", "maxProperties": 64, "additionalProperties": {"type": "string"}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "scene: glTF models by name: { name: path }"})json",
	 "glTF models (`.gltf` or `.glb`) by name: `{ \"robot\": \"models/robot.glb\" }`, at most 64, each read with the "
	 "`reader.imageFile` permission and checked. A node with `\"asset\": \"robot\"` holds the model's nodes (ids "
	 "`\"<its id>/<id>\"`); a click on them selects that node.",
	 "scene"},
    {"animations", R"json({"anyOf": [{"type": "array", "maxItems": 1000, "items": {"type": "object"}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "scene: animation clips"})json",
	 "Animation clips: `[{ \"name\", \"tracks\": [{ \"node\", \"path\": \"translation\" | \"rotation\" | \"scale\", "
	 "\"interpolation\": \"linear\" | \"step\" | \"cubic\", \"times\", \"values\" }] }]`, played at `time`.",
	 "scene"},
    {"time", "", "The animations' time, in seconds (default 0).", "scene"},
    {"shading", R"json({"anyOf": [{"enum": ["material", "solid", "wireframe"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "`\"material\"` (the default: lit, with the lights and materials), `\"solid\"` (one grey, a studio light) or "
	 "`\"wireframe\"`.",
	 "scene"},
    {"grid", "", "A ground grid (default true).", "scene"},
    {"axes", "flag",
	 "The X (red) and Z (blue) axes on the floor (default true); with `\"up\": \"z\"`, X and Y (green).", "scene"},
    {"glyphs", "flag",
	 "Lights and cameras drawn as a modeller draws them (default false: a small cross): a camera's frustum with its up "
	 "triangle, a light's circle with a line down to the floor (a sun's rays, a spot's cone); the selected ones in "
	 "`outlineColor`.",
	 "scene"},
    {"up", R"json({"anyOf": [{"enum": ["y", "z"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Which way is up in the scene's frame: `\"y\"` (the default, as glTF) or `\"z\"` (as CAD and modelling tools): "
	 "then the nodes' transforms, the world gizmo's axes, the picked `point`, the `cursor3d` and the pivot are Z up, "
	 "and the floor is the XY plane.",
	 "scene"},
    {"cursor3d", R"json({"anyOf": [{"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
		{"type": "null"}, {"$ref": "#/$defs/v2/dynamic"}], "description": "scene: a 3D cursor"})json",
	 "A 3D cursor drawn at `[x, y, z]` (none without it): a ring of red and white dashes facing the eye; e.g. set "
	 "from a pick's `point`, and used as the gizmo's `pivot` or where new nodes go.",
	 "scene"},
    {"background", "color", "Behind the scene (default the theme's).", "scene"},
    {"gridColor", "color", "The grid's lines.", "scene"},
    {"outlineColor", "color", "The selection's outline (default orange).", "scene"},
    {"exposure",
	 R"json({"anyOf": [{"type": "number", "minimum": 0, "maximum": 64}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The light's multiplier, before tone mapping (default 1).", "scene"},
    {"xray", R"json({"anyOf": [{"type": "number", "minimum": 0, "maximum": 1}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "How see-through the surfaces are, so that what is behind them shows (an X-ray): 0 (the default) opaque, up to "
	 "1 (kept at 0.95, so they still show).",
	 "scene"},
    {"lighting", R"json({"anyOf": [{"enum": ["scene", "studio"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "`\"scene\"` (the default: the scene's lights, a soft studio light if it has none) or `\"studio\"` (the studio "
	 "light only, the scene's lights left out: a material preview).",
	 "scene"},
    {"gizmo", R"json({"anyOf": [{"type": "object", "properties": {"visible": {"type": "boolean"},
		"mode": {"enum": ["translate", "rotate", "scale"]}, "space": {"enum": ["world", "local"]},
		"pixels": {"type": "number", "minimum": 10, "maximum": 400},
		"pivot": {"anyOf": [{"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3}, {"type": "null"}]},
		"snap": {"type": "object", "properties": {
		"translate": {"type": "number", "minimum": 0}, "rotate": {"type": "number", "minimum": 0},
		"scale": {"type": "number", "minimum": 0}}}}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "scene: the transform gizmo on the selection"})json",
	 "A gizmo on the selection (none without it): `{ \"visible\", \"mode\": \"translate\" | \"rotate\" | \"scale\", "
	 "\"space\": \"world\" | \"local\", \"pixels\" (its size), \"pivot\": [x, y, z] (where it turns and scales "
	 "about, if not the selection's centre), \"snap\": { \"translate\", \"rotate\" (degrees), \"scale\" } }`. "
	 "Dragging a handle runs `onTransform`.",
	 "scene"},
    {"controls", R"json({"anyOf": [{"type": "object", "properties": {"orbit": {"type": "string"},
		"pan": {"type": "string"}, "select": {"type": "string"}, "zoom": {"type": "boolean"},
		"selects": {"type": "boolean"}, "minimumDistance": {"type": "number", "minimum": 0},
		"maximumDistance": {"type": "number", "minimum": 0}}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "scene: the pointer's gestures"})json",
	 "The pointer's gestures: `{ \"orbit\": \"middle\", \"pan\": \"shift+middle\", \"select\": \"left\", \"zoom\": "
	 "true, \"selects\": true, \"minimumDistance\", \"maximumDistance\" }` (these are the defaults); a binding is "
	 "`\"left\"`, `\"middle\"` or `\"right\"` after any of `\"shift+\"`, `\"control+\"` and `\"alt+\"`, or `\"none\"`. "
	 "A click picks, a drag on nothing draws a box; `selects` false leaves the selection to the handlers.",
	 "scene"},
    {"onSelect", "handler",
	 "A click (`event.type` `\"pick\"`: `id`, or null on nothing, `button`, `shift`, `x`, `y`, the world `point`) or a "
	 "box (`\"boxSelect\"`: `ids`, `shift`, `box`), with the `selection` after it.",
	 "scene"},
    {"onTransform", "handler",
	 "A gizmo drag: `event.phase` (`\"preview\"`, `\"end\"` or `\"cancel\"`), `mode`, `axis`, and `nodes`: "
	 "`[{ \"id\", \"translation\", \"rotation\", \"rotationDegrees\", \"scale\" }]`, and `text` (e.g. `\"Move 1, 0, "
	 "0\"`).",
	 "scene"},
});

} // namespace

std::span<const KeyReference> sceneKeys() { return sceneTable; }

} // namespace affresco::format
