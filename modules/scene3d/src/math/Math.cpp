/// @file
/// @brief The maths of 3D scenes: vectors, quaternions, matrices and transforms (Math.hpp).

#include "affresco/scene3d/Math.hpp"

#include <algorithm>
#include <cmath>

namespace affresco::scene3d {

Vec3 normalize(Vec3 a) noexcept {
	const float size = length(a);
	return size > 0.0f ? a / size : a;
}

Vec3 minimum(Vec3 a, Vec3 b) noexcept { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }

Vec3 maximum(Vec3 a, Vec3 b) noexcept { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }

bool isFinite(Vec3 a) noexcept { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }

Quat Quat::fromAxisAngle(Vec3 axis, float radians) noexcept {
	const Vec3 unit = normalize(axis);
	const float half = radians * 0.5f;
	const float sine = std::sin(half);
	return {unit.x * sine, unit.y * sine, unit.z * sine, std::cos(half)};
}

Quat Quat::fromEuler(Vec3 radians) noexcept {
	// x first, then y, then z: q = qz * qy * qx
	return normalize(fromAxisAngle({0, 0, 1}, radians.z) * fromAxisAngle({0, 1, 0}, radians.y) *
	                 fromAxisAngle({1, 0, 0}, radians.x));
}

Vec3 Quat::toEuler() const noexcept {
	const Quat q = normalize(*this);
	// The matrix of q = qz * qy * qx: m20 = -sin(y), m21 = cos(y) sin(x), m22 = cos(y) cos(x), m10, m00 for z
	const float m20 = 2.0f * (q.x * q.z - q.w * q.y);
	const float sineY = std::clamp(-m20, -1.0f, 1.0f);
	const float pitch = std::asin(sineY);
	if(std::abs(sineY) > 0.9999f) { // gimbal lock: put the whole turn around z on x
		const float m01 = 2.0f * (q.x * q.y - q.w * q.z);
		const float m11 = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
		return {std::atan2(sineY > 0.0f ? m01 : -m01, m11), pitch, 0.0f};
	}
	const float m21 = 2.0f * (q.y * q.z + q.w * q.x);
	const float m22 = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	const float m10 = 2.0f * (q.x * q.y + q.w * q.z);
	const float m00 = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	return {std::atan2(m21, m22), pitch, std::atan2(m10, m00)};
}

Vec3 Quat::rotate(Vec3 v) const noexcept {
	const Vec3 axis{x, y, z};
	const Vec3 t = cross(axis, v) * 2.0f;
	return v + t * w + cross(axis, t);
}

Quat operator*(Quat a, Quat b) noexcept {
	return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
	        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

Quat normalize(Quat q) noexcept {
	const float size = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	if(!(size > 0.0f) || !std::isfinite(size)) return {};
	return {q.x / size, q.y / size, q.z / size, q.w / size};
}

Quat slerp(Quat a, Quat b, float t) noexcept {
	float cosine = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	if(cosine < 0.0f) { // the shorter arc
		b = {-b.x, -b.y, -b.z, -b.w};
		cosine = -cosine;
	}
	if(cosine > 0.9995f) // nearly the same: interpolate linearly
		return normalize({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t});
	const float angle = std::acos(cosine);
	const float sine = std::sin(angle);
	const float wa = std::sin((1.0f - t) * angle) / sine;
	const float wb = std::sin(t * angle) / sine;
	return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
}

Mat4 Mat4::translation(Vec3 offset) noexcept {
	Mat4 m;
	m.at(0, 3) = offset.x;
	m.at(1, 3) = offset.y;
	m.at(2, 3) = offset.z;
	return m;
}

Mat4 Mat4::scaling(Vec3 factors) noexcept {
	Mat4 m;
	m.at(0, 0) = factors.x;
	m.at(1, 1) = factors.y;
	m.at(2, 2) = factors.z;
	return m;
}

Mat4 Mat4::rotation(Quat q) noexcept {
	const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
	const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
	const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
	Mat4 m;
	m.at(0, 0) = 1.0f - 2.0f * (yy + zz);
	m.at(0, 1) = 2.0f * (xy - wz);
	m.at(0, 2) = 2.0f * (xz + wy);
	m.at(1, 0) = 2.0f * (xy + wz);
	m.at(1, 1) = 1.0f - 2.0f * (xx + zz);
	m.at(1, 2) = 2.0f * (yz - wx);
	m.at(2, 0) = 2.0f * (xz - wy);
	m.at(2, 1) = 2.0f * (yz + wx);
	m.at(2, 2) = 1.0f - 2.0f * (xx + yy);
	return m;
}

Mat4 Mat4::perspective(float fieldOfViewY, float aspect, float nearPlane, float farPlane) noexcept {
	const float focal = 1.0f / std::tan(fieldOfViewY * 0.5f);
	Mat4 m;
	m.at(0, 0) = focal / aspect;
	m.at(1, 1) = focal;
	m.at(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
	m.at(2, 3) = 2.0f * farPlane * nearPlane / (nearPlane - farPlane);
	m.at(3, 2) = -1.0f;
	m.at(3, 3) = 0.0f;
	return m;
}

Mat4 Mat4::orthographic(float halfWidth, float halfHeight, float nearPlane, float farPlane) noexcept {
	Mat4 m;
	m.at(0, 0) = 1.0f / halfWidth;
	m.at(1, 1) = 1.0f / halfHeight;
	m.at(2, 2) = -2.0f / (farPlane - nearPlane);
	m.at(2, 3) = -(farPlane + nearPlane) / (farPlane - nearPlane);
	return m;
}

Mat4 Mat4::lookAt(Vec3 eye, Vec3 target, Vec3 up) noexcept {
	const Vec3 forward = normalize(target - eye);
	Vec3 side = cross(forward, up);
	if(length(side) < 1e-6f) side = cross(forward, std::abs(forward.y) < 0.9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0});
	side = normalize(side);
	const Vec3 trueUp = cross(side, forward);
	Mat4 m;
	for(int column = 0; column < 3; ++column) {
		m.at(0, column) = side[column];
		m.at(1, column) = trueUp[column];
		m.at(2, column) = -forward[column];
	}
	m.at(0, 3) = -dot(side, eye);
	m.at(1, 3) = -dot(trueUp, eye);
	m.at(2, 3) = dot(forward, eye);
	return m;
}

Vec4 Mat4::transform(Vec4 p) const noexcept {
	Vec4 out;
	out.x = at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z + at(0, 3) * p.w;
	out.y = at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z + at(1, 3) * p.w;
	out.z = at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z + at(2, 3) * p.w;
	out.w = at(3, 0) * p.x + at(3, 1) * p.y + at(3, 2) * p.z + at(3, 3) * p.w;
	return out;
}

Vec3 Mat4::transformPoint(Vec3 point) const noexcept {
	const Vec4 p = transform({point.x, point.y, point.z, 1.0f});
	const float w = p.w != 0.0f ? p.w : 1.0f;
	return {p.x / w, p.y / w, p.z / w};
}

Vec3 Mat4::transformDirection(Vec3 direction) const noexcept {
	const Vec4 p = transform({direction.x, direction.y, direction.z, 0.0f});
	return {p.x, p.y, p.z};
}

Mat4 Mat4::transposed() const noexcept {
	Mat4 m;
	for(int row = 0; row < 4; ++row)
		for(int column = 0; column < 4; ++column) m.at(row, column) = at(column, row);
	return m;
}

std::optional<Mat4> Mat4::inverse() const noexcept {
	// Cofactors (the classic expansion, as in MESA's gluInvertMatrix), on the column-major array
	const auto& a = values;
	std::array<float, 16> inv{};
	inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] +
	         a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
	inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] -
	         a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
	inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] +
	         a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
	inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] -
	          a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
	inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] -
	         a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
	inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] +
	         a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
	inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] -
	         a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
	inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] +
	          a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
	inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] +
	         a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
	inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] -
	         a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
	inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] +
	          a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
	inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] -
	          a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
	inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] -
	         a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
	inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] +
	         a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
	inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] -
	          a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
	inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] -
	          a[8] * a[2] * a[5];
	const float determinant = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
	if(std::abs(determinant) < 1e-12f || !std::isfinite(determinant)) return std::nullopt;
	Mat4 result;
	for(std::size_t i = 0; i < 16; ++i) result.values[i] = inv[i] / determinant;
	return result;
}

Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
	Mat4 m;
	for(int column = 0; column < 4; ++column)
		for(int row = 0; row < 4; ++row) {
			float sum = 0.0f;
			for(int k = 0; k < 4; ++k) sum += a.at(row, k) * b.at(k, column);
			m.at(row, column) = sum;
		}
	return m;
}

