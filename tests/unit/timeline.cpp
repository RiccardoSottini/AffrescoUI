/// @file
/// @brief Unit tests for the timeline: time-to-pixel mapping and the ruler's ticks, snapping, easing and interpolation,
/// TimelineView's scrubbing, keyframe moves and clip trims, and the format's `timeline` node and keyframeValue().

#include "Expression.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/TimelineView.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::bezierEase;
using affresco::Easing;
using affresco::formatTime;
using affresco::json;
using affresco::Key;
using affresco::keyframeValueAt;
using affresco::Mods;
using affresco::MouseButton;
using affresco::px;
using affresco::rulerTicks;
using affresco::snapTime;
using affresco::TimeFormat;
using affresco::TimelineCamera;
using affresco::TimelineEvent;
using affresco::TimelineKeyframe;
using affresco::TimelineModel;
using affresco::TimelineView;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Three tracks: pos (keyframes at 0 and 1), opacity (a step keyframe at 0.5) and video (a clip from 1 to 3),
/// with a marker at 2 and 30 frames a second.
/// @return The model.
TimelineModel threeTracks() {
	TimelineModel model;
	model.duration = 5.0;
	model.fps = 30.0;
	model.tracks.push_back({"pos", "Position", 0, true, false, false, {{0.0, 0.0}, {1.0, 100.0}}, {}});
	model.tracks.push_back({"opacity", "Opacity", 0, true, false, false, {{0.5, 1.0, Easing::Step}}, {}});
	model.tracks.push_back({"video", "Video", 0, true, false, false, {}, {{"c1", "Clip", 1.0, 2.0, std::nullopt}}});
	model.markers.push_back({2.0, "beat"});
	return model;
}

/// @brief A TimelineView filling a 600 by 300 window, catching its edits.
struct TimelineFixture {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< 600 by 300.
	TimelineView* view = nullptr;             ///< The timeline.
	std::vector<TimelineEvent> events;        ///< Every onEdit, oldest first.

	/// @brief Opens the window with the three tracks.
	TimelineFixture() : window(createHeadlessWindow(600, 300, clock)) {
		view = &window->root().add<TimelineView>();
		view->setSize(px(600), px(300));
		view->setModel(threeTracks());
		view->onEdit.connect([this](const TimelineEvent& event) { events.push_back(event); });
		window->render();
	}

	/// @brief Where a time is on a track's row.
	/// @param time In seconds.
	/// @param track The track's index.
	/// @return In window units.
	[[nodiscard]] Vec2 at(double time, std::size_t track) const { return {view->timeX(time), view->trackY(track)}; }

	/// @brief Drags with the left button.
	/// @param from Where it is pressed.
	/// @param to Where it is released.
	/// @param mods The modifier keys held.
	void drag(Vec2 from, Vec2 to, Mods mods = {}) const {
		window->routeMouseButton({MouseButton::Left, Action::Press, mods, from});
		window->routeMouseMove({(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f});
		window->routeMouseMove(to);
		window->routeMouseButton({MouseButton::Left, Action::Release, mods, to});
		window->render();
	}

	/// @brief Presses a key on the timeline.
	/// @param key The key.
	/// @param mods The modifier keys held.
	void press(Key key, Mods mods = {}) const {
		window->routeKey({key, 0, Action::Press, mods});
		window->render();
	}
};

/// @brief Evaluates an expression with no state.
/// @param text The `{{...}}` text.
/// @return The result.
json evaluate(const std::string& text) {
	static const json state = json::object(), window = {{"width", 800}, {"height", 600}};
	static const json revision = 1, event = json::object(), globals = json::object();
	static const affresco::format::Scope locals{nullptr, nullptr, json::object(), {}, {}};
	const affresco::format::Context context{state, window, revision, event, locals, globals};
	return affresco::format::evaluate(text, context, "test");
}

/// @brief A screen whose timeline keeps its time and edits in state, through its handlers.
constexpr const char* timelineScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": {
		"time": 0,
		"tracks": [
			{ "id": "pos", "keyframes": [{ "time": 0, "value": 0 }, { "time": 1, "value": 100 }] },
			{ "id": "video", "clips": [{ "id": "c1", "start": 1, "duration": 2 }] }
		],
		"moved": null,
		"clip": null
	},
	"root": { "id": "root", "children": [
		{ "type": "timeline", "id": "timeline", "width": 600, "height": 300, "tracks": "{{state.tracks}}",
		  "time": "{{state.time}}", "duration": 5, "fps": 30, "markers": [2],
		  "onScrub": [{ "set": "state.time", "to": "{{event.time}}" }],
		  "onKeyframeMove": [{ "set": "state.moved", "to": "{{event}}" }],
		  "onClipChange": [{ "set": "state.clip", "to": "{{event}}" }] }
	] }
})json";

} // namespace

