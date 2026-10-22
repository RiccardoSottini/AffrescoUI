/// @file
/// @brief CurvesEditor and the curve maths: normalising and evaluating (monotone cubic, linear, Bézier).

#include "affresco/render/CurvesEditor.hpp"
#include "affresco/render/Window.hpp"

#include "image/CreativePixels.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace affresco {

namespace {

constexpr float pickDistance = 7.0f;     ///< How near a point a press picks it.
constexpr float removeDistance = 24.0f;  ///< How far out of the plot a dragged point is removed.
constexpr float markerRadius = 4.0f;     ///< The radius of a point's marker.
constexpr std::size_t sampleCount = 256; ///< The samples the drawn curve is made of.
constexpr float maxColumns = 4096.0f;    ///< The most columns the curve and the histogram are drawn in.
constexpr std::array<const char32_t*, curveChannelCount> tabLabels{U"RGB", U"R", U"G", U"B"}; ///< The tab labels.

/// @brief The columns a plot is drawn in: one per unit, and wider ones (a screen sets the size) in steps, so a frame
/// costs the same.
/// @param width The plot's width.
/// @param least The fewest columns.
/// @return {how many, how far apart}.
[[nodiscard]] std::pair<std::size_t, float> columnsOf(float width, float least) noexcept {
	if(!(width > maxColumns)) return {static_cast<std::size_t>(std::max(least, std::floor(width))), 1.0f};
	return {static_cast<std::size_t>(maxColumns), std::isfinite(width) ? width / maxColumns : 1.0f};
}

/// @brief The slope of a monotone cubic at a point (Fritsch–Butland), from the secants on each side.
/// @param points The points.
/// @param index The point.
/// @return The slope: 0 at a peak or valley, one-sided at the ends.
[[nodiscard]] double monotoneSlope(std::span<const CurvePoint> points, std::size_t index) noexcept {
	const auto secant = [points](std::size_t left) {
		const double width = points[left + 1].x - points[left].x;
		return width > 0.0 ? (points[left + 1].y - points[left].y) / width : 0.0;
	};
	if(index == 0) return secant(0);
	if(index + 1 == points.size()) return secant(index - 1);
	const double before = secant(index - 1), after = secant(index);
	if(before * after <= 0.0) return 0.0;
	const double h0 = points[index].x - points[index - 1].x, h1 = points[index + 1].x - points[index].x;
	return 3.0 * (h0 + h1) / ((2.0 * h1 + h0) / before + (h1 + 2.0 * h0) / after);
}

/// @brief A Bézier curve's point at a parameter (de Casteljau), on the stack.
/// @param points The control points (at most maxCurvePoints).
/// @param t The parameter, 0..1.
/// @return The point.
[[nodiscard]] CurvePoint bezierAt(std::span<const CurvePoint> points, double t) noexcept {
	std::array<CurvePoint, maxCurvePoints> work{};
	const std::size_t count = std::min(points.size(), maxCurvePoints);
	std::copy_n(points.begin(), count, work.begin());
	for(std::size_t level = count; level-- > 1;) {
		for(std::size_t index = 0; index < level; ++index) {
			work[index] = {work[index].x + (work[index + 1].x - work[index].x) * t,
			               work[index].y + (work[index + 1].y - work[index].y) * t};
		}
	}
	return work[0];
}

/// @brief A Bézier curve's output at an input, finding the parameter by bisection (x grows with t, the points being
/// sorted).
/// @param points The control points.
/// @param x The input.
/// @return The output.
[[nodiscard]] double bezierOutput(std::span<const CurvePoint> points, double x) noexcept {
	double low = 0.0, high = 1.0;
	for(int step = 0; step < 32; ++step) {
		const double middle = (low + high) / 2.0;
		(bezierAt(points, middle).x < x ? low : high) = middle;
	}
	return bezierAt(points, (low + high) / 2.0).y;
}

/// @brief A cache key for a curve.
/// @param points The points.
/// @param interpolation The interpolation.
/// @return The key.
[[nodiscard]] std::uint64_t curveKey(std::span<const CurvePoint> points, CurveInterpolation interpolation) noexcept {
	std::uint64_t key = detail::mixKey(detail::firstKey, static_cast<double>(interpolation));
	for(const CurvePoint& point : points) key = detail::mixKey(detail::mixKey(key, point.x), point.y);
	return key;
}

/// @brief Clamps a coordinate to 0..1 (not a number becomes 0).
/// @param value The coordinate.
/// @return The clamped coordinate.
[[nodiscard]] double unit(double value) noexcept { return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0; }

} // namespace

