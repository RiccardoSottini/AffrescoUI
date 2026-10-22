/// @file
/// @brief Strokes stamped on the CPU: each touched tile keeps its pixels from before the stroke and what the stroke
/// covers of it, so flow builds up to the opacity and never past it, and each pixel is recomposed from its original.

#include "affresco/imaging/Brush.hpp"

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Blend.hpp"
#include "affresco/imaging/Selection.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace affresco::imaging {

namespace {

constexpr std::array<BrushPreset, 7> presets{{
    {"Hard Round 5", {5.0f, 1.0f, 0.25f, 1.0f, 1.0f, true, false}},
    {"Hard Round 20", {20.0f, 1.0f, 0.25f, 1.0f, 1.0f, true, false}},
    {"Soft Round 30", {30.0f, 0.0f, 0.25f, 1.0f, 1.0f, true, false}},
    {"Soft Round 100", {100.0f, 0.0f, 0.25f, 1.0f, 1.0f, true, false}},
    {"Airbrush 60", {60.0f, 0.0f, 0.1f, 1.0f, 0.1f, false, true}},
    {"Pencil 1", {1.0f, 1.0f, 0.25f, 1.0f, 1.0f, false, false}},
    {"Chalk 40", {40.0f, 0.5f, 0.5f, 0.8f, 0.6f, true, true}},
}};

/// @brief A colour's lightness, what it paints on a mask.
/// @param colour The colour.
[[nodiscard]] float lightness(const Pixel& colour) noexcept {
	return clamped(0.2126f * colour.red + 0.7152f * colour.green + 0.0722f * colour.blue);
}

/// @brief The most dabs a segment walks one by one. Beyond, a point thrown far off the image would cost billions of
/// dabs, and float steps that no longer move the position would never end, so only the stretch near it is walked.
constexpr double longWalk = 1 << 20;

/// @brief Where a dab can touch an image: its size, and how far from it a dab's centre still reaches it.
struct Area {
	double width;  ///< The image's width.
	double height; ///< Its height.
	double reach;  ///< The largest dab's radius, and a pixel more.
};

/// @brief Where a walk starts and how it steps, along its segment.
struct Spacing {
	double start; ///< The first dab's distance from the segment's start.
	double step;  ///< The distance between dabs.
};

/// @brief Walks a long segment's dabs in doubles, only those on the stretch where they can touch the image.
/// @details Dabs land within a pixel while the points are within about 2^44 of the image; past that, doubles place them
/// only roughly, and the walk still lays no more dabs than the stretch holds.
/// @param from The segment's start. @param to Its end. @param area Where a dab can touch the image.
/// @param spacing Where the dabs are. @param lay Lays a dab at a point and pressure.
/// @return The distance walked past the last dab, carried to the next segment.
template <class Lay>
float walkVisible(const StrokePoint& from, const StrokePoint& to, const Area& area, const Spacing& spacing, Lay&& lay) {
	const double dx = static_cast<double>(to.x) - from.x, dy = static_cast<double>(to.y) - from.y;
	const double length = std::hypot(dx, dy);
	// The share of the segment inside the image grown by the reach, edge by edge (Liang-Barsky)
	const std::array<std::array<double, 2>, 4> edges{{{-dx, from.x + area.reach},
	                                                  {dx, area.width + area.reach - from.x},
	                                                  {-dy, from.y + area.reach},
	                                                  {dy, area.height + area.reach - from.y}}};
	double near = 0.0, far = 1.0;
	for(const auto& [towards, room] : edges) {
		if(towards == 0.0) far = room < 0.0 ? -1.0 : far;
		else if(towards < 0.0) near = std::max(near, room / towards);
		else far = std::min(far, room / towards);
	}
	const double end = std::floor((length - spacing.start) / spacing.step);
	const double first = std::max(0.0, std::ceil((near * length - spacing.start) / spacing.step));
	const double last = std::min(end, std::floor((far * length - spacing.start) / spacing.step));
	// Counted from the first, since dab numbers past 2^53 no longer step by one
	const double count = std::min(last - first, (far - near) * length / spacing.step + 1.0);
	for(double dab = 0.0; dab <= count; ++dab) {
		const double share = (spacing.start + (first + dab) * spacing.step) / length;
		lay(static_cast<float>(from.x + dx * share), static_cast<float>(from.y + dy * share),
		    static_cast<float>(from.pressure + (static_cast<double>(to.pressure) - from.pressure) * share));
	}
	// The remainder past the last dab, exact however far the segment runs
	if(end < 0.0) return static_cast<float>(length - spacing.start + spacing.step);
	return static_cast<float>(std::fmod(length - spacing.start, spacing.step));
}

} // namespace

