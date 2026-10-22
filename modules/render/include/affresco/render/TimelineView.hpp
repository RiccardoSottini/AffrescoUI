#pragma once

/// @file
/// @brief TimelineView: tracks of keyframes and clips under a time ruler, with a playhead, that the pointer or the
/// keyboard scrubs and edits.

#include "affresco/render/Font.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Panel.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief How a keyframe's value goes to the next keyframe's.
enum class Easing {
	Step,   ///< It stays until the next keyframe.
	Linear, ///< In a straight line.
	Bezier, ///< Along a cubic Bézier curve, as CSS's cubic-bezier() (TimelineKeyframe::handles).
};

/// @brief A keyframe: a value at a time.
struct TimelineKeyframe {
	double time = 0.0;                                   ///< When, in seconds.
	double value = 0.0;                                  ///< Its value.
	Easing easing = Easing::Linear;                      ///< How it goes to the next keyframe's value.
	std::array<double, 4> handles{0.42, 0.0, 0.58, 1.0}; ///< Bezier: x1, y1, x2, y2, the xs within [0, 1].

	/// @brief Equal when every member is equal.
	bool operator==(const TimelineKeyframe&) const = default;
};

/// @brief A clip: a span of time on a track.
struct TimelineClip {
	std::string id;             ///< Its name, unique in the timeline.
	std::string label;          ///< What is written on it; empty shows the id.
	double start = 0.0;         ///< When it starts, in seconds.
	double duration = 1.0;      ///< How long it lasts, in seconds.
	std::optional<Color> color; ///< Its fill; none for the timeline's clip colour.

	/// @brief Equal when every member is equal.
	bool operator==(const TimelineClip&) const = default;
};

/// @brief A track, a row of the timeline. Tracks are kept in pre-order: a track is a group of the tracks after it
/// that are deeper.
struct TimelineTrack {
	std::string id;                          ///< Its name, unique in the timeline.
	std::string label;                       ///< Its header's text; empty shows the id.
	std::size_t depth = 0;                   ///< How deep it is: 0 at the top, one more than its group.
	bool expanded = true;                    ///< Whether a group shows its tracks.
	bool muted = false;                      ///< Whether it is muted: drawn faded.
	bool locked = false;                     ///< Whether it is locked: its items can't be picked or moved.
	std::vector<TimelineKeyframe> keyframes; ///< Its keyframes.
	std::vector<TimelineClip> clips;         ///< Its clips.

	/// @brief Equal when every member is equal.
	bool operator==(const TimelineTrack&) const = default;
};

/// @brief A marker: a named time on the ruler.
struct TimelineMarker {
	double time = 0.0; ///< When, in seconds.
	std::string label; ///< Its name.

	/// @brief Equal when every member is equal.
	bool operator==(const TimelineMarker&) const = default;
};

/// @brief The tracks, markers and times of a timeline.
struct TimelineModel {
	std::vector<TimelineTrack> tracks;   ///< The tracks, in pre-order.
	std::vector<TimelineMarker> markers; ///< The markers.
	double duration = 10.0;              ///< Its length, in seconds.
	double fps = 30.0;                   ///< Frames per second: what times snap to.
	double playStart = 0.0;              ///< Where the play range starts, in seconds.
	double playEnd = 0.0;                ///< Where it ends; at playStart or before it, the whole duration.

	/// @brief Equal when every member is equal.
	bool operator==(const TimelineModel&) const = default;
};

/// @brief How the ruler writes times.
enum class TimeFormat {
	Frames,   ///< Frame numbers: 0, 15, 30.
	Seconds,  ///< Seconds: 0s, 0.5s, 1s.
	Timecode, ///< Hours, minutes, seconds and frames: 00:00:01:00.
};

/// @brief A keyframe's or a clip's place: its track and its index in the track's keyframes or clips.
struct TimelineItem {
	std::size_t track = 0; ///< The track's index.
	std::size_t index = 0; ///< The keyframe's or clip's index in it.
	bool clip = false;     ///< Whether it is a clip.

	/// @brief Ordered by track, then keyframes before clips, then index.
	auto operator<=>(const TimelineItem&) const = default;
};

/// @brief Where a timeline is seen from: the time at the lanes' left edge and the zoom.
struct TimelineCamera {
	static constexpr double minimumScale = 1.0;     ///< The smallest zoom, in pixels per second.
	static constexpr double maximumScale = 10000.0; ///< The largest.

