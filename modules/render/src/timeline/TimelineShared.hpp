#pragma once

/// @file
/// @brief What the TimelineView's sources share and keep out of its public header: the sizes, and the questions
/// asked of a model by TimelineView.cpp (the rows and the hit tests), TimelineInput.cpp (the pointer and the keys)
/// and TimelinePaint.cpp (the drawing).

#include <affresco/render/TimelineView.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace affresco::timeline {

constexpr float rulerHeight = 28.0f;    ///< The height of the ruler.
constexpr float indentStep = 12.0f;     ///< How far a header moves right per level of depth.
constexpr float chevronWidth = 16.0f;   ///< Room for a group's chevron, left of its label.
constexpr float buttonSize = 18.0f;     ///< The mute and lock buttons' size.
constexpr float keyframeRadius = 5.0f;  ///< Half a keyframe's width.
constexpr float edgeGrab = 5.0f;        ///< Window units inside a clip's edge that trim it.
constexpr float snapReach = 6.0f;       ///< How near a target a time snaps to it, in window units.
constexpr float majorSpacing = 80.0f;   ///< The least room between labelled ticks.
constexpr float minorSpacing = 6.0f;    ///< The least room between small ticks.
constexpr std::size_t tickLimit = 4000; ///< The most ticks drawn of each kind.
constexpr double keyZoom = 1.25;        ///< What + multiplies the zoom by.
constexpr std::size_t none = std::numeric_limits<std::size_t>::max(); ///< No track or row.

/// @brief The target nearest a time within a tolerance.
/// @param time In seconds.
/// @param targets The targets.
/// @param tolerance How far it may be.
/// @return It, or std::nullopt.
[[nodiscard]] inline std::optional<double> nearestTarget(double time, std::span<const double> targets,
                                                         double tolerance) noexcept {
	std::optional<double> best;
	for(const double target : targets)
		if(std::abs(target - time) <= tolerance && (!best || std::abs(target - time) < std::abs(*best - time)))
			best = target;
	return best;
}

/// @brief Whether a track is a group: the track after it is deeper.
/// @param tracks The tracks.
/// @param index The track's index.
/// @return true for a group.
[[nodiscard]] inline bool isGroup(const std::vector<TimelineTrack>& tracks, std::size_t index) noexcept {
	return index + 1 < tracks.size() && tracks[index + 1].depth > tracks[index].depth;
}

/// @brief The play range's end: its own, or the duration when it has none.
/// @param model The timeline.
/// @return In seconds.
[[nodiscard]] inline double playEndOf(const TimelineModel& model) noexcept {
	return model.playEnd > model.playStart ? model.playEnd : model.duration;
}

/// @brief Whether an item is in a model: its track, and its clip or keyframe, exist.
/// @param model The timeline.
/// @param item The item.
/// @return true if it is.
[[nodiscard]] inline bool holds(const TimelineModel& model, const TimelineItem& item) noexcept {
	if(item.track >= model.tracks.size()) return false;
	const TimelineTrack& track = model.tracks[item.track];
	return item.index < (item.clip ? track.clips.size() : track.keyframes.size());
}

/// @brief When an item starts: a clip's start or a keyframe's time.
/// @param model The timeline.
/// @param item The item.
/// @return In seconds; 0 for an item the model doesn't hold.
[[nodiscard]] inline double timeOf(const TimelineModel& model, const TimelineItem& item) noexcept {
	if(!holds(model, item)) return 0.0;
	const TimelineTrack& track = model.tracks[item.track];
	return item.clip ? track.clips[item.index].start : track.keyframes[item.index].time;
}

} // namespace affresco::timeline
