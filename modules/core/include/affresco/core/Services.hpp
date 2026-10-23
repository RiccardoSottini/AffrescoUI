#pragma once

/// @file
/// @brief The Services interface: long-lived programs and folders a screen talks to by method name (RFC 0002), as the
/// format module sees them. The services module implements it; a host or a test may pass its own.

#include "affresco/core/Json.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief What a method does to the world outside the screen, which decides whether a revision's use of it is reviewed.
enum class Effect {
	Read,  ///< Only reads (a file, a hover, a search).
	Write, ///< Changes something outside the screen (writes a file).
	Run,   ///< Runs a program.
};

/// @brief The name the format uses for an effect: "read", "write" or "run".
/// @param effect The effect.
/// @return Its name.
[[nodiscard]] constexpr std::string_view effectName(Effect effect) noexcept {
	switch(effect) {
	case Effect::Read: return "read";
	case Effect::Write: return "write";
	case Effect::Run: return "run";
	}
	return "read";
}

/// @brief One method a service offers.
struct MethodSpec {
	std::string name;             ///< E.g. "read", "textDocument/hover".
	Effect effect = Effect::Read; ///< What it does.
	bool revisable = true;        ///< Whether a revision may add a step that invokes it.
	bool streams = false;         ///< Whether it is a stream ("subscribe"), not a call ("invoke").
	json arguments;               ///< A JSON Schema for "with", or null for any value.
	std::string about;            ///< One line for people, e.g. "reads a text file under the root".
};

/// @brief One call to a method, or one subscription to a stream, as the service sees it.
///
/// The runtime implements it. Every member may be called from any thread; after finish() or fail() the others do
/// nothing, and once stopToken() says stop, nothing more reaches the screen.
class Call {
public:
	Call() = default;
	Call(const Call&) = delete;
	Call& operator=(const Call&) = delete;
	Call(Call&&) = delete;
	Call& operator=(Call&&) = delete;
	virtual ~Call() = default;

	/// @brief The arguments ("with"), already checked against the method's schema.
	[[nodiscard]] virtual const json& arguments() const noexcept = 0;
	/// @brief Asks the service to stop: the call was cancelled, timed out, or its screen went away.
	[[nodiscard]] virtual std::stop_token stopToken() const noexcept = 0;
	/// @brief Reports progress: {done, total, message}, any of them.
	/// @param progress The progress.
	virtual void progress(json progress) = 0;
	/// @brief Delivers one event of a stream (a line, a notification).
	/// @param event The event.
	virtual void event(json event) = 0;
	/// @brief Ends the call with its result. A subscription ends with null.
	/// @param result The result.
	virtual void finish(json result) = 0;
	/// @brief Ends the call with an error.
	/// @param code A short code, e.g. "timeout", "notFound", "refused", "failed", or a JSON-RPC code as text.
	/// @param message For people, in English; never a secret.
	/// @param data More detail, or null.
	virtual void fail(std::string code, std::string message, json data = nullptr) = 0;
};

/// @brief The error a Responder gets when nothing in the screen handles a request (JSON-RPC's MethodNotFound).
inline constexpr std::string_view methodNotFound = "method not found";

/// @brief Answers a request a service sent the screen: a value, or no value and an error message (methodNotFound when
/// nothing handles it).
using Responder = std::function<void(std::optional<json> value, std::string error)>;

/// @brief Handles a request a service sends the screen (e.g. a language server asking for its configuration). Runs on
/// the main thread; must call the responder once, at any time.
using RequestHandler =
    std::function<void(const std::string& service, const std::string& method, const json& params, Responder respond)>;

/// @brief The services a screen talks to: loaded from its "services" section, within what the host grants.
///
/// Members are called on the main thread. invoke() and subscribe() return at once; the call's results arrive through
/// the Call from any thread.
class Services {
public:
	Services() = default;
	Services(const Services&) = delete;
	Services& operator=(const Services&) = delete;
	Services(Services&&) = delete;
	Services& operator=(Services&&) = delete;
	virtual ~Services() = default;

	/// @brief Loads the services a document declares, replacing those loaded before (their calls are cancelled).
	/// @param section The "services" section (an object), or null for none.
	/// @return One line for each service that was refused (it then has the state "refused"); the others load.
	[[nodiscard]] virtual std::vector<std::string> load(const json& section) = 0;
	/// @brief Finds a method of a loaded service, refused or not.
	/// @param service The service's name.
	/// @param method The method's name.
	/// @return It, or nullptr if there is no such service or method.
	[[nodiscard]] virtual const MethodSpec* method(std::string_view service, std::string_view method) const = 0;
	/// @brief Starts a call to a method.
	/// @param service The service's name.
	/// @param method The method's name.
	/// @param call Where the result goes; kept until it ends.
	/// @return An error if the call can't start (call then hears nothing), else std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> invoke(std::string_view service, std::string_view method,
	                                                        std::shared_ptr<Call> call) = 0;
	/// @brief Starts a subscription to a stream: its events go to Call::event until it is stopped or the service ends.
	/// @param service The service's name.
	/// @param stream The stream's name.
	/// @param call Where the events go; kept until it ends.
	/// @return An error if it can't start, else std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> subscribe(std::string_view service, std::string_view stream,
	                                                           std::shared_ptr<Call> call) = 0;
	/// @brief Every service's state, as the "services" variable shows it: {name: {state, error, restarts, folder}}, the
	/// state "stopped", "starting", "ready", "failed" or "refused", the folder its root's own name (or null).
	[[nodiscard]] virtual json status() const = 0;
	/// @brief Sets what runs, on the main thread, after a service's state changes.
	/// @param callback The callback, or empty.
	virtual void onStatus(std::function<void()> callback) = 0;
	/// @brief Sets what handles the requests services send the screen. Without one, they get "method not found".
	/// @param handler The handler, or empty.
	virtual void onRequest(RequestHandler handler) = 0;
	/// @brief Whether the person may choose the folder a root names, in a folder dialog (the `chooseFolder` step).
	/// @param root The root's name.
	/// @return true if the host allows it.
	[[nodiscard]] virtual bool choosable(std::string_view /*root*/) const { return false; }
	/// @brief Points a root the person may choose at the folder they chose, and starts the services that use it
	/// afresh (their calls are cancelled).
	/// @param root The root's name.
	/// @param folder The folder (UTF-8), as the folder dialog gave it.
	/// @return Why it can't, or std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> chooseRoot(std::string_view root, std::string_view /*folder*/) {
		return "the folder '" + std::string(root) + "' can't be chosen";
	}
};

} // namespace affresco::services
