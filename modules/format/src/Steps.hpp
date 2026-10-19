#pragma once

/// @file
/// @brief format::Steps: runs actions and their steps.

#include "Record.hpp"
#include "StepRegistry.hpp"

#include <functional>
#include <string>

namespace affresco::format {

struct Engine;

/// @brief Runs handlers: an action's name (from "actions", else native, else the unhandled fallback) or inline steps,
/// each step in the group of the StepRegistry that recognises it.
///
/// Invariants: actions nest at most maxActionDepth deep; errors in a handler are reported, never thrown into the
/// event loop; a handler that changed the state asks for one update. While a proposal is previewed, control steps run
/// only from the user's input (checkControl()). Callers: the events subsystem, the scheduler (timers, "every"), the
/// reconciler (onShow, onHide, watch), Runtime::runAction() and build() (onLoad), and the step groups for nested
/// steps. Main thread only.
class Steps {
public:
	/// @brief The step runner of an engine.
	/// @param engine The runtime; outlives this object.
	explicit Steps(Engine& engine) : engine_(engine) {}

	/// @brief Runs an event handler (an action name or inline steps); errors are reported, never thrown.
	/// @param handler The handler.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The sender's variables.
	/// @param where Its place, for errors.
	void handle(const json& handler, Panel& sender, const json& event, const Scope& locals, const std::string& where);
	/// @brief Runs an action from "actions", else a native one, else the unhandled fallback.
	/// @param name The action.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals The caller's variables.
	/// @param where Its place, for errors.
	/// @throws DocumentError If a step fails, or nobody handles the action.
	void runNamed(const std::string& name, Panel& sender, const json& event, const Scope& locals,
	              const std::string& where);
	/// @brief Runs steps in order.
	/// @param steps A step or an array of steps.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals Variables.
	/// @param where Its place, for errors.
	/// @return false if a "stop" step ended the action.
	/// @throws DocumentError If a step fails or actions nest more than 32 levels deep.
	bool runSteps(const json& steps, Panel& sender, const json& event, const Scope& locals, const std::string& where);
	/// @brief Runs steps later, from a timer or after the event, with a sender that may be gone by then (the root
	/// stands in).
	/// @param steps The steps.
	/// @param sender The panel they come from.
	/// @param event The event.
	/// @param locals Variables.
	/// @param where Its place, for errors.
	/// @return The task.
	[[nodiscard]] std::function<void()> later(json steps, Panel& sender, json event, SharedScope locals,
	                                          std::string where);
	/// @brief Refuses a control step (call, send, connect, patch, review and history) while a proposal is previewed,
	/// unless the user's input runs it.
	/// @param where The step's place, for the error.
	/// @throws DocumentError If it may not run now.
	void checkControl(const std::string& where) const;
	/// @brief The runtime, for the step groups.
	[[nodiscard]] Engine& engine() const noexcept { return engine_; }

private:
	/// @brief Runs one step.
	/// @param step The step.
	/// @param sender The panel it comes from.
	/// @param event The event.
	/// @param locals Variables.
	/// @param where Its place, for errors.
	/// @return false if it was "stop".
	/// @throws DocumentError If it fails, or no group knows it.
	bool runStep(const json& step, Panel& sender, const json& event, const Scope& locals, const std::string& where);

	Engine& engine_;      ///< The runtime.
	int actionDepth_ = 0; ///< Nested action calls in progress.
};

} // namespace affresco::format
