/// @file
/// @brief TimelineView's editing and input: selecting, scrubbing, moving and trimming, the headers' buttons, the
/// pointer and the wheel (the keys are in TimelineKeys.cpp). Each event the owner hears is fired after the view's own
/// state is whole, since the owner may hand it a new model from inside the handler.

#include "timeline/TimelineShared.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace affresco {

using timeline::buttonSize;
using timeline::chevronWidth;
using timeline::holds;
using timeline::indentStep;
using timeline::isGroup;
using timeline::rulerHeight;
using timeline::snapReach;

namespace {

/// @brief A clip changing track: where it is and where it goes.
struct Hop {
	std::size_t track; ///< Its track.
	std::size_t index; ///< Its index there.
	std::size_t to;    ///< The track it goes to.
};

/// @brief A move of the selection, gathered before any of it is told, since the owner may change the model in between.
struct Moves {
	std::vector<TimelineEvent> events; ///< What to tell the owner.
	std::vector<Hop> hops;             ///< The clips changing track.
};

/// @brief The rows, as moving a clip up or down needs them.
struct RowMap {
	std::span<const std::size_t> rows;  ///< The tracks in row order.
	std::span<const std::size_t> rowOf; ///< Each track's row.
};

/// @brief Moves a keyframe in time, in the model too so the move shows at once; the owner's next model confirms or
/// undoes it.
/// @param keyframe The keyframe.
/// @param item Where it is.
/// @param delta How far, in seconds.
/// @return The event to tell, or std::nullopt if it stays.
std::optional<TimelineEvent> moveKeyframe(TimelineKeyframe& keyframe, const TimelineItem& item, double delta) {
	const double time = std::max(0.0, keyframe.time + delta);
	if(time == keyframe.time) return std::nullopt;
	TimelineEvent event;
	event.item = item;
	event.kind = TimelineEvent::Kind::KeyframeMove;
	event.from = keyframe.time;
	event.time = time;
	keyframe.time = time;
	return event;
}

/// @brief The track a clip goes to when the selection moves some rows: the one that many rows away, unless there is
/// none or it is locked.
/// @param model The timeline.
/// @param map The rows.
/// @param track The clip's track.
/// @param rowDelta How many rows, up if negative.
/// @return The track.
std::size_t destinationTrack(const TimelineModel& model, RowMap map, std::size_t track, long rowDelta) {
	const long row = static_cast<long>(map.rowOf[track]) + rowDelta;
	if(rowDelta != 0 && row >= 0 && static_cast<std::size_t>(row) < map.rows.size() &&
	   !model.tracks[map.rows[static_cast<std::size_t>(row)]].locked)
		return map.rows[static_cast<std::size_t>(row)];
	return track;
}

/// @brief Moves a clip in time, in the model too, and finds the track it goes to.
/// @param clip The clip.
/// @param item Where it is.
/// @param to The track it goes to.
/// @param delta How far, in seconds.
/// @return The event to tell, or std::nullopt if it stays.
std::optional<TimelineEvent> moveClip(TimelineClip& clip, const TimelineItem& item, std::size_t to, double delta) {
	const double start = std::max(0.0, clip.start + delta);
	if(start == clip.start && to == item.track) return std::nullopt;
	TimelineEvent event;
	event.item = item;
	event.kind = TimelineEvent::Kind::ClipChange;
	event.id = clip.id;
	event.from = clip.start;
	event.track = to;
	event.start = start;
	event.duration = clip.duration;
	event.edge = TimelineEvent::Edge::Move;
	clip.start = start;
	return event;
}

/// @brief Moves the selected items in time and the clips by rows, in the model, and gathers what to tell.
/// @param model The timeline.
/// @param selected The selected items.
/// @param map The rows.
/// @param delta How far in time, in seconds.
/// @param rowDelta How many rows, up if negative.
/// @return The events and the clips changing track; the latter are still where they were.
Moves gatherMoves(TimelineModel& model, std::span<const TimelineItem> selected, RowMap map, double delta,
                  long rowDelta) {
	Moves moves;
	for(const TimelineItem& item : selected) {
		TimelineTrack& track = model.tracks[item.track];
		if(track.locked) continue;
		if(!item.clip) {
			if(auto event = moveKeyframe(track.keyframes[item.index], item, delta))
				moves.events.push_back(std::move(*event));
			continue;
		}
		const std::size_t to = destinationTrack(model, map, item.track, rowDelta);
		if(auto event = moveClip(track.clips[item.index], item, to, delta)) {
			moves.events.push_back(std::move(*event));
			if(to != item.track) moves.hops.push_back({item.track, item.index, to});
		}
	}
	return moves;
}

/// @brief Moves clips to their new tracks, the last of each track first so the indices stay right, while those
/// tracks have room.
/// @param model The timeline.
/// @param hops The clips.
void hopClips(TimelineModel& model, std::vector<Hop> hops) {
	std::ranges::sort(
	    hops, [](const Hop& a, const Hop& b) { return a.track != b.track ? a.track < b.track : a.index > b.index; });
	std::vector<std::pair<std::size_t, TimelineClip>> moved;
	for(const Hop& hop : hops) {
		auto& clips = model.tracks[hop.track].clips;
		moved.emplace_back(hop.to, std::move(clips[hop.index]));
		clips.erase(clips.begin() + static_cast<std::ptrdiff_t>(hop.index));
	}
	for(auto& [to, clip] : moved)
		if(model.tracks[to].clips.size() < TimelineView::maximumItems)
			model.tracks[to].clips.push_back(std::move(clip));
}

/// @brief What a press on a header hits.
enum class HeaderPart { None, Lock, Mute, Chevron };

/// @brief What a press on a header hits: the lock and mute buttons at its right, or a group's chevron.
/// @param x Where, relative to the view.
/// @param lanes Where the lanes start.
/// @param depth The track's depth.
/// @return The part.
HeaderPart headerPart(float x, float lanes, std::size_t depth) {
	const float lock = lanes - 4.0f - buttonSize, mute = lock - 2.0f - buttonSize;
	const float chevron = static_cast<float>(depth) * indentStep;
	if(x >= lock && x < lock + buttonSize) return HeaderPart::Lock;
	if(x >= mute && x < mute + buttonSize) return HeaderPart::Mute;
	if(x >= chevron && x < chevron + chevronWidth) return HeaderPart::Chevron;
	return HeaderPart::None;
}

/// @brief The selection after pressing an item: Shift adds or removes it, else it alone unless it is selected.
/// @param selected The selection, sorted.
/// @param item The item.
/// @param shift Whether Shift is down.
/// @return The selection.
std::vector<TimelineItem> pressedSelection(std::span<const TimelineItem> selected, const TimelineItem& item,
                                           bool shift) {
	std::vector<TimelineItem> items(selected.begin(), selected.end());
	if(!shift) {
		if(!std::ranges::binary_search(selected, item)) items = {item};
		return items;
	}
	const auto found = std::ranges::find(items, item);
	if(found == items.end()) items.push_back(item);
	else items.erase(found);
	return items;
}

/// @brief A time, snapped to the nearest target in reach or to a frame if this drag snaps.
/// @param at The time.
/// @param snapping Whether it snaps.
/// @param fps Frames per second.
/// @param targets The targets.
/// @param scale Window units per second.
/// @return The time.
double snapAt(double at, bool snapping, double fps, std::span<const double> targets, double scale) {
	return snapping ? snapTime(at, fps, targets, snapReach / scale) : at;
}

/// @brief The rows a marquee spans: from the one under its top to the one under its bottom, below the ruler.
/// @param from Where the drag started, relative to the view.
/// @param to Where it ended.
/// @param firstRow The first row in view.
/// @param trackHeight A row's height.
/// @return The first row and the last.
std::pair<std::size_t, std::size_t> marqueeRows(Vec2 from, Vec2 to, std::size_t firstRow, float trackHeight) {
	const float top = std::max(std::min(from.y, to.y), rulerHeight);
	const float bottom = std::max(from.y, to.y);
	return {firstRow + static_cast<std::size_t>((top - rulerHeight) / trackHeight),
	        firstRow + static_cast<std::size_t>(std::max(bottom - rulerHeight, 0.0f) / trackHeight)};
}

/// @brief Where a trimmed edge goes as the pointer moves: at least a frame of the clip stays.
/// @param clip The clip.
/// @param at Where the pointer is, in seconds, snapped.
/// @param start Whether the edge is the start.
/// @param frame A frame's duration.
/// @return The edge's time.
double trimmedEdge(const TimelineClip& clip, double at, bool start, double frame) {
	return start ? std::clamp(at, 0.0, std::max(clip.start + clip.duration - frame, 0.0))
	             : std::clamp(at, clip.start + frame, TimelineView::maximumDuration);
}

/// @brief Whether a press is on the ruler, over the lanes (where it scrubs).
/// @param point The press, relative to the view.
/// @param lanes Where the lanes start.
/// @return true if it is.
bool onRuler(Vec2 point, float lanes) noexcept { return point.y < rulerHeight && point.x >= lanes; }

/// @brief The time of the clip edge a press grabbed, to trim it.
/// @param model The model.
/// @param item The item pressed; on the model.
/// @param edge The part of it pressed.
/// @return The edge's time, or std::nullopt if the press moves the item rather than trimming it.
std::optional<double> grabbedEdge(const TimelineModel& model, const TimelineItem& item, TimelineEvent::Edge edge) {
	if(!item.clip || edge == TimelineEvent::Edge::Move) return std::nullopt;
	const TimelineClip& clip = model.tracks[item.track].clips[item.index];
	return edge == TimelineEvent::Edge::Start ? clip.start : clip.start + clip.duration;
}

} // namespace

