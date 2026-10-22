/// @file
/// @brief TimelineView's keys: each is worked out into a command from what the view holds, then done, so the owner,
/// told of a change, may hand the view a new model without the key's own reading of the old one going stale.

#include "timeline/TimelineShared.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace affresco {

using timeline::keyZoom;
using timeline::none;
using timeline::playEndOf;
using timeline::timeOf;

namespace {

/// @brief What a key does.
enum class KeyAction { None, Move, Scrub, Row, Pick, Expand, Flag, Clear, Zoom };

/// @brief What a key does, worked out before any of it is done.
struct KeyCommand {
	KeyAction action = KeyAction::None; ///< What.
	double time = 0.0;                  ///< Move: how far; Scrub: where to; Zoom: the new scale.
	long rows = 0;                      ///< Move: how many rows.
	std::size_t row = 0;                ///< Row: the keyboard's new row.
	TimelineItem item{};                ///< Pick: the item to select.
	bool lock = false;                  ///< Flag: lock rather than mute.
};

/// @brief What the keys read of the view.
struct KeyState {
	const TimelineModel& model;           ///< The timeline.
	std::span<const TimelineItem> chosen; ///< The selected items, sorted.
	double time;                          ///< The playhead.
	double scale;                         ///< The camera's scale.
	std::size_t keyRow;                   ///< The keyboard's row.
	std::size_t rowCount;                 ///< How many rows there are.
	std::size_t track;                    ///< The keyboard's track, or none.
};

/// @brief The arrows: Alt moves the selection, else Left and Right step the playhead a frame (a second with Shift),
/// Up and Down the keyboard's row.
/// @param state The view.
/// @param event The key.
/// @return The command.
KeyCommand stepKey(const KeyState& state, const KeyEvent& event) {
	const double fps = state.model.fps;
	const double step = event.mods.shift() ? 1.0 : 1.0 / fps;
	const bool back = event.key == Key::Left || event.key == Key::Up;
	switch(event.key) {
	case Key::Left:
	case Key::Right:
		if(event.mods.alt()) return {.action = KeyAction::Move, .time = back ? -step : step};
		return {.action = KeyAction::Scrub, .time = std::round((state.time + (back ? -step : step)) * fps) / fps};
	case Key::Up:
	case Key::Down:
		if(event.mods.alt()) return {.action = KeyAction::Move, .rows = back ? -1 : 1};
		if(back ? state.keyRow > 0 : state.keyRow + 1 < state.rowCount)
			return {.action = KeyAction::Row, .row = back ? state.keyRow - 1 : state.keyRow + 1};
		return {};
	default:
		return {};
	}
}

/// @brief The item after (or before) the first selected one on the keyboard's track, in time order; the first (or
/// last) when none is selected there.
/// @param state The view.
/// @param forward Whether after.
/// @return It, or std::nullopt if the track is locked or empty.
std::optional<TimelineItem> cycledItem(const KeyState& state, bool forward) {
	const TimelineTrack& row = state.model.tracks[state.track];
	if(row.locked) return std::nullopt;
	std::vector<TimelineItem> items;
	for(std::size_t k = 0; k < row.keyframes.size(); ++k) items.push_back({state.track, k, false});
	for(std::size_t c = 0; c < row.clips.size(); ++c) items.push_back({state.track, c, true});
	if(items.empty()) return std::nullopt;
	std::ranges::stable_sort(items, [&](const TimelineItem& a, const TimelineItem& b) {
		return timeOf(state.model, a) < timeOf(state.model, b);
	});
	const auto current = std::ranges::find_if(
	    items, [&](const TimelineItem& item) { return std::ranges::binary_search(state.chosen, item); });
	if(current == items.end()) return forward ? items.front() : items.back();
	const auto index = static_cast<std::size_t>(current - items.begin());
	return items[forward ? (index + 1) % items.size() : (index + items.size() - 1) % items.size()];
}

/// @brief The nearest stop after (or before) the playhead, by more than half a frame: the markers, and the keyboard's
/// track's keyframes and clip edges.
/// @param state The view.
/// @param forward Whether after.
/// @return It, or std::nullopt.
std::optional<double> nearestStop(const KeyState& state, bool forward) {
	std::vector<double> stops;
	for(const TimelineMarker& marker : state.model.markers) stops.push_back(marker.time);
	if(state.track != none) {
		for(const TimelineKeyframe& keyframe : state.model.tracks[state.track].keyframes)
			stops.push_back(keyframe.time);
		for(const TimelineClip& clip : state.model.tracks[state.track].clips) {
			stops.push_back(clip.start);
			stops.push_back(clip.start + clip.duration);
		}
	}
	const double half = 1.0 / state.model.fps * 0.5;
	std::optional<double> best;
	for(const double stop : stops) {
		if(forward ? stop > state.time + half && (!best || stop < *best)
		           : stop < state.time - half && (!best || stop > *best))
			best = stop;
	}
	return best;
}

/// @brief The jumps: Page Up and Down pick the keyboard's track's items in turn, Home and End go to the play range's
/// ends, comma and period to the nearest stop.
/// @param state The view.
/// @param event The key.
/// @return The command.
KeyCommand jumpKey(const KeyState& state, const KeyEvent& event) {
	switch(event.key) {
	case Key::PageUp:
	case Key::PageDown:
		if(state.track == none) return {};
		if(const auto item = cycledItem(state, event.key == Key::PageDown))
			return {.action = KeyAction::Pick, .item = *item};
		return {};
	case Key::Home:
		return {.action = KeyAction::Scrub, .time = state.model.playStart};
	case Key::End:
		return {.action = KeyAction::Scrub, .time = playEndOf(state.model)};
	case Key::Comma:
	case Key::Period:
		if(const auto stop = nearestStop(state, event.key == Key::Period))
			return {.action = KeyAction::Scrub, .time = *stop};
		return {};
	default:
		return {};
	}
}

/// @brief The actions: Enter opens or closes the keyboard's group, M mutes and L locks its track, Escape clears the
/// selection, + and - zoom.
/// @param state The view.
/// @param event The key.
/// @return The command.
KeyCommand actionKey(const KeyState& state, const KeyEvent& event) {
	const bool onTrack = state.track != none;
	switch(event.key) {
	case Key::Enter:
		return onTrack ? KeyCommand{.action = KeyAction::Expand} : KeyCommand{};
	case Key::M:
	case Key::L:
		return onTrack ? KeyCommand{.action = KeyAction::Flag, .lock = event.key == Key::L} : KeyCommand{};
	case Key::Escape:
		return {.action = KeyAction::Clear};
	case Key::Equal:
		return {.action = KeyAction::Zoom, .time = state.scale * keyZoom};
	case Key::Minus:
		return {.action = KeyAction::Zoom, .time = state.scale / keyZoom};
	default:
		return {};
	}
}

/// @brief What a key does.
/// @param state The view.
/// @param event The key.
/// @return The command.
KeyCommand keyCommand(const KeyState& state, const KeyEvent& event) {
	KeyCommand command = stepKey(state, event);
	if(command.action == KeyAction::None) command = jumpKey(state, event);
	if(command.action == KeyAction::None) command = actionKey(state, event);
	return command;
}

/// @brief The camera that brings a time on screen, a quarter of the way in, if it is off screen.
/// @param camera The camera.
/// @param time The time.
/// @param width The lanes' width.
/// @return The camera, the same one if the time is on screen.
TimelineCamera revealing(TimelineCamera camera, double time, float width) {
	const float x = camera.toX(time);
	if(x < 0.0f || x > width) camera.offset = time - static_cast<double>(width) * 0.25 / camera.scale;
	return camera;
}

} // namespace