TEST_CASE("render: the timeline's camera maps times and points both ways and zooms about a point") {
	TimelineCamera camera;
	camera.offset = 2.0;
	camera.scale = 50.0;
	CHECK(camera.toX(2.0) == doctest::Approx(0.0));
	CHECK(camera.toX(4.0) == doctest::Approx(100.0));
	CHECK(camera.toTime(100.0f) == doctest::Approx(4.0));
	CHECK(camera.toTime(camera.toX(3.25)) == doctest::Approx(3.25));

	const double before = camera.toTime(200.0f);
	camera.zoomAt(200.0f, 400.0);
	CHECK(camera.scale == doctest::Approx(400.0));
	CHECK(camera.toTime(200.0f) == doctest::Approx(before));
	camera.zoomAt(0.0f, 1e9);
	CHECK(camera.scale == TimelineCamera::maximumScale);
	camera.zoomAt(0.0f, 0.0);
	CHECK(camera.scale == TimelineCamera::minimumScale);
	camera.zoomAt(0.0f, std::numeric_limits<double>::quiet_NaN());
	CHECK(camera.scale == TimelineCamera::minimumScale);

	const TimelineFixture fixture; // the lanes start right of the 160 wide headers, at 100 per second
	CHECK(fixture.view->timeX(0.0) == doctest::Approx(160.0));
	CHECK(fixture.view->timeX(1.5) == doctest::Approx(310.0));
	CHECK(fixture.view->trackY(0) == doctest::Approx(28.0 + 14.0));
	CHECK(fixture.view->trackY(2) == doctest::Approx(28.0 + 2.5 * 28.0));
}

TEST_CASE("render: the ruler's ticks follow the zoom, in frames, seconds or timecode") {
	for(const TimeFormat format : {TimeFormat::Frames, TimeFormat::Seconds, TimeFormat::Timecode})
		for(const double scale : {1.0, 7.0, 100.0, 333.0, 2500.0, 10000.0}) {
			CAPTURE(scale);
			const auto ticks = rulerTicks(scale, 30.0, format);
			CHECK(ticks.major * scale >= 80.0 - 1e-6);
			CHECK(ticks.major * scale < 80.0 * 10.0); // not much sparser than needed
			if(ticks.minor > 0.0) CHECK(ticks.minor * scale >= 6.0 - 1e-6);
			if(format != TimeFormat::Seconds && ticks.major < 1.0) // whole frames
				CHECK(std::abs(ticks.major * 30.0 - std::round(ticks.major * 30.0)) < 1e-9);
		}
	CHECK(rulerTicks(100.0, 30.0, TimeFormat::Frames).major == doctest::Approx(1.0));
	CHECK(rulerTicks(10000.0, 30.0, TimeFormat::Frames).major == doctest::Approx(1.0 / 30.0));
	CHECK(rulerTicks(100.0, 30.0, TimeFormat::Seconds).major == doctest::Approx(1.0));
	CHECK(rulerTicks(1000.0, 30.0, TimeFormat::Seconds).major == doctest::Approx(0.1));

	CHECK(formatTime(1.5, 30.0, TimeFormat::Frames) == "45");
	CHECK(formatTime(1.5, 30.0, TimeFormat::Seconds) == "1.5s");
	CHECK(formatTime(2.0, 30.0, TimeFormat::Seconds) == "2s");
	CHECK(formatTime(1.5, 30.0, TimeFormat::Timecode) == "00:00:01:15");
	CHECK(formatTime(3725.0, 24.0, TimeFormat::Timecode) == "01:02:05:00");
	CHECK(formatTime(std::numeric_limits<double>::infinity(), 30.0, TimeFormat::Frames) == "0");
}