	double offset = 0.0;  ///< The time at the left edge of the lanes, in seconds.
	double scale = 100.0; ///< Window units per second.

	/// @brief Turns a time into a point along the lanes.
	/// @param time In seconds.
	/// @return From the lanes' left edge, in window units.
	[[nodiscard]] float toX(double time) const noexcept;
	/// @brief Turns a point along the lanes into a time.
	/// @param x From the lanes' left edge, in window units.
	/// @return In seconds.
	[[nodiscard]] double toTime(float x) const noexcept;
	/// @brief Zooms, keeping the time at a point where it is.
	/// @param x From the lanes' left edge.
	/// @param scaleTo The new zoom; clamped to [minimumScale, maximumScale] (a NaN is ignored).
	void zoomAt(float x, double scaleTo) noexcept;

	/// @brief Equal when every member is equal.
	bool operator==(const TimelineCamera&) const = default;
};

/// @brief The ruler's steps at a zoom: labelled ticks, and the small ones between them.
struct RulerTicks {
	double major = 1.0; ///< Seconds between labelled ticks.
	double minor = 0.0; ///< Seconds between small ticks; 0 for none.
};

/// @brief The ruler's steps at a zoom: whole frames (and their round multiples) when the format counts frames, round
/// seconds otherwise, with labelled ticks at least 80 window units apart and small ones at least 6.
/// @param scale Window units per second.
/// @param fps Frames per second.
/// @param format How times are written.
/// @return The steps.
[[nodiscard]] RulerTicks rulerTicks(double scale, double fps, TimeFormat format);

/// @brief Writes a time as the ruler does.
/// @param time In seconds.
/// @param fps Frames per second.
/// @param format How: "45" (frames), "1.5s" (seconds), "00:00:01:15" (timecode).
/// @return The text.
[[nodiscard]] std::string formatTime(double time, double fps, TimeFormat format);

/// @brief Snaps a time: to the nearest target within a tolerance, else to the nearest frame.
/// @param time In seconds.
/// @param fps Frames per second; 0 or less leaves a time that no target is near as it is.
/// @param targets The times it may snap to (keyframes, clip edges, markers, the playhead).
/// @param tolerance How far a target may be, in seconds.
/// @return The snapped time.
[[nodiscard]] double snapTime(double time, double fps, std::span<const double> targets, double tolerance);

/// @brief The progress of a Bézier easing, as CSS's cubic-bezier(x1, y1, x2, y2).
/// @param handles x1, y1, x2, y2; the xs are clamped to [0, 1].
/// @param progress How far along the time is, in [0, 1].
/// @return How far along the value is (it may leave [0, 1] when a y does).
[[nodiscard]] double bezierEase(const std::array<double, 4>& handles, double progress);

/// @brief The value keyframes give at a time: the first's before it, the last's after it, and in between each
/// keyframe's easing to the next (keyframes out of order are taken in time order).
/// @param keyframes The keyframes.
/// @param time In seconds.
/// @return The value, or std::nullopt for no keyframes.
[[nodiscard]] std::optional<double> keyframeValueAt(std::span<const TimelineKeyframe> keyframes, double time);

/// @brief Something the user did to a timeline. The view doesn't change its owner's model: the owner applies it.
struct TimelineEvent {
	/// @brief What happened.
	enum class Kind {
		Scrub,        ///< The playhead moved (time).
		KeyframeMove, ///< A keyframe moved (item, time, from).
		ClipChange,   ///< A clip moved, maybe to another track, or was trimmed (item, track, start, duration, edge).
		Select,       ///< The selection changed (items).
		Expand,       ///< A group opened or closed (track, expanded).
		TrackChange,  ///< A track was muted, unmuted, locked or unlocked (track, muted, locked).
	};
	/// @brief Which part of a clip was dragged.
	enum class Edge { Move, Start, End };

