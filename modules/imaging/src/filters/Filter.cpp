/// @file
/// @brief Filters run tile by tile: each tile read with its halo from the tiles as they were before the run, filtered
/// premultiplied, and mixed with what was there by the selection.

#include "affresco/imaging/Filter.hpp"

#include "Kernels.hpp"
#include "affresco/core/Json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace affresco::imaging {

namespace {

constexpr std::array<std::string_view, 5> names{"gaussian-blur", "sharpen", "unsharp-mask", "add-noise", "motion-blur"};

/// @brief The step's name of a filter.
/// @param kind The filter.
[[nodiscard]] std::string stepName(FilterKind kind) {
	constexpr std::array<std::string_view, 5> titles{"Gaussian Blur", "Sharpen", "Unsharp Mask", "Add Noise",
	                                                 "Motion Blur"};
	return std::string(titles[static_cast<std::size_t>(kind)]);
}

/// @brief The unsharp settings sharpen uses.
/// @param filter The filter.
[[nodiscard]] Filter sharpenedAs(const Filter& filter) noexcept {
	if(filter.kind != FilterKind::sharpen) return filter;
	Filter unsharp = filter;
	unsharp.kind = FilterKind::unsharpMask, unsharp.radius = 0.7f, unsharp.amount = 0.6f, unsharp.threshold = 0;
	return unsharp;
}

/// @brief The filter a run applies: sharpen as its unsharp settings, normalised to the limits.
/// @param filter The filter asked for.
/// @param limits The document's limits.
[[nodiscard]] Filter runFilter(const Filter& filter, const Limits& limits) noexcept {
	Filter run = sharpenedAs(filter);
	run.normalize(limits);
	return run;
}

} // namespace

std::span<const std::string_view> filterKindNames() noexcept { return names; }

std::optional<FilterKind> filterKindOf(std::string_view name) noexcept {
	for(std::size_t index = 0; index < names.size(); ++index)
		if(names[index] == name) return static_cast<FilterKind>(index);
	return std::nullopt;
}

void Filter::normalize(const Limits& limits) noexcept {
	const auto most = static_cast<float>(std::max(limits.filterRadius, 1));
	const auto finite = [](float value, float otherwise) { return std::isfinite(value) ? value : otherwise; };
	radius = std::clamp(finite(radius, 2.0f), 0.1f, most);
	amount = std::clamp(finite(amount, 1.0f), 0.0f, kind == FilterKind::addNoise ? 1.0f : 5.0f);
	threshold = std::clamp(threshold, 0, 255);
	angle = std::fmod(finite(angle, 0.0f), 360.0f);
	distance = std::clamp(finite(distance, 10.0f), 1.0f, most);
}

int haloOf(const Filter& filter) noexcept {
	const Filter effective = sharpenedAs(filter);
	switch(effective.kind) {
	case FilterKind::addNoise:
		return 0;
	case FilterKind::motionBlur:
		return static_cast<int>(std::ceil(effective.distance / 2.0f)) + 2;
	default: {
		int halo = 0;
		for(const int radius : kernels::gaussianBoxes(effective.radius)) halo += radius;
		return halo + 1;
	}
	}
}

FilterRun::FilterRun(Document& document, std::uint64_t target, const Filter& filter, std::string name)
    : document_(&document), target_(target), filter_(runFilter(filter, document.limits())), halo_(haloOf(filter_)) {
	if(const auto refused = document.paintable(target); refused) throw DocumentError(*refused);
	if(document.image(target) == nullptr) throw DocumentError("imaging: no image to filter");
	document.begin(std::move(name));
}

FilterRun::~FilterRun() { cancel(); }

bool FilterRun::runRegion(int x, int y, int width, int height, std::size_t most) {
	if(document_ == nullptr) return true;
	const Image& image = *document_->image(target_);
	// The ends in 64 bits, so a region reaching past INT_MAX still covers the tiles it touches
	const auto end = [](int start, int extent) { return (std::int64_t{start} + extent + tileSize - 1) / tileSize; };
	const int left = std::max(0, x / tileSize), top = std::max(0, y / tileSize);
	const auto right = static_cast<int>(std::min<std::int64_t>(image.columns(), end(x, width)));
	const auto bottom = static_cast<int>(std::min<std::int64_t>(image.rows(), end(y, height)));
	std::size_t ran = 0;
	for(int row = top; row < bottom; ++row)
		for(int column = left; column < right; ++column) {
			if(done_.contains({column, row})) continue;
			if(ran++ == most) return false;
			runTile(column, row);
		}
	return true;
}

void FilterRun::finish() {
	if(document_ == nullptr) return;
	runRegion(0, 0, document_->width(), document_->height());
	document_->commit();
	document_ = nullptr;
}

void FilterRun::cancel() {
	if(document_ == nullptr) return;
	document_->cancel();
	document_ = nullptr;
}

std::shared_ptr<const Tile> FilterRun::original(int column, int row) const {
	if(const auto found = originals_.find({0, column, row}); found != originals_.end()) return found->second;
	return document_->image(target_)->tile({0, column, row});
}