TEST_CASE("render: a time snaps to the nearest target within reach, else to a frame") {
	const std::array<double, 2> targets{1.0, 2.0};
	CHECK(snapTime(1.03, 30.0, targets, 0.05) == doctest::Approx(1.0));
	CHECK(snapTime(1.97, 30.0, targets, 0.05) == doctest::Approx(2.0));
	CHECK(snapTime(1.51, 30.0, targets, 0.05) == doctest::Approx(1.5)); // frame 45
	CHECK(snapTime(1.51, 0.0, targets, 0.05) == doctest::Approx(1.51));
	CHECK(snapTime(0.52, 10.0, {}, 0.05) == doctest::Approx(0.5));
}

TEST_CASE("render: keyframes interpolate with step, linear and Bézier easing") {
	CHECK_FALSE(keyframeValueAt({}, 1.0));

	std::vector<TimelineKeyframe> keyframes{{0.0, 0.0}, {2.0, 10.0}};
	CHECK(*keyframeValueAt(keyframes, -1.0) == doctest::Approx(0.0));
	CHECK(*keyframeValueAt(keyframes, 0.5) == doctest::Approx(2.5));
	CHECK(*keyframeValueAt(keyframes, 3.0) == doctest::Approx(10.0));

	keyframes[0].easing = Easing::Step;
	CHECK(*keyframeValueAt(keyframes, 1.99) == doctest::Approx(0.0));
	CHECK(*keyframeValueAt(keyframes, 2.0) == doctest::Approx(10.0));

	keyframes[0].easing = Easing::Bezier; // ease-in-out: slow at the ends, half way at the middle
	CHECK(*keyframeValueAt(keyframes, 1.0) == doctest::Approx(5.0));
	CHECK(*keyframeValueAt(keyframes, 0.2) < 1.0);
	CHECK(*keyframeValueAt(keyframes, 1.8) > 9.0);

	const std::vector<TimelineKeyframe> unordered{{2.0, 10.0}, {0.0, 0.0}};
	CHECK(*keyframeValueAt(unordered, 1.0) == doctest::Approx(5.0));

	CHECK(bezierEase({0.0, 0.0, 1.0, 1.0}, 0.3) == doctest::Approx(0.3));
	CHECK(bezierEase({0.42, 0.0, 0.58, 1.0}, 0.0) == doctest::Approx(0.0));
	CHECK(bezierEase({0.42, 0.0, 0.58, 1.0}, 1.0) == doctest::Approx(1.0));
	CHECK(bezierEase({0.3, -1.0, 0.7, 2.0}, 0.5) == doctest::Approx(0.5));
	CHECK(bezierEase({0.42, 0.0, 0.58, 1.0}, std::numeric_limits<double>::quiet_NaN()) == doctest::Approx(0.0));

	CHECK(evaluate("{{keyframeValue([{ time: 0, value: 0 }, { time: 2, value: 10 }], 0.5)}}") == 2.5);
	CHECK(evaluate(
	          "{{keyframeValue({ keyframes: [{ time: 0, value: 0, easing: 'step' }, { time: 2, value: 10 }] }, 1)}}") ==
	      0);
	CHECK(
	    evaluate("{{keyframeValue({ keyframes: [{ time: 0, value: 0, easing: [0, 0, 1, 1] }, { time: 1, value: 4 }] }, "
		         "0.25)}}")
	        .get<double>() == doctest::Approx(1.0));
	CHECK(evaluate("{{keyframeValue({ id: 'empty' }, 1)}}").is_null());
	CHECK(evaluate("{{keyframeValue([{ time: 'x', value: 1 }], 1)}}").is_null());
}

