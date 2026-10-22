/// @file
/// @brief CanvasView: its content, the camera, laying out children through the camera, and painting.

#include "CanvasViewInternals.hpp"

#include "affresco/render/Font.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

CanvasView::CanvasView() : internals_(std::make_unique<Internals>()) {}

CanvasView::~CanvasView() { stopListening(); }

void CanvasView::setModel(std::shared_ptr<const CanvasItemModel> model) {
	model_ = std::move(model);
	modelChanged();
}

void CanvasView::modelChanged() {
	Internals& internals = *internals_;
	internals.bounds.clear();
	internals.ids.clear();
	internals.extent.reset();
	const std::size_t count = model_ ? std::min(model_->size(), maximumItems) : 0;
	internals.bounds.reserve(count);
	internals.ids.reserve(count);
	for(std::size_t i = 0; i < count; ++i) {
		const CanvasItem& item = model_->item(i);
		internals.bounds.push_back(item.bounds);
		internals.ids.emplace_back(item.id, static_cast<std::uint32_t>(i));
		const bool finite = std::isfinite(item.bounds.left) && std::isfinite(item.bounds.top) &&
		                    std::isfinite(item.bounds.right) && std::isfinite(item.bounds.bottom);
		if(finite) internals.extent = internals.extent ? canvas::united(*internals.extent, item.bounds) : item.bounds;
	}
	std::ranges::sort(internals.ids);
	internals.index.build(internals.bounds);
	invalidatePaint();
}

void CanvasView::setLevels(const std::string& childId, std::vector<CanvasLevel> levels) {
	const auto found = std::ranges::find(levels_, childId, &std::pair<std::string, std::vector<CanvasLevel>>::first);
	if(found != levels_.end()) {
		if(levels.empty()) levels_.erase(found);
		else found->second = std::move(levels);
	} else if(!levels.empty()) {
		levels_.emplace_back(childId, std::move(levels));
	}
	invalidate();
}

void CanvasView::setCamera(Camera2D camera) {
	camera.zoom = Camera2D::clampZoom(camera.zoom);
	if(!std::isfinite(camera.x) || !std::isfinite(camera.y)) camera.x = camera.y = 0.0f;
	if(camera == camera_) return;
	camera_ = camera;
	invalidate();
}

void CanvasView::setSelection(std::vector<std::string> ids) {
	std::ranges::sort(ids);
	ids.erase(std::ranges::unique(ids).begin(), ids.end());
	if(ids.size() > maximumSelection) ids.resize(maximumSelection);
	if(ids == selection_) return;
	selection_ = std::move(ids);
	// Announced as a state of the canvas: assistive tools read the description after its name
	setAccessibleDescription(selection_.empty() ? std::string() : std::to_string(selection_.size()) + " selected");
	invalidate();
}

void CanvasView::setGuides(CanvasGuides guides) {
	for(std::vector<float>* axis : {&guides.vertical, &guides.horizontal}) {
		std::erase_if(*axis, [](float position) { return !std::isfinite(position); });
		if(axis->size() > maximumGuides) axis->resize(maximumGuides);
	}
	if(guides == guides_) return;
	guides_ = std::move(guides);
	invalidatePaint();
}

void CanvasView::fit(bool selectionOnly) {
	std::optional<Rect> area = contentBounds(selectionOnly && !selection_.empty());
	if(!area) return;
	const Vec2 size = resolvedSize();
	setCamera(Camera2D::fitted(*area, size, canvas::fitMargin));
	onViewChanged();
}

std::optional<Rect> CanvasView::contentBounds(bool selectionOnly) const {
	if(selectionOnly) {
		selectedRects(internals_->rects);
		std::optional<Rect> area;
		for(const Rect& rect : internals_->rects) area = area ? canvas::united(*area, rect) : rect;
		return area;
	}
	std::optional<Rect> area = internals_->extent;
	for(const auto& child : children()) {
		const Rect rect = worldBoundsOf(*child);
		area = area ? canvas::united(*area, rect) : rect;
	}
	return area;
}