bool FilterRun::readRegion(int column, int row) {
	const Image& image = *document_->image(target_);
	const int side = tileSize + 2 * halo_;
	const bool premultiply = image.format() != PixelFormat::mask8;
	region_.resize(static_cast<std::size_t>(side) * static_cast<std::size_t>(side) * 4);
	bool any = false;
	for(int y = 0; y < side; ++y) {
		const int imageY = std::clamp(row * tileSize - halo_ + y, 0, image.height() - 1);
		int tileColumn = -1;
		std::shared_ptr<const Tile> tile;
		for(int x = 0; x < side; ++x) {
			const int imageX = std::clamp(column * tileSize - halo_ + x, 0, image.width() - 1);
			if(imageX / tileSize != tileColumn)
				tileColumn = imageX / tileSize, tile = original(tileColumn, imageY / tileSize);
			const Pixel pixel = pixelOf(tile.get(), imageX % tileSize, imageY % tileSize);
			const float alpha = premultiply ? pixel.alpha : 1.0f;
			float* to = &region_[static_cast<std::size_t>(y * side + x) * 4];
			to[0] = pixel.red * alpha, to[1] = pixel.green * alpha, to[2] = pixel.blue * alpha, to[3] = pixel.alpha;
			any = any || pixel.alpha != 0.0f;
		}
	}
	return any;
}

void FilterRun::runTile(int column, int row) {
	done_.insert({column, row});
	const bool masked = document_->hasSelection() && target_ != Document::selectionId;
	const std::shared_ptr<const Tile> selected = masked ? document_->selection().tile({0, column, row}) : nullptr;
	if((masked && !selected) || !readRegion(column, row)) return;
	filterTile(column, row);
	storeTile(column, row, selected.get());
}

void FilterRun::filterTile(int column, int row) {
	const int side = tileSize + 2 * halo_;
	const auto copyCentre = [&](std::vector<float>& to) {
		to.resize(tilePixels * 4);
		for(int y = 0; y < tileSize; ++y)
			std::copy_n(&region_[static_cast<std::size_t>((y + halo_) * side + halo_) * 4], tileSize * 4,
			            &to[static_cast<std::size_t>(y * tileSize) * 4]);
	};
	copyCentre(before_);
	if(filter_.kind == FilterKind::motionBlur) {
		kernels::motionBlur(region_, halo_, filter_.angle, filter_.distance, after_);
		return;
	}
	if(filter_.kind == FilterKind::addNoise) {
		after_ = before_;
		for(std::size_t at = 0; at < tilePixels; ++at) {
			const int x = column * tileSize + static_cast<int>(at % tileSize);
			const int y = row * tileSize + static_cast<int>(at / tileSize);
			for(int channel = 0; channel < 3; ++channel)
				after_[at * 4 + static_cast<std::size_t>(channel)] +=
				    filter_.amount * before_[at * 4 + 3] *
				    kernels::noise(filter_.seed, x, y, filter_.monochrome ? 0 : channel);
		}
		return;
	}
	kernels::gaussianBlur(region_, side, filter_.radius, scratch_);
	copyCentre(after_);
	if(filter_.kind != FilterKind::unsharpMask) return;
	for(std::size_t at = 0; at < after_.size(); ++at) {
		const float detail = before_[at] - after_[at];
		const bool sharpened = std::abs(detail) * 255.0f >= static_cast<float>(filter_.threshold);
		after_[at] = sharpened ? before_[at] + filter_.amount * detail : before_[at];
	}
}

void FilterRun::storeTile(int column, int row, const Tile* selected) {
	auto tile = std::make_shared<Tile>(document_->image(target_)->format());
	const bool premultiplied = tile->format() != PixelFormat::mask8;
	const auto straight = [premultiplied](const float* from) {
		const float alpha = clamped(from[3]);
		const float divide = !premultiplied ? 1.0f : alpha > 0.0f ? 1.0f / alpha : 0.0f;
		return clamped(Pixel{from[0] * divide, from[1] * divide, from[2] * divide, alpha});
	};
	bool any = false;
	for(std::size_t at = 0; at < tilePixels; ++at) {
		Pixel pixel = straight(&after_[at * 4]);
		if(selected != nullptr) {
			const float keep = 1.0f - unitFromByte(selected->bytes()[at]);
			const Pixel old = straight(&before_[at * 4]);
			pixel = {pixel.red + (old.red - pixel.red) * keep, pixel.green + (old.green - pixel.green) * keep,
			         pixel.blue + (old.blue - pixel.blue) * keep, pixel.alpha + (old.alpha - pixel.alpha) * keep};
		}
		tile->setPixel(static_cast<int>(at % tileSize), static_cast<int>(at / tileSize), pixel);
		any = any || pixel.alpha > 0.0f;
	}
	originals_.try_emplace({0, column, row}, document_->image(target_)->tile({0, column, row}));
	document_->replaceTile(target_, column, row, any ? std::shared_ptr<const Tile>(std::move(tile)) : nullptr);
}

void applyFilter(Document& document, std::uint64_t target, const Filter& filter) {
	FilterRun run(document, target, filter, stepName(filter.kind));
	run.finish();
}

void featherSelection(Document& document, float radius) {
	if(!document.hasSelection() || !(radius > 0.0f)) return;
	Filter blur;
	blur.radius = radius;
	FilterRun run(document, Document::selectionId, blur, "Feather");
	run.finish();
}

} // namespace affresco::imaging
