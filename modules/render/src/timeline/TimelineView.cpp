/// @file
/// @brief TimelineView: the time mapping, the ruler's steps, snapping, easing, row virtualisation and the editing.

#include "affresco/render/TimelineView.hpp"
#include "affresco/render/RichText.hpp"
#include "affresco/render/VectorPath.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <unordered_map>

namespace affresco {

namespace {

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

/// @brief Whether a duration is a whole number of frames.
/// @param seconds The duration.
/// @param fps Frames per second.
/// @return true if it is within a millionth of a frame of one.
bool wholeFrames(double seconds, double fps) noexcept {
	const double frames = seconds * fps;
	return std::abs(frames - std::round(frames)) < 1e-6 && std::round(frames) >= 1.0;
}

/// @brief The target nearest a time within a tolerance.
/// @param time In seconds.
/// @param targets The targets.
/// @param tolerance How far it may be.
/// @return It, or std::nullopt.
std::optional<double> nearestTarget(double time, std::span<const double> targets, double tolerance) noexcept {
	std::optional<double> best;
	for(const double target : targets)
		if(std::abs(target - time) <= tolerance && (!best || std::abs(target - time) < std::abs(*best - time)))
			best = target;
	return best;
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

/// @brief Whether a track is a group: the track after it is deeper.
/// @param tracks The tracks.
/// @param index The track's index.
/// @return true for a group.
bool isGroup(const std::vector<TimelineTrack>& tracks, std::size_t index) noexcept {
	return index + 1 < tracks.size() && tracks[index + 1].depth > tracks[index].depth;
}

/// @brief The play range's end: its own, or the duration when it has none.
/// @param model The timeline.
/// @return In seconds.
double playEndOf(const TimelineModel& model) noexcept {
	return model.playEnd > model.playStart ? model.playEnd : model.duration;
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
	double major = 0.0;
	if(format == TimeFormat::Seconds) {
		// 1, 2 or 5 times a power of ten
		const double power = std::pow(10.0, std::floor(std::log10(least)));
		for(const double step : {1.0, 2.0, 5.0, 10.0})
			if(step * power >= least * (1.0 - 1e-9)) {
				major = step * power;
				break;
			}
	} else {
		// Whole frames under a second, then round numbers of seconds
		for(const double frames : {1.0, 2.0, 5.0, 10.0, 15.0})
			if(frames / fps < 1.0 && frames / fps >= least) {
				major = frames / fps;
				break;
			}
		if(major == 0.0)
			for(const double step : {1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0, 600.0, 900.0, 1800.0, 3600.0,
			                         7200.0, 18000.0, 36000.0, 86400.0, 864000.0})
				if(step >= least) {
					major = step;
					break;
				}
		if(major == 0.0) major = 864000.0;
	}
	RulerTicks ticks{major, 0.0};
	for(const double parts : {10.0, 5.0, 4.0, 2.0}) {
		const double minor = major / parts;
		if(minor * scale < minorSpacing) continue;
		if(format != TimeFormat::Seconds && minor < 1.0 && !wholeFrames(minor, fps)) continue;
		ticks.minor = minor;
		break;
	}
	return ticks;
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

TimelineView::TimelineView() {
	setColor(Color::hex(0x1e2026));
	setAccessibleRole("timeline");
}

void TimelineView::setModel(TimelineModel model) {
	if(model.tracks.size() > maximumTracks) model.tracks.resize(maximumTracks);
	if(model.markers.size() > maximumMarkers) model.markers.resize(maximumMarkers);
	std::size_t deepest = 0; // a track is at most one deeper than the one before it
	for(std::size_t i = 0; i < model.tracks.size(); ++i) {
		TimelineTrack& track = model.tracks[i];
		track.depth = std::min({track.depth, maximumDepth, i == 0 ? std::size_t{0} : deepest + 1});
		deepest = track.depth;
		if(track.keyframes.size() > maximumItems) track.keyframes.resize(maximumItems);
		if(track.clips.size() > maximumItems) track.clips.resize(maximumItems);
	}
	model.duration = std::isnan(model.duration) ? 0.0 : std::clamp(model.duration, 0.0, maximumDuration);
	model.fps = std::isnan(model.fps) ? 30.0 : std::clamp(model.fps, 1.0, maximumFps);
	model.playStart = std::isnan(model.playStart) ? 0.0 : std::clamp(model.playStart, 0.0, model.duration);
	model.playEnd = std::isnan(model.playEnd) ? 0.0 : std::clamp(model.playEnd, 0.0, model.duration);
	if(model == model_) return;
	model_ = std::move(model);
	time_ = std::clamp(time_, 0.0, model_.duration);
	refreshRows();
	refreshSelected();
	invalidate();
}

void TimelineView::setTime(double time) {
	if(std::isnan(time)) return;
	time = std::clamp(time, 0.0, model_.duration);
	if(time == time_) return;
	time_ = time;
	describe();
	invalidatePaint();
}

void TimelineView::setGivenTime(double time) {
	if(givenTime_ == time) return;
	givenTime_ = time;
	setTime(time);
}

void TimelineView::setSelection(std::vector<std::string> keys) {
	if(keys == selection_) return;
	selection_ = std::move(keys);
	refreshSelected();
	invalidatePaint();
}

void TimelineView::setGivenSelection(std::vector<std::string> keys) {
	if(givenSelection_ == keys) return;
	givenSelection_ = keys;
	setSelection(std::move(keys));
}

std::string TimelineView::itemKey(const TimelineItem& item) const {
	if(item.track >= model_.tracks.size()) return {};
	const TimelineTrack& track = model_.tracks[item.track];
	if(item.clip) return item.index < track.clips.size() ? track.clips[item.index].id : std::string{};
	return item.index < track.keyframes.size() ? track.id + ":" + std::to_string(item.index) : std::string{};
}

void TimelineView::refreshSelected() {
	selectedItems_.clear();
	if(selection_.empty()) return;
	std::unordered_map<std::string_view, TimelineItem> byKey;
	std::unordered_map<std::string_view, std::size_t> tracks;
	for(std::size_t t = 0; t < model_.tracks.size(); ++t) {
		tracks.emplace(model_.tracks[t].id, t);
		for(std::size_t c = 0; c < model_.tracks[t].clips.size(); ++c)
			byKey.emplace(model_.tracks[t].clips[c].id, TimelineItem{t, c, true});
	}
	for(const std::string& key : selection_) {
		if(const auto clip = byKey.find(key); clip != byKey.end()) {
			selectedItems_.push_back(clip->second);
			continue;
		}
		// A keyframe: its track's id, a colon and its index
		const std::size_t colon = key.rfind(':');
		if(colon == std::string::npos) continue;
		const auto track = tracks.find(std::string_view(key).substr(0, colon));
		if(track == tracks.end()) continue;
		std::size_t index = 0;
		const std::string_view digits = std::string_view(key).substr(colon + 1);
		if(digits.empty() || digits.size() > 9 ||
		   !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }))
			continue;
		for(const char c : digits) index = index * 10 + static_cast<std::size_t>(c - '0');
		if(index < model_.tracks[track->second].keyframes.size())
			selectedItems_.push_back({track->second, index, false});
	}
	std::sort(selectedItems_.begin(), selectedItems_.end());
	selectedItems_.erase(std::unique(selectedItems_.begin(), selectedItems_.end()), selectedItems_.end());
}

void TimelineView::setCamera(TimelineCamera camera) {
	if(std::isnan(camera.scale) || !std::isfinite(camera.offset)) return;
	camera.scale = std::clamp(camera.scale, TimelineCamera::minimumScale, TimelineCamera::maximumScale);
	camera.offset = std::clamp(camera.offset, -maximumDuration, maximumDuration);
	if(camera == camera_) return;
	camera_ = camera;
	invalidate();
}

void TimelineView::setGivenCamera(TimelineCamera camera) {
	if(givenCamera_ == camera) return;
	givenCamera_ = camera;
	setCamera(camera);
}

void TimelineView::setTimeFormat(TimeFormat format) { setLook(format_, format); }

void TimelineView::setSnap(bool snap) { snap_ = snap; }

void TimelineView::setSizes(float headerWidth, float trackHeight) {
	if(std::isnan(headerWidth) || std::isnan(trackHeight)) return;
	headerWidth = std::clamp(headerWidth, 0.0f, 2000.0f);
	trackHeight = std::clamp(trackHeight, 12.0f, 400.0f);
	if(headerWidth == headerWidth_ && trackHeight == trackHeight_) return;
	headerWidth_ = headerWidth;
	trackHeight_ = trackHeight;
	invalidate();
}

void TimelineView::setColors(Color header, Color clip, Color text, Color accent) {
	setLook(headerColor_, header);
	setLook(clipColor_, clip);
	setLook(textColor_, text);
	setLook(accent_, accent);
}

void TimelineView::setFont(std::shared_ptr<const Font> font) { setProperty(font_, std::move(font)); }

const Font* TimelineView::font() const {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

float TimelineView::lanesLeft() const noexcept { return std::min(headerWidth_, resolvedSize().x); }

std::size_t TimelineView::rowsInView() const noexcept {
	const float room = resolvedSize().y - rulerHeight;
	return room > 0.0f ? static_cast<std::size_t>(room / trackHeight_) : 0;
}

std::size_t TimelineView::rowAt(float y) const noexcept {
	if(y < rulerHeight) return rows_.size();
	const std::size_t row = firstRow_ + static_cast<std::size_t>((y - rulerHeight) / trackHeight_);
	return row < firstRow_ + visibleRows_ && row < rows_.size() ? row : rows_.size();
}

float TimelineView::timeX(double time) const noexcept { return lanesLeft() + camera_.toX(time); }

float TimelineView::trackY(std::size_t track) const noexcept {
	if(track >= rowOf_.size()) return -1.0f;
	const std::size_t row = rowOf_[track];
	if(row < firstRow_ || row >= firstRow_ + visibleRows_) return -1.0f;
	return rulerHeight + (static_cast<float>(row - firstRow_) + 0.5f) * trackHeight_;
}

void TimelineView::refreshRows() {
	rows_.clear();
	rowOf_.assign(model_.tracks.size(), none);
	std::size_t closed = none; // the depth of the closed group being skipped
	for(std::size_t i = 0; i < model_.tracks.size(); ++i) {
		const TimelineTrack& track = model_.tracks[i];
		if(closed != none && track.depth > closed) continue;
		closed = none;
		rowOf_[i] = rows_.size();
		rows_.push_back(i);
		if(isGroup(model_.tracks, i) && !track.expanded) closed = track.depth;
	}
	for(std::size_t& row : rowOf_)
		if(row == none) row = rows_.size();
	if(keyRow_ >= rows_.size()) keyRow_ = rows_.empty() ? 0 : rows_.size() - 1;
}

void TimelineView::arrange() {
	if(rowOf_.size() != model_.tracks.size()) refreshRows();
	const std::size_t fit = rowsInView();
	firstRow_ = std::min(firstRow_, rows_.size() > fit ? rows_.size() - fit : std::size_t{0});
	// The rows in view, the last one maybe in part
	const float room = std::max(resolvedSize().y - rulerHeight, 0.0f);
	const auto partial = static_cast<std::size_t>(std::ceil(room / trackHeight_));
	visibleRows_ = std::min(partial, rows_.size() - firstRow_);

	// Each child is the header of the track at its index, in its row left of the buttons
	const float lanes = lanesLeft();
	const auto& children = this->children();
	for(std::size_t i = 0; i < children.size(); ++i) {
		Panel& header = *children[i];
		if(header.isOverlay()) continue;
		const std::size_t row = i < rowOf_.size() ? rowOf_[i] : rows_.size();
		const bool inView = row >= firstRow_ && row < firstRow_ + visibleRows_;
		const TimelineTrack* track = i < model_.tracks.size() ? &model_.tracks[i] : nullptr;
		const float left = track ? static_cast<float>(track->depth) * indentStep + chevronWidth : 0.0f;
		const float width = lanes - left - 2.0f * buttonSize - 10.0f;
		header.setVisible(inView && width > 0.0f);
		if(!inView || width <= 0.0f) continue;
		header.clearAnchor(Edge::Right);
		header.clearAnchor(Edge::Bottom);
		header.setAnchor(Edge::Left, px(left));
		header.setAnchor(Edge::Top, px(rulerHeight + static_cast<float>(row - firstRow_) * trackHeight_));
		arrangeSize(header, width, trackHeight_);
		if(header.accessibleRole().empty()) header.setAccessibleRole("rowheader");
		if(header.accessibleLabel().empty()) header.setAccessibleLabel(track->label.empty() ? track->id : track->label);
	}
}

std::optional<TimelineItem> TimelineView::itemAt(Vec2 point, TimelineEvent::Edge& edge) const {
	edge = TimelineEvent::Edge::Move;
	const std::size_t row = rowAt(point.y);
	if(row >= rows_.size() || point.x < lanesLeft()) return std::nullopt;
	const std::size_t index = rows_[row];
	const TimelineTrack& track = model_.tracks[index];
	if(track.locked) return std::nullopt;
	const float x = point.x - lanesLeft();
	// Keyframes are drawn over clips, the last on top
	for(std::size_t k = track.keyframes.size(); k-- > 0;)
		if(std::abs(camera_.toX(track.keyframes[k].time) - x) <= keyframeRadius + 2.0f)
			return TimelineItem{index, k, false};
	for(std::size_t c = track.clips.size(); c-- > 0;) {
		const TimelineClip& clip = track.clips[c];
		const float start = camera_.toX(clip.start), end = camera_.toX(clip.start + clip.duration);
		if(x < start || x > end) continue;
		if(end - start > 3.0f * edgeGrab) {
			if(x - start <= edgeGrab) edge = TimelineEvent::Edge::Start;
			else if(end - x <= edgeGrab) edge = TimelineEvent::Edge::End;
		}
		return TimelineItem{index, c, true};
	}
	return std::nullopt;
}

bool TimelineView::isSelected(const TimelineItem& item) const {
	return std::binary_search(selectedItems_.begin(), selectedItems_.end(), item);
}

double TimelineView::itemTime(const TimelineItem& item) const noexcept {
	if(item.track >= model_.tracks.size()) return 0.0;
	const TimelineTrack& track = model_.tracks[item.track];
	if(item.clip) return item.index < track.clips.size() ? track.clips[item.index].start : 0.0;
	return item.index < track.keyframes.size() ? track.keyframes[item.index].time : 0.0;
}

std::vector<TimelineItem> TimelineView::itemsIn(double first, double last, std::size_t top, std::size_t bottom) const {
	std::vector<TimelineItem> items;
	for(std::size_t row = top; row <= bottom && row < rows_.size(); ++row) {
		const std::size_t index = rows_[row];
		const TimelineTrack& track = model_.tracks[index];
		if(track.locked) continue;
		for(std::size_t k = 0; k < track.keyframes.size(); ++k)
			if(track.keyframes[k].time >= first && track.keyframes[k].time <= last) items.push_back({index, k, false});
		for(std::size_t c = 0; c < track.clips.size(); ++c) {
			const TimelineClip& clip = track.clips[c];
			if(clip.start <= last && clip.start + clip.duration >= first) items.push_back({index, c, true});
		}
	}
	return items;
}

void TimelineView::findSnapTargets(bool dragging) {
	snapTargets_ = {0.0, model_.duration, model_.playStart, playEndOf(model_)};
	if(dragging) snapTargets_.push_back(time_);
	for(const TimelineMarker& marker : model_.markers) snapTargets_.push_back(marker.time);
	for(std::size_t row = firstRow_; row < firstRow_ + visibleRows_ && row < rows_.size(); ++row) {
		const std::size_t index = rows_[row];
		const TimelineTrack& track = model_.tracks[index];
		for(std::size_t k = 0; k < track.keyframes.size(); ++k)
			if(!dragging || !isSelected({index, k, false})) snapTargets_.push_back(track.keyframes[k].time);
		for(std::size_t c = 0; c < track.clips.size(); ++c)
			if(!dragging || !isSelected({index, c, true})) {
				snapTargets_.push_back(track.clips[c].start);
				snapTargets_.push_back(track.clips[c].start + track.clips[c].duration);
			}
	}
}

double TimelineView::snapDelta(double delta, bool snap) const {
	if(snap) {
		// The pressed item's start, or a clip's end, lands on the nearest target; else its start on a frame
		const double start = itemTime(pressedItem_) + delta;
		const double tolerance = snapReach / camera_.scale;
		std::optional<double> moved;
		if(const auto target = nearestTarget(start, snapTargets_, tolerance)) moved = *target - start;
		if(pressedItem_.clip && pressedItem_.track < model_.tracks.size() &&
		   pressedItem_.index < model_.tracks[pressedItem_.track].clips.size()) {
			const double end = start + model_.tracks[pressedItem_.track].clips[pressedItem_.index].duration;
			if(const auto target = nearestTarget(end, snapTargets_, tolerance);
			   target && (!moved || std::abs(*target - end) < std::abs(*moved)))
				moved = *target - end;
		}
		delta += moved ? *moved : std::round(start * model_.fps) / model_.fps - start;
	}
	// Nothing moves before 0
	double earliest = std::numeric_limits<double>::max();
	for(const TimelineItem& item : selectedItems_)
		if(!model_.tracks[item.track].locked) earliest = std::min(earliest, itemTime(item));
	return earliest == std::numeric_limits<double>::max() ? delta : std::max(delta, -earliest);
}

void TimelineView::select(std::vector<TimelineItem> items) {
	std::sort(items.begin(), items.end());
	items.erase(std::unique(items.begin(), items.end()), items.end());
	std::vector<std::string> keys;
	keys.reserve(items.size());
	for(const TimelineItem& item : items) keys.push_back(itemKey(item));
	if(keys == selection_) return;
	selection_ = std::move(keys);
	selectedItems_ = items;
	invalidatePaint();
	TimelineEvent event;
	event.kind = TimelineEvent::Kind::Select;
	event.items = std::move(items);
	onEdit(event);
}

void TimelineView::scrubTo(double time) {
	if(std::isnan(time)) return;
	time = std::clamp(time, 0.0, model_.duration);
	if(time == time_) return;
	time_ = time;
	invalidatePaint();
	TimelineEvent event;
	event.kind = TimelineEvent::Kind::Scrub;
	event.time = time;
	onEdit(event);
}

void TimelineView::moveSelection(double delta, long rowDelta) {
	// Gathered before any is fired, since the owner may change the model in between
	std::vector<TimelineEvent> events;
	struct Hop {
		std::size_t track, index, to; ///< A clip changing track: where it is and where it goes.
	};
	std::vector<Hop> hops;
	for(const TimelineItem& item : selectedItems_) {
		TimelineTrack& track = model_.tracks[item.track];
		if(track.locked) continue;
		TimelineEvent event;
		event.item = item;
		if(!item.clip) {
			TimelineKeyframe& keyframe = track.keyframes[item.index];
			const double time = std::max(0.0, keyframe.time + delta);
			if(time == keyframe.time) continue;
			event.kind = TimelineEvent::Kind::KeyframeMove;
			event.from = keyframe.time;
			event.time = time;
			// Kept in the model too, so the move shows at once; the owner's next model confirms or undoes it
			keyframe.time = time;
		} else {
			TimelineClip& clip = track.clips[item.index];
			std::size_t to = item.track;
			const long row = static_cast<long>(rowOf_[item.track]) + rowDelta;
			if(rowDelta != 0 && row >= 0 && static_cast<std::size_t>(row) < rows_.size() &&
			   !model_.tracks[rows_[static_cast<std::size_t>(row)]].locked)
				to = rows_[static_cast<std::size_t>(row)];
			const double start = std::max(0.0, clip.start + delta);
			if(start == clip.start && to == item.track) continue;
			event.kind = TimelineEvent::Kind::ClipChange;
			event.id = clip.id;
			event.from = clip.start;
			event.track = to;
			event.start = start;
			event.duration = clip.duration;
			event.edge = TimelineEvent::Edge::Move;
			clip.start = start;
			if(to != item.track) hops.push_back({item.track, item.index, to});
		}
		events.push_back(std::move(event));
	}
	if(events.empty()) return;
	// The clips changing track, the last of each track first so the indices stay right
	std::sort(hops.begin(), hops.end(),
	          [](const Hop& a, const Hop& b) { return a.track != b.track ? a.track < b.track : a.index > b.index; });
	std::vector<std::pair<std::size_t, TimelineClip>> moved;
	for(const Hop& hop : hops) {
		auto& clips = model_.tracks[hop.track].clips;
		moved.emplace_back(hop.to, std::move(clips[hop.index]));
		clips.erase(clips.begin() + static_cast<std::ptrdiff_t>(hop.index));
	}
	for(auto& [to, clip] : moved)
		if(model_.tracks[to].clips.size() < maximumItems) model_.tracks[to].clips.push_back(std::move(clip));
	if(!hops.empty()) refreshSelected();
	invalidate();
	for(const TimelineEvent& event : events) onEdit(event);
}

void TimelineView::trim(const TimelineItem& item, TimelineEvent::Edge edge, double time) {
	if(item.track >= model_.tracks.size() || item.index >= model_.tracks[item.track].clips.size()) return;
	TimelineClip& clip = model_.tracks[item.track].clips[item.index];
	const double end = clip.start + clip.duration;
	const double start = edge == TimelineEvent::Edge::Start ? time : clip.start;
	const double duration = edge == TimelineEvent::Edge::Start ? end - time : time - clip.start;
	if(start == clip.start && duration == clip.duration) return;
	TimelineEvent event;
	event.kind = TimelineEvent::Kind::ClipChange;
	event.item = item;
	event.id = clip.id;
	event.from = clip.start;
	event.track = item.track;
	event.start = start;
	event.duration = duration;
	event.edge = edge;
	clip.start = start;
	clip.duration = duration;
	invalidate();
	onEdit(event);
}

void TimelineView::toggleExpanded(std::size_t track) {
	if(!isGroup(model_.tracks, track)) return;
	TimelineTrack& group = model_.tracks[track];
	group.expanded = !group.expanded;
	refreshRows();
	invalidate();
	TimelineEvent event;
	event.kind = TimelineEvent::Kind::Expand;
	event.track = track;
	event.expanded = group.expanded;
	onEdit(event);
}

void TimelineView::toggleFlag(std::size_t track, bool lock) {
	if(track >= model_.tracks.size()) return;
	TimelineTrack& changed = model_.tracks[track];
	(lock ? changed.locked : changed.muted) = !(lock ? changed.locked : changed.muted);
	invalidatePaint();
	TimelineEvent event;
	event.kind = TimelineEvent::Kind::TrackChange;
	event.track = track;
	event.muted = changed.muted;
	event.locked = changed.locked;
	onEdit(event);
}

void TimelineView::revealRow(std::size_t row) {
	const std::size_t fit = std::max(rowsInView(), std::size_t{1});
	if(row < firstRow_) firstRow_ = row;
	else if(row >= firstRow_ + fit) firstRow_ = row + 1 - fit;
	invalidate();
}

void TimelineView::mouseButtonEvent(const MouseButtonEvent& event) {
	const Vec2 point{event.position.x - frame().left, event.position.y - frame().top};
	const float lanes = lanesLeft();
	if(event.action == Action::Press && event.button == MouseButton::Left) {
		pressed_ = pointer_ = point;
		dragDelta_ = 0.0;
		dragRows_ = 0;
		snapping_ = snap_ && !event.mods.alt();
		TimelineEvent::Edge edge = TimelineEvent::Edge::Move;
		if(point.y < rulerHeight && point.x >= lanes) {
			drag_ = Drag::Scrub;
			findSnapTargets(false);
			const double at = camera_.toTime(point.x - lanes);
			scrubTo(snapping_ ? snapTime(at, model_.fps, snapTargets_, snapReach / camera_.scale) : at);
		} else if(point.x < lanes) {
			drag_ = Drag::None;
			if(const std::size_t row = rowAt(point.y); row < rows_.size()) {
				const std::size_t track = rows_[row];
				const float lock = lanes - 4.0f - buttonSize, mute = lock - 2.0f - buttonSize;
				const float chevron = static_cast<float>(model_.tracks[track].depth) * indentStep;
				keyRow_ = row;
				if(point.x >= lock && point.x < lock + buttonSize) toggleFlag(track, true);
				else if(point.x >= mute && point.x < mute + buttonSize) toggleFlag(track, false);
				else if(point.x >= chevron && point.x < chevron + chevronWidth) toggleExpanded(track);
			}
		} else if(const auto item = itemAt(point, edge)) {
			std::vector<TimelineItem> items = selectedItems_;
			if(event.mods.shift()) {
				const auto found = std::find(items.begin(), items.end(), *item);
				if(found == items.end()) items.push_back(*item);
				else items.erase(found);
			} else if(!isSelected(*item)) {
				items = {*item};
			}
			select(std::move(items));
			keyRow_ = rowOf_[item->track];
			pressedItem_ = *item;
			drag_ = Drag::None;
			if(isSelected(*item)) {
				findSnapTargets(true);
				if(item->clip && edge != TimelineEvent::Edge::Move) {
					const TimelineClip& clip = model_.tracks[item->track].clips[item->index];
					drag_ = edge == TimelineEvent::Edge::Start ? Drag::Start : Drag::End;
					trimTime_ = edge == TimelineEvent::Edge::Start ? clip.start : clip.start + clip.duration;
				} else {
					drag_ = Drag::Items;
				}
			}
		} else if(point.y >= rulerHeight) {
			drag_ = Drag::Marquee;
			pressSelection_ = event.mods.shift() ? selectedItems_ : std::vector<TimelineItem>{};
		}
		describe();
		invalidatePaint();
	} else if(event.action == Action::Press) {
		drag_ = Drag::Pan;
		pressed_ = point;
		pressCamera_ = camera_;
		pressFirstRow_ = firstRow_;
	} else if(event.action == Action::Release && drag_ != Drag::None) {
		const Drag drag = drag_;
		drag_ = Drag::None;
		if(drag == Drag::Items) {
			const double delta = dragDelta_;
			const long rows = dragRows_;
			dragDelta_ = 0.0;
			dragRows_ = 0;
			if(delta != 0.0 || rows != 0) moveSelection(delta, rows);
		} else if(drag == Drag::Start || drag == Drag::End) {
			trim(pressedItem_, drag == Drag::Start ? TimelineEvent::Edge::Start : TimelineEvent::Edge::End, trimTime_);
		} else if(drag == Drag::Marquee) {
			const double a = camera_.toTime(pressed_.x - lanes), b = camera_.toTime(point.x - lanes);
			const float top = std::max(std::min(pressed_.y, point.y), rulerHeight);
			const float bottom = std::max(pressed_.y, point.y);
			const std::size_t first = firstRow_ + static_cast<std::size_t>((top - rulerHeight) / trackHeight_);
			const std::size_t last =
			    firstRow_ + static_cast<std::size_t>(std::max(bottom - rulerHeight, 0.0f) / trackHeight_);
			std::vector<TimelineItem> items = pressSelection_;
			const std::vector<TimelineItem> found = itemsIn(std::min(a, b), std::max(a, b), first, last);
			items.insert(items.end(), found.begin(), found.end());
			select(std::move(items));
		}
		snapTargets_.clear();
		invalidate();
	}
	Panel::mouseButtonEvent(event);
}

void TimelineView::mouseMoveEvent(const MouseMoveEvent& event) {
	pointer_ = {event.position.x - frame().left, event.position.y - frame().top};
	const float lanes = lanesLeft();
	const double frame = 1.0 / model_.fps;
	const auto snapped = [&](double at) {
		return snapping_ ? snapTime(at, model_.fps, snapTargets_, snapReach / camera_.scale) : at;
	};
	switch(drag_) {
	case Drag::Scrub:
		scrubTo(snapped(camera_.toTime(pointer_.x - lanes)));
		break;
	case Drag::Items:
		dragDelta_ = snapDelta(static_cast<double>(pointer_.x - pressed_.x) / camera_.scale, snapping_);
		dragRows_ = std::lround((pointer_.y - pressed_.y) / trackHeight_);
		invalidatePaint();
		break;
	case Drag::Start:
	case Drag::End:
		if(pressedItem_.track < model_.tracks.size() &&
		   pressedItem_.index < model_.tracks[pressedItem_.track].clips.size()) {
			const TimelineClip& clip = model_.tracks[pressedItem_.track].clips[pressedItem_.index];
			const double at = snapped(camera_.toTime(pointer_.x - lanes));
			// At least a frame stays
			trimTime_ = drag_ == Drag::Start ? std::clamp(at, 0.0, std::max(clip.start + clip.duration - frame, 0.0))
			                                 : std::clamp(at, clip.start + frame, maximumDuration);
			invalidatePaint();
		}
		break;
	case Drag::Pan: {
		TimelineCamera camera = pressCamera_;
		camera.offset = pressCamera_.offset - static_cast<double>(pointer_.x - pressed_.x) / camera_.scale;
		setCamera(camera);
		const long rows = std::lround((pointer_.y - pressed_.y) / trackHeight_);
		const long first = std::max(static_cast<long>(pressFirstRow_) - rows, 0L);
		if(static_cast<std::size_t>(first) != firstRow_) {
			firstRow_ = static_cast<std::size_t>(first);
			invalidate();
		}
		break;
	}
	case Drag::Marquee:
		invalidatePaint();
		break;
	case Drag::None:
		break;
	}
	Panel::mouseMoveEvent(event);
}

bool TimelineView::scrollContent(Vec2 delta) {
	TimelineCamera camera = camera_;
	camera.offset += static_cast<double>(delta.x) / camera.scale;
	setCamera(camera);
	scrollRest_ += delta.y / trackHeight_;
	const float whole = std::trunc(scrollRest_);
	if(whole != 0.0f) {
		scrollRest_ -= whole;
		const long first = std::max(static_cast<long>(firstRow_) + static_cast<long>(whole), 0L);
		firstRow_ = static_cast<std::size_t>(first);
		invalidate();
	}
	return true;
}

void TimelineView::keyEvent(const KeyEvent& event) {
	if(event.action != Action::Release) {
		const double frame = 1.0 / model_.fps;
		const double step = event.mods.shift() ? 1.0 : frame;
		const bool alt = event.mods.alt();
		const std::size_t track = keyRow_ < rows_.size() ? rows_[keyRow_] : none;
		if((event.key == Key::Left || event.key == Key::Right) && alt) {
			moveSelection(event.key == Key::Left ? -step : step, 0);
		} else if(event.key == Key::Left || event.key == Key::Right) {
			const double moved = time_ + (event.key == Key::Left ? -step : step);
			scrubTo(std::round(moved * model_.fps) / model_.fps);
		} else if((event.key == Key::Up || event.key == Key::Down) && alt) {
			moveSelection(0.0, event.key == Key::Up ? -1 : 1);
		} else if(event.key == Key::Up && keyRow_ > 0) {
			revealRow(--keyRow_);
		} else if(event.key == Key::Down && keyRow_ + 1 < rows_.size()) {
			revealRow(++keyRow_);
		} else if((event.key == Key::PageUp || event.key == Key::PageDown) && track != none) {
			// The track's items in time order; the one after (or before) the first selected there
			const TimelineTrack& row = model_.tracks[track];
			std::vector<TimelineItem> items;
			for(std::size_t k = 0; k < row.keyframes.size(); ++k) items.push_back({track, k, false});
			for(std::size_t c = 0; c < row.clips.size(); ++c) items.push_back({track, c, true});
			std::stable_sort(items.begin(), items.end(),
			                 [&](const TimelineItem& a, const TimelineItem& b) { return itemTime(a) < itemTime(b); });
			if(!items.empty() && !row.locked) {
				const auto current = std::find_if(items.begin(), items.end(),
				                                  [&](const TimelineItem& item) { return isSelected(item); });
				std::size_t at = 0;
				if(current != items.end()) {
					const auto index = static_cast<std::size_t>(current - items.begin());
					at = event.key == Key::PageDown ? (index + 1) % items.size()
					                                : (index + items.size() - 1) % items.size();
				} else if(event.key == Key::PageUp) {
					at = items.size() - 1;
				}
				select({items[at]});
				// Bring it on screen
				const float x = camera_.toX(itemTime(items[at]));
				if(x < 0.0f || x > resolvedSize().x - lanesLeft()) {
					TimelineCamera camera = camera_;
					camera.offset =
					    itemTime(items[at]) - static_cast<double>(resolvedSize().x - lanesLeft()) * 0.25 / camera.scale;
					setCamera(camera);
				}
			}
		} else if(event.key == Key::Home) {
			scrubTo(model_.playStart);
		} else if(event.key == Key::End) {
			scrubTo(playEndOf(model_));
		} else if(event.key == Key::Comma || event.key == Key::Period) {
			// The keyboard's track's keyframes and clip edges, and the markers
			std::vector<double> stops;
			for(const TimelineMarker& marker : model_.markers) stops.push_back(marker.time);
			if(track != none) {
				for(const TimelineKeyframe& keyframe : model_.tracks[track].keyframes) stops.push_back(keyframe.time);
				for(const TimelineClip& clip : model_.tracks[track].clips) {
					stops.push_back(clip.start);
					stops.push_back(clip.start + clip.duration);
				}
			}
			const double half = frame * 0.5;
			std::optional<double> best;
			for(const double stop : stops) {
				if(event.key == Key::Period ? stop > time_ + half && (!best || stop < *best)
				                            : stop < time_ - half && (!best || stop > *best))
					best = stop;
			}
			if(best) scrubTo(*best);
		} else if(event.key == Key::Enter && track != none) {
			toggleExpanded(track);
		} else if(event.key == Key::M && track != none) {
			toggleFlag(track, false);
		} else if(event.key == Key::L && track != none) {
			toggleFlag(track, true);
		} else if(event.key == Key::Escape) {
			select({});
		} else if(event.key == Key::Equal || event.key == Key::Minus) {
			TimelineCamera camera = camera_;
			camera.zoomAt((resolvedSize().x - lanesLeft()) * 0.5f,
			              event.key == Key::Equal ? camera.scale * keyZoom : camera.scale / keyZoom);
			setCamera(camera);
		}
		describe();
		invalidatePaint();
	}
	Panel::keyEvent(event);
}

void TimelineView::describe() {
	std::string text = "time " + formatTime(time_, model_.fps, format_);
	if(keyRow_ < rows_.size()) {
		const std::size_t index = rows_[keyRow_];
		const TimelineTrack& track = model_.tracks[index];
		text += ", track " + (track.label.empty() ? track.id : track.label);
		if(isGroup(model_.tracks, index)) text += track.expanded ? ", expanded" : ", collapsed";
		if(track.muted) text += ", muted";
		if(track.locked) text += ", locked";
	}
	if(selectedItems_.size() == 1) {
		const TimelineItem& item = selectedItems_.front();
		const TimelineTrack& track = model_.tracks[item.track];
		if(item.clip) {
			const TimelineClip& clip = track.clips[item.index];
			text += ", clip " + (clip.label.empty() ? clip.id : clip.label) + " from " +
			        formatTime(clip.start, model_.fps, format_) + " to " +
			        formatTime(clip.start + clip.duration, model_.fps, format_);
		} else {
			text += ", keyframe at " + formatTime(track.keyframes[item.index].time, model_.fps, format_);
		}
	} else if(!selectedItems_.empty()) {
		text += ", " + std::to_string(selectedItems_.size()) + " selected";
	}
	setAccessibleDescription(std::move(text));
}

void TimelineView::focusEvent(bool focused) {
	invalidatePaint();
	Panel::focusEvent(focused);
}

void TimelineView::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& f = frame();
	const Vec2 size = resolvedSize();
	const float lanes = lanesLeft();
	const float laneLeft = f.left + lanes;
	const Font* text = font();
	const auto xOf = [&](double time) { return laneLeft + camera_.toX(time); };

	// The ruler and the headers' column
	painter.fillRect({f.left, f.top, f.right, f.top + rulerHeight}, headerColor_);
	painter.fillRect({f.left, f.top + rulerHeight, laneLeft, f.bottom}, Color::mix(headerColor_, color(), 0.35f));
	const double playEnd = playEndOf(model_);
	if(playEnd > model_.playStart) {
		const float start = std::max(xOf(model_.playStart), laneLeft), end = std::min(xOf(playEnd), f.right);
		if(end > start)
			painter.fillRect({start, f.top + rulerHeight - 6.0f, end, f.top + rulerHeight}, accent_.withAlpha(0.35f));
	}

	// The ticks, labelled and small
	const RulerTicks ticks = rulerTicks(camera_.scale, model_.fps, format_);
	const double first = std::max(camera_.offset, 0.0), last = camera_.toTime(size.x - lanes);
	const Color tick = Color::mix(headerColor_, textColor_, 0.45f);
	if(ticks.minor > 0.0) {
		const double from = std::ceil(first / ticks.minor - 1e-9);
		for(std::size_t i = 0; i < tickLimit; ++i) {
			const double at = (from + static_cast<double>(i)) * ticks.minor;
			if(at > last) break;
			const float x = xOf(at);
			painter.fillRect({x, f.top + rulerHeight - 5.0f, x + 1.0f, f.top + rulerHeight}, tick);
		}
	}
	const double from = std::ceil(first / ticks.major - 1e-9);
	for(std::size_t i = 0; i < tickLimit; ++i) {
		const double at = (from + static_cast<double>(i)) * ticks.major;
		if(at > last) break;
		const float x = xOf(at);
		painter.fillRect({x, f.top + rulerHeight - 12.0f, x + 1.0f, f.top + rulerHeight}, tick);
		if(text)
			painter.drawText(*text, codepointsOf(formatTime(at, model_.fps, format_)),
			                 {x + 3.0f, f.top + text->ascent() + 2.0f}, textColor_);
	}

	// The rows in view
	const Color laneLine = Color::mix(color(), textColor_, 0.08f);
	const auto& children = this->children();
	for(std::size_t row = firstRow_; row < firstRow_ + visibleRows_ && row < rows_.size(); ++row) {
		const std::size_t index = rows_[row];
		const TimelineTrack& track = model_.tracks[index];
		const float top = f.top + rulerHeight + static_cast<float>(row - firstRow_) * trackHeight_;
		const float bottom = top + trackHeight_, middle = top + trackHeight_ * 0.5f;
		if(row % 2 == 1) painter.fillRect({laneLeft, top, f.right, bottom}, Color::mix(color(), textColor_, 0.03f));
		painter.fillRect({f.left, bottom - 1.0f, f.right, bottom}, laneLine);
		if(hasFocus() && row == keyRow_) painter.fillRect({f.left, top, f.left + 3.0f, bottom}, accent_);

		// The header: chevron, label (unless a child is the header) and the mute and lock buttons
		const float indent = f.left + static_cast<float>(track.depth) * indentStep;
		if(isGroup(model_.tracks, index)) {
			VectorPath chevron;
			const float cx = indent - f.left + chevronWidth * 0.5f, cy = middle - f.top;
			if(track.expanded) {
				chevron.moveTo({cx - 4.0f, cy - 2.0f});
				chevron.lineTo({cx + 4.0f, cy - 2.0f});
				chevron.lineTo({cx, cy + 3.0f});
			} else {
				chevron.moveTo({cx - 2.0f, cy - 4.0f});
				chevron.lineTo({cx + 3.0f, cy});
				chevron.lineTo({cx - 2.0f, cy + 4.0f});
			}
			chevron.close();
			std::vector<MeshVertex> quads;
			appendFill(chevron.flatten({}, 0.25f), FillRule::nonZero, quads);
			painter.fillMesh(quads, {f.left, f.top}, textColor_);
		}
		const bool hasHeader = index < children.size() && !children[index]->isOverlay();
		if(text && !hasHeader)
			painter.drawText(*text, codepointsOf(track.label.empty() ? track.id : track.label),
			                 {indent + chevronWidth, middle - text->lineHeight() * 0.5f + text->ascent()},
			                 track.muted ? textColor_.withAlpha(0.5f) : textColor_);
		const float lock = laneLeft - 4.0f - buttonSize, mute = lock - 2.0f - buttonSize;
		for(const bool isLock : {false, true}) {
			const float left = isLock ? lock : mute;
			if(left < f.left) continue;
			const bool on = isLock ? track.locked : track.muted;
			const Rect box{left, middle - buttonSize * 0.5f, left + buttonSize, middle + buttonSize * 0.5f};
			painter.fillRoundedRect(box, CornerRadii::all(3.0f),
			                        on ? accent_ : Color::mix(headerColor_, textColor_, 0.12f));
			if(text) {
				const std::u32string letter = codepointsOf(isLock ? "L" : "M");
				painter.drawText(*text, letter,
				                 {box.left + (buttonSize - text->measure(letter)) * 0.5f,
				                  middle - text->lineHeight() * 0.5f + text->ascent()},
				                 textColor_);
			}
		}

		// The clips, then the keyframes over them
		const float fade = track.muted ? 0.4f : 1.0f;
		for(std::size_t c = 0; c < track.clips.size(); ++c) {
			const TimelineItem item{index, c, true};
			const bool selected = isSelected(item);
			if(selected && drag_ == Drag::Items) continue; // drawn where it is going, over everything
			const TimelineClip& clip = track.clips[c];
			double start = clip.start, end = clip.start + clip.duration;
			if((drag_ == Drag::Start || drag_ == Drag::End) && item == pressedItem_)
				(drag_ == Drag::Start ? start : end) = trimTime_;
			const float x0 = std::max(xOf(start), laneLeft), x1 = std::min(xOf(end), f.right);
			if(x1 < laneLeft || x0 > f.right || x1 <= x0) continue;
			const Rect box{x0, top + 3.0f, x1, bottom - 3.0f};
			painter.fillRoundedRect(box, CornerRadii::all(4.0f), clip.color.value_or(clipColor_).withAlpha(fade));
			if(selected) painter.strokeRoundedRect(box, CornerRadii::all(4.0f), 2.0f, accent_);
			if(text) {
				const std::u32string label = codepointsOf(clip.label.empty() ? clip.id : clip.label);
				if(text->measure(label) + 12.0f <= x1 - x0)
					painter.drawText(*text, label, {x0 + 6.0f, middle - text->lineHeight() * 0.5f + text->ascent()},
					                 textColor_.withAlpha(fade));
			}
		}
		std::vector<MeshVertex> diamonds, chosen;
		for(std::size_t k = 0; k < track.keyframes.size(); ++k) {
			const TimelineKeyframe& keyframe = track.keyframes[k];
			const bool selected = isSelected({index, k, false});
			const double at = keyframe.time + (selected && drag_ == Drag::Items ? dragDelta_ : 0.0);
			const float x = xOf(at);
			if(x < laneLeft - keyframeRadius || x > f.right + keyframeRadius) continue;
			const Color fill = (selected ? accent_ : Color::mix(textColor_, accent_, 0.25f)).withAlpha(fade);
			const float r = keyframeRadius;
			switch(keyframe.easing) {
			case Easing::Step: // a square: it holds
				painter.fillRect({x - r + 1.0f, middle - r + 1.0f, x + r - 1.0f, middle + r - 1.0f}, fill);
				break;
			case Easing::Bezier: // a circle: it eases
				painter.fillRoundedRect({x - r, middle - r, x + r, middle + r}, CornerRadii::all(r), fill);
				break;
			case Easing::Linear: { // a diamond
				VectorPath diamond;
				const float px0 = x - f.left, py0 = middle - f.top;
				diamond.moveTo({px0, py0 - r});
				diamond.lineTo({px0 + r, py0});
				diamond.lineTo({px0, py0 + r});
				diamond.lineTo({px0 - r, py0});
				diamond.close();
				std::vector<MeshVertex>& mesh = selected ? chosen : diamonds;
				if(mesh.size() / 4 < meshQuadLimit) appendFill(diamond.flatten({}, 0.25f), FillRule::nonZero, mesh);
				break;
			}
			}
		}
		if(!diamonds.empty())
			painter.fillMesh(diamonds, {f.left, f.top}, Color::mix(textColor_, accent_, 0.25f).withAlpha(fade));
		if(!chosen.empty()) painter.fillMesh(chosen, {f.left, f.top}, accent_.withAlpha(fade));
	}

	// Outside the play range, dimmed; the markers on the ruler and across the lanes
	if(playEnd > model_.playStart) {
		const Color dim = color().withAlpha(0.45f);
		const float start = xOf(model_.playStart), end = xOf(playEnd);
		if(start > laneLeft) painter.fillRect({laneLeft, f.top + rulerHeight, std::min(start, f.right), f.bottom}, dim);
		if(end < f.right) painter.fillRect({std::max(end, laneLeft), f.top + rulerHeight, f.right, f.bottom}, dim);
	}
	const Color mark = Color::mix(accent_, textColor_, 0.5f);
	for(const TimelineMarker& marker : model_.markers) {
		const float x = xOf(marker.time);
		if(x < laneLeft || x > f.right) continue;
		painter.fillRect({x, f.top + rulerHeight, x + 1.0f, f.bottom}, mark.withAlpha(0.35f));
		painter.fillRect({x - 3.0f, f.top + rulerHeight - 8.0f, x + 3.0f, f.top + rulerHeight}, mark);
		if(text && !marker.label.empty())
			painter.drawText(*text, codepointsOf(marker.label), {x + 5.0f, f.top + rulerHeight - 2.0f}, mark);
	}
}

void TimelineView::paintOver(Painter& painter) const {
	const Rect& f = frame();
	const float laneLeft = f.left + lanesLeft();
	const auto xOf = [&](double time) { return laneLeft + camera_.toX(time); };

	// The clips being dragged, where they are going
	if(drag_ == Drag::Items) {
		for(const TimelineItem& item : selectedItems_) {
			if(!item.clip || model_.tracks[item.track].locked) continue;
			std::size_t row = rowOf_[item.track];
			const long to = static_cast<long>(row) + dragRows_;
			if(dragRows_ != 0 && to >= 0 && static_cast<std::size_t>(to) < rows_.size() &&
			   !model_.tracks[rows_[static_cast<std::size_t>(to)]].locked)
				row = static_cast<std::size_t>(to);
			if(row < firstRow_ || row >= firstRow_ + visibleRows_) continue;
			const TimelineClip& clip = model_.tracks[item.track].clips[item.index];
			const double start = std::max(clip.start + dragDelta_, 0.0);
			const float top = f.top + rulerHeight + static_cast<float>(row - firstRow_) * trackHeight_;
			const float x0 = std::max(xOf(start), laneLeft), x1 = std::min(xOf(start + clip.duration), f.right);
			if(x1 <= x0) continue;
			const Rect box{x0, top + 3.0f, x1, top + trackHeight_ - 3.0f};
			painter.fillRoundedRect(box, CornerRadii::all(4.0f), clip.color.value_or(clipColor_).withAlpha(0.85f));
			painter.strokeRoundedRect(box, CornerRadii::all(4.0f), 2.0f, accent_);
		}
	}

	if(drag_ == Drag::Marquee) {
		const Rect area{f.left + std::min(pressed_.x, pointer_.x), f.top + std::min(pressed_.y, pointer_.y),
		                f.left + std::max(pressed_.x, pointer_.x), f.top + std::max(pressed_.y, pointer_.y)};
		painter.fillRect(area, accent_.withAlpha(0.15f));
		painter.strokeRoundedRect(area, {}, 1.0f, accent_);
	}

	// The playhead, over the headers' children too
	const float x = xOf(time_);
	if(x >= laneLeft && x <= f.right) {
		painter.fillRect({x - 0.5f, f.top, x + 1.5f, f.bottom}, accent_);
		painter.fillRoundedRect({x - 5.0f, f.top + 2.0f, x + 6.0f, f.top + 12.0f}, CornerRadii::all(2.0f), accent_);
	}
	Panel::paintOver(painter);
}

} // namespace affresco
