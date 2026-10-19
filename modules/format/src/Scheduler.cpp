/// @file
/// @brief format::Scheduler: timers, animations, debounces, transitions and deferred tasks.

#include "Scheduler.hpp"
#include "Engine.hpp"

#include "affresco/render/Label.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Sets one of the colours a transition can animate.
/// @param panel The panel; a label for "textColor".
/// @param property "color", "textColor" or "borderColor".
/// @param color The colour.
void setColorOf(Panel& panel, std::string_view property, Color color) {
	if(property == "textColor") {
		if(auto* label = dynamic_cast<Label*>(&panel)) label->setTextColor(color);
	} else if(property == "borderColor") panel.setBorder(panel.borderWidth(), color);
	else panel.setColor(color);
}

/// @brief The transition of one of a node's colours.
/// @param record The node's record.
/// @param property "color", "textColor" or "borderColor".
/// @return Its tween.
Tween& tweenOf(Record& record, std::string_view property) {
	if(property == "textColor") return record.textTween;
	if(property == "borderColor") return record.borderTween;
	return record.colorTween;
}

} // namespace

void Scheduler::defer(std::function<void()> task) const {
	engine_.window->defer([alive = alive_, task = std::move(task)] {
		if(*alive) task();
	});
}

void Scheduler::once(double delayMs, std::function<void()> task) {
	const auto id = std::make_shared<std::uint64_t>(); // known once set, before it can run
	*id = engine_.window->setTimer(delayMs, [this, id, task = std::move(task)] {
		once_.erase(*id);
		task();
	});
	once_.insert(*id);
}

void Scheduler::debounce(const std::string& key, double delayMs, std::function<void()> task) {
	Window& window = *engine_.window;
	if(const auto it = debounces_.find(key); it != debounces_.end()) window.cancelTimer(it->second);
	debounces_[key] = window.setTimer(delayMs, [this, key, task = std::move(task)] {
		debounces_.erase(key);
		task();
	});
}

std::uint64_t Scheduler::repeating(double delayMs, std::function<void(const Rearm&)> tick) {
	auto task = std::make_shared<std::function<void()>>();
	*task = [this, tick = std::move(tick), weak = std::weak_ptr(task)] {
		const Rearm rearm = [this, &weak](double ms) {
			return engine_.window->setTimer(ms, [self = weak.lock()] { (*self)(); });
		};
		tick(rearm);
	};
	return engine_.window->setTimer(delayMs, [task] { (*task)(); });
}

// A number in the state moves to "to" over "ms", one frame at a time
void Scheduler::animate(const json::json_pointer& pointer, double from, const json& to, double ms, std::string easing) {
	const std::string key = pointer.to_string();
	if(const auto it = animations_.find(key); it != animations_.end()) engine_.window->cancelTimer(it->second);
	animations_[key] = repeating(frameMs, [this, pointer, key, from, to, ms, easing = std::move(easing),
	                                       start = engine_.now()](const Rearm& rearm) {
		const double progress = std::min((engine_.now() - start) * 1000.0 / ms, 1.0);
		engine_.versions.state()[pointer] =
		    progress >= 1.0 ? to : json(from + (to.get<double>() - from) * ease(easing, progress));
		engine_.document.markStateChanged();
		engine_.reconciler.requestReconcile();
		if(progress < 1.0) animations_[key] = rearm(frameMs);
		else animations_.erase(key);
	});
}

void Scheduler::every(Panel& panel, Record& record, double everyMs) {
	if(everyMs == record.everyMs) return;
	Window& window = *engine_.window;
	if(record.everyTimer) window.cancelTimer(std::exchange(record.everyTimer, 0));
	record.everyMs = everyMs;
	if(everyMs <= 0.0) return;
	record.everyTimer = repeating(everyMs, [this, panel = &panel](const Rearm& rearm) {
		Record* r = engine_.reconciler.recordOf(panel);
		if(!r || !r->properties.contains("every")) return;
		r->everyTimer = rearm(r->everyMs);
		const SharedScope locals = r->locals;
		engine_.runner.handle(r->properties["every"]["do"], *panel, {{"type", "tick"}}, *locals, r->path + ".every.do");
	});
}

// With a transition, the colour moves from what was shown to the target over that many milliseconds
void Scheduler::transitionColor(Panel& panel, Record& record, const char* property, Color shown, Color target) {
	Window* window = engine_.window;
	Tween& tween = tweenOf(record, property);
	const double ms = record.transition.contains(property) ? record.transition[property].get<double>() : 0.0;

	if(!window || ms <= 0.0 || shown == target || !record.shown || engine_.handlersSuspended) {
		if(tween.timer && window) window->cancelTimer(std::exchange(tween.timer, 0));
		setColorOf(panel, property, target);
		return;
	}
	if(tween.timer && tween.to == target) { // already on its way there
		setColorOf(panel, property, shown);
		return;
	}
	if(tween.timer) window->cancelTimer(tween.timer);
	tween = {shown, target, engine_.now(), ms, 0};
	setColorOf(panel, property, shown);

	tween.timer = repeating(frameMs, [this, panel = &panel, name = std::string_view(property)](const Rearm& rearm) {
		Record* r = engine_.reconciler.recordOf(panel);
		if(!r) return;
		Tween& t = tweenOf(*r, name);
		const double progress = std::min((engine_.now() - t.start) * 1000.0 / t.ms, 1.0);
		setColorOf(*panel, name, Color::mix(t.from, t.to, static_cast<float>(ease("out", progress))));
		t.timer = progress < 1.0 ? rearm(frameMs) : 0;
	});
}

void Scheduler::cancel(Record& record) const {
	Window* window = engine_.window;
	if(!window) return;
	if(record.everyTimer) window->cancelTimer(record.everyTimer);
	for(const Tween* tween : record.tweens())
		if(tween->timer) window->cancelTimer(tween->timer);
}

void Scheduler::cancelAll() {
	*alive_ = false; // the window may still run the tasks deferred for this runtime
	Window* window = engine_.window;
	if(!window) return;
	// The timers point at the runtime: stop them, or the window would run them on freed memory
	for(auto& [panel, record] : engine_.reconciler.records()) cancel(record);
	for(const auto& [place, timer] : debounces_) window->cancelTimer(timer);
	for(const auto& [pointer, timer] : animations_) window->cancelTimer(timer);
	for(const std::uint64_t timer : once_) window->cancelTimer(timer);
}

} // namespace affresco::format