void TimelineView::select(std::vector<TimelineItem> items) {
	std::ranges::sort(items);
	items.erase(std::ranges::unique(items).begin(), items.end());
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
	Moves moves = gatherMoves(model_, selectedItems_, {rows_, rowOf_}, delta, rowDelta);
	if(moves.events.empty()) return;
	if(!moves.hops.empty()) {
		hopClips(model_, std::move(moves.hops));
		refreshSelected();
	}
	invalidate();
	for(const TimelineEvent& event : moves.events) onEdit(event);
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
		if(onRuler(point, lanes)) {
			drag_ = Drag::Scrub;
			findSnapTargets(false);
			scrubTo(snapAt(camera_.toTime(point.x - lanes), snapping_, model_.fps, snapTargets_, camera_.scale));
		} else if(point.x < lanes) {
			drag_ = Drag::None;
			if(const std::size_t row = rowAt(point.y); row < rows_.size()) {
				const std::size_t track = rows_[row];
				const HeaderPart part = headerPart(point.x, lanes, model_.tracks[track].depth);
				keyRow_ = row;
				if(part == HeaderPart::Chevron) toggleExpanded(track);
				else if(part != HeaderPart::None) toggleFlag(track, part == HeaderPart::Lock);
			}
		} else if(const auto item = itemAt(point, edge)) {
			select(pressedSelection(selectedItems_, *item, event.mods.shift()));
			drag_ = Drag::None;
			// The owner may answer the selection with a model that no longer holds the item: then nothing is dragged
			if(holds(model_, *item)) {
				keyRow_ = rowOf_[item->track];
				pressedItem_ = *item;
				if(isSelected(*item)) {
					findSnapTargets(true);
					const std::optional<double> trimmed = grabbedEdge(model_, *item, edge);
					drag_ = !trimmed ? Drag::Items : edge == TimelineEvent::Edge::Start ? Drag::Start : Drag::End;
					trimTime_ = trimmed.value_or(trimTime_);
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
		const Drag drag = std::exchange(drag_, Drag::None);
		if(drag == Drag::Items) {
			const double delta = std::exchange(dragDelta_, 0.0);
			const long rows = std::exchange(dragRows_, 0);
			if(delta != 0.0 || rows != 0) moveSelection(delta, rows);
		} else if(drag == Drag::Start || drag == Drag::End) {
			trim(pressedItem_, drag == Drag::Start ? TimelineEvent::Edge::Start : TimelineEvent::Edge::End, trimTime_);
		} else if(drag == Drag::Marquee) {
			const double a = camera_.toTime(pressed_.x - lanes), b = camera_.toTime(point.x - lanes);
			const auto [first, last] = marqueeRows(pressed_, point, firstRow_, trackHeight_);
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
	const double at = camera_.toTime(pointer_.x - lanesLeft());
	switch(drag_) {
	case Drag::Scrub:
		scrubTo(snapAt(at, snapping_, model_.fps, snapTargets_, camera_.scale));
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
			trimTime_ = trimmedEdge(model_.tracks[pressedItem_.track].clips[pressedItem_.index],
			                        snapAt(at, snapping_, model_.fps, snapTargets_, camera_.scale),
			                        drag_ == Drag::Start, 1.0 / model_.fps);
			invalidatePaint();
		}
		break;
	case Drag::Pan: {
		TimelineCamera camera = pressCamera_;
		camera.offset = pressCamera_.offset - static_cast<double>(pointer_.x - pressed_.x) / camera_.scale;
		setCamera(camera);
		const long rows = std::lround((pointer_.y - pressed_.y) / trackHeight_);
		const auto first = static_cast<std::size_t>(std::max(static_cast<long>(pressFirstRow_) - rows, 0L));
		if(first != firstRow_) {
			firstRow_ = first;
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

} // namespace affresco
