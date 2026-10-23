/// @file
/// @brief Reading a viewport's parameters, the scene at a time, and the gizmo over a selection (Viewport.hpp).

#include "affresco/scene3d/Viewport.hpp"

#include "affresco/scene3d/SceneJson.hpp"

#include <algorithm>
#include <cmath>
#include <exception>

namespace affresco::scene3d {

namespace {

/// @brief A finite number member, or a fallback. @param params The object. @param key The member.
/// @param fallback If it is missing or not a finite number. @return The value.
// NOLINTNEXTLINE(bugprone-exception-escape): json throws only on a wrong type, which is checked first
[[nodiscard]] float numberOr(const json& params, const char* key, float fallback) noexcept {
	const auto found = params.find(key);
	if(found == params.end() || !found->is_number()) return fallback;
	const auto value = found->get<double>();
	return std::isfinite(value) ? static_cast<float>(value) : fallback;
}

/// @brief A boolean member, or a fallback. @param params The object. @param key The member.
/// @param fallback If it is missing or not a boolean. @return The value.
// NOLINTNEXTLINE(bugprone-exception-escape): json throws only on a wrong type, which is checked first
[[nodiscard]] bool flagOr(const json& params, const char* key, bool fallback) noexcept {
	const auto found = params.find(key);
	return found != params.end() && found->is_boolean() ? found->get<bool>() : fallback;
}

/// @brief A string member, or empty. @param params The object. @param key The member. @return The value.
[[nodiscard]] std::string textOf(const json& params, const char* key) {
	const auto found = params.find(key);
	return found != params.end() && found->is_string() ? found->get<std::string>() : std::string();
}

/// @brief The value of one hexadecimal digit. @param digit It. @return 0..15, or -1.
[[nodiscard]] int hexValue(char digit) noexcept {
	if(digit >= '0' && digit <= '9') return digit - '0';
	if(digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
	if(digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
	return -1;
}

/// @brief Three finite numbers. @param value The JSON. @return The vector, or nothing.
// NOLINTNEXTLINE(bugprone-exception-escape): json throws only on a wrong type, which is checked first
[[nodiscard]] std::optional<Vec3> pointFromJson(const json& value) noexcept {
	if(!value.is_array() || value.size() != 3) return std::nullopt;
	for(const json& part : value)
		if(!part.is_number()) return std::nullopt;
	const Vec3 point{value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
	return isFinite(point) ? std::optional<Vec3>(point) : std::nullopt;
}

/// @brief Reads the gizmo's settings. @param value The `gizmo` member. @return The settings.
[[nodiscard]] GizmoSettings gizmoFromJson(const json& value) {
	GizmoSettings gizmo;
	if(!value.is_object()) return gizmo;
	gizmo.visible = flagOr(value, "visible", true);
	gizmo.mode = gizmoModeNamed(textOf(value, "mode")).value_or(GizmoMode::Translate);
	gizmo.local = textOf(value, "space") == "local";
	gizmo.active = gizmoAxisNamed(textOf(value, "axis")).value_or(GizmoAxis::None);
	gizmo.pixels = std::clamp(numberOr(value, "pixels", gizmo.pixels), 10.0f, 400.0f);
	if(const auto pivot = value.find("pivot"); pivot != value.end()) gizmo.pivot = pointFromJson(*pivot);
	return gizmo;
}

/// @brief Reads a box being dragged. @param value The `box` member. @return Its corners, or nothing.
// NOLINTNEXTLINE(bugprone-exception-escape): json throws only on a wrong type, which is checked first
[[nodiscard]] std::optional<std::array<float, 4>> boxFromJson(const json& value) noexcept {
	if(!value.is_array() || value.size() != 4) return std::nullopt;
	std::array<float, 4> box{};
	for(std::size_t i = 0; i < 4; ++i) {
		if(!value[i].is_number()) return std::nullopt;
		box.at(i) = value[i].get<float>();
		if(!std::isfinite(box.at(i))) return std::nullopt;
	}
	return box;
}

/// @brief The selected ids, at most a limit. @param value The `selection` member. @return Them.
[[nodiscard]] std::vector<std::string> selectionFromJson(const json& value) {
	constexpr std::size_t maxSelected = 100000;
	std::vector<std::string> ids;
	if(!value.is_array()) return ids;
	for(const auto& id : value)
		if(id.is_string() && ids.size() < maxSelected) ids.push_back(id.get<std::string>());
	return ids;
}

/// @brief A colour from three finite numbers, clamped to 0 to 1.
/// @param value The array of three numbers. @param fallback When one isn't finite. @return The colour.
// NOLINTNEXTLINE(bugprone-exception-escape): json throws only on a wrong type, which is checked first
[[nodiscard]] Vec3 arrayColor(const json& value, Vec3 fallback) noexcept {
	const Vec3 color{value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
	if(!isFinite(color)) return fallback;
	return {std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f), std::clamp(color.z, 0.0f, 1.0f)};
}

/// @brief A colour from `#rrggbb` (anything after it ignored).
/// @param text The text. @param fallback When it isn't one. @return The colour.
[[nodiscard]] Vec3 hexColor(const std::string& text, Vec3 fallback) noexcept {
	if(text.size() < 7 || text[0] != '#') return fallback;
	Vec3 color;
	for(int channel = 0; channel < 3; ++channel) {
		const std::size_t at = 1 + static_cast<std::size_t>(channel) * 2;
		const int high = hexValue(text[at]);
		const int low = hexValue(text[at + 1]);
		if(high < 0 || low < 0) return fallback;
		color[channel] = static_cast<float>(high * 16 + low) / 255.0f;
	}
	return color;
}

} // namespace

const char* shadingName(Shading shading) noexcept {
	return shading == Shading::Solid ? "solid" : shading == Shading::Wireframe ? "wireframe" : "material";
}

std::optional<Shading> shadingNamed(std::string_view name) noexcept {
	for(const Shading shading : {Shading::Material, Shading::Solid, Shading::Wireframe})
		if(name == shadingName(shading)) return shading;
	return std::nullopt;
}

// NOLINTNEXTLINE(bugprone-exception-escape): json throws only on a wrong type, which is checked first
Vec3 colorFromJson(const json& value, Vec3 fallback) noexcept {
	if(value.is_array() && value.size() == 3 && value[0].is_number() && value[1].is_number() && value[2].is_number())
		return arrayColor(value, fallback);
	if(!value.is_string()) return fallback;
	return hexColor(value.get_ref<const std::string&>(), fallback);
}

// NOLINTNEXTLINE(bugprone-exception-escape): json throws only on a wrong type, which is checked first
ViewportSettings viewportSettingsFromJson(const json& params) noexcept {
	ViewportSettings settings;
	if(!params.is_object()) return settings;
	try {
		if(const auto camera = params.find("camera"); camera != params.end()) settings.camera = cameraFromJson(*camera);
	} catch(const std::exception&) {
		settings.camera = {};
	}
	try {
		if(const auto selection = params.find("selection"); selection != params.end())
			settings.selection = selectionFromJson(*selection);
		if(const auto outlined = params.find("outlined"); outlined != params.end())
			settings.outlined = selectionFromJson(*outlined);
		settings.shading = shadingNamed(textOf(params, "shading")).value_or(Shading::Material);
		if(const auto gizmo = params.find("gizmo"); gizmo != params.end()) settings.gizmo = gizmoFromJson(*gizmo);
	} catch(const std::exception&) {
		settings.selection.clear(); // only an allocation can fail here
		settings.outlined.clear();
	}
	settings.grid = flagOr(params, "grid", true);
	settings.axes = flagOr(params, "axes", true);
	const auto member = [&params](const char* key) { return params.contains(key) ? params[key] : json(); };
	settings.background = colorFromJson(member("background"), settings.background);
	settings.gridColor = colorFromJson(member("gridColor"), settings.gridColor);
	settings.outline = colorFromJson(member("outline"), settings.outline);
	settings.exposure = std::clamp(numberOr(params, "exposure", 1.0f), 0.0f, 64.0f);
	settings.time = std::max(numberOr(params, "time", 0.0f), 0.0f);
	if(const auto box = params.find("box"); box != params.end()) settings.box = boxFromJson(*box);
	settings.zUp = textOf(params, "up") == "z";
	if(const auto cursor = params.find("cursor3d"); cursor != params.end()) settings.cursor = pointFromJson(*cursor);
	return settings;
}

Quat upTurn(bool zUp) noexcept {
	return zUp ? Quat::fromAxisAngle({1.0f, 0.0f, 0.0f}, -radians(90.0f)) : Quat{};
}

json withUpFrame(json scene) {
	if(!scene.is_object() || !scene.contains("nodes") || !scene["nodes"].is_array()) return scene;
	json& nodes = scene["nodes"];
	for(json& node : nodes)
		if(node.is_object() && (!node.contains("parent") || node["parent"] == "")) node["parent"] = upFrameId;
	nodes.push_back({{"id", upFrameId}, {"rotation", toJson(upTurn(true))}});
	return scene;
}

Scene sceneAtTime(const json& scene, const json& animations, float time, const SceneLimits& limits) {
	Scene result = sceneFromJson(scene, limits);
	if(animations.is_array())
		for(const auto& clip : clipsFromJson(animations)) clip.apply(result, time);
	return result;
}

std::optional<Gizmo> gizmoFor(const Scene& scene, const ViewportSettings& settings, float viewportHeight) noexcept {
	if(!settings.gizmo.visible || settings.selection.empty()) return std::nullopt;
	try {
		const auto world = scene.worldMatrices();
		Vec3 sum;
		std::size_t count = 0;
		const Mat4* active = nullptr;
		for(const auto& id : settings.selection) {
			const auto found = world.find(id);
			if(found == world.end()) continue;
			sum = sum + found->second.first.origin();
			active = &found->second.first;
			++count;
		}
		if(count == 0) return std::nullopt;
		Gizmo gizmo;
		gizmo.mode = settings.gizmo.mode;
		const Quat up = upTurn(settings.zUp);
		gizmo.origin = settings.gizmo.pivot ? up.rotate(*settings.gizmo.pivot) : sum / static_cast<float>(count);
		gizmo.orientation = up;
		if(settings.gizmo.local)
			if(const auto parts = Transform::decompose(*active)) gizmo.orientation = normalize(parts->rotation);
		gizmo.size = worldSizeForPixels(settings.camera, gizmo.origin, viewportHeight, settings.gizmo.pixels);
		return gizmo;
	} catch(const std::exception&) {
		return std::nullopt;
	}
}

} // namespace affresco::scene3d
