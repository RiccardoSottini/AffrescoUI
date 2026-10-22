#pragma once

/// @file
/// @brief The renderer's shaders: fixed source, in the common subset of GLSL 4.10 and GLSL ES 3.00, each placed
/// after the surface's prelude. A material graph's function (compileMaterialGraph()) goes between pbrHead and pbrMain,
/// after `#define GRAPH`; nothing else is ever inserted.

namespace affresco::scene3d::detail::shaders {

/// @brief A mesh vertex in world space, for the PBR, solid and id programs.
constexpr const char* meshVertex = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
uniform mat4 model;
uniform mat4 normalMatrix;
uniform mat4 viewProjection;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv;
void main() {
	vec4 world = model * vec4(position, 1.0);
	worldPosition = world.xyz;
	worldNormal = mat3(normalMatrix) * normal;
	uv = texCoord;
	gl_Position = viewProjection * world;
}
)";

/// @brief A position alone: the shadow map, the outline mask and the wireframe.
constexpr const char* positionVertex = R"(
layout(location = 0) in vec3 position;
uniform mat4 model;
uniform mat4 viewProjection;
void main() { gl_Position = viewProjection * model * vec4(position, 1.0); }
)";

/// @brief One colour, as given (premultiplied by the caller): the id pass, the mask and the wireframe.
constexpr const char* flatFragment = R"(
precision highp float;
uniform vec4 flatColor;
layout(location = 0) out vec4 color;
void main() { color = flatColor; }
)";

/// @brief Nothing: the shadow map only needs depth.
constexpr const char* depthFragment = R"(
precision highp float;
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0); }
)";

/// @brief The PBR fragment's inputs, uniforms and helpers.
constexpr const char* pbrHead = R"(
precision highp float;
in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv;
uniform vec4 baseColorFactor;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3 emissiveFactor;
uniform int alphaMode;
uniform float alphaCutoff;
uniform int maps;
uniform sampler2D baseColorMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D normalMap;
uniform sampler2D emissiveMap;
uniform sampler2D shadowMap;
uniform vec3 eye;
uniform int lightCount;
uniform vec4 lightPosition[8];
uniform vec4 lightDirection[8];
uniform vec3 lightColor[8];
uniform vec2 lightCone[8];
uniform int shadowLight;
uniform mat4 shadowMatrix;
uniform float exposure;
uniform int solid;
layout(location = 0) out vec4 color;
const float PI = 3.14159265;

vec3 perturb(vec3 n, vec3 p, vec2 tc, vec3 sampled) {
	vec3 dp1 = dFdx(p);
	vec3 dp2 = dFdy(p);
	vec2 duv1 = dFdx(tc);
	vec2 duv2 = dFdy(tc);
	vec3 dp2perp = cross(dp2, n);
	vec3 dp1perp = cross(n, dp1);
	vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
	float scale = inversesqrt(max(max(dot(t, t), dot(b, b)), 1e-12));
	return normalize(mat3(t * scale, b * scale, n) * (sampled * 2.0 - 1.0));
}

float shadowAt(vec3 p, vec3 n, vec3 l) {
	vec4 s = shadowMatrix * vec4(p, 1.0);
	vec3 q = s.xyz / s.w * 0.5 + 0.5;
	if(q.x < 0.0 || q.x > 1.0 || q.y < 0.0 || q.y > 1.0 || q.z > 1.0) return 1.0;
	float bias = max(0.0025 * (1.0 - dot(n, l)), 0.0006);
	vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
	float lit = 0.0;
	for(int x = -1; x <= 1; ++x)
		for(int y = -1; y <= 1; ++y)
			lit += q.z - bias <= texture(shadowMap, q.xy + vec2(float(x), float(y)) * texel).r ? 1.0 : 0.0;
	return lit / 9.0;
}

