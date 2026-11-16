#pragma once

/// @file
/// @brief What the tests of commands run by peers share: a screen (JSON or Lisp) on a headless window, a run control
/// that records what it is told, and runs under a runner's name.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/services/ServiceRegistry.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace commandrun {

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

constexpr int controlKey = 0x2; ///< GLFW_MOD_CONTROL.

/// @brief A run control that records what it is told; its stateDenial is what a test gives it.
class RecordingRunControl final : public affresco::RunControl {
public:
	/// @brief One finish() call.
	struct Finished {
		int run = 0;                                        ///< The run.
		affresco::RunState state = affresco::RunState::Ran; ///< How it ended.
		std::size_t changes = 0;                            ///< How many changes.
	};

	std::vector<Finished> finished = {}; ///< The finish() calls.
	int allowed = 0;                     ///< The allow() calls.
	int denied = 0;                      ///< The deny() calls.
	/// What stateDenial() says, given the runner and the state before and after; empty: nothing is refused.
	std::function<std::optional<std::string>(std::string_view, const json&, const json&)> denial = {};

	/// @brief Allows everything.
	/// @return Allow.
	[[nodiscard]] affresco::RunJudgement judge(std::string_view /*author*/, std::string_view /*command*/,
	                                           const affresco::CommandReach& /*reach*/) const override {
		return {affresco::RunVerdict::Allow, ""};
	}
	/// @brief Begins nothing.
	/// @return No run.
	Begun begin(std::string_view /*author*/, std::string_view /*command*/, const json& /*arguments*/,
	            std::string_view /*title*/, affresco::RunVerdict /*verdict*/, std::string_view /*reason*/) override {
		return {0, "no"};
	}
	/// @brief Counts the call.
	/// @return std::nullopt.
	std::optional<std::string> allow(int /*run*/, std::string_view /*reviewer*/, bool /*always*/,
	                                 std::string_view /*duration*/) override {
		++allowed;
		return std::nullopt;
	}
	/// @brief Counts the call.
	/// @return std::nullopt.
	std::optional<std::string> deny(int /*run*/, std::string_view /*reason*/) override {
		++denied;
		return std::nullopt;
	}
	/// @brief Records the call.
	/// @param run The run.
	/// @param state How it ended.
	/// @param changes How many changes.
	void finish(int run, affresco::RunState state, std::string_view /*reason*/, std::size_t changes,
	            std::vector<std::string> /*errors*/, affresco::Undoable /*undoable*/) override {
		finished.push_back({run, state, changes});
	}
	/// @brief None.
	/// @return Nothing.
	[[nodiscard]] std::vector<affresco::RunRecord> runs() const override { return {}; }
	/// @brief None.
	/// @return Nothing.
	[[nodiscard]] std::vector<affresco::Grant> grants() const override { return {}; }
	/// @brief Revokes nothing.
	/// @return false.
	bool revoke(std::string_view /*rule*/) override { return false; }
	/// @brief Expires nothing.
	void expire(std::int64_t /*now*/) override {}
	/// @brief What the test gave.
	/// @param author The runner.
	/// @param before The state before.
	/// @param after The state after.
	/// @return Why it is refused, or std::nullopt.
	[[nodiscard]] std::optional<std::string> stateDenial(std::string_view author, const json& before,
	                                                     const json& after) const override {
		return denial ? denial(author, before, after) : std::nullopt;
	}
};

/// @brief The format's rules, without checking documents: a screen may hold any step, written any way, so that every
/// control step can be tried in a run, whatever it would need to be valid.
class Unchecked final : public affresco::DocumentRules {
public:
	/// @brief Checks nothing.
	void validate(const json& /*document*/) const override {}
	/// @copydoc DocumentRules::findNode
	[[nodiscard]] std::optional<std::string> findNode(const json& document, std::string_view id) const override {
		return rules_.findNode(document, id);
	}
	/// @copydoc DocumentRules::permissions
	[[nodiscard]] affresco::Permissions permissions(const json& document) const override {
		return rules_.permissions(document);
	}
	/// @brief Checks nothing.
	void checkChange(const json& /*before*/, const json& /*after*/) const override {}

private:
	affresco::format::Rules rules_; ///< The rules.
};

