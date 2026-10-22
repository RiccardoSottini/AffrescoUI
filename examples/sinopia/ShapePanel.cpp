/// @file
/// @brief The "sinopiaShape" type: one of Sinopia's shapes, filled with a colour or a banded gradient, stroked and
/// shadowed along its outline.

#include "ShapePanel.hpp"

#include "ShapeGeometry.hpp"

#include <affresco/Screen.hpp>
#include <affresco/core/Json.hpp>
#include <affresco/render/Painter.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace sinopia {

using affresco::Color;
using affresco::json;
using affresco::MeshVertex;
using affresco::Vec2;
using affresco::VectorPath;

namespace {

constexpr int gradientBands = 32; ///< How many bands of colour draw a gradient.

/// @brief A number at a key, or a default.
/// @param value The object.
/// @param key The key.
/// @param fallback The default.
/// @return It.
[[nodiscard]] float numberAt(const json& value, const char* key, float fallback) {
	const auto found = value.find(key);
	return found != value.end() && found->is_number() ? found->get<float>() : fallback;
}

/// @brief A colour at a key, resolved, or transparent.
/// @param screen Resolves it.
/// @param value The object.
/// @param key The key.
/// @param fallback The default when missing.
/// @return It.
[[nodiscard]] Color colorAt(const affresco::Screen& screen, const json& value, const char* key, const char* fallback) {
	const auto found = value.find(key);
	const std::string text = found != value.end() && found->is_string() ? found->get<std::string>() : fallback;
	return text.empty() ? affresco::colors::transparent : screen.resolveColor(text);
}

} // namespace

void ShapePanel::apply(const affresco::Screen& screen, const std::string& properties) {
	const json value = json::parse(properties);
	kind_ = value.value("shape", std::string("rect"));
	sides_ = std::clamp(static_cast<int>(numberAt(value, "sides", 5.0f)), 3, 64);
	inner_ = std::clamp(numberAt(value, "inner", 0.5f), 0.05f, 0.95f);
	cornerRadius_ = std::max(0.0f, numberAt(value, "cornerRadius", 0.0f));
	gradient_.reset();
	if(const auto found = value.find("gradient"); found != value.end() && found->is_object())
		gradient_ = ShapeGradient{colorAt(screen, *found, "from", ""), colorAt(screen, *found, "to", ""),
		                          numberAt(*found, "angle", 90.0f)};
	shadow_.reset();
	if(const auto found = value.find("shadow"); found != value.end() && found->is_object())
		shadow_ = ShapeShadow{{numberAt(*found, "x", 0.0f), numberAt(*found, "y", 4.0f)},
		                      std::clamp(numberAt(*found, "blur", 8.0f), 0.0f, 100.0f),
		                      colorAt(screen, *found, "color", "#00000040")};
	builtFor_.reset();
	invalidatePaint();
}

std::vector<Vec2> ShapePanel::clipToHalfPlane(const std::vector<Vec2>& points, Vec2 normal, float at) {
	std::vector<Vec2> kept;
	const auto side = [normal, at](Vec2 point) { return point.x * normal.x + point.y * normal.y - at; };
	for(std::size_t i = 0; i < points.size(); ++i) {
		const Vec2 from = points[i];
		const Vec2 to = points[(i + 1) % points.size()];
		const float fromSide = side(from);
		const float toSide = side(to);
		if(fromSide >= 0.0f) kept.push_back(from);
		if((fromSide >= 0.0f) != (toSide >= 0.0f)) {
			const float t = fromSide / (fromSide - toSide);
			kept.push_back({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t});
		}
	}
	if(kept.size() < 3) kept.clear();
	return kept;
}

std::vector<VectorPath::Contour> ShapePanel::contours(Vec2 size) const {
	if(kind_ != "path")
		return VectorPath::parse(shapePath({kind_, size.x, size.y, cornerRadius_, sides_, inner_}))
		    .flatten({{1.0f, 1.0f}, {0.0f, 0.0f}});
	// A pen path's view box is stretched over the frame
	const std::optional<affresco::Rect>& box = viewBox();
	if(!box || !(box->width() > 0.0f) || !(box->height() > 0.0f)) return path().flatten({});
	const Vec2 scale{size.x / box->width(), size.y / box->height()};
	return path().flatten({scale, {-box->left * scale.x, -box->top * scale.y}});
}