	Kind kind = Kind::Select;        ///< See Kind.
	double time = 0.0;               ///< Scrub: the playhead's time; KeyframeMove: the keyframe's new time.
	double from = 0.0;               ///< KeyframeMove: its time before; ClipChange: the clip's start before.
	TimelineItem item;               ///< KeyframeMove, ClipChange: the item, where it was.
	std::string id;                  ///< ClipChange: the clip's id.
	std::size_t track = 0;           ///< ClipChange: the track it is on now; Expand, TrackChange: the track.
	double start = 0.0;              ///< ClipChange: its new start.
	double duration = 0.0;           ///< ClipChange: its new duration.
	Edge edge = Edge::Move;          ///< ClipChange: what was dragged.
	std::vector<TimelineItem> items; ///< Select: the selected items.
	bool expanded = false;           ///< Expand: whether it is open now.
	bool muted = false;              ///< TrackChange: whether it is muted now.
	bool locked = false;             ///< TrackChange: whether it is locked now.
};

/// @brief Draws a timeline and turns the pointer and the keyboard into TimelineEvents.
///
/// A press or a drag on the ruler scrubs; a keyframe or a clip is picked with a click (Shift adds to the selection)
/// and the selection dragged together, snapping to frames, keyframes, clip edges, markers and the playhead (Alt
/// leaves snapping off). A clip's edges trim it, and a clip dragged up or down changes track. A drag on an empty
/// lane draws a marquee. The header's chevron opens and closes a group, its M and L buttons mute and lock. The wheel
/// scrolls the rows (whole ones, only those in view being drawn) and, sideways, the time; the right or middle button
/// pans; + and - zoom. Each child of the view is the header of the track at its index, placed in its row and hidden
/// while the row isn't shown.
///
/// With the keyboard: Left and Right move the playhead a frame (a second with Shift), Comma and Period to the previous
/// or next keyframe or marker, Home and End to the play range's ends; Up and Down pick a track, Page Up and Page Down
/// select its previous or next item; Alt with the arrows moves the selection a frame (a second with Shift) or, up and
/// down, its clips to another track; Enter opens or closes a group, M mutes, L locks, Escape clears the selection.
class TimelineView : public Panel {
public:
	static constexpr std::size_t maximumTracks = 10000; ///< The most tracks setModel() keeps.
	static constexpr std::size_t maximumDepth = 16;     ///< The deepest a track may be.
	static constexpr std::size_t maximumItems = 10000;  ///< The most keyframes, and clips, a track keeps.
	static constexpr std::size_t maximumMarkers = 1000; ///< The most markers setModel() keeps.
	static constexpr double maximumDuration = 1.0e6;    ///< The longest a timeline is, in seconds.
	static constexpr double maximumFps = 1000.0;        ///< The most frames per second.

	/// @brief An empty timeline.
	TimelineView();

	/// @brief Sets the tracks, markers and times, kept within the maximums, without firing events.
	/// @param model The timeline.
	void setModel(TimelineModel model);
	/// @brief The tracks, markers and times.
	[[nodiscard]] const TimelineModel& model() const noexcept { return model_; }

	/// @brief Sets the playhead, without firing events.
	/// @param time In seconds, kept within the duration.
	void setTime(double time);
	/// @brief The playhead's time, in seconds.
	[[nodiscard]] double time() const noexcept { return time_; }
	/// @brief Sets the playhead a document gives: only when it differs from the one given last, so a scrub stays until
	/// the document gives another.
	/// @param time In seconds.
	void setGivenTime(double time);

	/// @brief Sets the selected items, without firing events.
	/// @param keys Their keys (itemKey()); unknown ones are ignored.
	void setSelection(std::vector<std::string> keys);
	/// @brief The keys of the selected items.
	[[nodiscard]] const std::vector<std::string>& selection() const noexcept { return selection_; }
	/// @brief Sets the selection a document gives: only when it differs from the one given last.
	/// @param keys The selected items' keys.
	void setGivenSelection(std::vector<std::string> keys);
	/// @brief The key that names an item in a selection: a clip's id, or a keyframe's track id and index, "pos:2".
	/// @param item The item.
	/// @return The key; empty if there is no such item.
	[[nodiscard]] std::string itemKey(const TimelineItem& item) const;

	/// @brief Sets the zoom and the time at the lanes' left edge.
	/// @param camera The camera; the zoom is clamped.
	void setCamera(TimelineCamera camera);
	/// @brief The zoom and the time at the lanes' left edge.
	[[nodiscard]] const TimelineCamera& camera() const noexcept { return camera_; }
	/// @brief Sets the camera a document gives: only when it differs from the one given last.
	/// @param camera The camera.
	void setGivenCamera(TimelineCamera camera);

