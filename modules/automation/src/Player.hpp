#pragma once

/// @file
/// @brief detail::Player: plays a scenario's steps on a screen and its window, on a clock it moves by hand.

#include "affresco/automation/Scenarios.hpp"
#include "affresco/core/Channel.hpp"
#include "affresco/render/Geometry.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

/// @brief The peers a scenario's `command` steps speak as: one channel per author's channel, connected to the screen
/// when a step first needs it, exactly as a peer on a socket would be. Declared before the screen, which keeps a
/// reference to each channel it was connected to. Main thread only.
class Peers {
public:
	/// @brief Sends a message as an author's peer, and gives the screen's answer to it.
	/// @param screen The scenario's screen.
	/// @param author The author: its channel, then the name the peer gives itself ("agent/helper").
	/// @param message The message, without its "id" and "author".
	/// @return The answer with the message's id, or a "rejected" saying none came.
	[[nodiscard]] json ask(Screen& screen, std::string_view author, json message);

private:
	/// @brief A channel kept in memory: what the screen sends the peer, kept until the next message.
	class Line final : public Channel {
	public:
		/// @brief Keeps a message the screen sent.
		/// @param message The message.
		void send(std::string_view message) override { sent.emplace_back(message); }

		std::vector<std::string> sent; ///< What the screen sent since the last message, oldest first.
	};

	std::map<std::string, std::unique_ptr<Line>, std::less<>> lines_; ///< By channel.
	int nextId_ = 0;                                                  ///< The last message's id.
};

/// @brief Where a scenario is played, and where its results go. The references must outlive the Player.
struct Stage {
	Screen& screen;                      ///< The screen, built into the window.
	Window& window;                      ///< Its window: headless, or hidden with OpenGL.
	affresco::detail::ManualClock& clock; ///< The window's clock.
	const Scenario& scenario;            ///< What is played.
	const Options& options;              ///< How.
	std::filesystem::path scratch;       ///< A folder of the run's own, for drawn frames.
	Peers& peers;                        ///< Who `command` steps run as; declared before the screen.
};

/// @brief Plays a scenario's steps, one at a time, the way a person would give them: input through the window's route
/// methods, time through the manual clock. Main thread only.
class Player {
public:
	/// @brief Prepares to play.
	/// @param stage Where; its references must outlive the player.
	/// @param result Receives the failures and pictures; must outlive the player.
	Player(Stage stage, ScenarioResult& result) : stage_(std::move(stage)), result_(result) {}
	/// @brief Gives the window's file dialogs back to the OS, if choose() answered them.
	~Player();

	Player(const Player&) = delete;
	Player& operator=(const Player&) = delete;
	Player(Player&&) = delete;
	Player& operator=(Player&&) = delete;

	/// @brief Plays every step, recording the expectations that fail; stops at a step that can't be played.
	void play();

private:
	/// @brief Plays one step.
	/// @param step The step, checked when the file was read.
	/// @param number Its position, from 1.
	/// @throws StepError If it can't be played.
	void playStep(const json& step, std::size_t number);
	/// @brief Plays the steps that move the pointer or press its buttons: move, click, rightClick, press, release,
	/// drag, scroll.
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
	/// @brief Copies a file of the scenario's screens folder (a fixture) into the run's own folder, for choose().
	/// @param from Its path, relative to that folder (checked by the scenario file's reader: no '..').
	/// @param name The name it takes in the run's own folder.
	/// @throws StepError If it isn't a file inside that folder (a link out of it, say), or can't be copied.
	void copyFixture(const std::string& from, const std::string& name);
	/// @brief Answers the file dialogs that follow with a file in the run's own folder, as the person would choose it.
	/// @param file Its name, or null to cancel them.
	void choose(const json& file);
	/// @brief Runs a command as an author's peer would, or checks the verdict a run would meet (Commands.cpp).
	/// @param step `{"command", "with"?, "as"?, "verdict"?}`.
	/// @param number The step's position, from 1.
	/// @throws StepError If the command is refused, would wait, or fails.
	void command(const json& step, std::size_t number);
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
	std::string chosen_;     ///< The file the dialogs are answered with (choose()); empty to cancel them.
	bool answering_ = false; ///< Whether the window's dialogs are answered by choose().
};

/// @brief Checks a state expectation.
/// @param screen The screen.
/// @param expect {"state": path, "is": value}.
/// @return The problem, or an empty string if it holds.
[[nodiscard]] std::string checkState(const Screen& screen, const json& expect);

/// @brief Checks a value of the screen's records, as the `data` variable gives them (Expectations.cpp).
/// @param screen The screen.
/// @param expect `{"data": "notes[0].title", "is": "Plan"}`.
/// @return "" if it holds, else what is wrong.
[[nodiscard]] std::string checkData(Screen& screen, const json& expect);

/// @brief Checks a node expectation: whether it exists, its text, or a property.
/// @param root Where to look: the window's root panel.
/// @param expect {"node": "#id" or { "role", "name"? }, "nth", "exists" | "text" | "property" + "is"}.
/// @return The problem, or an empty string if it holds.
[[nodiscard]] std::string checkNode(Panel& root, const json& expect);

/// @brief Finds the n-th node a target names: "#id" (shown only, or any), or { "role", "name"? } (shown only: hidden
/// nodes aren't in the accessibility tree).
/// @param root Where to look.
/// @param target "#id" or { "role", "name"? }, as the file was checked.
/// @param nth How many matches to skip.
/// @param shownOnly For "#id": whether hidden nodes are skipped.
/// @return The node, or nullptr.
[[nodiscard]] Panel* findTarget(Panel& root, const json& target, long long nth, bool shownOnly);

/// @brief How a problem names a target.
/// @param target "#id" or { "role", "name"? }.
/// @param nth Its "nth".
/// @return "#title", "#card (nth 2)", "button 'Save'".
[[nodiscard]] std::string targetName(const json& target, long long nth);

} // namespace affresco::automation::detail
