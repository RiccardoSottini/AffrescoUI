#pragma once

/// @file
/// @brief format::StepRegistry: the steps an action can have, registered by group, each group from its own file.

#include "affresco/core/Json.hpp"
#include "affresco/render/Panel.hpp"

#include "Expression.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

class Steps;

/// @brief A step being run: the step, where it comes from, and how to read its values.
struct Running {
	Steps& steps;             ///< The runner.
	const json& step;         ///< The step.
	Panel& sender;            ///< The panel it comes from.
	const json& event;        ///< The event being handled.
	const Scope& locals;      ///< Variables.
	const std::string& where; ///< Its place, for errors.

	/// @brief Evaluates a value of the step.
	/// @param value The value.
	/// @param at Its place, for errors.
	/// @return The evaluated value.
	[[nodiscard]] json value(const json& value, const std::string& at) const;
	/// @brief Evaluates the step's value under a key.
	/// @param key The key, e.g. "to".
	/// @return The evaluated value.
	[[nodiscard]] json value(const char* key) const { return value(step[key], where + "." + key); }
	/// @brief The state path the step changes, e.g. "set": "state.count" is /count.
	/// @param key The key holding the path.
	/// @return The pointer into the state.
	[[nodiscard]] json::json_pointer target(const char* key) const;
	/// @brief A list given as a path ("state.items") or as any value.
	/// @param source The path or value.
	/// @param at Its place, for errors.
	/// @return The list (not checked to be one).
	[[nodiscard]] json list(const json& source, const std::string& at) const;
	/// @brief A duration under a key, never negative.
	/// @param key The key.
	/// @return Milliseconds, at most Scheduler::maxDelayMs; 0 for NaN.
	[[nodiscard]] double milliseconds(const char* key) const;
	/// @brief Runs nested steps with the same sender and event.
	/// @param nested The steps.
	/// @param scope Their variables.
	/// @param at Their place.
	/// @return false if a "stop" step ended them.
	bool run(const json& nested, const Scope& scope, const std::string& at) const;
};

/// @brief Runs a group's steps: std::nullopt for a step that isn't one of its kinds, else whether to keep going
/// (false after "stop").
using StepGroupRunner = std::optional<bool> (*)(const Running& step);

/// @brief One kind of step.
struct StepType {
	std::string name;              ///< The key that names it in a step, e.g. "set".
	std::vector<std::string> keys; ///< The other keys it reads, e.g. "to".
	/// Whether it controls changes or reaches outside the screen: a revision may never add or alter it (Rules), and
	/// while a proposal is previewed it runs only from the user's input.
	bool control = false;
	/// Whether the catalog lists it under "steps"; the review and history steps are listed under "controlSteps" only.
	bool listed = true;
};

/// @brief A group of steps run by one function, registered from its own file.
struct StepGroup {
	std::string name;              ///< The group, e.g. "state".
	std::vector<StepType> steps;   ///< Its steps, in the order the catalog lists them.
	StepGroupRunner run = nullptr; ///< Runs them.
};

/// @brief The steps an action can have: groups registered in order, each from its own file (registerBuiltInSteps()).
///
/// A step runs in the first group, in registration order, that recognises it. Invariants: every group has a runner;
/// a step's name is in one group only. builtInSteps() fills one and shares it read-only. Callers: the step runner
/// (run()) and the catalog (groups()).
class StepRegistry {
public:
	/// @brief Adds a group after the others.
	/// @param group The group.
	/// @throws std::logic_error If it has no runner, or one of its steps is already registered.
	void add(StepGroup group);
	/// @brief Runs a step in the first group that recognises it.
	/// @param step The step.
	/// @return std::nullopt if no group does, else whether to keep going.
	[[nodiscard]] std::optional<bool> run(const Running& step) const;
	/// @brief Finds a kind of step.
	/// @param name Its name.
	/// @return The step, or nullptr.
	[[nodiscard]] const StepType* find(std::string_view name) const;
	/// @brief The groups, in registration order.
	[[nodiscard]] const std::vector<StepGroup>& groups() const noexcept { return groups_; }

private:
	std::vector<StepGroup> groups_; ///< See groups().
};

/// @brief Registers the built-in steps, group by group, in the order they are tried.
/// @param registry The registry.
void registerBuiltInSteps(StepRegistry& registry);

/// @brief The built-in steps, registered once and shared: they never change, so every runtime reads the same.
/// @return The registry.
[[nodiscard]] std::shared_ptr<const StepRegistry> builtInSteps();

/// @brief set, push, remove, toggle, increment (StateSteps.cpp).
/// @param registry The registry.
void registerStateSteps(StepRegistry& registry);
/// @brief if, forEach, while, run (FlowSteps.cpp).
/// @param registry The registry.
void registerFlowSteps(StepRegistry& registry);
/// @brief call, emit, send, connect, patch (OutsideSteps.cpp).
/// @param registry The registry.
void registerOutsideSteps(StepRegistry& registry);
/// @brief accept, reject, preview, endPreview, restore, revert, undo (ReviewSteps.cpp).
/// @param registry The registry.
void registerReviewSteps(StepRegistry& registry);
/// @brief focus, scrollTo, navigate (NavigationSteps.cpp).
/// @param registry The registry.
void registerNavigationSteps(StepRegistry& registry);
/// @brief after, debounce, animate (TimeSteps.cpp).
/// @param registry The registry.
void registerTimeSteps(StepRegistry& registry);
/// @brief copy, log, stop, note (OtherSteps.cpp).
/// @param registry The registry.
void registerOtherSteps(StepRegistry& registry);
/// @brief Registers the service steps: invoke, subscribe, respond (ServiceSteps.cpp).
/// @param registry The registry.
void registerServiceSteps(StepRegistry& registry);
/// @brief Registers the sign-in steps: signIn, signOut (AuthSteps.cpp).
/// @param registry The registry.
void registerAuthSteps(StepRegistry& registry);
/// @brief Registers the request step: ask (AskSteps.cpp, in assistant/).
/// @param registry The registry.
void registerAssistantSteps(StepRegistry& registry);

/// @brief Registers the folder step: chooseFolder (FolderSteps.cpp).
/// @param registry The registry.
void registerFolderSteps(StepRegistry& registry);

/// @brief Registers the scene steps: camera (SceneSteps.cpp).
/// @param registry The registry.
void registerSceneSteps(StepRegistry& registry);
/// @brief Registers the image step: image (imaging/ImageSteps.cpp).
/// @param registry The registry.
void registerImageSteps(StepRegistry& registry);

} // namespace affresco::format