	/// @brief Sets how the ruler writes times.
	/// @param format The format (frames by default).
	void setTimeFormat(TimeFormat format);
	/// @brief Sets whether drags snap.
	/// @param snap true to snap (the default).
	void setSnap(bool snap);
	/// @brief Sets the sizes of the parts.
	/// @param headerWidth The width of the headers' column.
	/// @param trackHeight The height of a row.
	void setSizes(float headerWidth, float trackHeight);
	/// @brief Sets the look.
	/// @param header The headers' and the ruler's fill.
	/// @param clip The clips' fill.
	/// @param text The text.
	/// @param accent The playhead, the selection and the keyframes.
	void setColors(Color header, Color clip, Color text, Color accent);
	/// @brief Sets the font of the ruler, the headers and the clips.
	/// @param font The font, or nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);

	/// @brief The tracks shown as rows, in order: the open ones (those in a closed group are left out).
	[[nodiscard]] const std::vector<std::size_t>& rows() const noexcept { return rows_; }
	/// @brief The first row in view.
	[[nodiscard]] std::size_t firstRow() const noexcept { return firstRow_; }
	/// @brief How many rows the last layout pass found in view.
	[[nodiscard]] std::size_t visibleRows() const noexcept { return visibleRows_; }
	/// @brief Where a time is, after layout.
	/// @param time In seconds.
	/// @return Relative to the view's left edge, in window units.
	[[nodiscard]] float timeX(double time) const noexcept;
	/// @brief Where a track's row is, after layout.
	/// @param track The track's index.
	/// @return Its centre, relative to the view's top edge; negative when it isn't in view.
	[[nodiscard]] float trackY(std::size_t track) const noexcept;

	Signal<const TimelineEvent&> onEdit; ///< The user scrubbed or edited the timeline; the owner changes the model.

protected:
	void arrange() override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	bool scrollContent(Vec2 delta) override;
	void keyEvent(const KeyEvent& event) override;
	/// @brief Repaints: the keyboard's track shows only while the timeline has the focus.
	/// @param focused Whether it gained the focus.
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;
	void paintOver(Painter& painter) const override;

private:
	/// @brief What a left drag is doing.
	enum class Drag { None, Scrub, Items, Start, End, Marquee, Pan };

	/// @brief The font in use.
	[[nodiscard]] const Font* font() const;
	/// @brief The lanes' left edge, relative to the view.
	[[nodiscard]] float lanesLeft() const noexcept;
	/// @brief How many whole rows fit under the ruler.
	[[nodiscard]] std::size_t rowsInView() const noexcept;
	/// @brief The row under a point.
	/// @param y Relative to the view's top edge.
	/// @return Its position in rows(), or rows().size() for none.
	[[nodiscard]] std::size_t rowAt(float y) const noexcept;
	/// @brief The item under a point, in the lanes.
	/// @param point Relative to the view's top-left corner.
	/// @param edge Where a clip's edge is under it: Start, End or Move.
	/// @return It, or std::nullopt.
	[[nodiscard]] std::optional<TimelineItem> itemAt(Vec2 point, TimelineEvent::Edge& edge) const;
	/// @brief Whether an item is selected.
	/// @param item The item.
	[[nodiscard]] bool isSelected(const TimelineItem& item) const;
	/// @brief An item's time: a keyframe's, or a clip's start.
	/// @param item The item.
	[[nodiscard]] double itemTime(const TimelineItem& item) const noexcept;
	/// @brief The items in a time range on rows.
	/// @param first The first time.
	/// @param last The last time.
	/// @param top The first row's position in rows().
	/// @param bottom The last row's.
	[[nodiscard]] std::vector<TimelineItem> itemsIn(double first, double last, std::size_t top,
	                                                std::size_t bottom) const;
	/// @brief Finds the times a drag or a scrub snaps to (frames aside), into snapTargets_: the markers, the play
	/// range's ends, and the keyframes and clip edges on the rows in view.
	/// @param dragging true for a drag of the selection: its own items are left out and the playhead counts.
	void findSnapTargets(bool dragging);
	/// @brief Snaps how far the selection moves, so its first item's time (or a clip's end) lands on a target.
	/// @param delta In seconds.
	/// @param snap Whether to snap.
	/// @return The delta, snapped.
	[[nodiscard]] double snapDelta(double delta, bool snap) const;
	/// @brief Changes the selection and fires Select.
	/// @param items The new selection.
	void select(std::vector<TimelineItem> items);
	/// @brief Moves the playhead and fires Scrub when it changed.
	/// @param time In seconds, kept within the duration.
	void scrubTo(double time);
	/// @brief Moves the selected items in time and clips across rows, and fires KeyframeMove and ClipChange.
	/// @param delta In seconds.
	/// @param rowDelta How many rows the clips move.
	void moveSelection(double delta, long rowDelta);
	/// @brief Trims the pressed clip and fires ClipChange.
	/// @param item The clip.
	/// @param edge Which edge.
	/// @param time The edge's new time.
	void trim(const TimelineItem& item, TimelineEvent::Edge edge, double time);
	/// @brief Opens or closes a group and fires Expand.
	/// @param track The track's index.
	void toggleExpanded(std::size_t track);
	/// @brief Mutes or locks a track and fires TrackChange.
	/// @param track The track's index.
	/// @param lock true to toggle the lock, false the mute.
	void toggleFlag(std::size_t track, bool lock);
	/// @brief Recomputes rows_ from the tracks and what is open.
	void refreshRows();
	/// @brief Recomputes selectedItems_ from the selection and the model.
	void refreshSelected();
	/// @brief Scrolls so a row is in view.
	/// @param row Its position in rows().
	void revealRow(std::size_t row);
	/// @brief Tells the accessibility tree where the playhead and the keyboard are.
	void describe();