void CanvasView::setGrid(float spacing) { setLook(grid_, std::isfinite(spacing) ? std::max(spacing, 0.0f) : 0.0f); }

void CanvasView::setSnap(bool snap) { snap_ = snap; }

void CanvasView::setRulers(bool rulers) { setLook(rulers_, rulers); }

void CanvasView::setTextFonts(std::vector<std::shared_ptr<const Font>> fonts) {
	std::erase(fonts, nullptr);
	std::ranges::sort(fonts, {}, [](const auto& font) { return font->pixelHeight(); });
	fonts_ = std::move(fonts);
	invalidatePaint();
}

void CanvasView::setAccentColor(Color color) { setLook(accent_, color); }

void CanvasView::itemsIn(Rect area, std::vector<std::uint32_t>& found) const { internals_->index.query(area, found); }

Vec2 CanvasView::worldAt(Vec2 point) const noexcept {
	return camera_.toWorld(Vec2{point.x - frame().left, point.y - frame().top});
}

const CanvasItem* CanvasView::modelItem(std::string_view id) const noexcept {
	const auto& ids = internals_->ids;
	const auto found = std::ranges::lower_bound(ids, id, {}, &std::pair<std::string_view, std::uint32_t>::first);
	if(found == ids.end() || found->first != id) return nullptr;
	return &model_->item(found->second);
}

bool CanvasView::isSelected(const std::string& id) const noexcept {
	return !id.empty() && std::ranges::binary_search(selection_, id);
}

Rect CanvasView::worldBoundsOf(const Panel& child) noexcept {
	const float left = child.x().value, top = child.y().value;
	return {left, top, left + child.width().value, top + child.height().value};
}

Rect CanvasView::previewed(const std::string& id, const Rect& bounds) const noexcept {
	const bool dragging = gesture_ == Gesture::move || gesture_ == Gesture::resize;
	if(!dragging || !isSelected(id)) return bounds;
	return canvas::carried(bounds, from_, to_);
}

Rect CanvasView::onScreen(const Rect& world) const noexcept {
	const Rect local = camera_.toScreen(world);
	return {local.left + frame().left, local.top + frame().top, local.right + frame().left, local.bottom + frame().top};
}

void CanvasView::selectedItems(std::vector<CanvasTransform>& out) const {
	out.clear();
	for(const auto& child : children())
		if(isSelected(child->id())) out.push_back({child->id(), worldBoundsOf(*child), 0.0f});
	for(const std::string& id : selection_)
		if(const CanvasItem* item = modelItem(id)) out.push_back({item->id, item->bounds, item->rotate});
}

void CanvasView::selectedRects(std::vector<Rect>& out) const {
	out.clear();
	for(const auto& child : children())
		if(isSelected(child->id())) out.push_back(previewed(child->id(), worldBoundsOf(*child)));
	for(const std::string& id : selection_)
		if(const CanvasItem* item = modelItem(id)) out.push_back(previewed(id, item->bounds));
}

std::optional<Rect> CanvasView::boundsOf(const std::vector<CanvasTransform>& items) noexcept {
	std::optional<Rect> area;
	for(const CanvasTransform& item : items) area = area ? canvas::united(*area, item.bounds) : item.bounds;
	return area;
}

/* Layout */

void CanvasView::arrange() {
	listen();
	placeChildren();
}

Panel* CanvasView::standIn(const Panel& child) const noexcept {
	if(child.id().empty()) return nullptr;
	const auto found = std::ranges::find(levels_, child.id(), &std::pair<std::string, std::vector<CanvasLevel>>::first);
	if(found == levels_.end()) return nullptr;
	// The most detailed level that applies: the smallest "below" still above the zoom
	const CanvasLevel* chosen = nullptr;
	for(const CanvasLevel& level : found->second)
		if(camera_.zoom < level.below && (!chosen || level.below < chosen->below)) chosen = &level;
	if(!chosen) return nullptr;
	for(const auto& sibling : children())
		if(sibling.get() != &child && sibling->id() == chosen->use) return sibling.get();
	return nullptr;
}