void normalizeCurve(std::vector<CurvePoint>& points) {
	if(points.size() > maxCurvePoints) points.resize(maxCurvePoints);
	for(CurvePoint& point : points) point = {unit(point.x), unit(point.y)};
	std::ranges::stable_sort(points, {}, &CurvePoint::x);
	if(points.empty()) points = {{0.0, 0.0}, {1.0, 1.0}};
	if(points.size() == 1) points = {{0.0, points.front().y}, {1.0, points.front().y}};
}

double evaluateCurve(std::span<const CurvePoint> points, CurveInterpolation interpolation, double x) noexcept {
	points = points.first(std::min(points.size(), maxCurvePoints));
	if(points.empty()) return unit(x);
	if(!std::isfinite(x) || x <= points.front().x) return unit(points.front().y);
	if(x >= points.back().x) return unit(points.back().y);
	if(interpolation == CurveInterpolation::Bezier) return unit(bezierOutput(points, x));
	std::size_t right = 1;
	while(points[right].x < x) ++right;
	const CurvePoint& a = points[right - 1];
	const CurvePoint& b = points[right];
	const double width = b.x - a.x;
	if(width <= 0.0) return unit(b.y);
	const double t = (x - a.x) / width;
	if(interpolation == CurveInterpolation::Linear) return unit(a.y + (b.y - a.y) * t);
	const double slopeA = monotoneSlope(points, right - 1) * width, slopeB = monotoneSlope(points, right) * width;
	const double t2 = t * t, t3 = t2 * t; // cubic Hermite
	return unit((2 * t3 - 3 * t2 + 1) * a.y + (t3 - 2 * t2 + t) * slopeA + (-2 * t3 + 3 * t2) * b.y +
	            (t3 - t2) * slopeB);
}

CurvesEditor::CurvesEditor() : samples_(std::make_unique<Samples>()) {
	Panel::setColor(colors::transparent);
	setCursor(Cursor::Hand);
	for(auto& curve : curves_) normalizeCurve(curve);
	samples_->values.resize(sampleCount);
}

CurvesEditor::~CurvesEditor() = default;

std::vector<CurvePoint>& CurvesEditor::points() noexcept { return curves_[static_cast<std::size_t>(channel_)]; }

const std::vector<CurvePoint>& CurvesEditor::points() const noexcept {
	return curves_[static_cast<std::size_t>(channel_)];
}

void CurvesEditor::setCurve(CurveChannel channel, std::vector<CurvePoint> points) {
	normalizeCurve(points);
	auto& curve = curves_[static_cast<std::size_t>(channel)];
	if(points == curve) return;
	curve = std::move(points);
	selected_ = std::min(selected_, this->points().size() - 1);
	invalidatePaint();
}

std::span<const CurvePoint> CurvesEditor::curve(CurveChannel channel) const noexcept {
	return curves_[static_cast<std::size_t>(channel)];
}

void CurvesEditor::setChannel(CurveChannel channel) {
	if(!showsChannels_) channel = CurveChannel::Rgb;
	if(channel == channel_) return;
	channel_ = channel;
	selected_ = std::min(selected_, points().size() - 1);
	invalidatePaint();
}

void CurvesEditor::setShowsChannels(bool shown) {
	setLook(showsChannels_, shown);
	if(!shown) setChannel(CurveChannel::Rgb);
}

void CurvesEditor::setInterpolation(CurveInterpolation interpolation) { setLook(interpolation_, interpolation); }

void CurvesEditor::setHistogram(std::span<const double> bins) {
	bins = bins.first(std::min(bins.size(), maxHistogramBins));
	double highest = 0.0;
	for(const double bin : bins) highest = std::isfinite(bin) ? std::max(highest, bin) : highest;
	histogram_.clear();
	for(const double bin : bins) {
		histogram_.push_back(highest > 0.0 && std::isfinite(bin) ? static_cast<float>(std::max(bin, 0.0) / highest)
		                                                         : 0.0f);
	}
	invalidatePaint();
}

