#pragma once

/// @file
/// @brief format::Scheduler: what a screen runs later, on its window's clock: "every" timers, "after", "debounce" and
/// "animate" steps, colour transitions, and tasks deferred to the end of the event.

#include "Record.hpp"

#include "affresco/render/Panel.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace affresco::format {

struct Engine;

/// @brief Everything the runtime runs later, on its window's timers and clock.
///
/// Invariants: every timer it sets is known to it (a record's "every" and transition timers, the debounces, the
/// animations and the one-off timers) until it runs or is cancelled, so cancelAll() leaves nothing on the window that
/// points at the runtime; deferred tasks do nothing once the runtime is gone (stop()). Nothing is scheduled before
/// the runtime has a window. Callers: the properties subsystem ("every", transitions), the reconciler (deferred
/// updates, a destroyed panel's timers) and the steps. Main thread only.
class Scheduler {
public:
	/// @brief A scheduler for an engine.
	/// @param engine The runtime; outlives this object.
	explicit Scheduler(Engine& engine) : engine_(engine) {}

	Scheduler(const Scheduler&) = delete;
	Scheduler& operator=(const Scheduler&) = delete;
	Scheduler(Scheduler&&) = delete;
	Scheduler& operator=(Scheduler&&) = delete;
	~Scheduler() = default;

	/// @brief Runs a task after the current event (Window::defer), unless the runtime is gone by then: the window
	/// can't cancel what it has deferred.
	/// @param task The work.
	void defer(std::function<void()> task) const;
	/// @brief Sets a window timer that runs once, kept until it runs (cancelOnce() and cancelAll() cancel it).
	/// @param delayMs Milliseconds from now.
	/// @param task The work.
	/// @return An id for cancelOnce().
	std::uint64_t once(double delayMs, std::function<void()> task);
	/// @brief Cancels a timer once() set, unless it has run or is running.
	/// @param id From once().
	void cancelOnce(std::uint64_t id);
	/// @brief Runs a task after a delay, unless it is scheduled again under the same key before then.
	/// @param key What it is (the step's place and its sender).
	/// @param delayMs Milliseconds from now.
	/// @param task The work.
	void debounce(const std::string& key, double delayMs, std::function<void()> task);
	/// @brief Moves a number in the state to a target, one frame at a time, replacing an animation of the same path.
	/// @param pointer The state path.
	/// @param from Where it starts.
	/// @param to Where it ends.
	/// @param ms How long it takes.
	/// @param easing "linear", "in", "out" or "inOut".
	/// @param where The step's place: a frame that can't write the path (a revision changed the state) stops the
	/// animation and is reported with it.
	void animate(const json::json_pointer& pointer, double from, const json& to, double ms, std::string easing,
	             std::string where);

	/// @brief Starts, restarts or stops a panel's "every" timer, when its period changed.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param everyMs The period now, or 0 for none.
	void every(Panel& panel, Record& record, double everyMs);
	/// @brief Sets a colour, animating it if the record has a transition for it.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param property "color", "textColor" or "borderColor": a string literal, since running transitions keep it.
	/// @param shown The colour shown before this update.
	/// @param target The new colour.
	void transitionColor(Panel& panel, Record& record, const char* property, Color shown, Color target);
	/// @brief Cancels a record's timers: its "every" and its transitions (its panel is going away).
	/// @param record The record.
	void cancel(Record& record) const;

	/// @brief Cancels every timer it set, and makes its deferred tasks do nothing (~Runtime).
	void cancelAll();

private:
	/// @brief Sets the next timer of a repeating task, returning its id.
	using Rearm = std::function<std::uint64_t(double delayMs)>;
	/// @brief Sets a timer whose task can set the next one, again and again: the timer owns the task, which holds
	/// itself weakly, as a strong reference to itself would never be freed.
	/// @param delayMs Milliseconds from now.
	/// @param tick The work; gets a function that sets its next timer and returns its id.
	/// @return The first timer.
	std::uint64_t repeating(double delayMs, std::function<void(const Rearm& rearm)> tick);

	Engine& engine_;                                  ///< The runtime.
	std::map<std::string, std::uint64_t> debounces_;  ///< "debounce" steps waiting, by place.
	std::map<std::string, std::uint64_t> animations_; ///< "animate" steps running, by state pointer.
	std::set<std::uint64_t> once_;                    ///< "after", "focus" and "scrollTo" timers waiting.
	/// Cleared by cancelAll(): the tasks deferred on the window then do nothing.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace affresco::format
