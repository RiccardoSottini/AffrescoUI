#pragma once

/// @file
/// @brief Provider and Service: a kind of service a host trusts (fs, process, jsonrpc, an application's own), and one
/// service a screen declared, created from it within what the host granted (RFC 0002).

#include "affresco/core/Executor.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/Services.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief Where a service is in its life, as `services.<name>.state` shows it.
enum class ServiceState {
	Stopped,  ///< Not started yet (services start at their first use), or ended.
	Starting, ///< Starting: its process runs, it hasn't answered yet.
	Ready,    ///< Taking calls.
	Failed,   ///< Ended unexpectedly; `restart` decides what follows.
	Refused,  ///< Its declaration isn't within what the host grants; it never starts.
};

/// @brief The name the format uses for a state: "stopped", "starting", "ready", "failed" or "refused".
/// @param state The state.
/// @return Its name.
[[nodiscard]] constexpr std::string_view stateName(ServiceState state) noexcept {
	switch(state) {
	case ServiceState::Stopped: return "stopped";
	case ServiceState::Starting: return "starting";
	case ServiceState::Ready: return "ready";
	case ServiceState::Failed: return "failed";
	case ServiceState::Refused: return "refused";
	}
	return "stopped";
}

/// @brief One run of a granted program, as the host's ProcessShaper sees it: what the call may add.
struct ProcessRun {
	std::vector<std::string> arguments; ///< Appended after the grant's own; the call's `with.arguments` at first.
	std::filesystem::path directory;    ///< Where it runs: the declaration's root at first, or empty for the host's.
};

/// @brief The host's rule for a program's per-call arguments: it checks the call's `with.arguments` (already in
/// `run.arguments`), may change them and the folder the run starts in, and returns an error to refuse the run. Called
/// on the main thread, before the run starts.
using ProcessShaper = std::function<std::optional<std::string>(ProcessRun& run)>;

/// @brief A program the host allows services to start: never a shell command, always an absolute path.
struct ProcessGrant {
	std::filesystem::path program;      ///< The program, an absolute path.
	std::vector<std::string> arguments; ///< Its arguments, fixed by the host.
	/// @brief Its whole environment ("NAME=value" each), or std::nullopt to pass the host's on.
	std::optional<std::vector<std::string>> environment;
	/// @brief Checks each run's own arguments; without one, a call that passes `with.arguments` is refused.
	ProcessShaper shape;
};

/// @brief What the host granted one service: the declaration names these (`"root": "workspace"`), never paths.
struct Grant {
	std::filesystem::path root;          ///< The folder it may use (canonical), or empty if it named none.
	std::string rootName;                ///< The name the declaration used for it, for messages.
	std::optional<ProcessGrant> process; ///< The program it may start, if it named one.
	std::string processName;             ///< The name the declaration used for it.
};

/// @brief What a service is given to work with.
struct ServiceContext {
	Executor& executor; ///< The main thread.
	Grant grant;        ///< What it may touch.
	/// @brief Reports a change of state, with a reason for Failed (for people; never a secret). Any thread.
	std::function<void(ServiceState state, std::string reason)> report;
	/// @brief Sends the screen a request (e.g. LSP's `workspace/configuration`); the responder may be called on the
	/// main thread at any time after, once. Any thread.
	std::function<void(std::string method, json params, Responder respond)> request;
	/// @brief Whether each call's work runs on the calling thread before invoke() returns, for scripted runs that must
	/// see the same results every time (ServiceRegistry::setInline). Services that wait on programs may ignore it.
	bool now = false;
};

/// @brief One service a screen declared, created on the main thread and destroyed there.
///
/// It does its work on its own threads and reports through each Call (from any thread). Its destructor cancels its
/// calls, ends its processes and joins its threads, so nothing it started outlives it.
class Service {
public:
	Service() = default;
	Service(const Service&) = delete;
	Service& operator=(const Service&) = delete;
	Service(Service&&) = delete;
	Service& operator=(Service&&) = delete;
	virtual ~Service() = default;

	/// @brief Starts a call to one of its methods. Main thread; returns at once.
	/// @param method The method, as the provider declared it (its arguments already checked).
	/// @param call Where the result goes.
	/// @return An error if the call can't start (the call then hears nothing), else std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) = 0;
	/// @brief Starts a subscription to one of its streams. Main thread; returns at once.
	/// @param stream The stream's name.
	/// @param call Where the events go; it ends when its stop token is requested or the service ends.
	/// @return An error if it can't start, else std::nullopt. By default a service has no streams.
	[[nodiscard]] virtual std::optional<std::string> subscribe(const std::string& stream, std::shared_ptr<Call> call);
};

/// @brief A kind of service: its configuration, its methods and a factory. Registered in a ServiceRegistry.
class Provider {
public:
	Provider() = default;
	Provider(const Provider&) = delete;
	Provider& operator=(const Provider&) = delete;
	Provider(Provider&&) = delete;
	Provider& operator=(Provider&&) = delete;
	virtual ~Provider() = default;

	/// @brief The name declarations use (`"provider": "fs"`).
	[[nodiscard]] virtual std::string_view name() const = 0;
	/// @brief One sentence for the catalog.
	[[nodiscard]] virtual std::string_view about() const = 0;
	/// @brief A JSON Schema (checkSchema()) of its configuration: a declaration's keys other than `provider`, `start`
	/// and `restart`. `root` and `process`, when it takes them, are names the registry resolves into the Grant.
	[[nodiscard]] virtual json configurationSchema() const = 0;
	/// @brief Its methods and streams under a configuration (a generic JSON-RPC server's come from it).
	/// @param configuration A configuration its schema accepted.
	/// @return The methods.
	[[nodiscard]] virtual std::vector<MethodSpec> methods(const json& configuration) const = 0;
	/// @brief Whether its services also take methods the configuration doesn't list (a generic JSON-RPC server's).
	/// Such a method may be invoked or subscribed to by the screen file only, never by a revision; its effect counts
	/// as "run". By default, no.
	[[nodiscard]] virtual bool takesUnlistedMethods() const noexcept { return false; }
	/// @brief Creates a service. Main thread.
	/// @param configuration A configuration its schema accepted.
	/// @param context What the service works with.
	/// @return The service.
	/// @throws std::runtime_error If it can't start (e.g. its program won't run); the message is for people.
	[[nodiscard]] virtual std::unique_ptr<Service> create(const json& configuration, ServiceContext context) = 0;
};

} // namespace affresco::services
