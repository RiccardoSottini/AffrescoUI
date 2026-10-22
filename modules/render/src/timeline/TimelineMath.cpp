/// @file
/// @brief The timeline's arithmetic, free of any view: the time mapping, the ruler's steps, the time formats,
/// snapping and easing.

#include "timeline/TimelineShared.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace affresco {

using timeline::majorSpacing;
using timeline::minorSpacing;
using timeline::nearestTarget;

namespace {

/// @brief Whether a duration is a whole number of frames.
/// @param seconds The duration.
/// @param fps Frames per second.
/// @return true if it is within a millionth of a frame of one.
bool wholeFrames(double seconds, double fps) noexcept {
	const double frames = seconds * fps;
	return std::abs(frames - std::round(frames)) < 1e-6 && std::round(frames) >= 1.0;
}

/// @brief The value of a cubic Bézier coordinate from 0 to 1 with handles a and b.
/// @param a The first handle's coordinate.
/// @param b The second's.
/// @param t The curve's parameter.
/// @return The coordinate.
double bezierAt(double a, double b, double t) noexcept {
	const double u = 1.0 - t;
	return 3.0 * u * u * t * a + 3.0 * u * t * t * b + t * t * t;
}

/// @brief The labelled step in seconds: 1, 2 or 5 times a power of ten.
/// @param least The least step that leaves room for a label, in seconds.
/// @return The step; 0 if none fits.
double decimalStep(double least) {
	const double power = std::pow(10.0, std::floor(std::log10(least)));
	for(const double step : {1.0, 2.0, 5.0, 10.0})
		if(step * power >= least * (1.0 - 1e-9)) return step * power;
	return 0.0;
}

/// @brief The labelled step in frames or timecode: whole frames under a second, then round numbers of seconds.
/// @param least The least step that leaves room for a label, in seconds.
/// @param fps Frames per second.
/// @return The step, in seconds.
double frameStep(double least, double fps) {
	for(const double frames : {1.0, 2.0, 5.0, 10.0, 15.0})
		if(frames / fps < 1.0 && frames / fps >= least) return frames / fps;
	for(const double step : {1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0, 600.0, 900.0, 1800.0, 3600.0, 7200.0,
	                         18000.0, 36000.0, 86400.0, 864000.0})
		if(step >= least) return step;
	return 864000.0;
}

/// @brief The small step between labelled ones: a tenth, fifth, quarter or half, roomy enough, and whole frames
/// under a second unless the format is seconds.
/// @param major The labelled step.
/// @param scale Window units per second.
/// @param fps Frames per second.
/// @param format The ruler's format.
/// @return The step; 0 for none.
double minorStep(double major, double scale, double fps, TimeFormat format) {
	for(const double parts : {10.0, 5.0, 4.0, 2.0}) {
		const double minor = major / parts;
		if(minor * scale < minorSpacing) continue;
		if(format != TimeFormat::Seconds && minor < 1.0 && !wholeFrames(minor, fps)) continue;
		return minor;
	}
	return 0.0;
}

} // namespace

float TimelineCamera::toX(double time) const noexcept { return static_cast<float>((time - offset) * scale); }

double TimelineCamera::toTime(float x) const noexcept { return offset + static_cast<double>(x) / scale; }

void TimelineCamera::zoomAt(float x, double scaleTo) noexcept {
	if(std::isnan(scaleTo)) return;
	const double anchor = toTime(x);
	scale = std::clamp(scaleTo, minimumScale, maximumScale);
	offset = anchor - static_cast<double>(x) / scale;
}

RulerTicks rulerTicks(double scale, double fps, TimeFormat format) {
	if(!(scale > 0.0)) scale = TimelineCamera::minimumScale;
	if(!(fps > 0.0)) fps = 30.0;
	const double least = majorSpacing / scale;
	const double major = format == TimeFormat::Seconds ? decimalStep(least) : frameStep(least, fps);
	return {major, minorStep(major, scale, fps, format)};
}