vec3 brdf(vec3 n, vec3 v, vec3 l, vec3 base, float metallic, float roughness) {
	vec3 h = normalize(l + v);
	float nl = max(dot(n, l), 0.0);
	float nv = max(dot(n, v), 1e-4);
	float nh = max(dot(n, h), 0.0);
	float a = roughness * roughness;
	float a2 = a * a;
	float d = a2 / (PI * pow(nh * nh * (a2 - 1.0) + 1.0, 2.0));
	float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
	float g = nl / (nl * (1.0 - k) + k) * nv / (nv * (1.0 - k) + k);
	vec3 f0 = mix(vec3(0.04), base, metallic);
	vec3 f = f0 + (1.0 - f0) * pow(1.0 - max(dot(h, v), 0.0), 5.0);
	vec3 specular = d * g * f / max(4.0 * nl * nv, 1e-4);
	vec3 diffuse = (1.0 - f) * (1.0 - metallic) * base / PI;
	return (diffuse + specular) * nl;
}

vec3 radiance(int i, vec3 p, out vec3 l) {
	float type = lightPosition[i].w;
	if(type < 0.5) {
		l = -lightDirection[i].xyz;
		return lightColor[i];
	}
	vec3 toLight = lightPosition[i].xyz - p;
	float distance2 = max(dot(toLight, toLight), 1e-4);
	l = toLight * inversesqrt(distance2);
	float attenuation = 1.0 / distance2;
	float range = lightDirection[i].w;
	if(range > 0.0) {
		float ratio = sqrt(distance2) / range;
		attenuation *= clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);
	}
	if(type > 1.5) {
		float cd = dot(lightDirection[i].xyz, -l);
		attenuation *= smoothstep(lightCone[i].x, lightCone[i].y, cd);
	}
	return lightColor[i] * attenuation;
}

vec3 aces(vec3 x) {
	return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 encodeSrgb(vec3 c) {
	vec3 low = c * 12.92;
	vec3 high = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
	return mix(low, high, step(vec3(0.0031308), c));
}
)";

/// @brief The PBR fragment's main: the material, the lights, then exposure, tone mapping and sRGB, premultiplied.
constexpr const char* pbrMain = R"(
void main() {
	vec4 base = baseColorFactor;
	if((maps & 1) != 0) base *= texture(baseColorMap, uv);
	if(alphaMode == 1 && base.a < alphaCutoff) discard;
	float metallic = metallicFactor;
	float roughness = roughnessFactor;
	if((maps & 2) != 0) {
		vec4 mr = texture(metallicRoughnessMap, uv);
		roughness *= mr.g;
		metallic *= mr.b;
	}
	vec3 emissive = emissiveFactor;
	if((maps & 8) != 0) emissive *= texture(emissiveMap, uv).rgb;
	vec3 n = normalize(worldNormal);
	if(!gl_FrontFacing) n = -n;
	if((maps & 4) != 0) n = perturb(n, worldPosition, uv, texture(normalMap, uv).rgb);
	vec3 v = normalize(eye - worldPosition);
	vec3 albedo = base.rgb;
#ifdef GRAPH
	graphMaterial(worldPosition, n, v, albedo, metallic, roughness, emissive);
#endif
	metallic = clamp(metallic, 0.0, 1.0);
	roughness = clamp(roughness, 0.04, 1.0);
	vec3 lit;
	if(solid != 0) {
		vec3 key = normalize(vec3(0.4, 0.8, 0.45));
		vec3 fill = normalize(vec3(-0.6, 0.2, -0.3));
		lit = albedo * (0.25 + 0.75 * max(dot(n, key), 0.0) + 0.25 * max(dot(n, fill), 0.0));
	} else {
		lit = albedo * mix(vec3(0.05, 0.045, 0.04), vec3(0.11, 0.12, 0.14), n.y * 0.5 + 0.5) + emissive;
		for(int i = 0; i < 8; ++i) {
			if(i >= lightCount) break;
			vec3 l;
			vec3 light = radiance(i, worldPosition, l);
			if(i == shadowLight) light *= shadowAt(worldPosition, n, l);
			lit += brdf(n, v, l, albedo, metallic, roughness) * light;
		}
	}
	float alpha = alphaMode == 2 ? base.a : 1.0;
	color = vec4(encodeSrgb(aces(lit * exposure)) * alpha, alpha);
}
)";

