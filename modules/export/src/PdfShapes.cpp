/// @file
/// @brief detail functions that write numbers, strings and paths in PDF syntax.

#include "PdfShapes.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace affresco::detail {

namespace {

/// How far a cubic Bézier's control points are from a quarter circle's ends, per radius.
constexpr float kappa = 0.5522847f;

/// @brief Appends a point and an operator.
/// @param out Where it goes.
/// @param point The point.
/// @param op The operator ("m", "l").
void appendPoint(std::string& out, Vec2 point, std::string_view op) {
	appendNumber(out, point.x);
	appendNumber(out, point.y);
	out += op;
	out += '\n';
}

/// @brief Appends a quarter circle from the current point.
/// @param out Where it goes.
/// @param from Where it starts (the current point).
/// @param to Where it ends.
/// @param corner The corner of the rectangle it rounds.
void appendCorner(std::string& out, Vec2 from, Vec2 to, Vec2 corner) {
	appendNumber(out, from.x + (corner.x - from.x) * kappa);
	appendNumber(out, from.y + (corner.y - from.y) * kappa);
	appendNumber(out, to.x + (corner.x - to.x) * kappa);
	appendNumber(out, to.y + (corner.y - to.y) * kappa);
	appendPoint(out, to, "c");
}

/// @brief A corner of a quad cut at coverage one half: the part where it is at least one half.
/// @param quad The quad's 4 vertices.
/// @param polygon Where its points go.
/// @return How many points it has (0 if none is covered).
std::size_t clipToHalf(std::span<const MeshVertex> quad, std::array<Vec2, 8>& polygon) noexcept {
	std::size_t count = 0;
	for(std::size_t i = 0; i < 4; ++i) {
		const MeshVertex& p = quad[i];
		const MeshVertex& q = quad[(i + 1) % 4];
		const bool pIn = p.coverage >= 0.5f;
		const bool qIn = q.coverage >= 0.5f;
		if(pIn) polygon[count++] = p.position;
		if(pIn != qIn) {
			const float t = (0.5f - p.coverage) / (q.coverage - p.coverage);
			polygon[count++] = {p.position.x + (q.position.x - p.position.x) * t,
			                    p.position.y + (q.position.y - p.position.y) * t};
		}
	}
	return count;
}

/// @brief Twice the signed area of a polygon.
/// @param points Its points.
/// @return Positive when they go one way, negative the other.
float signedArea(std::span<const Vec2> points) noexcept {
	float area = 0.0f;
	for(std::size_t i = 0; i < points.size(); ++i) {
		const Vec2 p = points[i];
		const Vec2 q = points[(i + 1) % points.size()];
		area += p.x * q.y - q.x * p.y;
	}
	return area;
}

/// @brief Appends a code point as UTF-16BE hex.
/// @param out Where it goes.
/// @param codepoint The code point.
void appendUtf16(std::string& out, std::uint32_t codepoint) {
	char unit[5];
	if(codepoint >= 0x10000) {
		codepoint -= 0x10000;
		std::snprintf(unit, sizeof unit, "%04X", static_cast<unsigned>(0xD800 + (codepoint >> 10)));
		out += unit;
		codepoint = 0xDC00 + (codepoint & 0x3FF);
	}
	std::snprintf(unit, sizeof unit, "%04X", static_cast<unsigned>(codepoint));
	out += unit;
}

/// @brief How many continuation bytes follow a UTF-8 lead byte.
/// @param lead The byte, 0x80 or more.
/// @return 1 to 3, or 0 for a continuation byte or a byte past F4.
std::size_t continuationBytes(unsigned char lead) noexcept {
	if(lead > 0xF4) return 0;
	return lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
}

/// @brief Decodes one UTF-8 code point.
/// @param text The text.
/// @param at Where it starts, before the end; moved past it.
/// @return It, or U+FFFD if the bytes aren't valid UTF-8.
std::uint32_t decodeUtf8(std::string_view text, std::size_t& at) noexcept {
	const auto lead = static_cast<unsigned char>(text[at++]);
	if(lead < 0x80) return lead;
	const std::size_t extra = continuationBytes(lead);
	if(extra == 0 || at + extra > text.size()) return 0xFFFD;
	std::uint32_t codepoint = lead & (0x3Fu >> extra);
	for(std::size_t i = 0; i < extra; ++i) {
		const auto next = static_cast<unsigned char>(text[at]);
		if((next & 0xC0) != 0x80) return 0xFFFD;
		codepoint = (codepoint << 6) | (next & 0x3Fu);
		++at;
	}
	const bool surrogate = codepoint >= 0xD800 && codepoint < 0xE000;
	return surrogate || codepoint > 0x10FFFF ? 0xFFFD : codepoint;
}

} // namespace