void CurvesEditor::selectPoint(std::size_t index) {
	index = std::min(index, points().size() - 1);
	if(index == selected_) return;
	selected_ = index;
	invalidatePaint();
}

bool CurvesEditor::addPoint(double x, double y) {
	auto& curve = points();
	if(curve.size() >= maxCurvePoints) return false;
	const CurvePoint point{unit(x), unit(y)};
	const auto place = std::ranges::upper_bound(curve, point.x, {}, &CurvePoint::x);
	selected_ = static_cast<std::size_t>(place - curve.begin());
	curve.insert(place, point);
	changed();
	return true;
}

bool CurvesEditor::removePoint(std::size_t index) {
	auto& curve = points();
	if(index >= curve.size() || curve.size() <= 2) return false;
	curve.erase(curve.begin() + static_cast<std::ptrdiff_t>(index));
	selected_ = std::min(selected_ > index ? selected_ - 1 : selected_, curve.size() - 1);
	changed();
	return true;
}

void CurvesEditor::movePoint(std::size_t index, double x, double y) {
	auto& curve = points();
	if(index >= curve.size()) return;
	const double lowest = index > 0 ? curve[index - 1].x : 0.0;
	const double highest = index + 1 < curve.size() ? curve[index + 1].x : 1.0;
	const CurvePoint moved{std::clamp(unit(x), lowest, highest), unit(y)};
	if(moved == curve[index]) return;
	curve[index] = moved;
	changed();
}

void CurvesEditor::setFont(std::shared_ptr<const Font> font) { setLook(font_, std::move(font)); }

void CurvesEditor::setColors(Color text, Color accent) {
	setLook(text_, text);
	setLook(accent_, accent);
}

const Font* CurvesEditor::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

void CurvesEditor::changed() {
	invalidatePaint();
	onCurveChanged(channel_);
}

CurvesEditor::Layout CurvesEditor::layout() const noexcept {
	const Rect& f = frame();
	Layout out;
	float top = f.top;
	if(showsChannels_) {
		const Font* text = font();
		const float height = text ? std::ceil(text->lineHeight()) + 6.0f : 20.0f;
		out.tabs = {f.left, f.top, f.right, f.top + height};
		top += height + 6.0f;
	}
	const float inset = markerRadius + 1.0f; // so the points at the edges show whole
	out.plot = {f.left + inset, top + inset, f.right - inset, std::max(top + inset, f.bottom - inset)};
	return out;
}

Vec2 CurvesEditor::toWindow(const Layout& area, CurvePoint point) const noexcept {
	return {area.plot.left + static_cast<float>(point.x) * area.plot.width(),
	        area.plot.bottom - static_cast<float>(point.y) * area.plot.height()};
}

CurvePoint CurvesEditor::fromWindow(const Layout& area, Vec2 position) const noexcept {
	const float width = std::max(area.plot.width(), 1.0f), height = std::max(area.plot.height(), 1.0f);
	return {(position.x - area.plot.left) / width, (area.plot.bottom - position.y) / height};
}

std::size_t CurvesEditor::pointAt(const Layout& area, Vec2 position) const noexcept {
	const auto& curve = points();
	std::size_t best = curve.size();
	float bestDistance = pickDistance;
	for(std::size_t index = 0; index < curve.size(); ++index) {
		const Vec2 at = toWindow(area, curve[index]);
		const float distance = std::hypot(position.x - at.x, position.y - at.y);
		if(distance <= bestDistance) {
			best = index;
			bestDistance = distance;
		}
	}
	return best;
}

