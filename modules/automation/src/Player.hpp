#pragma once

/// @file
/// @brief detail::Player: plays a scenario's steps on a screen and its window, on a clock it moves by hand.

#include "affresco/automation/Scenarios.hpp"
#include "affresco/render/Geometry.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace affresco {
class Panel;
class Screen;
class Window;
namespace detail {
class ManualClock;
}
} // namespace affresco

namespace affresco::automation::detail {

/// @brief A step that can't be played (no such node to click, a revision refused): it ends the scenario.
class StepError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

/// @brief Where a scenario is played, and where its results go. The references must outlive the Player.
struct Stage {
	Screen& screen;                      ///< The screen, built into the window.
	Window& window;                      ///< Its window: headless, or hidden with OpenGL.
	affresco::detail::ManualClock& clock; ///< The window's clock.
	const Scenario& scenario;            ///< What is played.
	const Options& options;              ///< How.
	std::filesystem::path scratch;       ///< A folder of the run's own, for drawn frames.
};

/// @brief Plays a scenario's steps, one at a time, the way a person would give them: input through the window's route
/// methods, time through the manual clock. Main thread only.
class Player {
public:
	/// @brief Prepares to play.
	/// @param stage Where; its references must outlive the player.
	/// @param result Receives the failures and pictures; must outlive the player.
	Player(Stage stage, ScenarioResult& result) : stage_(std::move(stage)), result_(result) {}

	/// @brief Plays every step, recording the expectations that fail; stops at a step that can't be played.
	void play();

private:
	/// @brief Plays one step.
	/// @param step The step, checked when the file was read.
	/// @param number Its position, from 1.
	/// @throws StepError If it can't be played.
	void playStep(const json& step, std::size_t number);
	/// @brief Plays the steps that move the pointer or press its button: move, click, press, release, drag, scroll.
	/// @param step The step.
	/// @return Whether it was one of them.
	/// @throws StepError If a target isn't there.
	bool playPointer(const json& step);
	/// @brief Plays a drag: press on a target, move to the other in steps, release.
	/// @param step The step.
	/// @throws StepError If a target isn't there.
	void drag(const json& step);
	/// @brief Types text, a character at a time (UTF-8, by code point).
	/// @param text The text.
	void type(const std::string& text);
	/// @brief Presses and releases a key.
	/// @param name The key, e.g. "cmd+z".
	void key(const std::string& name);
	/// @brief Moves the clock forward, running the timers that come due in order, as real time would.
	/// @param milliseconds How long.
	/// @throws StepError If timers keep coming due without the time moving.
	void wait(double milliseconds);
	/// @brief Checks an expectation, and records it if it fails.
	/// @param expect The expectation.
	/// @param number The step's position, from 1.
	void expect(const json& expect, std::size_t number);
	/// @brief Draws a picture and records it, and a failure if it differs.
	/// @param name The name in the step.
	/// @param capture true for capture, false for expect image.
	/// @param number The step's position, from 1.
	void picture(const std::string& name, bool capture, std::size_t number);

	/// @brief Where a step points: [x, y], or the centre of the nth shown panel with an id.
	/// @param target The target.
	/// @param nth Which match of an id.
	/// @return The point, in window units.
	/// @throws StepError If no shown panel has that id.
	[[nodiscard]] Vec2 pointOf(const json& target, int nth);

	Stage stage_;            ///< Where it plays.
	ScenarioResult& result_; ///< What it records.
};

/// @brief Checks a state expectation.
/// @param screen The screen.
/// @param expect {"state": path, "is": value}.
/// @return The problem, or an empty string if it holds.
[[nodiscard]] std::string checkState(const Screen& screen, const json& expect);

/// @brief Checks a node expectation: whether it exists, its text, or a property.
/// @param root Where to look: the window's root panel.
/// @param expect {"node": "#id", "nth", "exists" | "text" | "property" + "is"}.
/// @return The problem, or an empty string if it holds.
[[nodiscard]] std::string checkNode(Panel& root, const json& expect);

} // namespace affresco::automation::detail