std::string formatTime(double time, double fps, TimeFormat format) {
	if(!std::isfinite(time)) time = 0.0;
	if(!(fps > 0.0)) fps = 30.0;
	switch(format) {
	case TimeFormat::Frames:
		return std::format("{}", std::llround(time * fps));
	case TimeFormat::Seconds: {
		std::string text = std::format("{:.3f}", time);
		while(text.back() == '0') text.pop_back();
		if(text.back() == '.') text.pop_back();
		if(text == "-0") text = "0";
		return text + "s";
	}
	case TimeFormat::Timecode: {
		const long long perSecond = std::max(1LL, std::llround(fps));
		const long long frames = std::llround(std::abs(time) * static_cast<double>(perSecond));
		const long long seconds = frames / perSecond;
		return std::format("{}{:02}:{:02}:{:02}:{:02}", time < 0.0 && frames > 0 ? "-" : "", seconds / 3600,
		                   seconds / 60 % 60, seconds % 60, frames % perSecond);
	}
	}
	return {};
}

double snapTime(double time, double fps, std::span<const double> targets, double tolerance) {
	if(const auto target = nearestTarget(time, targets, tolerance)) return *target;
	if(fps > 0.0) return std::round(time * fps) / fps;
	return time;
}

double bezierEase(const std::array<double, 4>& handles, double progress) {
	const double x1 = std::clamp(handles[0], 0.0, 1.0), x2 = std::clamp(handles[2], 0.0, 1.0);
	const double p = std::clamp(std::isnan(progress) ? 0.0 : progress, 0.0, 1.0);
	// The curve's parameter where its x is the progress: Newton's steps, then halving if they don't settle
	double t = p;
	bool found = false;
	for(int step = 0; step < 8; ++step) {
		const double error = bezierAt(x1, x2, t) - p;
		if(std::abs(error) < 1e-7) {
			found = true;
			break;
		}
		const double u = 1.0 - t;
		const double slope = 3.0 * u * u * x1 + 6.0 * u * t * (x2 - x1) + 3.0 * t * t * (1.0 - x2);
		if(std::abs(slope) < 1e-9) break;
		t -= error / slope;
		if(t < 0.0 || t > 1.0) break;
	}
	if(!found) {
		double low = 0.0, high = 1.0;
		t = p;
		for(int step = 0; step < 60; ++step) {
			t = (low + high) * 0.5;
			if(bezierAt(x1, x2, t) < p) low = t;
			else high = t;
		}
	}
	return bezierAt(handles[1], handles[3], t);
}

std::optional<double> keyframeValueAt(std::span<const TimelineKeyframe> keyframes, double time) {
	if(keyframes.empty()) return std::nullopt;
	const auto earlier = [](const TimelineKeyframe& a, const TimelineKeyframe& b) { return a.time < b.time; };
	std::vector<TimelineKeyframe> sorted;
	if(!std::is_sorted(keyframes.begin(), keyframes.end(), earlier)) {
		sorted.assign(keyframes.begin(), keyframes.end());
		std::stable_sort(sorted.begin(), sorted.end(), earlier);
		keyframes = sorted;
	}
	if(std::isnan(time) || time <= keyframes.front().time) return keyframes.front().value;
	if(time >= keyframes.back().time) return keyframes.back().value;
	const auto next = std::upper_bound(keyframes.begin(), keyframes.end(), time,
	                                   [](double at, const TimelineKeyframe& keyframe) { return at < keyframe.time; });
	const TimelineKeyframe& a = *(next - 1);
	const TimelineKeyframe& b = *next;
	const double span = b.time - a.time;
	if(!(span > 0.0)) return b.value;
	const double progress = (time - a.time) / span;
	switch(a.easing) {
	case Easing::Step:
		return a.value;
	case Easing::Linear:
		return a.value + (b.value - a.value) * progress;
	case Easing::Bezier:
		return a.value + (b.value - a.value) * bezierEase(a.handles, progress);
	}
	return a.value;
}

} // namespace affresco