void ShapePanel::buildGradient(const std::vector<VectorPath::Contour>& outline, Vec2 size) const {
	const float radians = gradient_->angle * std::numbers::pi_v<float> / 180.0f;
	const Vec2 normal{std::cos(radians), std::sin(radians)};
	// The gradient spans the frame's corners along its direction
	float low = 0.0f;
	float high = 0.0f;
	for(const Vec2 corner : {Vec2{size.x, 0.0f}, Vec2{0.0f, size.y}, size}) {
		low = std::min(low, corner.x * normal.x + corner.y * normal.y);
		high = std::max(high, corner.x * normal.x + corner.y * normal.y);
	}
	for(int band = 0; band < gradientBands; ++band) {
		const float t = (static_cast<float>(band) + 0.5f) / static_cast<float>(gradientBands);
		std::vector<VectorPath::Contour> clipped;
		const float at = low + (high - low) * static_cast<float>(band) / static_cast<float>(gradientBands);
		for(const VectorPath::Contour& contour : outline)
			if(band == 0) clipped.push_back(contour);
			else if(std::vector<Vec2> kept = clipToHalfPlane(contour.points, normal, at); !kept.empty())
				clipped.push_back({std::move(kept), true});
		if(clipped.empty()) break;
		std::vector<MeshVertex> mesh;
		appendFill(clipped, fillRule(), mesh);
		fills_.emplace_back(Color::mix(gradient_->from, gradient_->to, t), std::move(mesh));
	}
}

void ShapePanel::build(Vec2 size) const {
	if(builtFor_ == size && builtFill_ == fillColor() && builtStroke_ == strokeColor() &&
	   builtStyle_ == strokeStyle() && builtPath_ == path())
		return;
	builtFor_ = size;
	builtFill_ = fillColor();
	builtStroke_ = strokeColor();
	builtStyle_ = strokeStyle();
	builtPath_ = path();
	shadowMesh_.clear();
	fills_.clear();
	strokeMesh_.clear();
	const std::vector<VectorPath::Contour> outline = contours(size);
	const bool open = isOpenShape(kind_);
	if(shadow_ && shadow_->color.a > 0.0f && !open) appendFill(outline, fillRule(), shadowMesh_, 1.0f + shadow_->blur);
	if(gradient_ && !open) buildGradient(outline, size);
	else if(fillColor().a > 0.0f && !open) {
		fills_.emplace_back(fillColor(), std::vector<MeshVertex>{});
		appendFill(outline, fillRule(), fills_.back().second);
	}
	if(strokeColor().a > 0.0f && strokeStyle().width > 0.0f) appendStroke(outline, strokeStyle(), strokeMesh_);
}

void ShapePanel::paint(affresco::Painter& painter) const {
	Panel::paint(painter); // NOLINT(bugprone-parent-virtual-call): the path view's own meshes aren't drawn
	const affresco::Rect& box = frame();
	build({box.width(), box.height()});
	const Vec2 corner{box.left, box.top};
	if(!shadowMesh_.empty())
		painter.fillMesh(shadowMesh_, {corner.x + shadow_->offset.x, corner.y + shadow_->offset.y}, shadow_->color);
	for(const auto& [color, mesh] : fills_)
		if(!mesh.empty()) painter.fillMesh(mesh, corner, color);
	if(!strokeMesh_.empty()) painter.fillMesh(strokeMesh_, corner, strokeColor());
}

void registerShape(affresco::Screen& screen) {
	screen.registerType(
	    "sinopiaShape", [] { return std::make_unique<ShapePanel>(); },
	    {"shape", "sides", "inner", "cornerRadius", "gradient", "shadow"},
	    [&screen](affresco::Panel& panel, const std::string& properties) {
		    dynamic_cast<ShapePanel&>(panel).apply(screen, properties); // the factory made it
	    });
}

} // namespace sinopia
