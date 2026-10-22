/// @file
/// @brief TimelineView's drawing: the ruler and its ticks, the rows' headers, clips and keyframes, the play range,
/// the markers, and over the children the dragged clips, the marquee and the playhead.

#include "timeline/TimelineShared.hpp"

#include "affresco/render/RichText.hpp"
#include "affresco/render/VectorPath.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace affresco {

using timeline::buttonSize;
using timeline::chevronWidth;
using timeline::indentStep;
using timeline::isGroup;
using timeline::keyframeRadius;
using timeline::playEndOf;
using timeline::rulerHeight;
using timeline::tickLimit;

namespace {

/// @brief The view's colours.
struct Palette {
	Color background; ///< The lanes.
	Color header;     ///< The ruler and the headers' column.
	Color clip;       ///< A clip without a colour of its own.
	Color text;       ///< Labels and keyframes.
	Color accent;     ///< The selection, the playhead and the buttons that are on.
};

/// @brief What drawing a frame needs of the view, gathered once so the parts below are free functions.
struct Scene {
	Painter& painter;                     ///< Where to draw.
	const TimelineModel& model;           ///< The timeline.
	const TimelineCamera& camera;         ///< The time mapping.
	const Rect& frame;                    ///< The view's frame.
	const Font* font;                     ///< The labels' font, or nullptr to draw none.
	float laneLeft;                       ///< Where the lanes start, in window units.
	std::span<const TimelineItem> chosen; ///< The selected items, sorted.
	Palette colors;                       ///< The colours.
	bool moving;                          ///< The selection is being dragged, and drawn over all where it goes.
	double moved;                         ///< How far the dragged keyframes have gone, in seconds.
	std::optional<TimelineItem> trimmed;  ///< The clip whose edge is being dragged.
	bool trimStart;                       ///< Whether that edge is the start.
	double trimTime;                      ///< Where that edge is.

	/// @brief Where a time is on screen.
	/// @param time In seconds.
	/// @return In window units.
	[[nodiscard]] float x(double time) const noexcept { return laneLeft + camera.toX(time); }

