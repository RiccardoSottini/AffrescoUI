/// @file
/// @brief RasterView: the image it shows, kept in step with the store, and the pointer played through the current
/// tool (its overlays are in RasterOverlays.cpp).

#include "RasterView.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

namespace affresco::format {

RasterView::RasterView(std::weak_ptr<editing::ImageDocuments> images) : images_(std::move(images)) {}

RasterView::~RasterView() {
	if(const std::shared_ptr<editing::ImageDocuments> images = images_.lock(); images && !name_.empty())
		images->unwatch(name_, *this);
}

bool RasterView::show(const std::string& name, const RasterLook& look) {
	const std::shared_ptr<editing::ImageDocuments> images = images_.lock();
	if(!images) return false;
	look_ = look;
	if(name != name_) {
		if(!name_.empty()) images->unwatch(name_, *this);
		name_ = name;
		shown_.reset();
		images->watch(name_, *this);
	}
	editing::ImageDocument* declared = images->declared(name_); // made, or made again, the first time: told then
	if(declared == nullptr) return false;
	imageChanged(*declared);
	return true;
}

// A new canvas gets a surface of its own name (SurfaceView::setSurface() keeps a surface of the same name)
void RasterView::imageChanged(editing::ImageDocument& image) {
	if(shown_.lock() != image.canvas) {
		shown_ = image.canvas;
		setSurface("raster:" + name_ + ":" + std::to_string(++surfaces_), imaging::canvasSurfaceFactory(image.canvas));
		if(size(image)) { // fitted: the screen hears of the view, and this is told again
			if(const std::shared_ptr<editing::ImageDocuments> images = images_.lock()) images->changed(image, true);
			return;
		}
	}
	setParams({{"zoom", image.view.zoom},
	           {"x", image.view.x},
	           {"y", image.view.y},
	           {"ants", true},
	           {"quickMask", image.settings.quickMask},
	           {"checker", look_.checker},
	           {"scale", scale()}, // the view is in the window's pixels: a capture at another scale zooms by the ratio
	           {"change", ++changes_}});
	setContinuous(image.canvas->document().hasSelection()); // the ants march
	invalidatePaint();                                      // the overlays
}

editing::ImageDocument* RasterView::image() const {
	const std::shared_ptr<editing::ImageDocuments> images = images_.lock();
	return images ? images->find(name_) : nullptr;
}

bool RasterView::size(editing::ImageDocument& image) const {
	const Rect& rect = frame();
	const int width = static_cast<int>(std::lround(rect.width() * scale()));
	const int height = static_cast<int>(std::lround(rect.height() * scale()));
	if(width <= 0 || height <= 0) return false;
	imaging::CanvasView& drawn = image.canvas->view; // the surface sets it again as it draws, to the same size
	const bool first = drawn.width == 0;             // no view has sized or drawn this canvas yet
	drawn.width = width;
	drawn.height = height;
	image.view.width = width;
	image.view.height = height;
	if(first) editing::fitView(image);
	return first;
}

void RasterView::arrange() {
	editing::ImageDocument* shown = image();
	if(shown == nullptr || shown->canvas != shown_.lock()) return;
	const int width = shown->view.width;
	const int height = shown->view.height;
	const bool fitted = size(*shown);
	if(!fitted && shown->view.width == width && shown->view.height == height) return;
	try {
		if(const std::shared_ptr<editing::ImageDocuments> images = images_.lock()) images->changed(*shown, fitted);
	} catch(const std::exception& error) {
		log(LogLevel::Warning, "AffrescoUI screen: raster \"" + name_ + "\": " + error.what());
	}
}

float RasterView::scale() const noexcept { return window() ? window()->contentScale() : 1.0f; }

// The same path as the "pointer" operation: editing::applyPointer(), then the store tells the screen and the views
void RasterView::play(SurfacePointer pointer) {
	const std::shared_ptr<editing::ImageDocuments> images = images_.lock();
	editing::ImageDocument* shown = images ? images->find(name_) : nullptr;
	if(shown == nullptr) return;
	const float pixels = scale();
	pointer.position = {(pointer.position.x - frame().left) * pixels, (pointer.position.y - frame().top) * pixels};
	const auto [x, y] = shown->view.toDocument(pointer.position.x, pointer.position.y);
	try {
		images->changed(*shown, editing::applyPointer(*shown, imaging::CanvasPointer{pointer, x, y}));
	} catch(const std::exception& error) {
		log(LogLevel::Warning, "AffrescoUI screen: raster \"" + name_ + "\": " + error.what());
	}
}

// A move with no button down changes nothing but the brush's outline and the polygon lasso's last edge
void RasterView::mouseMoveEvent(const MouseMoveEvent& event) {
	Panel::mouseMoveEvent(event); // not SurfaceView's: the canvas surface runs no tools
	pointer_ = event.position;
	hovered_ = true;
	const editing::ImageDocument* shown = image();
	if(shown != nullptr && shown->press.down) play({PointerKind::move, event.position, MouseButton::Left, {}, mods_});
	else invalidatePaint();
}

void RasterView::mouseButtonEvent(const MouseButtonEvent& event) {
	Panel::mouseButtonEvent(event);
	mods_ = event.mods;
	pointer_ = event.position;
	const PointerKind kind = event.action == Action::Release ? PointerKind::release : PointerKind::press;
	play({kind, event.position, event.button, {}, event.mods});
}

void RasterView::scrollEvent(const ScrollEvent& event) {
	Panel::scrollEvent(event);
	play({PointerKind::scroll, event.position, MouseButton::Left, event.offset, mods_});
}

void RasterView::keyEvent(const KeyEvent& event) {
	if(editing::ImageDocument* shown = image(); shown != nullptr && editing::typingInto(*shown) &&
	   event.action != Action::Release && !event.mods.super() && !event.mods.control()) {
		const bool erase = event.key == Key::Backspace, newline = event.key == Key::Enter || event.key == Key::KeypadEnter;
		if(event.key == Key::Escape) shown->typing = false; // done typing: the tool's clicks add or pick type again
		if(erase || newline) type(*shown, erase ? "" : "\n");
		if(erase || newline || event.key == Key::Escape) {
			invalidatePaint();
			return;
		}
	}
	if(event.key == Key::Space)
		if(editing::ImageDocument* shown = image()) {
			shown->spaceHeld = event.action != Action::Release;
			invalidatePaint(); // the brush's outline hides while it pans
		}
	SurfaceView::keyEvent(event);
}

namespace {

/// @brief A character as UTF-8.
/// @param codepoint The character (a surrogate or one past U+10FFFF becomes U+FFFD).
[[nodiscard]] std::string utf8Of(char32_t codepoint) {
	if((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) codepoint = 0xFFFD;
	std::string out;
	if(codepoint < 0x80) {
		out += static_cast<char>(codepoint);
	} else if(codepoint < 0x800) {
		out += static_cast<char>(0xC0 | (codepoint >> 6));
		out += static_cast<char>(0x80 | (codepoint & 0x3F));
	} else if(codepoint < 0x10000) {
		out += static_cast<char>(0xE0 | (codepoint >> 12));
		out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (codepoint & 0x3F));
	} else {
		out += static_cast<char>(0xF0 | (codepoint >> 18));
		out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	return out;
}

} // namespace

void RasterView::charEvent(const CharEvent& event) {
	if(editing::ImageDocument* shown = image(); shown != nullptr && editing::typingInto(*shown) &&
	   event.codepoint >= 0x20 && event.codepoint != 0x7F)
		type(*shown, utf8Of(event.codepoint));
	SurfaceView::charEvent(event);
}

void RasterView::type(editing::ImageDocument& image, std::string_view added) {
	const std::shared_ptr<editing::ImageDocuments> images = images_.lock();
	try {
		editing::typeText(image, added);
		if(images) images->changed(image, true);
	} catch(const std::exception& error) {
		image.message = error.what();
		log(LogLevel::Warning, "AffrescoUI screen: raster \"" + name_ + "\": " + error.what());
	}
}

bool RasterView::typesText() const {
	const editing::ImageDocument* shown = image();
	return shown != nullptr && editing::typingInto(*shown);
}

bool typesIntoImage(const Panel* panel) {
	const auto* view = dynamic_cast<const RasterView*>(panel);
	return view != nullptr && view->typesText();
}

void RasterView::focusEvent(bool focused) {
	if(editing::ImageDocument* shown = image(); shown != nullptr && !focused) shown->spaceHeld = false;
	SurfaceView::focusEvent(focused);
}

void RasterView::hoverEvent(bool hovered) {
	hovered_ = hovered;
	invalidatePaint();
	SurfaceView::hoverEvent(hovered);
}

} // namespace affresco::format
