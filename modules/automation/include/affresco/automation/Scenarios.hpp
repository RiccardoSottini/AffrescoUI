#pragma once

/// @file
/// @brief Scenarios: scripted checks of a screen (click, type, wait, revision, expect), read from a scenarios file and
/// played on the screen, headless unless a step needs a picture.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {
class Screen;
}

namespace affresco::automation {

/// @brief One scenario: a screen, and the steps played on it from the moment it is loaded.
struct Scenario {
	std::string name;              ///< Unique in its file; the prefix of its pictures' names.
	std::filesystem::path screen;  ///< The screen file it plays on.
	int scale = 1;                 ///< Pixels per window unit in its pictures (1, or 2 like a Retina display).
	std::vector<json> steps;       ///< The steps, each checked when the file was read (SCREEN_FORMAT.md, "Scenarios").
	bool needsPictures = false;    ///< Whether a step draws (capture, expect image): it then plays on a hidden
	                               ///< window with OpenGL, else headless.
};

/// @brief A scenarios file (`*.scenarios.json`), read and checked: every step known, every key in its place.
struct ScenarioFile {
	std::filesystem::path path;      ///< Where it was read from.
	std::vector<Scenario> scenarios; ///< In the file's order.

	/// @brief Reads and checks a scenarios file.
	/// @param path The file.
	/// @return It.
	/// @throws DocumentError If it can't be read, isn't JSON, or a scenario or step is malformed; the message names the
	/// place: "AffrescoUI scenarios: index.scenarios.json: scenarios[1].steps[3]: unknown step 'clik'".
	[[nodiscard]] static ScenarioFile load(const std::filesystem::path& path);

	/// @brief Checks a scenarios file given as text.
	/// @param text The JSON text.
	/// @param path Where it is: screens are found relative to its folder, and errors name it.
	/// @return It.
	/// @throws DocumentError As load().
	[[nodiscard]] static ScenarioFile parse(std::string_view text, const std::filesystem::path& path);
};

/// @brief What happens to an image expectation without a golden image, or with one.
enum class Goldens {
	Compare,       ///< Compare; a missing golden image fails.
	RecordMissing, ///< Compare; a missing golden image is recorded from this frame (how the rendering check starts).
	Update         ///< Record every golden image from this run's frames (only for an intended change in pictures).
};

/// @brief How scenarios are played.
struct Options {
	std::filesystem::path goldens;       ///< The golden images' folder (`<scenario>-<name>.png`); empty: none.
	Goldens policy = Goldens::Compare;   ///< See Goldens.
	std::filesystem::path output;        ///< Where captures, and frames that differ with their diffs, are written;
	                                     ///< empty: a capture fails, and a frame that differs is only counted.
	std::optional<std::string> document; ///< Play on this document (JSON text) instead of each scenario's screen file,
	                                     ///< e.g. the running screen's, to check its revisions; files are still found
	                                     ///< relative to the screen file.
	std::vector<std::string> only;       ///< Play only these scenarios; empty: all.
	bool pictures = true;                ///< Whether scenarios that draw may open a hidden OpenGL window; if not, they
	                                     ///< fail at their first picture.
	/// @brief Called on each screen before it is built: registers the application's types and native functions.
	std::function<void(Screen&)> prepare;
};

/// @brief What became of one picture a scenario expected.
struct Picture {
	/// @brief Its outcome.
	enum class Outcome {
		Same,     ///< Pixel for pixel the golden image.
		Recorded, ///< No golden image was there (or Goldens::Update): this frame is now the golden image.
		Differs,  ///< Another size, or pixels that differ.
		Saved     ///< A capture: saved, nothing compared.
	};
	std::string name;               ///< `<scenario>-<name>`, the golden image's name without `.png`.
	Outcome outcome = Outcome::Same; ///< See Outcome.
	std::size_t changedPixels = 0;   ///< For Differs: how many pixels differ (0 when the sizes differ).
	std::string problem;             ///< For Differs: what differs, and where the frame and its diff were written.
};

/// @brief A step that failed: an expectation that didn't hold, or a step that couldn't be played.
struct Failure {
	std::size_t step = 0; ///< Its position in the scenario, from 1.
	std::string problem;  ///< What went wrong, in a line: "state cards.length is 6, expected 7".
};

/// @brief How one scenario went.
struct ScenarioResult {
	std::string name;               ///< The scenario's name.
	std::vector<Failure> failures;  ///< Expectations that failed, then the step that couldn't be played, if any (it
	                                ///< ends the scenario; a failed expectation doesn't).
	std::vector<Picture> pictures;  ///< Its pictures, in order.
	std::size_t stepsPlayed = 0;    ///< Steps played to their end.

	/// @brief Whether it passed: no failure.
	/// @return true if every step played and every expectation held.
	[[nodiscard]] bool passed() const noexcept { return failures.empty(); }
};

/// @brief How a scenarios file went.
struct Report {
	std::filesystem::path file;            ///< The scenarios file.
	std::vector<ScenarioResult> scenarios; ///< The scenarios played, in order.

	/// @brief How many scenarios passed.
	/// @return The count.
	[[nodiscard]] std::size_t passed() const noexcept;
	/// @brief How many scenarios failed.
	/// @return The count.
	[[nodiscard]] std::size_t failed() const noexcept;
	/// @brief A compact report, cheap to read: one line for the file, then one per failure.
	///
	/// `index.scenarios.json: 6 passed, 1 failed` then `FAILED add-card, step 5: state cards.length is 6, expected 7`.
	/// @return The text, ending with a line break.
	[[nodiscard]] std::string text() const;
	/// @brief The same as JSON: `{"file", "passed", "failed", "failures": [{"scenario", "step", "problem"}]}`.
	/// @return The object.
	[[nodiscard]] json toJson() const;
};

/// @brief Plays the scenarios of a file, each on a screen loaded fresh: no saved history or state, channels finding
/// no host, a hidden window that only gets the events the steps send, and a clock that moves only when a step waits.
/// So a scenario plays the same way on any machine and whatever ran before.
///
/// Main thread only. A scenario that draws opens an affresco::Application for its time, so none may exist already.
/// @param file The scenarios.
/// @param options How to play them.
/// @return How each went.
[[nodiscard]] Report run(const ScenarioFile& file, const Options& options);

/// @brief Loads a screen connected to nobody: its channels, its includes' too, are pointed at one socket in a folder
/// that is never created. A channel's own path can be absolute, and would then reach a running host's socket (its
/// studio.sock, a reviewer's) and send it whatever the screen's `open` handler says. Scenarios play on such a screen,
/// and `affrescoui render` draws one.
/// @param document The screen's document, as JSON text.
/// @param directory Where its relative paths ("include", fonts) are found.
/// @param nowhere The folder that is never created.
/// @return The screen.
/// @throws std::exception If the document isn't a valid screen.
[[nodiscard]] Screen unconnectedScreen(const std::string& document, const std::filesystem::path& directory,
                                       const std::filesystem::path& nowhere);

} // namespace affresco::automation