	/// @brief Whether an item is selected.
	/// @param item The item.
	/// @return true if it is.
	[[nodiscard]] bool selected(const TimelineItem& item) const {
		return std::binary_search(chosen.begin(), chosen.end(), item);
	}
};

/// @brief A row in view.
struct RowBox {
	std::size_t track; ///< The track it shows.
	std::size_t row;   ///< Its row among all.
	float top;         ///< Its top, in window units.
	float bottom;      ///< Its bottom.
	float middle;      ///< Its middle.
};

/// @brief The baseline that centres a line of text on a height.
/// @param font The font.
/// @param middle The height.
/// @return The baseline.
float centredBaseline(const Font& font, float middle) { return middle - font.lineHeight() * 0.5f + font.ascent(); }

/// @brief Draws the ruler, the headers' column and the play range's strip on the ruler.
/// @param scene The frame.
void paintRuler(const Scene& scene) {
	const Rect& f = scene.frame;
	const Color& header = scene.colors.header;
	scene.painter.fillRect({f.left, f.top, f.right, f.top + rulerHeight}, header);
	scene.painter.fillRect({f.left, f.top + rulerHeight, scene.laneLeft, f.bottom},
	                       Color::mix(header, scene.colors.background, 0.35f));
	const double playEnd = playEndOf(scene.model);
	if(playEnd <= scene.model.playStart) return;
	const float start = std::max(scene.x(scene.model.playStart), scene.laneLeft);
	const float end = std::min(scene.x(playEnd), f.right);
	if(end > start)
		scene.painter.fillRect({start, f.top + rulerHeight - 6.0f, end, f.top + rulerHeight},
		                       scene.colors.accent.withAlpha(0.35f));
}

/// @brief Draws the ruler's ticks, labelled and small.
/// @param scene The frame.
/// @param format How the labels read.
/// @param width The lanes' width.
void paintTicks(const Scene& scene, TimeFormat format, float width) {
	const Rect& f = scene.frame;
	const RulerTicks ticks = rulerTicks(scene.camera.scale, scene.model.fps, format);
	const double first = std::max(scene.camera.offset, 0.0), last = scene.camera.toTime(width);
	const Color tick = Color::mix(scene.colors.header, scene.colors.text, 0.45f);
	if(ticks.minor > 0.0) {
		const double from = std::ceil(first / ticks.minor - 1e-9);
		for(std::size_t i = 0; i < tickLimit; ++i) {
			const double at = (from + static_cast<double>(i)) * ticks.minor;
			if(at > last) break;
			const float x = scene.x(at);
			scene.painter.fillRect({x, f.top + rulerHeight - 5.0f, x + 1.0f, f.top + rulerHeight}, tick);
		}
	}
	const double from = std::ceil(first / ticks.major - 1e-9);
	for(std::size_t i = 0; i < tickLimit; ++i) {
		const double at = (from + static_cast<double>(i)) * ticks.major;
		if(at > last) break;
		const float x = scene.x(at);
		scene.painter.fillRect({x, f.top + rulerHeight - 12.0f, x + 1.0f, f.top + rulerHeight}, tick);
		if(scene.font)
			scene.painter.drawText(*scene.font, codepointsOf(formatTime(at, scene.model.fps, format)),
			                       {x + 3.0f, f.top + scene.font->ascent() + 2.0f}, scene.colors.text);
	}
}

/// @brief Draws a row's lane: every other one shaded, the line under it and the keyboard's mark.
/// @param scene The frame.
/// @param box The row.
/// @param focused Whether the keyboard is on it.
void paintLane(const Scene& scene, const RowBox& box, bool focused) {
	const Rect& f = scene.frame;
	const Palette& colors = scene.colors;
	if(box.row % 2 == 1)
		scene.painter.fillRect({scene.laneLeft, box.top, f.right, box.bottom},
		                       Color::mix(colors.background, colors.text, 0.03f));
	scene.painter.fillRect({f.left, box.bottom - 1.0f, f.right, box.bottom},
	                       Color::mix(colors.background, colors.text, 0.08f));
	if(focused) scene.painter.fillRect({f.left, box.top, f.left + 3.0f, box.bottom}, colors.accent);
}

/// @brief Draws a group's chevron: down when it is open, right when it is closed.
/// @param scene The frame.
/// @param box The group's row.
/// @param indent Where its header starts.
/// @param expanded Whether it is open.
void paintChevron(const Scene& scene, const RowBox& box, float indent, bool expanded) {
	const Rect& f = scene.frame;
	VectorPath chevron;
	const float cx = indent - f.left + chevronWidth * 0.5f, cy = box.middle - f.top;
	if(expanded) {
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
	scene.painter.fillMesh(quads, {f.left, f.top}, scene.colors.text);
}

/// @brief Draws a track's mute and lock buttons, right of its header, where they fit.
/// @param scene The frame.
/// @param box The track's row.
/// @param track The track.
void paintButtons(const Scene& scene, const RowBox& box, const TimelineTrack& track) {
	const Palette& colors = scene.colors;
	const float lock = scene.laneLeft - 4.0f - buttonSize, mute = lock - 2.0f - buttonSize;
	for(const bool isLock : {false, true}) {
		const float left = isLock ? lock : mute;
		if(left < scene.frame.left) continue;
		const bool on = isLock ? track.locked : track.muted;
		const Rect button{left, box.middle - buttonSize * 0.5f, left + buttonSize, box.middle + buttonSize * 0.5f};
		scene.painter.fillRoundedRect(button, CornerRadii::all(3.0f),
		                              on ? colors.accent : Color::mix(colors.header, colors.text, 0.12f));
		if(!scene.font) continue;
		const std::u32string_view letter = isLock ? U"L" : U"M";
		scene.painter.drawText(
		    *scene.font, letter,
		    {button.left + (buttonSize - scene.font->measure(letter)) * 0.5f, centredBaseline(*scene.font, box.middle)},
		    colors.text);
	}
}

/// @brief Draws a track's header: its chevron, its label unless a child is the header, and its buttons.
/// @param scene The frame.
/// @param box The track's row.
/// @param hasHeader Whether a child is the header.
void paintHeader(const Scene& scene, const RowBox& box, bool hasHeader) {
	const TimelineTrack& track = scene.model.tracks[box.track];
	const float indent = scene.frame.left + static_cast<float>(track.depth) * indentStep;
	if(isGroup(scene.model.tracks, box.track)) paintChevron(scene, box, indent, track.expanded);
	if(scene.font && !hasHeader)
		scene.painter.drawText(*scene.font, codepointsOf(track.label.empty() ? track.id : track.label),
		                       {indent + chevronWidth, centredBaseline(*scene.font, box.middle)},
		                       track.muted ? scene.colors.text.withAlpha(0.5f) : scene.colors.text);
	paintButtons(scene, box, track);
}

/// @brief Draws a track's clips, but not the dragged ones, which paintOver draws where they go.
/// @param scene The frame.
/// @param box The track's row.
void paintClips(const Scene& scene, const RowBox& box) {
	const Rect& f = scene.frame;
	const TimelineTrack& track = scene.model.tracks[box.track];
	const float fade = track.muted ? 0.4f : 1.0f;
	for(std::size_t c = 0; c < track.clips.size(); ++c) {
		const TimelineItem item{box.track, c, true};
		const bool selected = scene.selected(item);
		if(selected && scene.moving) continue;
		const TimelineClip& clip = track.clips[c];
		double start = clip.start, end = clip.start + clip.duration;
		if(scene.trimmed == item) (scene.trimStart ? start : end) = scene.trimTime;
		const float x0 = std::max(scene.x(start), scene.laneLeft), x1 = std::min(scene.x(end), f.right);
		if(x1 < scene.laneLeft || x0 > f.right || x1 <= x0) continue;
		const Rect area{x0, box.top + 3.0f, x1, box.bottom - 3.0f};
		scene.painter.fillRoundedRect(area, CornerRadii::all(4.0f),
		                              clip.color.value_or(scene.colors.clip).withAlpha(fade));
		if(selected) scene.painter.strokeRoundedRect(area, CornerRadii::all(4.0f), 2.0f, scene.colors.accent);
		if(!scene.font) continue;
		const std::u32string label = codepointsOf(clip.label.empty() ? clip.id : clip.label);
		if(scene.font->measure(label) + 12.0f <= x1 - x0)
			scene.painter.drawText(*scene.font, label, {x0 + 6.0f, centredBaseline(*scene.font, box.middle)},
			                       scene.colors.text.withAlpha(fade));
	}
}

/// @brief Adds a diamond, a linear keyframe, to a mesh while it has room.
/// @param scene The frame.
/// @param centre Its centre, in window units.
/// @param mesh The mesh.
void addDiamond(const Scene& scene, Vec2 centre, std::vector<MeshVertex>& mesh) {
	if(mesh.size() / 4 >= meshQuadLimit) return;
	const float r = keyframeRadius;
	const float px0 = centre.x - scene.frame.left, py0 = centre.y - scene.frame.top;
	VectorPath diamond;
	diamond.moveTo({px0, py0 - r});
	diamond.lineTo({px0 + r, py0});
	diamond.lineTo({px0, py0 + r});
	diamond.lineTo({px0 - r, py0});
	diamond.close();
	appendFill(diamond.flatten({}, 0.25f), FillRule::nonZero, mesh);
}

/// @brief Draws a track's keyframes over its clips: a square holds, a circle eases, a diamond is linear.
/// @param scene The frame.
/// @param box The track's row.
void paintKeyframes(const Scene& scene, const RowBox& box) {
	const Rect& f = scene.frame;
	const Palette& colors = scene.colors;
	const TimelineTrack& track = scene.model.tracks[box.track];
	const float fade = track.muted ? 0.4f : 1.0f, r = keyframeRadius, middle = box.middle;
	const Color plain = Color::mix(colors.text, colors.accent, 0.25f);
	std::vector<MeshVertex> diamonds, chosen;
	for(std::size_t k = 0; k < track.keyframes.size(); ++k) {
		const TimelineKeyframe& keyframe = track.keyframes[k];
		const bool selected = scene.selected({box.track, k, false});
		const float x = scene.x(keyframe.time + (selected && scene.moving ? scene.moved : 0.0));
		if(x < scene.laneLeft - r || x > f.right + r) continue;
		const Color fill = (selected ? colors.accent : plain).withAlpha(fade);
		switch(keyframe.easing) {
		case Easing::Step:
			scene.painter.fillRect({x - r + 1.0f, middle - r + 1.0f, x + r - 1.0f, middle + r - 1.0f}, fill);
			break;
		case Easing::Bezier:
			scene.painter.fillRoundedRect({x - r, middle - r, x + r, middle + r}, CornerRadii::all(r), fill);
			break;
		case Easing::Linear:
			addDiamond(scene, {x, middle}, selected ? chosen : diamonds);
			break;
		}
	}
	if(!diamonds.empty()) scene.painter.fillMesh(diamonds, {f.left, f.top}, plain.withAlpha(fade));
	if(!chosen.empty()) scene.painter.fillMesh(chosen, {f.left, f.top}, colors.accent.withAlpha(fade));
}

/// @brief Dims the lanes outside the play range and draws the markers, on the ruler and across the lanes.
/// @param scene The frame.
void paintMarkers(const Scene& scene) {
	const Rect& f = scene.frame;
	const double playEnd = playEndOf(scene.model);
	if(playEnd > scene.model.playStart) {
		const Color dim = scene.colors.background.withAlpha(0.45f);
		const float start = scene.x(scene.model.playStart), end = scene.x(playEnd);
		if(start > scene.laneLeft)
			scene.painter.fillRect({scene.laneLeft, f.top + rulerHeight, std::min(start, f.right), f.bottom}, dim);
		if(end < f.right)
			scene.painter.fillRect({std::max(end, scene.laneLeft), f.top + rulerHeight, f.right, f.bottom}, dim);
	}
	const Color mark = Color::mix(scene.colors.accent, scene.colors.text, 0.5f);
	for(const TimelineMarker& marker : scene.model.markers) {
		const float x = scene.x(marker.time);
		if(x < scene.laneLeft || x > f.right) continue;
		scene.painter.fillRect({x, f.top + rulerHeight, x + 1.0f, f.bottom}, mark.withAlpha(0.35f));
		scene.painter.fillRect({x - 3.0f, f.top + rulerHeight - 8.0f, x + 3.0f, f.top + rulerHeight}, mark);
		if(scene.font && !marker.label.empty())
			scene.painter.drawText(*scene.font, codepointsOf(marker.label), {x + 5.0f, f.top + rulerHeight - 2.0f},
			                       mark);
	}
}

/// @brief What drawing the dragged clips needs of the view.
struct Dragged {
	const TimelineModel& model;          ///< The timeline.
	const TimelineCamera& camera;        ///< The time mapping.
	const Rect& frame;                   ///< The view's frame.
	float laneLeft;                      ///< Where the lanes start, in window units.
	std::span<const TimelineItem> items; ///< The selected items, which are being dragged.
	std::span<const std::size_t> rows;   ///< The tracks in row order.
	std::span<const std::size_t> rowOf;  ///< Each track's row.
	std::size_t firstRow;                ///< The first row in view.
	std::size_t visibleRows;             ///< How many rows are in view.
	float trackHeight;                   ///< A row's height.
	long rowsMoved;                      ///< How many rows the clips moved.
	double moved;                        ///< How far they moved, in seconds.
	Color clip;                          ///< A clip without a colour of its own.
	Color accent;                        ///< The selection's colour.
};

/// @brief The row a dragged clip goes to: its own moved by the drag, unless that is out of range or locked.
/// @param drag The drag.
/// @param track The clip's track.
/// @return The row.
std::size_t draggedRow(const Dragged& drag, std::size_t track) {
	const std::size_t row = drag.rowOf[track];
	const long to = static_cast<long>(row) + drag.rowsMoved;
	if(drag.rowsMoved != 0 && to >= 0 && static_cast<std::size_t>(to) < drag.rows.size() &&
	   !drag.model.tracks[drag.rows[static_cast<std::size_t>(to)]].locked)
		return static_cast<std::size_t>(to);
	return row;
}

/// @brief Draws the clips being dragged where they are going, over everything.
/// @param painter Where to draw.
/// @param drag The drag.
void paintDragged(Painter& painter, const Dragged& drag) {
	const Rect& f = drag.frame;
	for(const TimelineItem& item : drag.items) {
		if(!item.clip || drag.model.tracks[item.track].locked) continue;
		const std::size_t row = draggedRow(drag, item.track);
		if(row < drag.firstRow || row >= drag.firstRow + drag.visibleRows) continue;
		const TimelineClip& clip = drag.model.tracks[item.track].clips[item.index];
		const double start = std::max(clip.start + drag.moved, 0.0);
		const float top = f.top + rulerHeight + static_cast<float>(row - drag.firstRow) * drag.trackHeight;
		const float x0 = std::max(drag.laneLeft + drag.camera.toX(start), drag.laneLeft);
		const float x1 = std::min(drag.laneLeft + drag.camera.toX(start + clip.duration), f.right);
		if(x1 <= x0) continue;
		const Rect box{x0, top + 3.0f, x1, top + drag.trackHeight - 3.0f};
		painter.fillRoundedRect(box, CornerRadii::all(4.0f), clip.color.value_or(drag.clip).withAlpha(0.85f));
		painter.strokeRoundedRect(box, CornerRadii::all(4.0f), 2.0f, drag.accent);
	}
}

} // namespace

void TimelineView::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& f = frame();
	const float lanes = lanesLeft();
	const bool trimming = drag_ == Drag::Start || drag_ == Drag::End;
	const Scene scene{painter,
	                  model_,
	                  camera_,
	                  f,
	                  font(),
	                  f.left + lanes,
	                  selectedItems_,
	                  {color(), headerColor_, clipColor_, textColor_, accent_},
	                  drag_ == Drag::Items,
	                  dragDelta_,
	                  trimming ? std::optional{pressedItem_} : std::nullopt,
	                  drag_ == Drag::Start,
	                  trimTime_};
	paintRuler(scene);
	paintTicks(scene, format_, resolvedSize().x - lanes);
	const auto& children = this->children();
	for(std::size_t row = firstRow_; row < firstRow_ + visibleRows_ && row < rows_.size(); ++row) {
		const std::size_t index = rows_[row];
		const float top = f.top + rulerHeight + static_cast<float>(row - firstRow_) * trackHeight_;
		const RowBox box{index, row, top, top + trackHeight_, top + trackHeight_ * 0.5f};
		paintLane(scene, box, hasFocus() && row == keyRow_);
		paintHeader(scene, box, index < children.size() && !children[index]->isOverlay());
		paintClips(scene, box);
		paintKeyframes(scene, box);
	}
	paintMarkers(scene);
}

