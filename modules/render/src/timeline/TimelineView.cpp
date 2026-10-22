/// @file
/// @brief TimelineView: its model and look, the rows (the tracks open groups show, and those in view), the hit tests
/// and snapping's targets, and what it says to a screen reader. The arithmetic is in TimelineMath.cpp, the input in
/// TimelineInput.cpp and the drawing in TimelinePaint.cpp.

#include "timeline/TimelineShared.hpp"

#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_map>

namespace affresco {

using timeline::buttonSize;
using timeline::chevronWidth;
using timeline::edgeGrab;
using timeline::indentStep;
using timeline::isGroup;
using timeline::keyframeRadius;
using timeline::nearestTarget;
using timeline::none;
using timeline::playEndOf;
using timeline::rulerHeight;
using timeline::snapReach;
using timeline::timeOf;

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

double TimelineView::itemTime(const TimelineItem& item) const noexcept { return timeOf(model_, item); }

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

} // namespace affresco
