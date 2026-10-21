#pragma once

/// @file
/// @brief format::Engine: the runtime's composition root, which owns its registries and subsystems.
///
/// The runtime is split into subsystems, each a class with its own header that says what it does, what it keeps true
/// and who calls it:
/// - DocumentContext: colours, fonts, the theme, the globals, computed values, the screen shown;
/// - Builder: a node's children after "use", "repeat", "when", "let", "with", "check" and styles;
/// - Reconciler: the records of the panels built, keeping them in line with the document;
/// - Properties: a node's evaluated values on its panel;
/// - Events: input on the panels and the window, as handlers;
/// - Steps: actions and their steps;
/// - Scheduler: timers, animations, debounces, transitions and deferred tasks;
/// - and the catalog (Catalog.hpp), built from the registries.
/// Node types (TypeRegistry), steps (StepRegistry) and document sections (SectionRegistry) register from their own
/// files.

#include "affresco/core/VersionControl.hpp"
#include "affresco/format/Runtime.hpp"

#include "AsyncCalls.hpp"
#include "Builder.hpp"
#include "DocumentContext.hpp"
#include "Events.hpp"
#include "Models.hpp"
#include "Properties.hpp"
#include "Reconciler.hpp"
#include "Scheduler.hpp"
#include "SectionRegistry.hpp"
#include "ServiceCalls.hpp"
#include "StepRegistry.hpp"
#include "Steps.hpp"
#include "TypeRegistry.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace affresco::format {

/// @brief The runtime's state: its registries and subsystems, and what the application registered. It observes the
/// VersionControl.
///
/// Subsystems reach each other through it. Declaration order is construction order: the registries first, then the
/// subsystems, which only keep a reference to the engine while it is built. Main thread only.
struct Engine : VersionObserver {
	/// @brief Sets the runtime up: registers the built-in types and steps.
	/// @param runtime The public object, for its signals.
	/// @param vcs The document and state; outlive the engine.
	/// @param baseDirectory Where font paths are resolved from.
	Engine(Runtime& runtime, VersionControl& vcs, std::filesystem::path baseDirectory);

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	Engine(Engine&&) = delete;
	Engine& operator=(Engine&&) = delete;
	~Engine() override = default;

	Runtime& owner;           ///< For onEvent, onError, onSend, onConnect and onUpdated.
	VersionControl& versions; ///< The document and state.
	Window* window = nullptr; ///< Set by Runtime::build().

	TypeRegistry types;                                         ///< Node types: built in, then the application's.
	std::shared_ptr<const StepRegistry> steps = builtInSteps(); ///< Steps, by group (shared, read-only).
	std::shared_ptr<const SectionRegistry> sections = builtInSections(); ///< Document sections, for the catalog.

	std::map<std::string, Runtime::Action, std::less<>> nativeActions; ///< Native actions.
	std::map<std::string, Runtime::Call, std::less<>> nativeCalls;     ///< Native functions for "call" with results.
	std::map<std::string, Runtime::AsyncCall, std::less<>> asyncCalls; ///< Slow native functions, on workers.
	Runtime::UnhandledAction unhandled;                                ///< Actions nobody handles.
	bool placeholderTypes = false;                                     ///< Unknown types become plain panels.
	bool handlersSuspended = false;                                    ///< See Runtime::setHandlersSuspended().
	json menuBar = json::array();                                      ///< The native menu bar last shown (showMenuBar()).

	DocumentContext document; ///< Colours, fonts, theme, globals, computed values, the screen shown.
	Builder builder;          ///< Expands children.
	Scheduler scheduler;      ///< Timers and deferred tasks.
	Properties properties;    ///< Values on panels.
	Reconciler reconciler;    ///< Records; updates.
	Events events;            ///< Input.
	Steps runner;             ///< Actions.
	Models models;            ///< The application's data models (their changes reach the others).
	ServiceCalls serviceCalls; ///< Service calls and subscriptions (RFC 0002).
	AsyncCalls calls;         ///< Slow native calls (last: destroyed first, joining their workers).

	/* VersionObserver */
	/// @brief Re-reads colours and fonts, then reconciles.
	void documentChanged() override;
	/// @brief Refreshes the globals and schedules a reconcile (a review UI may list the proposals).
	void proposalsChanged() override;

	/// @brief Logs an error (stderr unless the application set a log sink) and fires onError.
	/// @param message The error.
	void report(const std::string& message);
	/// @brief Seconds on the window's clock, which its timers read too (a steady clock before build()).
	/// @return Now.
	[[nodiscard]] double now() const noexcept;
};

} // namespace affresco::format