bool CurvesEditor::pointKey(const KeyEvent& event) {
	const auto& curve = points();
	const CurvePoint point = curve[selected_];
	const double step = event.mods.shift() ? 0.1 : 0.01;
	switch(event.key) {
	case Key::Left:
	case Key::Right: {
		const int direction = event.key == Key::Right ? 1 : -1;
		if(event.mods.shift()) movePoint(selected_, point.x + direction * 0.01, point.y);
		else if(direction > 0) selectPoint(selected_ + 1);
		else selectPoint(selected_ == 0 ? 0 : selected_ - 1);
		return true;
	}
	case Key::Up:
		movePoint(selected_, point.x, point.y + step);
		return true;
	case Key::Down:
		movePoint(selected_, point.x, point.y - step);
		return true;
	case Key::Home:
		selectPoint(0);
		return true;
	case Key::End:
		selectPoint(curve.size() - 1);
		return true;
	default:
		return false;
	}
}

void CurvesEditor::keyEvent(const KeyEvent& event) {
	if(event.action == Action::Release || pointKey(event)) return Panel::keyEvent(event);
	const auto& curve = points();
	switch(event.key) {
	case Key::Insert:
	case Key::Equal:
	case Key::KeypadAdd: {
		const std::size_t left = selected_ + 1 < curve.size() ? selected_ : selected_ - 1;
		const double x = (curve[left].x + curve[left + 1].x) / 2.0;
		static_cast<void>(addPoint(x, evaluateCurve(curve, interpolation_, x))); // full: nothing to add
		return;
	}
	case Key::Delete:
	case Key::Backspace:
		static_cast<void>(removePoint(selected_));
		return;
	case Key::C:
		if(!showsChannels_) break;
		setChannel(static_cast<CurveChannel>((static_cast<std::size_t>(channel_) + 1) % curveChannelCount));
		return;
	default:
		break;
	}
	Panel::keyEvent(event);
}

void CurvesEditor::mouseButtonEvent(const MouseButtonEvent& event) {
	if(event.button != MouseButton::Left) return Panel::mouseButtonEvent(event);
	const Layout area = layout();
	if(event.action == Action::Press) {
		offPlot_ = false;
		if(area.tabs.contains(event.position)) {
			const float width = area.tabs.width() / static_cast<float>(curveChannelCount);
			const auto tab = static_cast<std::size_t>((event.position.x - area.tabs.left) / std::max(width, 1.0f));
			setChannel(static_cast<CurveChannel>(std::min(tab, curveChannelCount - 1)));
			return Panel::mouseButtonEvent(event);
		}
		const std::size_t hit = pointAt(area, event.position);
		if(hit < points().size()) selectPoint(hit);
		const CurvePoint at = fromWindow(area, event.position);
		dragging_ = hit < points().size() || (area.plot.contains(event.position) && addPoint(at.x, at.y));
	} else if(dragging_) {
		dragging_ = false;
		if(offPlot_) static_cast<void>(removePoint(selected_));
		offPlot_ = false;
		invalidatePaint();
	}
	Panel::mouseButtonEvent(event);
}

void CurvesEditor::mouseMoveEvent(const MouseMoveEvent& event) {
	if(dragging_) {
		const Layout area = layout();
		const Vec2 p = event.position;
		const float away =
		    std::max({area.plot.left - p.x, p.x - area.plot.right, area.plot.top - p.y, p.y - area.plot.bottom});
		const bool off = away > removeDistance && points().size() > 2;
		if(off != offPlot_) {
			offPlot_ = off;
			invalidatePaint();
		}
		const CurvePoint at = fromWindow(area, p);
		movePoint(selected_, at.x, at.y);
	}
	Panel::mouseMoveEvent(event);
}

void CurvesEditor::focusEvent(bool focused) {
	invalidatePaint();
	Panel::focusEvent(focused);
}

void CurvesEditor::paint(Painter& painter) const {
	Panel::paint(painter);
	const Layout area = layout();
	if(showsChannels_) paintTabs(painter, area);
	if(area.plot.width() <= 0.0f || area.plot.height() <= 0.0f) return;
	paintBackground(painter, area);
	paintCurve(painter, area);
}