void CanvasView::placeChildren() {
	auto& standIns = internals_->standIns;
	standIns.clear();
	for(const auto& child : children())
		if(Panel* other = standIn(*child)) standIns.emplace_back(other, child.get());
	const auto place = [this](Panel& panel, std::optional<Rect> world) {
		// Hidden: no size, far outside the view, so it neither paints nor takes the pointer
		const Rect screen = world ? camera_.toScreen(*world) : Rect{-1e7f, -1e7f, -1e7f, -1e7f};
		panel.clearAnchor(Edge::Right);
		panel.clearAnchor(Edge::Bottom);
		panel.setAnchor(Edge::Left, px(screen.left));
		panel.setAnchor(Edge::Top, px(screen.top));
		arrangeSize(panel, std::max(screen.width(), 0.0f), std::max(screen.height(), 0.0f));
	};
	for(const auto& child : children()) {
		Panel& panel = *child;
		const auto standing = std::ranges::find(standIns, &panel, &std::pair<const Panel*, const Panel*>::first);
		const bool replaced = std::ranges::contains(standIns, &panel, &std::pair<const Panel*, const Panel*>::second);
		const bool anyLevel = std::ranges::any_of(levels_, [&panel](const auto& entry) {
			return std::ranges::contains(entry.second, panel.id(), &CanvasLevel::use);
		});
		if(standing != standIns.end())
			place(panel, previewed(standing->second->id(), worldBoundsOf(*standing->second)));
		else if(replaced || (anyLevel && !panel.id().empty())) place(panel, std::nullopt);
		else place(panel, previewed(panel.id(), worldBoundsOf(panel)));
	}
}

/* Painting */

void CanvasView::paint(Painter& painter) const {
	Panel::paint(painter);
	paintGrid(painter);
	drawn_ = 0;
	if(!model_ || internals_->bounds.empty()) return;
	const Vec2 size = resolvedSize();
	internals_->index.query(camera_.visible(size), internals_->found);
	for(const std::uint32_t index : internals_->found) {
		if(index >= model_->size()) break;
		paintItem(painter, model_->item(index));
		++drawn_;
	}
}

void CanvasView::paintGrid(Painter& painter) const {
	const float spacing = grid_ * camera_.zoom;
	if(!(spacing >= 8.0f)) return;
	const Color line = Color::hex(0xe5e7eb);
	const Rect& box = frame();
	const float firstX = std::ceil(camera_.x / grid_) * grid_, firstY = std::ceil(camera_.y / grid_) * grid_;
	for(float x = (firstX - camera_.x) * camera_.zoom; x <= box.width(); x += spacing)
		painter.fillRect({box.left + std::round(x), box.top, box.left + std::round(x) + 1.0f, box.bottom}, line);
	for(float y = (firstY - camera_.y) * camera_.zoom; y <= box.height(); y += spacing)
		painter.fillRect({box.left, box.top + std::round(y), box.right, box.top + std::round(y) + 1.0f}, line);
}

void CanvasView::paintItem(Painter& painter, const CanvasItem& item) const {
	const Rect box = onScreen(previewed(item.id, item.bounds));
	const CornerRadii radii = CornerRadii::all(std::max(item.radius, 0.0f) * camera_.zoom);
	if(radii.isSquare()) painter.fillRect(box, item.color);
	else painter.fillRoundedRect(box, radii, item.color);
	const float border = item.borderWidth * camera_.zoom;
	if(border >= 0.25f) painter.strokeRoundedRect(box, radii, std::max(border, 1.0f), item.borderColor);
	paintText(painter, item, box);
}