void BrushTip::normalize(const Limits& limits) noexcept {
	const auto finite = [](float value, float otherwise) { return std::isfinite(value) ? value : otherwise; };
	size = std::clamp(finite(size, 20.0f), 1.0f, static_cast<float>(std::max(limits.brushSize, 1)));
	hardness = clamped(finite(hardness, 0.8f));
	spacing = std::clamp(finite(spacing, 0.25f), 0.01f, 10.0f);
	opacity = clamped(finite(opacity, 1.0f));
	flow = clamped(finite(flow, 1.0f));
}

std::span<const BrushPreset> brushPresets() noexcept { return presets; }

float stampCoverage(float distance, float radius, float hardness) noexcept {
	const float fade = std::max(radius * (1.0f - hardness), 1.0f);
	const float share = clamped((radius - distance) / fade);
	return share * share * (3.0f - 2.0f * share);
}

Stroke::Stroke(Document& document, std::uint64_t target, const BrushTip& tip, StrokeMode mode, const Pixel& colour,
               std::string name)
    : document_(&document), target_(target), tip_(tip), mode_(mode), colour_(colour), mask_(false),
      selected_(document.hasSelection() && target != Document::selectionId) {
	tip_.normalize(document.limits());
	if(const auto refused = document.paintable(target); refused) throw DocumentError(*refused);
	const Image* image = document.image(target);
	if(image == nullptr) throw DocumentError("imaging: no image to paint");
	mask_ = image->format() == PixelFormat::mask8;
	document.begin(std::move(name));
}

Stroke::~Stroke() { cancel(); }

void Stroke::add(const StrokePoint& given) {
	if(document_ == nullptr || !std::isfinite(given.x) || !std::isfinite(given.y)) return;
	StrokePoint point = given;
	point.pressure = clamped(std::isfinite(point.pressure) ? point.pressure : 1.0f);
	if(!started_) {
		started_ = true, last_ = point, travelled_ = 0.0f;
		stamp(point.x, point.y, point.pressure);
		return;
	}
	const float length = std::hypot(point.x - last_.x, point.y - last_.y);
	const float step = std::max(tip_.spacing * tip_.size, 0.5f);
	float position = step - travelled_;
	if(static_cast<double>(length) / step > longWalk) {
		const Image& image = *document_->image(target_);
		const Area area{static_cast<double>(image.width()), static_cast<double>(image.height()), tip_.size / 2.0 + 1.0};
		travelled_ = walkVisible(last_, point, area, {position, step},
		                         [this](float x, float y, float pressure) { stamp(x, y, pressure); });
		last_ = point;
		return;
	}
	for(; position <= length; position += step) {
		const float share = position / length;
		stamp(last_.x + (point.x - last_.x) * share, last_.y + (point.y - last_.y) * share,
		      last_.pressure + (point.pressure - last_.pressure) * share);
	}
	travelled_ = length - (position - step);
	last_ = point;
}

void Stroke::finish() {
	if(document_ == nullptr) return;
	document_->commit(target_ == Document::selectionId ? std::optional<bool>(selectsAnything(document_->selection()))
	                                                   : std::nullopt);
	document_ = nullptr;
}

void Stroke::cancel() {
	if(document_ == nullptr) return;
	document_->cancel();
	document_ = nullptr;
}

