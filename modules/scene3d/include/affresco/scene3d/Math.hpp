#pragma once

/// @file
/// @brief The maths of 3D scenes: vectors, quaternions, 4 by 4 matrices and translation-rotation-scale transforms.
///
/// Right-handed, y up, the camera looking down -z (as glTF and OpenGL). Matrices are column-major, as OpenGL reads
/// them, and multiply column vectors: `projection * view * model * point`.

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace affresco::scene3d {

/// @brief A point or direction in 3D.
struct Vec3 {
	float x = 0.0f; ///< Right.
	float y = 0.0f; ///< Up.
	float z = 0.0f; ///< Towards the viewer.

	/// @brief Equal when every component is equal.
	constexpr bool operator==(const Vec3&) const noexcept = default;
	/// @brief The component at an axis.
	/// @param axis 0 (x), 1 (y) or 2 (z).
	/// @return It.
	[[nodiscard]] constexpr float operator[](int axis) const noexcept { return axis == 0 ? x : (axis == 1 ? y : z); }
	/// @brief The component at an axis, writable.
	/// @param axis 0 (x), 1 (y) or 2 (z).
	/// @return It.
	[[nodiscard]] constexpr float& operator[](int axis) noexcept { return axis == 0 ? x : (axis == 1 ? y : z); }
};

/// @brief Adds component by component. @param a One. @param b The other. @return The sum.
[[nodiscard]] constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
/// @brief Subtracts component by component. @param a One. @param b The other. @return The difference.
[[nodiscard]] constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
/// @brief Negates. @param a The vector. @return -a.
[[nodiscard]] constexpr Vec3 operator-(Vec3 a) noexcept { return {-a.x, -a.y, -a.z}; }
/// @brief Scales. @param a The vector. @param s The factor. @return a * s.
[[nodiscard]] constexpr Vec3 operator*(Vec3 a, float s) noexcept { return {a.x * s, a.y * s, a.z * s}; }
/// @brief Scales. @param s The factor. @param a The vector. @return s * a.
[[nodiscard]] constexpr Vec3 operator*(float s, Vec3 a) noexcept { return a * s; }
/// @brief Divides. @param a The vector. @param s The divisor, not 0. @return a / s.
[[nodiscard]] constexpr Vec3 operator/(Vec3 a, float s) noexcept { return {a.x / s, a.y / s, a.z / s}; }
/// @brief Multiplies component by component. @param a One. @param b The other. @return The product.
[[nodiscard]] constexpr Vec3 multiply(Vec3 a, Vec3 b) noexcept { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
/// @brief The dot product. @param a One. @param b The other. @return a · b.
[[nodiscard]] constexpr float dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
/// @brief The cross product. @param a One. @param b The other. @return a × b.
[[nodiscard]] constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
/// @brief The length. @param a The vector. @return |a|.
[[nodiscard]] inline float length(Vec3 a) noexcept { return std::sqrt(dot(a, a)); }
/// @brief The same direction, of length 1. @param a The vector. @return a / |a|, or a itself if it has no length.
[[nodiscard]] Vec3 normalize(Vec3 a) noexcept;
/// @brief Interpolates. @param a At 0. @param b At 1. @param t Where. @return a + (b - a) * t.
[[nodiscard]] constexpr Vec3 lerp(Vec3 a, Vec3 b, float t) noexcept { return a + (b - a) * t; }
/// @brief The smaller of each component. @param a One. @param b The other. @return The minimum.
[[nodiscard]] Vec3 minimum(Vec3 a, Vec3 b) noexcept;
/// @brief The larger of each component. @param a One. @param b The other. @return The maximum.
[[nodiscard]] Vec3 maximum(Vec3 a, Vec3 b) noexcept;
/// @brief Whether every component is finite. @param a The vector. @return true if so.
[[nodiscard]] bool isFinite(Vec3 a) noexcept;

/// @brief Four components: a homogeneous point, or a colour with alpha.
struct Vec4 {
	float x = 0.0f; ///< First.
	float y = 0.0f; ///< Second.
	float z = 0.0f; ///< Third.
	float w = 0.0f; ///< Fourth.

	/// @brief Equal when every component is equal.
	constexpr bool operator==(const Vec4&) const noexcept = default;
};

/// @brief A rotation, as a unit quaternion (x, y, z, w), w the scalar part, as glTF stores it.
struct Quat {
	float x = 0.0f; ///< Vector part, x.
	float y = 0.0f; ///< Vector part, y.
	float z = 0.0f; ///< Vector part, z.
	float w = 1.0f; ///< Scalar part.

	/// @brief Equal when every component is equal.
	constexpr bool operator==(const Quat&) const noexcept = default;

	/// @brief A rotation around an axis.
	/// @param axis The axis; normalised here.
	/// @param radians The angle, counter-clockwise looking down the axis.
	/// @return The rotation.
	[[nodiscard]] static Quat fromAxisAngle(Vec3 axis, float radians) noexcept;
	/// @brief A rotation from Euler angles, applied x, then y, then z (as Blender's default XYZ).
	/// @param radians The angles around x, y and z.
	/// @return The rotation.
	[[nodiscard]] static Quat fromEuler(Vec3 radians) noexcept;
	/// @brief The Euler angles of this rotation, in the order fromEuler() takes them.
	/// @return The angles, in radians.
	[[nodiscard]] Vec3 toEuler() const noexcept;
	/// @brief The inverse of a unit quaternion. @return (-x, -y, -z, w).
	[[nodiscard]] constexpr Quat conjugate() const noexcept { return {-x, -y, -z, w}; }
	/// @brief Turns a point or direction. @param v It. @return It, rotated.
	[[nodiscard]] Vec3 rotate(Vec3 v) const noexcept;
};

/// @brief Composes rotations: b first, then a. @param a The second. @param b The first. @return a * b.
[[nodiscard]] Quat operator*(Quat a, Quat b) noexcept;
/// @brief Of length 1. @param q The quaternion. @return q / |q|, or the identity if it has no length.
[[nodiscard]] Quat normalize(Quat q) noexcept;
/// @brief Spherical interpolation, along the shorter arc. @param a At 0. @param b At 1. @param t Where.
/// @return The rotation between.
[[nodiscard]] Quat slerp(Quat a, Quat b, float t) noexcept;

/// @brief A 4 by 4 matrix, column-major: element (row, column) is at `values[column * 4 + row]`.
struct Mat4 {
	std::array<float, 16> values{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; ///< The identity by default.

	/// @brief Equal when every element is equal.
	constexpr bool operator==(const Mat4&) const noexcept = default;
	/// @brief An element. @param row Its row. @param column Its column. @return It.
	[[nodiscard]] constexpr float at(int row, int column) const noexcept {
		// NOLINTNEXTLINE(bugprone-misplaced-widening-cast): row and column are 0 to 3, so the int can't overflow
		return values[static_cast<std::size_t>(column * 4 + row)];
	}
	/// @brief An element, writable. @param row Its row. @param column Its column. @return It.
	[[nodiscard]] constexpr float& at(int row, int column) noexcept {
		// NOLINTNEXTLINE(bugprone-misplaced-widening-cast): row and column are 0 to 3, so the int can't overflow
		return values[static_cast<std::size_t>(column * 4 + row)];
	}

	/// @brief A translation. @param offset How far. @return The matrix.
	[[nodiscard]] static Mat4 translation(Vec3 offset) noexcept;
	/// @brief A scale. @param factors Per axis. @return The matrix.
	[[nodiscard]] static Mat4 scaling(Vec3 factors) noexcept;
	/// @brief A rotation. @param rotation A unit quaternion. @return The matrix.
	[[nodiscard]] static Mat4 rotation(Quat rotation) noexcept;
	/// @brief A perspective projection to OpenGL's clip space (z from -1 to 1).
	/// @param fieldOfViewY The vertical angle, in radians, between 0 and pi.
	/// @param aspect Width over height, above 0.
	/// @param nearPlane The near distance, above 0.
	/// @param farPlane The far distance, beyond the near one.
	/// @return The matrix.
	[[nodiscard]] static Mat4 perspective(float fieldOfViewY, float aspect, float nearPlane, float farPlane) noexcept;
	/// @brief An orthographic projection to OpenGL's clip space.
	/// @param halfWidth Half the visible width. @param halfHeight Half the visible height.
	/// @param nearPlane The near distance. @param farPlane The far distance.
	/// @return The matrix.
	[[nodiscard]] static Mat4 orthographic(float halfWidth, float halfHeight, float nearPlane, float farPlane) noexcept;
	/// @brief A camera's view matrix.
	/// @param eye Where it is. @param target What it looks at, not at the eye. @param up Roughly up.
	/// @return The matrix that takes world space into the camera's.
	[[nodiscard]] static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) noexcept;

	/// @brief Transforms a point (w = 1), dividing by w. @param point The point. @return It, transformed.
	[[nodiscard]] Vec3 transformPoint(Vec3 point) const noexcept;
	/// @brief Transforms a direction (w = 0). @param direction It. @return It, transformed, not normalised.
	[[nodiscard]] Vec3 transformDirection(Vec3 direction) const noexcept;
	/// @brief Transforms a homogeneous point. @param point It. @return It, transformed, not divided.
	[[nodiscard]] Vec4 transform(Vec4 point) const noexcept;
	/// @brief The inverse. @return It, or nothing if the matrix is singular.
	[[nodiscard]] std::optional<Mat4> inverse() const noexcept;
	/// @brief Rows and columns swapped. @return The transpose.
	[[nodiscard]] Mat4 transposed() const noexcept;
	/// @brief The translation part (the last column). @return It.
	[[nodiscard]] constexpr Vec3 origin() const noexcept { return {at(0, 3), at(1, 3), at(2, 3)}; }
};

/// @brief Composes: b first, then a. @param a The second. @param b The first. @return a * b.
[[nodiscard]] Mat4 operator*(const Mat4& a, const Mat4& b) noexcept;

/// @brief A node's local transform: scale, then rotation, then translation (glTF's TRS).
struct Transform {
	Vec3 translation;             ///< Where.
	Quat rotation;                ///< How it is turned.
	Vec3 scale{1.0f, 1.0f, 1.0f}; ///< How large, per axis.

	/// @brief Equal when every part is equal.
	constexpr bool operator==(const Transform&) const noexcept = default;
	/// @brief The matrix T * R * S. @return It.
	[[nodiscard]] Mat4 matrix() const noexcept;
	/// @brief Splits a matrix with no shear or projection into its parts.
	/// @param matrix The matrix.
	/// @return The transform, or nothing if a scale is 0 or the matrix projects.
	[[nodiscard]] static std::optional<Transform> decompose(const Mat4& matrix) noexcept;
};

/// @brief Degrees to radians. @param degrees The angle. @return In radians.
[[nodiscard]] constexpr float radians(float degrees) noexcept { return degrees * 0.017453292519943295f; }
/// @brief Radians to degrees. @param radians The angle. @return In degrees.
[[nodiscard]] constexpr float degrees(float radians) noexcept { return radians * 57.29577951308232f; }

} // namespace affresco::scene3d
