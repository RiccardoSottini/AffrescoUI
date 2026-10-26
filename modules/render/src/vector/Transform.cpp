/// @file
/// @brief Affine2::inverse, out of line: inlined into a caller whose matrix is a constant that flattens the plane,
/// MSVC warns of a division by 0 (C4723) on the path that returns before dividing.

#include "affresco/render/Transform.hpp"

#include <cmath>
#include <optional>

namespace affresco {

std::optional<Affine2> Affine2::inverse() const noexcept {
	const float det = determinant();
	if(!std::isfinite(det) || std::abs(det) < 1e-12f) return std::nullopt;
	const float ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
	return Affine2{ia, ib, ic, id, -(ia * tx + ic * ty), -(ib * tx + id * ty)};
}

} // namespace affresco
