/// @file
/// @brief Keyframe tracks and clips (Animation.hpp).

#include "affresco/scene3d/Animation.hpp"

#include <algorithm>
#include <cmath>

namespace affresco::scene3d {

namespace {

/// @brief The floats of one key's value. @param track The track. @param key Which key. @param part For cubic keys:
/// 0 in-tangent, 1 value, 2 out-tangent. @return The floats, padded with 0.
std::array<float, 4> keyValue(const Track& track, std::size_t key, std::size_t part) {
	const std::size_t width = track.width();
	const std::size_t stride = track.interpolation == Interpolation::Cubic ? 3 : 1;
	const std::size_t offset = (key * stride + (stride == 3 ? part : 0)) * width;
	std::array<float, 4> value{};
	for(std::size_t i = 0; i < width; ++i) value[i] = track.values[offset + i];
	return value;
}

/// @brief The two keys a cubic segment runs between, each with the tangent on the segment's side.
struct Segment {
	std::array<float, 4> a;    ///< The first key's value.
	std::array<float, 4> outA; ///< Its out-tangent.
	std::array<float, 4> b;    ///< The second key's value.
	std::array<float, 4> inB;  ///< Its in-tangent.
};

/// @brief Cubic Hermite interpolation, as glTF's CUBICSPLINE. @param segment The keys. @param span The time between
/// the keys. @param t From 0 to 1. @return The value.
std::array<float, 4> hermite(const Segment& segment, float span, float t) {
	const auto& [a, outA, b, inB] = segment;
	const float t2 = t * t, t3 = t2 * t;
	std::array<float, 4> value{};
	for(std::size_t i = 0; i < 4; ++i)
		value[i] = (2 * t3 - 3 * t2 + 1) * a[i] + (t3 - 2 * t2 + t) * span * outA[i] + (-2 * t3 + 3 * t2) * b[i] +
		           (t3 - t2) * span * inB[i];
	return value;
}

/// @brief A 4-float value as a quaternion. @param value It. @return The quaternion.
Quat asQuat(const std::array<float, 4>& value) noexcept { return {value[0], value[1], value[2], value[3]}; }

} // namespace

const char* interpolationName(Interpolation interpolation) noexcept {
	switch(interpolation) {
	case Interpolation::Step:
		return "step";
	case Interpolation::Linear:
		return "linear";
	case Interpolation::Cubic:
		return "cubic";
	}
	return "linear";
}

std::optional<Interpolation> interpolationNamed(std::string_view name) noexcept {
	for(const Interpolation interpolation : {Interpolation::Step, Interpolation::Linear, Interpolation::Cubic})
		if(name == interpolationName(interpolation)) return interpolation;
	return std::nullopt;
}

const char* trackPathName(TrackPath path) noexcept {
	switch(path) {
	case TrackPath::Translation:
		return "translation";
	case TrackPath::Rotation:
		return "rotation";
	case TrackPath::Scale:
		return "scale";
	}
	return "translation";
}

std::optional<TrackPath> trackPathNamed(std::string_view name) noexcept {
	for(const TrackPath path : {TrackPath::Translation, TrackPath::Rotation, TrackPath::Scale})
		if(name == trackPathName(path)) return path;
	return std::nullopt;
}

std::string Track::validate() const {
	const std::size_t stride = interpolation == Interpolation::Cubic ? 3 : 1;
	if(values.size() != times.size() * stride * width()) return "the values don't match the times";
	for(std::size_t i = 0; i < times.size(); ++i)
		if(!std::isfinite(times[i]) || times[i] < 0.0f || (i > 0 && times[i] < times[i - 1]))
			return "the times aren't finite and ascending";
	for(const float value : values)
		if(!std::isfinite(value)) return "a value isn't finite";
	return {};
}

std::optional<std::array<float, 4>> Track::sample(float time) const {
	if(times.empty() || !validate().empty()) return std::nullopt;
	if(time <= times.front()) return keyValue(*this, 0, 1);
	if(time >= times.back()) return keyValue(*this, times.size() - 1, 1);
	const auto next = static_cast<std::size_t>(std::ranges::upper_bound(times, time) - times.begin());
	const std::size_t previous = next - 1;
	const float span = times[next] - times[previous];
	const float t = span > 0.0f ? (time - times[previous]) / span : 0.0f;
	const auto a = keyValue(*this, previous, 1), b = keyValue(*this, next, 1);
	std::array<float, 4> value{};
	if(interpolation == Interpolation::Step) value = a;
	else if(interpolation == Interpolation::Cubic)
		value = hermite({a, keyValue(*this, previous, 2), b, keyValue(*this, next, 0)}, span, t);
	else if(path == TrackPath::Rotation) {
		const Quat q = slerp(asQuat(a), asQuat(b), t);
		value = {q.x, q.y, q.z, q.w};
	} else
		for(std::size_t i = 0; i < 3; ++i) value[i] = a[i] + (b[i] - a[i]) * t;
	if(path == TrackPath::Rotation) {
		const Quat q = normalize(asQuat(value));
		value = {q.x, q.y, q.z, q.w};
	}
	return value;
}

float AnimationClip::duration() const noexcept {
	float last = 0.0f;
	for(const Track& track : tracks)
		if(!track.times.empty()) last = std::max(last, track.times.back());
	return last;
}

void AnimationClip::apply(Scene& scene, float time) const {
	for(const Track& track : tracks) {
		Node* node = scene.node(track.node);
		const auto value = node ? track.sample(time) : std::nullopt;
		if(!value) continue;
		const Vec3 vector{(*value)[0], (*value)[1], (*value)[2]};
		switch(track.path) {
		case TrackPath::Translation:
			node->transform.translation = vector;
			break;
		case TrackPath::Rotation:
			node->transform.rotation = asQuat(*value);
			break;
		case TrackPath::Scale:
			node->transform.scale = vector;
			break;
		}
	}
}

void setKey(Track& track, float time, const std::array<float, 4>& value) {
	// A cubic key is the in-tangent, the value and the out-tangent: new keys get flat tangents
	const std::size_t width = track.width();
	const std::size_t stride = track.interpolation == Interpolation::Cubic ? 3 : 1;
	std::vector<float> block(width * stride, 0.0f);
	std::copy_n(value.begin(), width, block.begin() + static_cast<std::ptrdiff_t>(stride == 3 ? width : 0));
	const auto at = std::ranges::lower_bound(track.times, time);
	const auto offset = (at - track.times.begin()) * static_cast<std::ptrdiff_t>(block.size());
	if(at != track.times.end() && *at == time) {
		std::ranges::copy(block, track.values.begin() + offset);
		return;
	}
	track.times.insert(at, time);
	track.values.insert(track.values.begin() + offset, block.begin(), block.end());
}

std::array<float, 4> partValue(const Node& node, TrackPath path) noexcept {
	const Transform& transform = node.transform;
	switch(path) {
	case TrackPath::Translation:
		return {transform.translation.x, transform.translation.y, transform.translation.z, 0};
	case TrackPath::Rotation:
		return {transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w};
	case TrackPath::Scale:
		return {transform.scale.x, transform.scale.y, transform.scale.z, 0};
	}
	return {};
}

} // namespace affresco::scene3d