Mat4 Transform::matrix() const noexcept {
	Mat4 m = Mat4::rotation(rotation);
	for(int row = 0; row < 3; ++row) {
		m.at(row, 0) *= scale.x;
		m.at(row, 1) *= scale.y;
		m.at(row, 2) *= scale.z;
	}
	m.at(0, 3) = translation.x;
	m.at(1, 3) = translation.y;
	m.at(2, 3) = translation.z;
	return m;
}

std::optional<Transform> Transform::decompose(const Mat4& matrix) noexcept {
	if(matrix.at(3, 0) != 0.0f || matrix.at(3, 1) != 0.0f || matrix.at(3, 2) != 0.0f || matrix.at(3, 3) != 1.0f)
		return std::nullopt;
	Transform result;
	result.translation = matrix.origin();
	std::array<Vec3, 3> axes{};
	for(int column = 0; column < 3; ++column) {
		axes[static_cast<std::size_t>(column)] = {matrix.at(0, column), matrix.at(1, column), matrix.at(2, column)};
		result.scale[column] = length(axes[static_cast<std::size_t>(column)]);
		if(!(result.scale[column] > 1e-12f)) return std::nullopt;
		axes[static_cast<std::size_t>(column)] = axes[static_cast<std::size_t>(column)] / result.scale[column];
	}
	if(dot(cross(axes[0], axes[1]), axes[2]) < 0.0f) { // a mirror: put it on x
		result.scale.x = -result.scale.x;
		axes[0] = -axes[0];
	}
	// The rotation matrix's quaternion (Shepperd's method)
	const float m00 = axes[0].x, m11 = axes[1].y, m22 = axes[2].z;
	const float trace = m00 + m11 + m22;
	Quat q;
	if(trace > 0.0f) {
		const float s = std::sqrt(trace + 1.0f) * 2.0f;
		q = {(axes[1].z - axes[2].y) / s, (axes[2].x - axes[0].z) / s, (axes[0].y - axes[1].x) / s, 0.25f * s};
	} else if(m00 > m11 && m00 > m22) {
		const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
		q = {0.25f * s, (axes[1].x + axes[0].y) / s, (axes[2].x + axes[0].z) / s, (axes[1].z - axes[2].y) / s};
	} else if(m11 > m22) {
		const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
		q = {(axes[1].x + axes[0].y) / s, 0.25f * s, (axes[2].y + axes[1].z) / s, (axes[2].x - axes[0].z) / s};
	} else {
		const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
		q = {(axes[2].x + axes[0].z) / s, (axes[2].y + axes[1].z) / s, 0.25f * s, (axes[0].y - axes[1].x) / s};
	}
	result.rotation = normalize(q);
	return result;
}

} // namespace affresco::scene3d