	TimelineModel model_;                                    ///< See model().
	double time_ = 0.0;                                      ///< See time().
	std::vector<std::string> selection_;                     ///< See selection().
	TimelineCamera camera_;                                  ///< See camera().
	TimeFormat format_ = TimeFormat::Frames;                 ///< See setTimeFormat().
	bool snap_ = true;                                       ///< See setSnap().
	float headerWidth_ = 160.0f;                             ///< See setSizes().
	float trackHeight_ = 28.0f;                              ///< See setSizes().
	Color headerColor_ = Color::hex(0x2b2e36);               ///< See setColors().
	Color clipColor_ = Color::hex(0x4b6584);                 ///< See setColors().
	Color textColor_ = Color::hex(0xeceef2);                 ///< See setColors().
	Color accent_ = colors::accent;                          ///< See setColors().
	std::shared_ptr<const Font> font_;                       ///< See setFont().
	std::optional<double> givenTime_;                        ///< See setGivenTime().
	std::optional<TimelineCamera> givenCamera_;              ///< See setGivenCamera().
	std::optional<std::vector<std::string>> givenSelection_; ///< See setGivenSelection().

	std::vector<std::size_t> rows_;           ///< See rows().
	std::vector<TimelineItem> selectedItems_; ///< The selected items, sorted.
	std::size_t firstRow_ = 0;                ///< See firstRow().
	std::size_t visibleRows_ = 0;             ///< See visibleRows().

	Drag drag_ = Drag::None;                   ///< What the left button is doing.
	Vec2 pressed_;                             ///< Where the drag started, relative to the view.
	Vec2 pointer_;                             ///< Where the pointer is, relative to the view.
	TimelineItem pressedItem_;                 ///< The item the drag started on.
	double dragDelta_ = 0.0;                   ///< How far the dragged items moved, in seconds (snapped).
	long dragRows_ = 0;                        ///< How many rows the dragged clips moved.
	double trimTime_ = 0.0;                    ///< The trimmed edge's time, while trimming.
	TimelineCamera pressCamera_;               ///< The camera when a pan started.
	std::size_t pressFirstRow_ = 0;            ///< The first row when a pan started.
	std::vector<TimelineItem> pressSelection_; ///< The selection when a marquee with Shift started.
	bool snapping_ = true;                     ///< Whether this drag snaps (Alt was up when it started).
	std::vector<double> snapTargets_;          ///< See findSnapTargets().
	float scrollRest_ = 0.0f;                  ///< Wheel movement not yet a whole row.
	std::vector<std::size_t> rowOf_;           ///< Each track's position in rows(), or rows().size() while hidden.

	std::size_t keyRow_ = 0; ///< The row the keyboard is on.
};

} // namespace affresco