void Stroke::stamp(float x, float y, float pressure) {
	const float radius = tip_.size / 2.0f * (tip_.pressureSize ? std::max(pressure, 0.05f) : 1.0f);
	const float cap = tip_.opacity * (tip_.pressureOpacity ? pressure : 1.0f);
	if(cap <= 0.0f) return;
	const Image& image = *document_->image(target_);
	if(x + radius < 0.0f || y + radius < 0.0f) return;
	// Past the right or bottom no tile is touched; the dab still counts, and its far edge is never cast to an int
	if(x - radius >= static_cast<float>(image.width()) || y - radius >= static_cast<float>(image.height())) {
		++stamps_;
		return;
	}
	const int left = std::max(0, static_cast<int>(std::floor(x - radius)) / tileSize);
	const int top = std::max(0, static_cast<int>(std::floor(y - radius)) / tileSize);
	const int right = std::min(image.columns() - 1, static_cast<int>(std::floor(x + radius)) / tileSize);
	const int bottom = std::min(image.rows() - 1, static_cast<int>(std::floor(y + radius)) / tileSize);
	for(int row = top; row <= bottom; ++row)
		for(int column = left; column <= right; ++column) stampTile(column, row, x, y, radius, cap);
	++stamps_;
}

void Stroke::stampTile(int column, int row, float x, float y, float radius, float cap) {
	const Image& image = *document_->image(target_);
	const int tileLeft = column * tileSize, tileTop = row * tileSize;
	const int left = std::max({tileLeft, static_cast<int>(std::floor(x - radius)), 0});
	const int right = std::min({tileLeft + tileSize, static_cast<int>(std::ceil(x + radius)) + 1, image.width()});
	const int top = std::max({tileTop, static_cast<int>(std::floor(y - radius)), 0});
	const int bottom = std::min({tileTop + tileSize, static_cast<int>(std::ceil(y + radius)) + 1, image.height()});
	if(left >= right || top >= bottom) return;
	const std::shared_ptr<const Tile> selection = selected_ ? document_->selection().tile({0, column, row}) : nullptr;
	if(selected_ && !selection) return;
	Touched& share = touched(column, row);
	Tile& tile = document_->editTile(target_, column, row);
	for(int py = top; py < bottom; ++py)
		for(int px = left; px < right; ++px) {
			const auto at = static_cast<std::size_t>((py - tileTop) * tileSize + px - tileLeft);
			float laid = stampCoverage(std::hypot(static_cast<float>(px) + 0.5f - x, static_cast<float>(py) + 0.5f - y),
			                           radius, tip_.hardness) *
			             tip_.flow;
			if(selection) laid *= unitFromByte(selection->bytes()[at]);
			float& covered = share.coverage[at];
			if(laid <= 0.0f || covered >= cap) continue;
			covered += (cap - covered) * laid;
			const Pixel under = share.original->pixel(px - tileLeft, py - tileTop);
			tile.setPixel(px - tileLeft, py - tileTop, painted(under, px, py, covered));
		}
}

Stroke::Touched& Stroke::touched(int column, int row) {
	const TileKey key{0, column, row};
	auto found = touched_.find(key);
	if(found != touched_.end()) return found->second;
	const Tile& tile = document_->editTile(target_, column, row);
	return touched_.emplace(key, Touched{std::make_shared<const Tile>(tile), std::vector<float>(tilePixels, 0.0f)})
	    .first->second;
}

Pixel Stroke::original(int x, int y) const {
	const Image& image = *document_->image(target_);
	if(x < 0 || y < 0 || x >= image.width() || y >= image.height()) return {};
	const auto found = touched_.find({0, x / tileSize, y / tileSize});
	if(found != touched_.end()) return found->second.original->pixel(x % tileSize, y % tileSize);
	return image.pixel(x, y);
}

Pixel Stroke::painted(const Pixel& under, int x, int y, float coverage) const {
	if(mask_) {
		float value = 0.0f;
		if(mode_ == StrokeMode::paint) value = lightness(colour_);
		else if(mode_ == StrokeMode::clone)
			value =
			    original(x + static_cast<int>(std::lround(cloneX_)), y + static_cast<int>(std::lround(cloneY_))).alpha;
		const float mixed = under.alpha + (value - under.alpha) * coverage;
		return {mixed, mixed, mixed, mixed};
	}
	if(mode_ == StrokeMode::erase) return {under.red, under.green, under.blue, under.alpha * (1.0f - coverage)};
	Pixel source = colour_;
	if(mode_ == StrokeMode::clone)
		source = original(x + static_cast<int>(std::lround(cloneX_)), y + static_cast<int>(std::lround(cloneY_)));
	source.alpha *= coverage;
	return blendPixels(under, source, BlendMode::normal, 1.0f);
}

} // namespace affresco::imaging
