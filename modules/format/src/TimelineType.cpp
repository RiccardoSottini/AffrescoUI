/// @file
/// @brief The "timeline" node type: a TimelineView of tracks, keyframes and clips from state, its playhead, camera
/// and selection, and its edits (scrub, a keyframe moved, a clip moved or trimmed, select, open, mute, lock) as the
/// node's handlers.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/TimelineView.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t maxTotalItems = 100'000; ///< The most keyframes and clips a timeline has in all.
constexpr double maxValue = 1.0e12;            ///< The largest a keyframe's value is, either side of 0.

/// @brief The place of an item of a list, for errors: "path[3]".
/// @param path The list's place.
/// @param index The item's position.
/// @return The place.
std::string itemPlace(const std::string& path, std::size_t index) { return path + "[" + std::to_string(index) + "]"; }

/// @brief Reads a number within a range, keeping its precision.
/// @param value The value.
/// @param at Its place, for errors.
/// @param low The smallest it may be.
/// @param high The largest it may be.
/// @return The number.
/// @throws DocumentError If it isn't a number within the range.
double numberWithin(const json& value, const std::string& at, double low, double high) {
	const double read = number(value, at);
	if(!(read >= low && read <= high))
		fail(at, "expected a number from " + text(json(low)) + " to " + text(json(high)));
	return read;
}

/// @brief Reads a time in seconds, from 0 to the longest a timeline is.
/// @param value The value.
/// @param at Its place, for errors.
/// @return The time.
/// @throws DocumentError If it isn't such a number.
double secondsOf(const json& value, const std::string& at) {
	return numberWithin(value, at, 0.0, TimelineView::maximumDuration);
}

/// @brief Reads a text key of an object, or a default when it is missing.
/// @param object The object.
/// @param key The key.
/// @param path The object's place, for errors.
/// @return The text, or "".
std::string textOr(const json& object, const char* key, const std::string& path) {
	return object.contains(key) ? string(object[key], path + "." + key) : std::string{};
}

/// @brief Reads a flag key of an object, or a default when it is missing or not a flag.
/// @param object The object.
/// @param key The key.
/// @param otherwise The default.
/// @return The flag.
bool flagOr(const json& object, const char* key, bool otherwise) {
	return object.contains(key) && object[key].is_boolean() ? object[key].get<bool>() : otherwise;
}

/// @brief Reads a keyframe's "easing": "step", "linear", "bezier" (CSS's ease-in-out), or [x1, y1, x2, y2].
/// @param value The value.
/// @param at Its place, for errors.
/// @param keyframe The keyframe it sets.
/// @throws DocumentError If it is none of those, or the xs aren't within [0, 1].
void easingOf(const json& value, const std::string& at, TimelineKeyframe& keyframe) {
	if(value.is_array()) {
		if(value.size() != 4) fail(at, "expected [x1, y1, x2, y2]");
		keyframe.easing = Easing::Bezier;
		for(std::size_t i = 0; i < 4; ++i)
			keyframe.handles[i] =
			    numberWithin(value[i], itemPlace(at, i), i % 2 == 0 ? 0.0 : -100.0, i % 2 == 0 ? 1.0 : 100.0);
		return;
	}
	const std::string name = string(value, at);
	if(name == "step") keyframe.easing = Easing::Step;
	else if(name == "linear") keyframe.easing = Easing::Linear;
	else if(name == "bezier") keyframe.easing = Easing::Bezier;
	else fail(at, R"(expected "step", "linear", "bezier" or [x1, y1, x2, y2])");
}

/// @brief Reads a track's "keyframes": { time, value, easing }.
/// @param value The list.
/// @param path Its place, for errors.
/// @return The keyframes.
/// @throws DocumentError If it isn't such a list or has more than 10,000.
std::vector<TimelineKeyframe> keyframesOf(const json& value, const std::string& path) {
	if(!value.is_array()) fail(path, "expected a list of keyframes");
	if(value.size() > TimelineView::maximumItems) fail(path, "too many keyframes (at most 10000)");
	std::vector<TimelineKeyframe> keyframes;
	keyframes.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) {
		const json& entry = value[i];
		const std::string place = itemPlace(path, i);
		if(!entry.is_object() || !entry.contains("time")) fail(place, "expected { time, value, easing }");
		TimelineKeyframe keyframe;
		keyframe.time = secondsOf(entry["time"], place + ".time");
		if(entry.contains("value"))
			keyframe.value = numberWithin(entry["value"], place + ".value", -maxValue, maxValue);
		if(entry.contains("easing")) easingOf(entry["easing"], place + ".easing", keyframe);
		keyframes.push_back(keyframe);
	}
	return keyframes;
}