void appendNumber(std::string& out, double value) {
	const double rounded = std::isfinite(value) ? std::round(value * 1000.0) / 1000.0 : 0.0;
	char text[48];
	const auto result =
	    std::to_chars(text, text + sizeof text, rounded == 0.0 ? 0.0 : rounded, std::chars_format::fixed, 3);
	std::string_view number(text, static_cast<std::size_t>(result.ptr - text));
	while(number.back() == '0') number.remove_suffix(1);
	if(number.back() == '.') number.remove_suffix(1);
	out += number;
	out += ' ';
}

void appendMatrix(std::string& out, const Affine2& transform) {
	for(const float value : {transform.a, transform.b, transform.c, transform.d, transform.tx, transform.ty})
		appendNumber(out, value);
}

void appendRect(std::string& out, const Rect& rect) {
	appendNumber(out, rect.left);
	appendNumber(out, rect.top);
	appendNumber(out, rect.width());
	appendNumber(out, rect.height());
	out += "re\n";
}

void appendRoundedRect(std::string& out, const Rect& rect, const CornerRadii& radii) {
	if(radii.isSquare()) return appendRect(out, rect);
	const float most = std::max(0.0f, std::min(rect.width(), rect.height()) / 2.0f);
	const auto clamp = [most](float radius) { return std::clamp(radius, 0.0f, most); };
	const float tl = clamp(radii.topLeft);
	const float tr = clamp(radii.topRight);
	const float br = clamp(radii.bottomRight);
	const float bl = clamp(radii.bottomLeft);
	appendPoint(out, {rect.left + tl, rect.top}, "m");
	appendPoint(out, {rect.right - tr, rect.top}, "l");
	appendCorner(out, {rect.right - tr, rect.top}, {rect.right, rect.top + tr}, {rect.right, rect.top});
	appendPoint(out, {rect.right, rect.bottom - br}, "l");
	appendCorner(out, {rect.right, rect.bottom - br}, {rect.right - br, rect.bottom}, {rect.right, rect.bottom});
	appendPoint(out, {rect.left + bl, rect.bottom}, "l");
	appendCorner(out, {rect.left + bl, rect.bottom}, {rect.left, rect.bottom - bl}, {rect.left, rect.bottom});
	appendPoint(out, {rect.left, rect.top + tl}, "l");
	appendCorner(out, {rect.left, rect.top + tl}, {rect.left + tl, rect.top}, {rect.left, rect.top});
	out += "h\n";
}

bool appendMeshOutline(std::string& out, std::span<const MeshVertex> quads, Vec2 offset) {
	bool any = false;
	std::array<Vec2, 8> polygon{};
	for(std::size_t first = 0; first + 4 <= quads.size(); first += 4) {
		const std::size_t count = clipToHalf(quads.subspan(first, 4), polygon);
		const std::span<const Vec2> points(polygon.data(), count);
		const float area = count >= 3 ? signedArea(points) : 0.0f;
		if(std::abs(area) < 1e-6f) continue;
		for(std::size_t i = 0; i < count; ++i) {
			const Vec2 point = points[area > 0.0f ? i : count - 1 - i]; // all one way: overlaps add up, never cancel
			appendPoint(out, {point.x + offset.x, point.y + offset.y}, i == 0 ? "m" : "l");
		}
		out += "h\n";
		any = true;
	}
	return any;
}

std::string textString(std::string_view utf8) {
	std::string out = "<FEFF";
	for(std::size_t at = 0; at < utf8.size();) appendUtf16(out, decodeUtf8(utf8, at));
	out += '>';
	return out;
}

std::string utf16Hex(std::uint32_t codepoint) {
	std::string out = "<";
	appendUtf16(out, codepoint);
	return out + '>';
}

CornerRadii insetRadii(const CornerRadii& radii, float by) noexcept {
	const auto inset = [by](float radius) { return std::max(0.0f, radius - by); };
	return {inset(radii.topLeft), inset(radii.topRight), inset(radii.bottomRight), inset(radii.bottomLeft)};
}

} // namespace affresco::detail