TEST_CASE("render: the ruler and the keyboard scrub the playhead, snapping to frames and markers") {
	TimelineFixture fixture;
	const float ruler = 14.0f;
	fixture.drag({fixture.view->timeX(0.51), ruler}, {fixture.view->timeX(1.983), ruler});
	REQUIRE_FALSE(fixture.events.empty());
	CHECK(fixture.events.front().kind == TimelineEvent::Kind::Scrub);
	CHECK(fixture.events.front().time == doctest::Approx(0.5)); // to frame 15
	CHECK(fixture.view->time() == doctest::Approx(2.0));        // to the marker
	CHECK(fixture.events.back().time == doctest::Approx(2.0));

	fixture.events.clear();
	fixture.press(Key::Right);
	CHECK(fixture.view->time() == doctest::Approx(2.0 + 1.0 / 30.0));
	fixture.press(Key::Left, Mods{0x0001});
	CHECK(fixture.view->time() == doctest::Approx(1.0 + 1.0 / 30.0));
	fixture.press(Key::Home);
	CHECK(fixture.view->time() == doctest::Approx(0.0));
	CHECK(fixture.events.size() == 3);
	CHECK(fixture.events.back().kind == TimelineEvent::Kind::Scrub);

	fixture.view->setGivenTime(4.0);
	CHECK(fixture.view->time() == doctest::Approx(4.0));
	fixture.view->setTime(99.0); // kept within the duration
	CHECK(fixture.view->time() == doctest::Approx(5.0));
}

TEST_CASE("render: dragging a keyframe moves it, snapping to a marker or a frame; Alt leaves snapping off") {
	TimelineFixture fixture;
	fixture.drag(fixture.at(1.0, 0), fixture.at(2.03, 0)); // 3 window units from the marker
	REQUIRE_FALSE(fixture.events.empty());
	const TimelineEvent& move = fixture.events.back();
	CHECK(move.kind == TimelineEvent::Kind::KeyframeMove);
	CHECK(move.item.track == 0);
	CHECK(move.item.index == 1);
	CHECK(move.from == doctest::Approx(1.0));
	CHECK(move.time == doctest::Approx(2.0));
	CHECK(fixture.view->model().tracks[0].keyframes[1].time == doctest::Approx(2.0)); // shown at once
	CHECK(fixture.view->selection() == std::vector<std::string>{"pos:1"});

	fixture.events.clear();
	fixture.drag(fixture.at(2.0, 0), fixture.at(3.513, 0));
	REQUIRE_FALSE(fixture.events.empty());
	CHECK(fixture.events.back().time == doctest::Approx(3.5)); // frame 105

	fixture.events.clear();
	fixture.drag(fixture.at(3.5, 0), fixture.at(3.7, 0), Mods{0x0004});
	REQUIRE_FALSE(fixture.events.empty());
	CHECK(fixture.events.back().time == doctest::Approx(3.7).epsilon(1e-4));
}

TEST_CASE("render: a clip is trimmed from either edge, moved to another track, and a locked track holds still") {
	TimelineFixture fixture;
	// The end edge, 2 window units inside, to 3.5 seconds
	fixture.drag({fixture.view->timeX(3.0) - 2.0f, fixture.view->trackY(2)}, fixture.at(3.51, 2));
	REQUIRE_FALSE(fixture.events.empty());
	const TimelineEvent trim = fixture.events.back();
	CHECK(trim.kind == TimelineEvent::Kind::ClipChange);
	CHECK(trim.edge == TimelineEvent::Edge::End);
	CHECK(trim.id == "c1");
	CHECK(trim.start == doctest::Approx(1.0));
	CHECK(trim.duration == doctest::Approx(2.5));

	fixture.events.clear();
	fixture.drag({fixture.view->timeX(1.0) + 2.0f, fixture.view->trackY(2)}, fixture.at(0.5, 2));
	REQUIRE_FALSE(fixture.events.empty());
	CHECK(fixture.events.back().edge == TimelineEvent::Edge::Start);
	CHECK(fixture.events.back().start == doctest::Approx(0.5));
	CHECK(fixture.events.back().duration == doctest::Approx(3.0));

	fixture.events.clear();
	fixture.drag(fixture.at(2.0, 2), fixture.at(2.0, 1)); // up a row
	REQUIRE_FALSE(fixture.events.empty());
	CHECK(fixture.events.back().edge == TimelineEvent::Edge::Move);
	CHECK(fixture.events.back().item.track == 2);
	CHECK(fixture.events.back().track == 1);
	CHECK(fixture.view->model().tracks[1].clips.size() == 1);

	fixture.events.clear();
	fixture.press(Key::Up); // the keyboard's track: opacity, which now holds the clip
	fixture.press(Key::L);
	REQUIRE_FALSE(fixture.events.empty());
	CHECK(fixture.events.back().kind == TimelineEvent::Kind::TrackChange);
	CHECK(fixture.events.back().locked);
	fixture.events.clear();
	fixture.drag(fixture.at(2.0, 1), fixture.at(3.0, 1));
	for(const TimelineEvent& event : fixture.events) CHECK(event.kind != TimelineEvent::Kind::ClipChange);
}

