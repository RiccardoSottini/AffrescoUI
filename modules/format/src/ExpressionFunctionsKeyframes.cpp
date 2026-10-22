/// @file
/// @brief Expression functions of animation: keyframeValue, a timeline track's value at a time.
///
/// The keyframes are read as a "timeline" node reads them ({ time, value, easing }) and eased as TimelineView draws
/// them, by keyframeValueAt(): step, linear or a cubic Bézier.

#include "ExpressionFunctions.hpp"

#include "affresco/render/TimelineView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

namespace affresco::format::detail {
namespace {

/// @brief Reads a keyframe's easing: "step", "linear", "bezier", or [x1, y1, x2, y2].
/// @param value The value.
/// @param keyframe The keyframe it sets; anything else leaves it linear.
void readEasing(const Value& value, TimelineKeyframe& keyframe) {
	if(value.isText()) {
		const std::string_view name = value.text();
		if(name == "step") keyframe.easing = Easing::Step;
		else if(name == "bezier") keyframe.easing = Easing::Bezier;
		return;
	}
	if(!value.isList() || value.size() != 4) return;
	std::array<double, 4> handles{};
	for(std::size_t i = 0; i < 4; ++i) {
		const Value handle = value.item(i);
		if(!handle.isNumber() || !std::isfinite(handle.number())) return;
		handles[i] = handle.number();
	}
	keyframe.easing = Easing::Bezier;
	keyframe.handles = handles;
}

/// @brief keyframeValue(track, time): a track's value at a time, between its keyframes.
/// @param call The track ({ keyframes } or the list of them) and the time, in seconds.
/// @return The value, or null when the track has no keyframes.
Value keyframeValue(const Call& call) {
	const double time = call.number(1);
	Value list = call[0];
	if(list.isObject()) {
		std::optional<Value> keyframes = list.member("keyframes");
		if(!keyframes) return {};
		list = std::move(*keyframes);
	}
	if(!list.isList()) call.fail("expected a track, { keyframes }, or a list of keyframes");
	// As many as a track keeps; items that aren't { time, value } are skipped
	std::vector<TimelineKeyframe> keyframes;
	const std::size_t count = std::min(list.size(), TimelineView::maximumItems);
	keyframes.reserve(count);
	for(std::size_t i = 0; i < count; ++i) {
		const Value item = list.item(i);
		if(!item.isObject()) continue;
		const std::optional<Value> at = item.member("time"), value = item.member("value");
		if(!at || !at->isNumber() || !value || !value->isNumber()) continue;
		TimelineKeyframe keyframe;
		keyframe.time = at->number();
		keyframe.value = value->number();
		if(!std::isfinite(keyframe.time) || !std::isfinite(keyframe.value)) continue;
		if(const std::optional<Value> easing = item.member("easing")) readEasing(*easing, keyframe);
		keyframes.push_back(keyframe);
	}
	const std::optional<double> result = keyframeValueAt(keyframes, time);
	return result ? Value::fromNumber(*result) : Value{};
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "keyframeValue",
	 .arguments = "track, time",
	 .about = "A timeline track's value at a time (seconds), eased between its keyframes { time, value, easing } "
	          "(step, linear, bezier or [x1, y1, x2, y2]); null without keyframes",
	 .fewest = 2,
	 .most = 2,
	 .apply = keyframeValue},
});

} // namespace

std::span<const Function> keyframeFunctions() { return table; }

} // namespace affresco::format::detail