void CanvasView::paintText(Painter& painter, const CanvasItem& item, const Rect& box) const {
	if(item.text.empty()) return;
	const float height = item.textSize * camera_.zoom, inset = 8.0f * camera_.zoom;
	// The largest font no taller than the text on screen (or the window's, when none was given)
	const Font* font = nullptr;
	for(const auto& candidate : fonts_)
		if(candidate->pixelHeight() <= height) font = candidate.get();
	if(fonts_.empty() && window() && height >= canvas::readablePixels) font = window()->defaultFont();
	if(!font || height < canvas::readablePixels) {
		// Too small to read: a bar where the line would be, about as long as the text
		const float length = std::min(static_cast<float>(item.text.size()) * height * 0.5f, box.width() - 2.0f * inset);
		if(length <= 0.0f || height < 0.5f) return;
		const float top = box.top + inset + height * 0.2f;
		painter.fillRect({box.left + inset, top, box.left + inset + length, top + std::max(height * 0.6f, 1.0f)},
		                 item.textColor.withAlpha(item.textColor.a * 0.35f));
		return;
	}
	painter.drawText(*font, item.text, {box.left + inset, box.top + inset + font->ascent()}, item.textColor);
}

void CanvasView::paintOver(Painter& painter) const {
	paintSelection(painter);
	if(gesture_ == Gesture::marquee) {
		const Rect marquee = canvas::spanning(pressedAt_, pointer_);
		painter.fillRect(marquee, accent_.withAlpha(0.08f));
		painter.strokeRoundedRect(marquee, {}, 1.0f, accent_);
	}
	paintGuides(painter);
	if(!rulers_) return;
	const Font* font = window() ? window()->defaultFont() : nullptr;
	for(const auto& candidate : fonts_)
		if(candidate->pixelHeight() <= 11.0f) font = candidate.get();
	paintRulers(painter, font, frame(), camera_, RulerStyle{}, internals_->ticks);
}

void CanvasView::paintSelection(Painter& painter) const {
	if(selection_.empty()) return;
	auto& rects = internals_->rects;
	selectedRects(rects);
	if(rects.empty()) return;
	Rect group = rects.front();
	for(const Rect& rect : rects) {
		group = canvas::united(group, rect);
		painter.strokeRoundedRect(onScreen(rect), {}, 1.0f, accent_.withAlpha(0.6f));
	}
	const Rect box = onScreen(group);
	painter.strokeRoundedRect(box, {}, 1.0f, accent_);
	const bool rotatable = rects.size() == 1 && modelItem(selection_.front()) != nullptr;
	for(int handle = 0; handle < (rotatable ? 9 : 8); ++handle) {
		const Vec2 at = canvas::handlePoint(box, handle);
		const float half = handleSize / 2.0f;
		const Rect square{at.x - half, at.y - half, at.x + half, at.y + half};
		painter.fillRoundedRect(square, CornerRadii::all(handle == 8 ? half : 0.0f), Color::hex(0xffffff));
		painter.strokeRoundedRect(square, CornerRadii::all(handle == 8 ? half : 0.0f), 1.0f, accent_);
	}
}

void CanvasView::paintGuides(Painter& painter) const {
	const Rect& box = frame();
	const Color guide = Color::hex(0x06b6d4), smart = Color::hex(0xec4899);
	for(const float x : guides_.vertical) {
		const float at = std::round(box.left + (x - camera_.x) * camera_.zoom);
		painter.fillRect({at, box.top, at + 1.0f, box.bottom}, guide);
	}
	for(const float y : guides_.horizontal) {
		const float at = std::round(box.top + (y - camera_.y) * camera_.zoom);
		painter.fillRect({box.left, at, box.right, at + 1.0f}, guide);
	}
	for(const auto& line : internals_->lines) {
		if(!line) continue;
		const Rect world = line->vertical ? Rect{line->position, line->from, line->position, line->to}
		                                  : Rect{line->from, line->position, line->to, line->position};
		Rect screen = onScreen(world);
		screen.left = std::round(screen.left);
		screen.top = std::round(screen.top);
		screen.right = std::max(std::round(screen.right), screen.left + 1.0f);
		screen.bottom = std::max(std::round(screen.bottom), screen.top + 1.0f);
		painter.fillRect(screen, smart);
	}
}

} // namespace affresco