/// @brief A triangle over the whole viewport from gl_VertexID; ndc is -1..1.
constexpr const char* fullscreenVertex = R"(
out vec2 ndc;
void main() {
	vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
	ndc = corner * 2.0 - 1.0;
	gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

/// @brief The floor grid (y = 0) by casting a ray per pixel: minor and major lines, the x and z axes, fading away.
constexpr const char* gridFragment = R"(
precision highp float;
in vec2 ndc;
uniform mat4 inverseViewProjection;
uniform mat4 viewProjection;
uniform vec3 gridColor;
uniform float cell;
uniform float fade;
uniform int showGrid;
uniform int showAxes;
layout(location = 0) out vec4 color;
float lineAt(vec2 coordinate, float size) {
	vec2 g = coordinate / size;
	vec2 width = fwidth(g);
	vec2 d = abs(fract(g - 0.5) - 0.5) / max(width, vec2(1e-6));
	return 1.0 - min(min(d.x, d.y), 1.0);
}
void main() {
	vec4 nearPoint = inverseViewProjection * vec4(ndc, -1.0, 1.0);
	vec4 farPoint = inverseViewProjection * vec4(ndc, 1.0, 1.0);
	vec3 a = nearPoint.xyz / nearPoint.w;
	vec3 b = farPoint.xyz / farPoint.w;
	float t = -a.y / (b.y - a.y);
	if(!(t > 0.0) || b.y == a.y) discard;
	vec3 p = a + (b - a) * t;
	vec4 clip = viewProjection * vec4(p, 1.0);
	float depth = clip.z / clip.w;
	if(depth > 1.0) discard;
	gl_FragDepth = depth * 0.5 + 0.5;
	float distanceFade = clamp(1.0 - length(p.xz - a.xz) / fade, 0.0, 1.0);
	vec4 c = vec4(0.0);
	if(showGrid != 0) {
		float minor = lineAt(p.xz, cell) * 0.35;
		float major = lineAt(p.xz, cell * 10.0) * 0.75;
		c = vec4(gridColor, max(minor, major));
	}
	if(showAxes != 0) {
		vec2 width = fwidth(p.xz) * 1.5;
		float xAxis = 1.0 - min(abs(p.z) / max(width.y, 1e-6), 1.0);
		float zAxis = 1.0 - min(abs(p.x) / max(width.x, 1e-6), 1.0);
		if(xAxis > 0.0) c = vec4(mix(c.rgb, vec3(0.86, 0.3, 0.32), xAxis), max(c.a, xAxis));
		if(zAxis > 0.0) c = vec4(mix(c.rgb, vec3(0.3, 0.5, 0.92), zAxis), max(c.a, zAxis));
	}
	c.a *= distanceFade;
	if(c.a < 0.004) discard;
	color = vec4(c.rgb * c.a, c.a);
}
)";

/// @brief The selection outline: where the mask is empty but a neighbour isn't; brighter around the active node.
constexpr const char* outlineFragment = R"(
precision highp float;
in vec2 ndc;
uniform sampler2D mask;
uniform vec3 outlineColor;
uniform float radius;
layout(location = 0) out vec4 color;
void main() {
	vec2 texel = 1.0 / vec2(textureSize(mask, 0));
	vec2 at = ndc * 0.5 + 0.5;
	vec4 here = texture(mask, at);
	float selected = 0.0;
	float activeNear = 0.0;
	for(int x = -2; x <= 2; ++x)
		for(int y = -2; y <= 2; ++y) {
			vec2 offset = vec2(float(x), float(y));
			if(length(offset) > radius + 0.01) continue;
			vec4 m = texture(mask, at + offset * texel);
			selected = max(selected, m.r);
			activeNear = max(activeNear, m.g);
		}
	float edge = max(selected - here.r, 0.0);
	if(edge <= 0.0) discard;
	vec3 c = activeNear > 0.5 && here.g < 0.5 ? mix(outlineColor, vec3(1.0), 0.45) : outlineColor;
	color = vec4(c, 1.0);
}
)";

/// @brief Coloured lines: the gizmo, light and camera markers, the box being dragged.
constexpr const char* lineVertex = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 lineColor;
uniform mat4 viewProjection;
out vec4 vertexColor;
void main() {
	vertexColor = lineColor;
	gl_Position = viewProjection * vec4(position, 1.0);
}
)";

/// @brief A line's colour, premultiplied.
constexpr const char* lineFragment = R"(
precision highp float;
in vec4 vertexColor;
layout(location = 0) out vec4 color;
void main() { color = vec4(vertexColor.rgb * vertexColor.a, vertexColor.a); }
)";

} // namespace affresco::scene3d::detail::shaders
