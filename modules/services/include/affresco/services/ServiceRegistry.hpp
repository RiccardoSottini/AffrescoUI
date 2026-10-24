#pragma once

/// @file
/// @brief ServiceRegistry: the providers a host trusts and what it grants them (folders by name, programs by name),
/// and the services a screen declared, started at their first use and restarted as declared (RFC 0002).

#include "affresco/core/Services.hpp"
#include "affresco/services/Provider.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <set>

namespace affresco::services {

/// @brief The services of one screen, and the providers and grants they come from. Main thread only.
///
/// The general providers `fs` (a folder: read, write, list), `process` (an allowed program, its output streamed) and
/// `jsonrpc` (an allowed program spoken to in JSON-RPC 2.0, LSP-style) are registered already; they can touch nothing
/// until the host adds a root or allows a program.
class ServiceRegistry final : public Services {
public:
	/// @brief Starts a registry with the general providers, no roots and no programs.
	ServiceRegistry();
	/// @brief Ends every service (cancelling their calls and ending their processes).
	~ServiceRegistry() override;
	ServiceRegistry(const ServiceRegistry&) = delete;
	ServiceRegistry& operator=(const ServiceRegistry&) = delete;
	ServiceRegistry(ServiceRegistry&&) = delete;
	ServiceRegistry& operator=(ServiceRegistry&&) = delete;

	/// @brief Registers a provider, replacing one with the same name. Takes effect at the next load().
	/// @param provider The provider.
	void add(std::unique_ptr<Provider> provider);
	/// @brief Names a folder declarations may use (`"root": "workspace"`).
	/// @param name The name.
	/// @param folder The folder; it must exist.
	/// @param readOnly true to grant it for reading only: an `fs` declaration on it must say `"readOnly": true`, and
	/// one whose `copyFrom` names it only copies from it.
	/// @throws std::runtime_error If it doesn't.
	void addRoot(std::string name, const std::filesystem::path& folder, bool readOnly = false);
	/// @brief Lets the person choose the folder a root names, in a folder dialog the document opens (`chooseFolder`):
	/// until they do, a declaration that uses it is refused. The person's choice is the grant; the document never
	/// names the folder.
	/// @param name The root's name.
	void allowChosenRoot(std::string name);
	/// @brief Names a program declarations may start (`"process": "clangd"`). It runs with the user's rights.
	/// @param name The name.
	/// @param program The program, an absolute path.
	/// @param arguments Its arguments.
	/// @param environment Its whole environment, or std::nullopt for the host's.
	/// @param shape Checks the arguments a call adds (`with.arguments`) and may set the folder each run starts in;
	/// empty to refuse every call that adds arguments.
	/// @throws std::runtime_error If the path isn't absolute.
	void allowProcess(std::string name, const std::filesystem::path& program, std::vector<std::string> arguments = {},
	                  std::optional<std::vector<std::string>> environment = std::nullopt, ProcessShaper shape = {});
	/// @brief Names a program declarations may start, with every part of its grant (e.g. ProcessGrant::detach).
	/// @param name The name.
	/// @param grant The grant; its program an absolute path.
	/// @throws std::runtime_error If the path isn't absolute.
	void allowProcess(std::string name, ProcessGrant grant);
	/// @brief Names a loopback port a `dap` service may attach to (`"attach": {"port": 5678}`): whatever listens there
	/// is spoken to as a debug adapter, which can run code.
	/// @param port The port.
	void allowAttach(std::uint16_t port);
	/// @brief Sets the main thread services report to. Without one, no service starts.
	/// @param executor The executor, or nullptr; it must outlive the registry.
	void setExecutor(Executor* executor) noexcept;
	/// @brief Has the services started from now on do each call's work before invoke() returns (ServiceContext::now),
	/// so a scripted run sees its results at once and the same every time. Off by default.
	/// @param now Whether to.
	void setInline(bool now) noexcept;
	/// @brief The providers, for the catalog: {name: {about, configuration, methods}} (methods as the default
	/// configuration gives them).
	[[nodiscard]] json catalog() const;

	[[nodiscard]] std::vector<std::string> load(const json& section) override;
	[[nodiscard]] const MethodSpec* method(std::string_view service, std::string_view method) const override;
	[[nodiscard]] std::optional<std::string> invoke(std::string_view service, std::string_view method,
	                                                std::shared_ptr<Call> call) override;
	[[nodiscard]] std::optional<std::string> subscribe(std::string_view service, std::string_view stream,
	                                                   std::shared_ptr<Call> call) override;
	[[nodiscard]] json status() const override;
	void onStatus(std::function<void()> callback) override;
	void onRequest(RequestHandler handler) override;
	[[nodiscard]] bool choosable(std::string_view root) const override;
	[[nodiscard]] std::optional<std::string> chooseRoot(std::string_view root, std::string_view folder) override;

private:
	struct Entry;
	/// @brief Checks a declaration against the providers and grants, and makes its entry.
	/// @param name The service's name.
	/// @param declaration Its declaration.
	/// @return The entry (refused, with the reason, if it isn't within the grants).
	[[nodiscard]] std::unique_ptr<Entry> declare(const std::string& name, const json& declaration) const;
	/// @brief Resolves `root` and `process` against the grants.
	/// @param name The service's name, for messages.
	/// @param configuration Its configuration.
	/// @param grant Receives what they name.
	/// @return Why they aren't granted, or std::nullopt.
	[[nodiscard]] std::optional<std::string> resolve(const std::string& name, const json& configuration,
	                                                 Grant& grant) const;
	/// @brief Starts a service if it isn't running, restarting it if its declaration allows.
	/// @param entry The service.
	/// @return Why it can't take calls, or std::nullopt.
	[[nodiscard]] std::optional<std::string> ensureStarted(Entry& entry);
	/// @brief Records a service's new state and tells the screen.
	/// @param entry The service.
	/// @param state Its state.
	/// @param reason Why, for Failed.
	void setState(Entry& entry, ServiceState state, std::string reason);
	/// @brief Finds a loaded service.
	/// @param name Its name.
	/// @return It, or nullptr.
	[[nodiscard]] Entry* find(std::string_view name) const;

	std::map<std::string, std::unique_ptr<Provider>, std::less<>> providers_; ///< By name.
	std::map<std::string, std::filesystem::path, std::less<>> roots_;         ///< Granted folders, canonical.
	std::set<std::string, std::less<>> readOnlyRoots_;                        ///< Those granted for reading only.
	std::map<std::string, ProcessGrant, std::less<>> processes_;              ///< Granted programs.
	std::set<std::uint16_t> attachPorts_;                                     ///< Granted loopback ports.
	std::set<std::string, std::less<>> choosableRoots_;                       ///< Roots the person may choose.
	std::map<std::string, std::unique_ptr<Entry>, std::less<>> entries_;      ///< The loaded services.
	Executor* executor_ = nullptr;                                            ///< The main thread.
	bool inline_ = false;                                                     ///< See setInline().
	std::uint64_t generations_ = 0;                                           ///< Starts so far, to number them.
	std::function<void()> statusChanged_;                                     ///< See onStatus().
	RequestHandler requests_;                                                 ///< See onRequest().
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards what services post to the main thread.
};

} // namespace affresco::services