void CurvesEditor::paintBackground(Painter& painter, const Layout& area) const {
	const Rect& plot = area.plot;
	painter.strokeRoundedRect(plot, CornerRadii::all(0.0f), 1.0f, text_.withAlpha(0.3f));
	for(int quarter = 1; quarter < 4; ++quarter) {
		const float x = std::round(plot.left + plot.width() * static_cast<float>(quarter) / 4.0f);
		const float y = std::round(plot.top + plot.height() * static_cast<float>(quarter) / 4.0f);
		painter.fillRect({x, plot.top, x + 1.0f, plot.bottom}, text_.withAlpha(0.12f));
		painter.fillRect({plot.left, y, plot.right, y + 1.0f}, text_.withAlpha(0.12f));
	}
	if(histogram_.empty()) return;
	const auto [columns, step] = columnsOf(plot.width(), 1.0f);
	for(std::size_t column = 0; column < columns; ++column) {
		const float bin = histogram_[column * histogram_.size() / columns];
		if(bin <= 0.0f) continue;
		const float x = plot.left + static_cast<float>(column) * step;
		painter.fillRect({x, plot.bottom - bin * plot.height(), x + step, plot.bottom}, text_.withAlpha(0.15f));
	}
}

void CurvesEditor::paintCurve(Painter& painter, const Layout& area) const {
	const auto& curve = points();
	if(const std::uint64_t key = curveKey(curve, interpolation_); key != samples_->key) {
		for(std::size_t index = 0; index < sampleCount; ++index) {
			const double x = static_cast<double>(index) / (sampleCount - 1);
			samples_->values[index] = static_cast<float>(evaluateCurve(curve, interpolation_, x));
		}
		samples_->key = key;
	}
	static constexpr std::array<Color, curveChannelCount> tints{
	    Color{}, Color{0.85f, 0.2f, 0.2f, 1.0f}, Color{0.2f, 0.65f, 0.25f, 1.0f}, Color{0.2f, 0.4f, 0.9f, 1.0f}};
	const Color line = channel_ == CurveChannel::Rgb ? text_ : tints[static_cast<std::size_t>(channel_)];
	const Rect& plot = area.plot;
	const auto [columns, step] = columnsOf(plot.width(), 2.0f);
	float previous = plot.bottom - samples_->values.front() * plot.height();
	for(std::size_t column = 0; column < columns; ++column) {
		const std::size_t sample = column * (sampleCount - 1) / (columns - 1);
		const float y = plot.bottom - samples_->values[sample] * plot.height();
		const float x = plot.left + static_cast<float>(column) * step;
		painter.fillRect({x - 0.5f, std::min(previous, y) - 1.0f, x + step, std::max(previous, y) + 1.0f}, line);
		previous = y;
	}
	for(std::size_t index = 0; index < curve.size(); ++index) {
		const Vec2 at = toWindow(area, curve[index]);
		const bool selected = index == selected_;
		const Rect dot{at.x - markerRadius, at.y - markerRadius, at.x + markerRadius, at.y + markerRadius};
		const float fade = selected && offPlot_ ? 0.3f : 1.0f;
		painter.fillRoundedRect(dot, CornerRadii::all(markerRadius),
		                        (selected ? accent_ : colors::white).withAlpha(fade));
		painter.strokeRoundedRect(dot, CornerRadii::all(markerRadius), 1.0f, line.withAlpha(fade));
		if(selected && hasFocus()) detail::drawMarker(painter, at, markerRadius + 3.0f);
	}
}

void CurvesEditor::paintTabs(Painter& painter, const Layout& area) const {
	const Font* text = font();
	const float width = area.tabs.width() / static_cast<float>(curveChannelCount);
	for(std::size_t index = 0; index < curveChannelCount; ++index) {
		const float left = area.tabs.left + width * static_cast<float>(index);
		const Rect tab{left, area.tabs.top, left + width - 2.0f, area.tabs.bottom};
		const bool shown = static_cast<std::size_t>(channel_) == index;
		painter.fillRoundedRect(tab, CornerRadii::all(3.0f), shown ? accent_.withAlpha(0.18f) : text_.withAlpha(0.06f));
		if(shown && hasFocus()) painter.strokeRoundedRect(tab, CornerRadii::all(3.0f), 1.0f, accent_);
		if(!text) continue;
		const std::u32string_view label = tabLabels[index];
		const float baseline = detail::centredBaseline(tab.top, tab.height(), text->lineHeight(), text->ascent());
		const float x = std::round(tab.left + (tab.width() - text->measure(label)) / 2.0f);
		painter.drawText(*text, label, {x, baseline}, text_);
	}
}

} // namespace affresco