/// @brief Reads a track's "clips": { id, label, start, duration, color }.
/// @param value The list.
/// @param path Its place, for errors.
/// @param reader Reads colours, through the theme.
/// @return The clips.
/// @throws DocumentError If it isn't such a list or has more than 10,000.
std::vector<TimelineClip> clipsOf(const json& value, const std::string& path, const PropertyReader& reader) {
	if(!value.is_array()) fail(path, "expected a list of clips");
	if(value.size() > TimelineView::maximumItems) fail(path, "too many clips (at most 10000)");
	std::vector<TimelineClip> clips;
	clips.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) {
		const json& entry = value[i];
		const std::string place = itemPlace(path, i);
		if(!entry.is_object() || !entry.contains("id") || !entry.contains("start"))
			fail(place, "expected { id, label, start, duration }");
		TimelineClip clip;
		clip.id = text(entry["id"]);
		clip.label = textOr(entry, "label", place);
		clip.start = secondsOf(entry["start"], place + ".start");
		if(entry.contains("duration"))
			clip.duration = numberWithin(entry["duration"], place + ".duration", 1.0e-6, TimelineView::maximumDuration);
		if(entry.contains("color")) clip.color = reader.color(entry["color"], place + ".color");
		clips.push_back(std::move(clip));
	}
	return clips;
}

/// @brief Reads "tracks" into a flat list, each group followed by its tracks (a group is a track with "tracks").
/// @param value The list.
/// @param path Its place, for errors.
/// @param reader Reads colours, through the theme.
/// @param depth How deep the list is.
/// @param tracks The tracks read so far, which it adds to.
/// @param items The keyframes and clips read so far, which it adds to.
/// @throws DocumentError If it isn't such a list, nests deeper than 16, or has too many tracks or items.
void tracksOf(const json& value, const std::string& path, const PropertyReader& reader, std::size_t depth,
              std::vector<TimelineTrack>& tracks, std::size_t& items) {
	if(!value.is_array()) fail(path, "expected a list of tracks");
	if(depth > TimelineView::maximumDepth) fail(path, "tracks nested too deep (at most 16)");
	for(std::size_t i = 0; i < value.size(); ++i) {
		const json& entry = value[i];
		const std::string place = itemPlace(path, i);
		if(tracks.size() >= TimelineView::maximumTracks) fail(place, "too many tracks (at most 10000)");
		if(!entry.is_object() || !entry.contains("id")) fail(place, "expected { id, label, keyframes, clips, tracks }");
		TimelineTrack track;
		track.id = text(entry["id"]);
		track.label = textOr(entry, "label", place);
		track.depth = depth;
		track.expanded = flagOr(entry, "expanded", true);
		track.muted = flagOr(entry, "muted", false);
		track.locked = flagOr(entry, "locked", false);
		if(entry.contains("keyframes")) track.keyframes = keyframesOf(entry["keyframes"], place + ".keyframes");
		if(entry.contains("clips")) track.clips = clipsOf(entry["clips"], place + ".clips", reader);
		items += track.keyframes.size() + track.clips.size();
		if(items > maxTotalItems) fail(place, "too many keyframes and clips (at most 100000 in all)");
		tracks.push_back(std::move(track));
		if(entry.contains("tracks")) tracksOf(entry["tracks"], place + ".tracks", reader, depth + 1, tracks, items);
	}
}

/// @brief Fails when two of the names are the same; sorting copies is O(n log n) where comparing pairs would be O(n²).
/// @param names Each name and its place, for errors.
/// @param what What they name, for errors.
/// @throws DocumentError If two are the same.
void checkUnique(std::vector<std::pair<std::string_view, std::string>> names, const char* what) {
	std::ranges::sort(names);
	for(std::size_t i = 1; i < names.size(); ++i)
		if(names[i].first == names[i - 1].first)
			fail(names[i].second,
			     std::string("another ") + what + " has the id \"" + std::string(names[i].first) + "\"");
}

/// @brief Reads "markers": times, or { time, label } (at most 1,000).
/// @param value The list.
/// @param path Its place, for errors.
/// @return The markers.
/// @throws DocumentError If it isn't such a list.
std::vector<TimelineMarker> markersOf(const json& value, const std::string& path) {
	if(!value.is_array()) fail(path, "expected a list of markers");
	if(value.size() > TimelineView::maximumMarkers) fail(path, "too many markers (at most 1000)");
	std::vector<TimelineMarker> markers;
	markers.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) {
		const json& entry = value[i];
		const std::string place = itemPlace(path, i);
		if(entry.is_number()) markers.push_back({secondsOf(entry, place), {}});
		else if(entry.is_object() && entry.contains("time"))
			markers.push_back({secondsOf(entry["time"], place + ".time"), textOr(entry, "label", place)});
		else fail(place, "expected a time, or { time, label }");
	}
	return markers;
}