/// @brief An executor whose posted tasks the test runs by hand, on the main thread.
class PumpedExecutor final : public affresco::Executor {
public:
	/// @brief Keeps a task.
	/// @param task The task.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		posted_.notify_all();
	}
	/// @brief Runs posted tasks until a condition holds.
	/// @param done The condition, checked after each batch.
	/// @return Whether it held within ten seconds.
	bool pumpUntil(const std::function<bool()>& done) {
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while(!done()) {
			std::vector<std::function<void()>> tasks;
			{
				std::unique_lock lock(mutex_);
				if(!posted_.wait_until(lock, end, [this] { return !tasks_.empty(); })) return done();
				tasks.swap(tasks_);
			}
			for(const auto& task : tasks) task();
		}
		return true;
	}

private:
	std::mutex mutex_;                         ///< Guards tasks_.
	std::condition_variable posted_;           ///< A task was posted.
	std::vector<std::function<void()>> tasks_; ///< The tasks not run yet.
};

/// @brief A folder of its own under the system's temporary folder, holding one file, removed at the end.
struct TemporaryFolder {
	std::filesystem::path path = std::filesystem::temp_directory_path() / "affrescoui-command-runs-test"; ///< It.
	/// @brief Makes it, with note.txt.
	TemporaryFolder();
	/// @brief Removes it.
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	TemporaryFolder(TemporaryFolder&&) = delete;
	TemporaryFolder& operator=(TemporaryFolder&&) = delete;
};

inline TemporaryFolder::TemporaryFolder() {
	std::filesystem::remove_all(path);
	std::filesystem::create_directories(path);
	std::FILE* file = std::fopen((path / "note.txt").string().c_str(), "wb");
	REQUIRE(file != nullptr);
	std::fputs("inside", file);
	std::fclose(file);
}

/// @brief A screen on a headless window with a run control, whose commands peers run.
struct App {
	ManualClock clock;                                  ///< The window's clock.
	std::unique_ptr<affresco::Window> window;           ///< The window.
	std::unique_ptr<affresco::DocumentRules> rules;     ///< What revisions are judged by.
	affresco::vcs::Repository repository;               ///< The history.
	RecordingRunControl control;                        ///< The run control.
	PumpedExecutor executor;                            ///< Where answers of services arrive.
	affresco::services::ServiceRegistry services;       ///< The services, the folder "work" granted when given.
	std::unique_ptr<affresco::format::Runtime> runtime; ///< The running screen.
	int hits = 0;                                       ///< How many times the application's function "hit" ran.
	int natives = 0;                                    ///< How many times the native action "native" ran.
	std::vector<std::string> errors;                    ///< What was reported.
	int next = 0;                                       ///< The last run's number.

	/// @brief Builds a screen.
	/// @param document The document.
	/// @param checked Whether the document is checked as valid (else any step, written any way, may be in it).
	/// @param work A folder granted as the root "work", or empty.
	/// @param build Whether to build it on the window.
	explicit App(const json& document, bool checked = true, const std::filesystem::path& work = {}, bool build = true)
	    : window(createHeadlessWindow(400, 300, clock)),
	      rules(checked ? std::unique_ptr<affresco::DocumentRules>(std::make_unique<affresco::format::Rules>())
		                : std::make_unique<Unchecked>()),
	      repository(*rules, document) {
		if(!work.empty()) services.addRoot("work", work);
		services.setExecutor(&executor);
		runtime = std::make_unique<affresco::format::Runtime>(repository, "");
		runtime->setExecutor(&executor);
		runtime->setServices(&services);
		runtime->setRunControl(&control);
		runtime->onCall("hit", [this](const json&) {
			++hits;
			return json();
		});
		runtime->onAction("native", [this](affresco::Panel&) { ++natives; });
		runtime->onError.connect([this](const std::string& message) { errors.push_back(message); });
		if(!build) return;
		runtime->build(*window);
		window->render();
	}
	App(const App&) = delete;
	App& operator=(const App&) = delete;
	App(App&&) = delete;
	App& operator=(App&&) = delete;
	/// @brief Takes the run control and the services away first: they go after the runtime.
	~App() {
		runtime->setRunControl(nullptr);
		runtime.reset();
	}