TEST_CASE("render: a marquee selects the items it touches; groups close and the rows are bounded") {
	TimelineFixture fixture;
	fixture.drag({fixture.view->timeX(0.3), fixture.view->trackY(0) - 10.0f},
	             {fixture.view->timeX(1.5), fixture.view->trackY(2) + 10.0f});
	CHECK(fixture.view->selection() == std::vector<std::string>{"pos:1", "opacity:0", "c1"});

	TimelineModel model;
	model.tracks.push_back({"group", "", 0, true, false, false, {}, {}});
	model.tracks.push_back({"a", "", 1, true, false, false, {}, {}});
	model.tracks.push_back({"b", "", 0, true, false, false, {}, {}});
	fixture.view->setModel(model);
	fixture.window->render();
	CHECK(fixture.view->rows().size() == 3);
	fixture.press(Key::Down);
	fixture.press(Key::Up);
	fixture.press(Key::Enter);
	CHECK(fixture.view->rows() == std::vector<std::size_t>{0, 2});

	TimelineModel big;
	big.tracks.resize(TimelineView::maximumTracks + 5);
	for(std::size_t i = 0; i < big.tracks.size(); ++i) big.tracks[i].id = "t" + std::to_string(i);
	fixture.view->setModel(std::move(big));
	fixture.window->render();
	CHECK(fixture.view->model().tracks.size() == TimelineView::maximumTracks);
	CHECK(fixture.view->visibleRows() < 20); // only the rows in view
}

TEST_CASE("format: a timeline node scrubs and edits through its handlers, and refuses broken data") {
	const ManualClock clock;
	const auto headless = createHeadlessWindow(600, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(timelineScreen);
	screen.build(*headless);
	headless->render();
	auto* timeline = dynamic_cast<TimelineView*>(screen.find("timeline"));
	REQUIRE(timeline);
	REQUIRE(timeline->model().tracks.size() == 2);

	const auto drag = [&](Vec2 from, Vec2 to) {
		headless->routeMouseButton({MouseButton::Left, Action::Press, {}, from});
		headless->routeMouseMove(to);
		headless->routeMouseButton({MouseButton::Left, Action::Release, {}, to});
		headless->render();
	};
	drag({timeline->timeX(1.0), 14.0f}, {timeline->timeX(1.5), 14.0f});
	CHECK(json::parse(screen.stateJson())["time"] == 1.5);

	drag({timeline->timeX(1.0), timeline->trackY(0)}, {timeline->timeX(2.02), timeline->trackY(0)});
	const json moved = json::parse(screen.stateJson())["moved"];
	CHECK(moved["track"] == "pos");
	CHECK(moved["index"] == 1);
	CHECK(moved["time"] == 2);
	CHECK(moved["from"] == 1);
	CHECK(moved["value"] == 100);

	drag({timeline->timeX(3.0) - 2.0f, timeline->trackY(1)}, {timeline->timeX(4.0), timeline->trackY(1)});
	const json clip = json::parse(screen.stateJson())["clip"];
	CHECK(clip["id"] == "c1");
	CHECK(clip["edge"] == "end");
	CHECK(clip["track"] == "video");
	CHECK(clip["duration"] == 3);

	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#timeline/tracks": [{"label": "no id"}]}})", &error));
	CHECK(error.find("tracks") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#timeline/tracks": [{"id": "a"}, {"id": "a"}]}})", &error));
	CHECK(error.find("tracks") != std::string::npos);
	CHECK_FALSE(
	    screen.applyRevision(R"({"set": {"#timeline/tracks": [{"id": "a", "clips": [{"id": "c"}]}]}})", &error));
	CHECK(error.find("start") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#timeline/fps": 0}})", &error));
	CHECK(error.find("fps") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#timeline/timeFormat": "beats"}})", &error));
	CHECK(error.find("timeFormat") != std::string::npos);
}