void TimelineView::paintOver(Painter& painter) const {
	const Rect& f = frame();
	const float laneLeft = f.left + lanesLeft();
	if(drag_ == Drag::Items)
		paintDragged(painter, {model_, camera_, f, laneLeft, selectedItems_, rows_, rowOf_, firstRow_, visibleRows_,
		                       trackHeight_, dragRows_, dragDelta_, clipColor_, accent_});
	if(drag_ == Drag::Marquee) {
		const Rect area{f.left + std::min(pressed_.x, pointer_.x), f.top + std::min(pressed_.y, pointer_.y),
		                f.left + std::max(pressed_.x, pointer_.x), f.top + std::max(pressed_.y, pointer_.y)};
		painter.fillRect(area, accent_.withAlpha(0.15f));
		painter.strokeRoundedRect(area, {}, 1.0f, accent_);
	}

	// The playhead, over the headers' children too
	const float x = laneLeft + camera_.toX(time_);
	if(x >= laneLeft && x <= f.right) {
		painter.fillRect({x - 0.5f, f.top, x + 1.5f, f.bottom}, accent_);
		painter.fillRoundedRect({x - 5.0f, f.top + 2.0f, x + 6.0f, f.top + 12.0f}, CornerRadii::all(2.0f), accent_);
	}
	Panel::paintOver(painter);
}

} // namespace affresco