void TimelineView::keyEvent(const KeyEvent& event) {
	if(event.action != Action::Release) {
		const std::size_t track = keyRow_ < rows_.size() ? rows_[keyRow_] : none;
		const KeyCommand command =
		    keyCommand({model_, selectedItems_, time_, camera_.scale, keyRow_, rows_.size(), track}, event);
		switch(command.action) {
		case KeyAction::None:
			break;
		case KeyAction::Move:
			moveSelection(command.time, command.rows);
			break;
		case KeyAction::Scrub:
			scrubTo(command.time);
			break;
		case KeyAction::Row:
			keyRow_ = command.row;
			revealRow(keyRow_);
			break;
		case KeyAction::Pick: // the camera is read after select(), since the owner may answer with a new one
			select({command.item});
			setCamera(revealing(camera_, itemTime(command.item), resolvedSize().x - lanesLeft()));
			break;
		case KeyAction::Expand:
			toggleExpanded(track);
			break;
		case KeyAction::Flag:
			toggleFlag(track, command.lock);
			break;
		case KeyAction::Clear:
			select({});
			break;
		case KeyAction::Zoom: {
			TimelineCamera camera = camera_;
			camera.zoomAt((resolvedSize().x - lanesLeft()) * 0.5f, command.time);
			setCamera(camera);
			break;
		}
		}
		describe();
		invalidatePaint();
	}
	Panel::keyEvent(event);
}

} // namespace affresco