/// @brief Reads "selected": a key, or a list of keys (a clip's id, or a track's id, ":" and a keyframe's index).
/// @param value The value.
/// @return The keys (at most as many items as a timeline keeps).
std::vector<std::string> keysOf(const json& value) {
	std::vector<std::string> keys;
	if(value.is_string() || value.is_number()) keys.push_back(text(value));
	else if(value.is_array())
		for(std::size_t i = 0; i < value.size() && i < maxTotalItems; ++i)
			if(value[i].is_string() || value[i].is_number()) keys.push_back(text(value[i]));
	return keys;
}

/// @brief Shows a timeline node: its tracks, markers, play range, playhead, look, camera and selection.
/// @param panel The panel, a TimelineView.
/// @param values Its values, if given.
/// @param reader Reads colours and fonts, through the theme.
/// @param path The node's place, for errors.
void applyTimeline(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* view = dynamic_cast<TimelineView*>(&panel);
	if(!view) return;
	TimelineModel model;
	if(values.contains("tracks")) {
		std::size_t items = 0;
		tracksOf(values["tracks"], path + ".tracks", reader, 0, model.tracks, items);
		std::vector<std::pair<std::string_view, std::string>> trackIds, clipIds;
		for(std::size_t t = 0; t < model.tracks.size(); ++t) {
			trackIds.emplace_back(model.tracks[t].id, path + ".tracks: \"" + model.tracks[t].id + "\"");
			for(const TimelineClip& clip : model.tracks[t].clips)
				clipIds.emplace_back(clip.id, path + ".tracks: \"" + model.tracks[t].id + "\".clips");
		}
		checkUnique(std::move(trackIds), "track");
		checkUnique(std::move(clipIds), "clip");
	}
	if(values.contains("markers")) model.markers = markersOf(values["markers"], path + ".markers");
	if(values.contains("duration")) model.duration = secondsOf(values["duration"], path + ".duration");
	if(values.contains("fps")) model.fps = numberWithin(values["fps"], path + ".fps", 1.0, TimelineView::maximumFps);
	if(values.contains("playRange")) {
		const json& range = values["playRange"];
		if(!range.is_array() || range.size() != 2) fail(path + ".playRange", "expected [start, end]");
		model.playStart = secondsOf(range[0], path + ".playRange[0]");
		model.playEnd = secondsOf(range[1], path + ".playRange[1]");
	}
	view->setModel(std::move(model));
	if(values.contains("timeFormat")) {
		const std::string format = string(values["timeFormat"], path + ".timeFormat");
		if(format == "frames") view->setTimeFormat(TimeFormat::Frames);
		else if(format == "seconds") view->setTimeFormat(TimeFormat::Seconds);
		else if(format == "timecode") view->setTimeFormat(TimeFormat::Timecode);
		else fail(path + ".timeFormat", R"(expected "frames", "seconds" or "timecode")");
	} else {
		view->setTimeFormat(TimeFormat::Frames);
	}
	view->setSnap(flagOr(values, "snap", true));
	const auto sizeOr = [&](const char* key, double low, double high, float otherwise) {
		return values.contains(key) ? static_cast<float>(numberWithin(values[key], path + "." + key, low, high))
		                            : otherwise;
	};
	view->setSizes(sizeOr("headerWidth", 0.0, 2000.0, 160.0f), sizeOr("trackHeight", 12.0, 400.0, 28.0f));
	const auto colorOr = [&](const char* key, Color otherwise) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : otherwise;
	};
	view->setColors(colorOr("headerColor", Color::hex(0x2b2e36)), colorOr("clipColor", Color::hex(0x4b6584)),
	                colorOr("textColor", Color::hex(0xeceef2)), colorOr("accentColor", colors::accent));
	view->setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	if(values.contains("time")) view->setGivenTime(secondsOf(values["time"], path + ".time"));
	if(values.contains("scale") || values.contains("offset")) {
		TimelineCamera camera;
		if(values.contains("scale"))
			camera.scale = numberWithin(values["scale"], path + ".scale", TimelineCamera::minimumScale,
			                            TimelineCamera::maximumScale);
		if(values.contains("offset"))
			camera.offset = numberWithin(values["offset"], path + ".offset", -TimelineView::maximumDuration,
			                             TimelineView::maximumDuration);
		view->setGivenCamera(camera);
	}
	if(values.contains("selected")) view->setGivenSelection(keysOf(values["selected"]));
}

/// @brief A time for an event: rounded to a millionth of a second, so a drag doesn't give 1.0000000002.
/// @param time In seconds.
/// @return The time.
double eventTime(double time) { return std::round(time * 1.0e6) / 1.0e6; }