	/// @brief A screen of a Lisp program, from the app's file.
	/// @param text The program.
	/// @return Its document.
	[[nodiscard]] static json lisp(const std::string& text) { return affresco::lisp::sourceDocument(text, "app.lisp"); }

	/// @brief Runs a command as a peer, then lets a frame pass.
	/// @param command The command.
	/// @param arguments Its arguments.
	/// @param author The runner.
	/// @return What happened.
	affresco::CommandOutcome run(const std::string& command, const json& arguments = json::object(),
	                             const std::string& author = "agent/x") {
		affresco::CommandOutcome outcome = runtime->runCommandAs({++next, author, command, arguments});
		window->render();
		return outcome;
	}

	/// @brief Undoes a run, then lets a frame pass.
	/// @param run The run.
	/// @return What happened.
	affresco::CommandOutcome undo(int run) const {
		affresco::CommandOutcome outcome = runtime->undoCommandRun(run);
		window->render();
		return outcome;
	}

	/// @brief Runs an action as the screen does for the person, then what it deferred.
	/// @param name The action.
	void act(const char* name) const {
		runtime->runAction(name, window->root(), json::object());
		window->runDeferredTasks();
		window->render();
	}

	/// @brief Presses Ctrl and a key, the person's own input.
	/// @param key The key.
	void press(affresco::Key key) const {
		window->routeKey({key, 0, affresco::Action::Press, affresco::Mods{controlKey}});
		window->routeKey({key, 0, affresco::Action::Release, affresco::Mods{controlKey}});
		window->render();
	}

	/// @brief Moves the clock on, with frames: timers fire by themselves.
	/// @param ms How long.
	void wait(double ms) {
		clock.advance(ms);
		window->render();
		window->render();
	}

	/// @brief Reads a state key.
	/// @param key Its name.
	/// @return Its value, or null.
	[[nodiscard]] json state(const char* key) const {
		return repository.state().contains(key) ? repository.state()[key] : json();
	}

	/// @brief The records of a collection.
	/// @param collection Its name.
	/// @return How many it holds.
	[[nodiscard]] std::size_t records(const std::string& collection) const {
		affresco::data::Query query;
		query.from = collection;
		return runtime->dataStore().count(query);
	}

	/// @brief Whether an error holding a text was reported.
	/// @param text The text.
	/// @return true if one was.
	[[nodiscard]] bool reported(std::string_view text) const {
		return std::ranges::any_of(errors, [text](const std::string& error) { return error.contains(text); });
	}
};

/// @brief A screen whose state and actions a test gives, around a label showing state.n.
/// @param state The state.
/// @param actions The actions.
/// @param extra Other sections (models, permissions, services).
/// @return The document.
inline json screen(const json& state, const json& actions, const json& extra = json::object()) {
	json document = {{"format", "affrescoui-screen"},
	                 {"version", 2},
	                 {"state", state},
	                 {"actions", actions},
	                 {"root",
	                  {{"type", "column"},
	                   {"id", "root"},
	                   {"children", json::array({{{"type", "label"}, {"id", "shown"}, {"text", "{{state.n}}"}}})}}}};
	document.update(extra);
	return document;
}

/// @brief A command of a JSON screen.
/// @param steps What it does.
/// @return `{"title", "do"}`.
inline json command(const json& steps) { return {{"title", "C"}, {"do", steps}}; }

/// @brief A model of notes, with a title.
/// @return The "models" section.
inline json notesModel() { return {{"notes", {{"version", 1}, {"fields", {{"title", {{"type", "string"}}}}}}}}; }

} // namespace commandrun
