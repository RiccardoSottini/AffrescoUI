/// @file
/// @brief SurfaceView: the panel that shows a GpuSurface. Its state is shared with the renderer (gl::Surfaces), which
/// draws the surface when that state's version changes.

#include "affresco/render/GpuSurface.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "gl/SurfaceState.hpp"

#include <utility>

namespace affresco {

namespace {

constexpr double frameMs = 1000.0 / 60.0; ///< A continuous surface's redraw interval.

} // namespace

SurfaceView::SurfaceView() : state_(std::make_shared<detail::SurfaceState>()) {
	setColor(colors::transparent);
	state_->view = this;
}

SurfaceView::~SurfaceView() {
	state_->view = nullptr;
	if(timer_ != 0 && window()) window()->cancelTimer(timer_);
}

void SurfaceView::setSurface(const std::string& name, SurfaceFactory factory, SurfaceOptions options) {
	if(name == state_->name && state_->factory) return;
	state_->name = name;
	state_->factory = std::move(factory);
	state_->options = options;
	state_->failed = false;
	++state_->surface;
	redraw();
}

const std::string& SurfaceView::surfaceName() const noexcept { return state_->name; }

void SurfaceView::setParams(const json& params) {
	if(params == state_->params) return;
	state_->params = params.is_object() ? params : json::object();
	redraw();
}

const json& SurfaceView::params() const noexcept { return state_->params; }

void SurfaceView::setContinuous(bool continuous) {
	if(continuous == state_->continuous) return;
	state_->continuous = continuous;
	if(!continuous && timer_ != 0 && window()) window()->cancelTimer(timer_);
	if(!continuous) timer_ = 0;
	redraw();
}

bool SurfaceView::continuous() const noexcept { return state_->continuous; }

bool SurfaceView::failed() const noexcept { return state_->failed; }

void SurfaceView::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& rect = frame();
	state_->size = {rect.width(), rect.height()};
	if(state_->time == 0.0 && window()) state_->time = window()->clock().seconds();
	painter.drawSurface(*this, rect);
	if(state_->continuous) scheduleNext();
}

void SurfaceView::mouseMoveEvent(const MouseMoveEvent& event) {
	Panel::mouseMoveEvent(event);
	push({PointerKind::move, event.position, MouseButton::Left, {}});
}

void SurfaceView::mouseButtonEvent(const MouseButtonEvent& event) {
	Panel::mouseButtonEvent(event);
	const PointerKind kind = event.action == Action::Release ? PointerKind::release : PointerKind::press;
	push({kind, event.position, event.button, {}});
}

void SurfaceView::scrollEvent(const ScrollEvent& event) {
	Panel::scrollEvent(event);
	push({PointerKind::scroll, event.position, MouseButton::Left, event.offset});
}

// Window units to pixels from the top-left corner; the oldest events go first when too many wait
void SurfaceView::push(SurfacePointer pointer) {
	const float scale = window() ? window()->contentScale() : 1.0f;
	pointer.position = {(pointer.position.x - frame().left) * scale, (pointer.position.y - frame().top) * scale};
	std::vector<SurfacePointer>& input = state_->input;
	if(input.size() >= detail::SurfaceState::maxInput) input.erase(input.begin());
	input.push_back(pointer);
	redraw();
}

void SurfaceView::redraw() noexcept {
	++state_->version;
	if(window()) state_->time = window()->clock().seconds();
	invalidatePaint();
}

// The timer holds the state weakly: once the view is gone, it does nothing
void SurfaceView::scheduleNext() const {
	if(timer_ != 0 || !window()) return;
	timer_ = window()->setTimer(frameMs, [weak = std::weak_ptr<detail::SurfaceState>(state_)] {
		const std::shared_ptr<detail::SurfaceState> state = weak.lock();
		if(!state || !state->view) return;
		state->view->timer_ = 0;
		state->view->redraw();
	});
}

} // namespace affresco