/// @brief The name of a clip's dragged part.
/// @param edge The part.
/// @return "move", "start" or "end".
const char* edgeName(TimelineEvent::Edge edge) {
	switch(edge) {
	case TimelineEvent::Edge::Start:
		return "start";
	case TimelineEvent::Edge::End:
		return "end";
	case TimelineEvent::Edge::Move:
		break;
	}
	return "move";
}

/// @brief Runs "onScrub", "onKeyframeMove", "onClipChange", "onSelect", "onExpand" and "onChange" for the user's
/// edits.
/// @param panel The panel, a TimelineView.
/// @param sink Runs a handler.
/// @return The connections.
EventConnections listenToTimelineEvents(Panel& panel, EventSink sink) {
	auto* view = dynamic_cast<TimelineView*>(&panel);
	if(!view) return {};
	// The view outlives the connection: stopListeningToTimelineEvents() disconnects it before the panel goes
	const Connection edit = view->onEdit.connect([view, sink = std::move(sink)](const TimelineEvent& event) {
		const TimelineModel& model = view->model();
		const auto trackId = [&](std::size_t track) {
			return track < model.tracks.size() ? model.tracks[track].id : std::string{};
		};
		switch(event.kind) {
		case TimelineEvent::Kind::Scrub:
			sink("onScrub", json{{"type", "scrub"},
			                     {"time", eventTime(event.time)},
			                     {"frame", static_cast<std::int64_t>(std::llround(event.time * model.fps))}});
			break;
		case TimelineEvent::Kind::KeyframeMove: {
			const std::size_t track = event.item.track;
			json fields{{"type", "keyframeMove"},
			            {"track", trackId(track)},
			            {"index", static_cast<std::int64_t>(event.item.index)},
			            {"time", eventTime(event.time)},
			            {"from", eventTime(event.from)}};
			if(track < model.tracks.size() && event.item.index < model.tracks[track].keyframes.size())
				fields["value"] = model.tracks[track].keyframes[event.item.index].value;
			sink("onKeyframeMove", std::move(fields));
			break;
		}
		case TimelineEvent::Kind::ClipChange:
			sink("onClipChange", json{{"type", "clipChange"},
			                          {"id", event.id},
			                          {"track", trackId(event.track)},
			                          {"fromTrack", trackId(event.item.track)},
			                          {"index", static_cast<std::int64_t>(event.item.index)},
			                          {"start", eventTime(event.start)},
			                          {"duration", eventTime(event.duration)},
			                          {"from", eventTime(event.from)},
			                          {"edge", edgeName(event.edge)}});
			break;
		case TimelineEvent::Kind::Select: {
			json keys = json::array();
			for(const TimelineItem& item : event.items) keys.push_back(view->itemKey(item));
			sink("onSelect", json{{"type", "select"}, {"ids", std::move(keys)}});
			break;
		}
		case TimelineEvent::Kind::Expand:
			sink("onExpand", json{{"type", "expand"}, {"id", trackId(event.track)}, {"expanded", event.expanded}});
			break;
		case TimelineEvent::Kind::TrackChange:
			sink("onChange", json{{"type", "track"},
			                      {"id", trackId(event.track)},
			                      {"muted", event.muted},
			                      {"locked", event.locked}});
			break;
		}
	});
	return {edit, 0, 0, 0};
}

/// @brief Disconnects listenToTimelineEvents().
/// @param panel The panel.
/// @param connections The connections.
void stopListeningToTimelineEvents(Panel& panel, const EventConnections& connections) {
	if(auto* view = dynamic_cast<TimelineView*>(&panel)) view->onEdit.disconnect(connections[0]);
}

} // namespace

void registerTimelineType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {
	    {"tracks", ValueKind::Any},         {"markers", ValueKind::Any},       {"duration", ValueKind::Number},
	    {"fps", ValueKind::Number},         {"playRange", ValueKind::Any},     {"time", ValueKind::Number},
	    {"timeFormat", ValueKind::Text},    {"snap", ValueKind::Boolean},      {"scale", ValueKind::Number},
	    {"offset", ValueKind::Number},      {"selected", ValueKind::Any},      {"headerWidth", ValueKind::Number},
	    {"trackHeight", ValueKind::Number}, {"headerColor", ValueKind::Color}, {"clipColor", ValueKind::Color},
	    {"textColor", ValueKind::Color},    {"accentColor", ValueKind::Color}, {"font", ValueKind::Font}};
	set->apply = applyTimeline;
	set->everyUpdate = true; // colours follow the theme; an unchanged model, playhead, camera or selection is skipped
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const TimelineView*>(&panel) != nullptr; };
	set->events = {listenToTimelineEvents, stopListeningToTimelineEvents};

	registry.add(
	    {"timeline",
		 "Tracks of keyframes and clips under a time ruler, with a playhead; scrubs and edits run its handlers",
		 [] { return std::make_unique<TimelineView>(); }, set, false});
}

} // namespace affresco::format
