/// @file
/// @brief detail::Player: the steps that act (pointer, keyboard, revisions, actions, resizing, time) and the ones that
/// draw. Expectations of state and nodes are in Expectations.cpp.

#include "Player.hpp"

#include "Headless.hpp" // the render module's ManualClock
#include "Internal.hpp"
#include "Pictures.hpp"
#include "detail/Utf8.hpp"

#include <affresco/Screen.hpp>
#include <affresco/render/Window.hpp>

#include <algorithm>
#include <cmath>

namespace affresco::automation::detail {

namespace {

/// @brief The name a step's kind: the key that says what it does.
/// @param step The step.
/// @return The kind, or "note" for a step that is only a comment.
std::string kindOf(const json& step) {
	for(const auto& [key, value] : step.items())
		if(key != "nth" && key != "note" && key != "to" && key != "toNth" && key != "dx" && key != "dy" &&
		   key != "moves" && key != "event")
			return key;
	return "note";
}

} // namespace

Player::~Player() {
	if(answering_) stage_.window.setFileDialogHandler({});
}

void Player::play() {
	const std::vector<json>& steps = stage_.scenario.steps;
	for(std::size_t i = 0; i < steps.size(); ++i) {
		try {
			// Time moves only on `wait`: what a handler leaves for the next frame (a focus step, a timer of 0 ms) runs
			// at the next wait, `{"wait": 0}` included. Running a frame after every step instead would change what
			// the rendering check's golden images recorded (the code editor's palette), so it is the maintainer's call
			playStep(steps[i], i + 1);
		} catch(const std::exception& error) { // StepError, or what the screen threw (a DocumentError)
			result_.failures.push_back({i + 1, kindOf(steps[i]) + ": " + error.what()});
			return;
		}
		result_.stepsPlayed = i + 1;
	}
}

void Player::playStep(const json& step, std::size_t number) {
	Screen& screen = stage_.screen;
	Window& window = stage_.window;
	if(playPointer(step)) return;
	if(step.contains("type")) {
		type(step["type"].get<std::string>());
	} else if(step.contains("key")) {
		key(step["key"].get<std::string>());
	} else if(step.contains("revision")) {
		std::string error;
		if(!screen.applyRevision(step["revision"].dump(), &error)) throw StepError("refused: " + error);
	} else if(step.contains("propose")) {
		const RevisionResult result = screen.submitRevision(step["propose"].dump());
		if(result.status == RevisionStatus::Rejected) throw StepError("refused: " + result.error);
	} else if(step.contains("action")) {
		screen.runAction(step["action"].get<std::string>(), window.root(), step.value("event", json::object()).dump());
	} else if(step.contains("resize")) {
		window.setSize(step["resize"][0].get<int>(), step["resize"][1].get<int>());
	} else if(step.contains("wait")) {
		wait(step["wait"].get<double>());
	} else if(step.contains("choose")) {
		choose(step["choose"]);
	} else if(step.contains("capture")) {
		picture(step["capture"].get<std::string>(), true, number);
	} else if(step.contains("expect")) {
		expect(step["expect"], number);
	} // else a step that is only a "note"
}

void Player::choose(const json& file) {
	chosen_ = file.is_string() ? (stage_.scratch / file.get<std::string>()).string() : std::string();
	if(answering_) return;
	answering_ = true;
	stage_.window.setFileDialogHandler([this](const FileDialog&) {
		return chosen_.empty() ? std::vector<std::string>() : std::vector<std::string>{chosen_};
	});
}

bool Player::playPointer(const json& step) {
	using affresco::Action;
	Window& window = stage_.window;
	const int nth = step.value("nth", 0);
	const auto button = [&](const json& target, Action action, MouseButton which = MouseButton::Left) {
		const Vec2 at = pointOf(target, nth);
		window.routeMouseMove(at);
		window.routeMouseButton({which, action, {}, at});
	};
	if(step.contains("move")) {
		window.routeMouseMove(pointOf(step["move"], nth));
	} else if(step.contains("click")) {
		button(step["click"], Action::Press);
		button(step["click"], Action::Release);
	} else if(step.contains("rightClick")) {
		button(step["rightClick"], Action::Press, MouseButton::Right);
		button(step["rightClick"], Action::Release, MouseButton::Right);
	} else if(step.contains("press")) {
		button(step["press"], Action::Press);
	} else if(step.contains("release")) {
		button(step["release"], Action::Release);
	} else if(step.contains("drag")) {
		drag(step);
	} else if(step.contains("scroll")) {
		const Vec2 at = pointOf(step["scroll"], nth);
		window.routeMouseMove(at);
		window.routeScroll({{step.value("dx", 0.0f), step.value("dy", 0.0f)}, at});
	} else {
		return false;
	}
	return true;
}

void Player::drag(const json& step) {
	Window& window = stage_.window;
	const int nth = step.value("nth", 0);
	const Vec2 from = pointOf(step["drag"], nth);
	window.routeMouseMove(from);
	window.routeMouseButton({MouseButton::Left, affresco::Action::Press, {}, from});
	// The drop target is found once the drag has started: what it moves may change the layout
	const Vec2 to = pointOf(step["to"], step.value("toNth", 0));
	const int moves = step.value("moves", 4);
	for(int i = 1; i <= moves; ++i) {
		const float along = static_cast<float>(i) / static_cast<float>(moves);
		window.routeMouseMove({from.x + (to.x - from.x) * along, from.y + (to.y - from.y) * along});
	}
	window.routeMouseButton({MouseButton::Left, affresco::Action::Release, {}, to});
}

void Player::type(const std::string& text) {
	for(const char32_t codepoint : affresco::detail::fromUtf8(text)) stage_.window.routeChar({codepoint});
}

void Player::key(const std::string& name) {
	const std::optional<KeyPress> press = parseKey(name); // checked when the file was read
	if(!press) throw StepError("unknown key '" + name + "'");
	stage_.window.routeKey({press->key, 0, affresco::Action::Press, press->mods});
	stage_.window.routeKey({press->key, 0, affresco::Action::Release, press->mods});
}

void Player::wait(double milliseconds) {
	// Each timer runs at its own time, as it would in real time: a repeating timer runs once per period, and an
	// animation draws each of its frames. Times are rounded up to the microsecond, so a timer's due time is never
	// missed by a rounding error.
	double remaining = milliseconds;
	for(std::size_t runs = 0;; ++runs) {
		if(runs > maxTimerRunsPerWait) throw StepError("timers keep coming due: they never let the time move on");
		const std::optional<double> next = stage_.window.secondsToNextTimer();
		const double step = next ? std::min(std::ceil(*next * 1e6) / 1e3, remaining) : remaining;
		stage_.clock.advance(step);
		remaining -= step;
		stage_.window.render(); // runs the due timers and deferred tasks; draws on a window that can
		if(remaining <= 0.0) return;
	}
}

void Player::picture(const std::string& name, bool capture, std::size_t number) {
	if(!stage_.options.pictures) throw StepError("pictures aren't available here: this run has no OpenGL");
	stage_.screen.waitForImages(); // image files decode on a worker thread: every one belongs in the picture
	Picture taken =
	    takePicture({stage_.window, stage_.scenario.scale, stage_.scratch}, stage_.scenario.name + "-" + name,
		            capture ? PictureKind::Capture : PictureKind::Expect, stage_.options);
	if(!taken.problem.empty()) result_.failures.push_back({number, "image " + taken.name + ": " + taken.problem});
	result_.pictures.push_back(std::move(taken));
}

void Player::expect(const json& expect, std::size_t number) {
	if(expect.contains("image")) return picture(expect["image"].get<std::string>(), false, number);

	// What the last event changed is shown once its deferred tasks have run, and positions once laid out
	stage_.window.runDeferredTasks();
	static_cast<void>(stage_.window.panelAt({}));
	const std::string problem = expect.contains("state") ? checkState(stage_.screen, expect)
	                            : expect.contains("data") ? checkData(stage_.screen, expect)
	                                                      : checkNode(stage_.window.root(), expect);
	if(!problem.empty()) result_.failures.push_back({number, problem});
}

Vec2 Player::pointOf(const json& target, int nth) {
	if(target.is_array()) return {target[0].get<float>(), target[1].get<float>()};

	static_cast<void>(stage_.window.panelAt({})); // lays out, so the bounds are current
	const Panel* panel = findTarget(stage_.window.root(), target, nth, true);
	if(!panel)
		throw StepError("no shown node " + (target.is_string() ? "'" + target.get<std::string>() + "'" +
		                                                             (nth ? " (nth " + std::to_string(nth) + ")" : "")
		                                                       : targetName(target, nth)));
	const Rect& bounds = panel->bounds(); // its centre, through its transforms
	return panel->worldTransform().apply({(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f});
}

} // namespace affresco::automation::detail
