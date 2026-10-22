/// @file
/// @brief ServiceRegistry: providers and grants, declarations checked against them, lazy start and restart.

#include "affresco/services/ServiceRegistry.hpp"

#include "Providers.hpp"
#include "search/SearchProvider.hpp"
#include "affresco/services/Schema.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <stdexcept>
#include <utility>

namespace affresco::services {

namespace {

using Clock = std::chrono::steady_clock;

/// @brief The keys of a declaration that are the registry's, not the provider's.
constexpr std::string_view sharedKeys[] = {"provider", "start", "restart"};

/// @brief Lists a map's keys for a message: "a, b", or "none".
/// @param map The map.
/// @return The list.
template <typename Map> std::string listNames(const Map& map) {
	std::string names;
	for(const auto& [name, value] : map) names += (names.empty() ? "" : ", ") + name;
	return names.empty() ? "none" : names;
}

/// @brief Whether a configuration holds a binding (`{{ }}`) anywhere: configurations are literal.
/// @param value The configuration.
/// @return Whether it does.
bool holdsBinding(const json& value) {
	if(value.is_string()) return value.get_ref<const std::string&>().contains("{{");
	if(value.is_structured())
		for(const auto& [key, member] : value.items())
			if(key.contains("{{") || holdsBinding(member)) return true;
	return false;
}

/// @brief Whether a service's name is a word: a letter, then letters, digits or `_`, at most 64.
/// @param name The name.
/// @return Whether it is.
bool isWord(std::string_view name) noexcept {
	if(name.empty() || name.size() > 64) return false;
	const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
	if(!letter(name.front())) return false;
	for(const char c : name)
		if(!letter(c) && !(c >= '0' && c <= '9') && c != '_') return false;
	return true;
}

/// @brief Reads a declaration's `start` and `restart`.
/// @param place "services.<name>", for messages.
/// @param declaration The declaration.
/// @param startOnLoad Receives whether it starts on load.
/// @param after Receives `restart.after`.
/// @param times Receives `restart.times`.
/// @return What is wrong, or std::nullopt.
std::optional<std::string> readLife(const std::string& place, const json& declaration, bool& startOnLoad, int& after,
                                    int& times) {
	if(const auto start = declaration.find("start"); start != declaration.end()) {
		if(*start != "onDemand" && *start != "onLoad") return place + ".start: must be \"onDemand\" or \"onLoad\"";
		startOnLoad = *start == "onLoad";
	}
	if(const auto restart = declaration.find("restart"); restart != declaration.end()) {
		const json schema = {{"type", "object"},
		                     {"additionalProperties", false},
		                     {"properties",
		                      {{"after", {{"type", "integer"}, {"minimum", 0}, {"maximum", 3600000}}},
		                       {"times", {{"type", "integer"}, {"minimum", 0}, {"maximum", 100}}}}}};
		if(auto problem = checkSchema(schema, *restart, place + ".restart")) return problem;
		after = restart->value("after", 1000);
		times = restart->value("times", 1);
	}
	return std::nullopt;
}

/// @brief Finds a declaration's provider by its `provider` key.
/// @param providers The registered providers, by name.
/// @param place "services.<name>", for messages.
/// @param declaration The declaration.
/// @return The provider, or why there is none.
std::expected<Provider*, std::string>
findProvider(const std::map<std::string, std::unique_ptr<Provider>, std::less<>>& providers, const std::string& place,
             const json& declaration) {
	const json& name = declaration.contains("provider") ? declaration["provider"] : json();
	const auto provider = name.is_string() ? providers.find(name.get<std::string>()) : providers.end();
	if(provider != providers.end()) return provider->second.get();
	return std::unexpected(place + ".provider: no provider " +
	                       (name.is_string() ? "'" + name.get<std::string>() + "'" : "named") +
	                       " (registered: " + listNames(providers) + ")");
}

/// @brief A declaration's configuration: every key but the registry's own.
/// @param declaration The declaration.
/// @return The provider's keys.
json configurationOf(const json& declaration) {
	json configuration = json::object();
	for(const auto& [key, value] : declaration.items())
		if(std::ranges::find(sharedKeys, key) == std::end(sharedKeys)) configuration[key] = value;
	return configuration;
}

/// @brief The methods a provider offers for a configuration, by name.
/// @param provider The provider.
/// @param configuration The service's configuration.
/// @return The methods.
std::map<std::string, MethodSpec, std::less<>> methodsOf(const Provider& provider, const json& configuration) {
	std::map<std::string, MethodSpec, std::less<>> methods;
	for(MethodSpec& spec : provider.methods(configuration)) {
		std::string name = spec.name;
		methods.insert_or_assign(std::move(name), std::move(spec));
	}
	return methods;
}

/// @brief Whether a failed service may start again: it has restarts left and has waited long enough.
/// @param restarts How often it was restarted.
/// @param times `restart.times`.
/// @param after `restart.after`, in ms.
/// @param failedAt When it last failed.
/// @return Whether it may.
bool mayRestart(int restarts, int times, int after, Clock::time_point failedAt) {
	const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - failedAt);
	return restarts < times && waited.count() >= after;
}

/// @brief Sets a context's report and request so they run on the main thread, and not at all once the registry
/// has ended: a service reports from its own threads, and may outlive neither.
/// @param context The context, whose executor is the main thread.
/// @param alive Expires when the registry ends.
/// @param report What a report does on the main thread.
/// @param request What a request does on the main thread.
void postToMainThread(ServiceContext& context, const std::weak_ptr<bool>& alive,
                      decltype(ServiceContext::report) report, decltype(ServiceContext::request) request) {
	Executor* executor = &context.executor;
	context.report = [executor, alive, report = std::move(report)](ServiceState state, std::string reason) {
		// NOLINTNEXTLINE(readability-redundant-lambda-parameter-list): `] mutable` without () is C++23
		executor->post([alive, report, state, reason = std::move(reason)]() mutable {
			if(!alive.expired()) report(state, std::move(reason));
		});
	};
	context.request = [executor, alive, request = std::move(request)](std::string method, json params,
	                                                                  Responder respond) {
		executor->post([alive, request, method = std::move(method), params = std::move(params),
		                // NOLINTNEXTLINE(readability-redundant-lambda-parameter-list): `] mutable` without () is C++23
		                respond = std::move(respond)]() mutable {
			if(!alive.expired()) request(std::move(method), std::move(params), std::move(respond));
		});
	};
}

/// @brief The spec to hand a service: a listed method's, or the unlisted one under its real name.
/// @param spec What method() found.
/// @param name The method's name.
/// @return The spec.
MethodSpec named(const MethodSpec& spec, std::string_view name) {
	MethodSpec copy = spec;
	copy.name = std::string(name);
	return copy;
}

} // namespace

/// @brief One declared service: its provider, configuration and grant, and its life.
struct ServiceRegistry::Entry {
	std::string name;                                             ///< Its name.
	Provider* provider = nullptr;                                 ///< Its provider, or nullptr if refused for want of one.
	json configuration = json::object();                          ///< Its provider's keys.
	Grant grant;                                                  ///< What it may touch.
	std::map<std::string, MethodSpec, std::less<>> methods;       ///< By name.
	MethodSpec unlisted;                                          ///< For a method it doesn't list (if it takes them).
	bool startOnLoad = false;                                     ///< `"start": "onLoad"`.
	int restartAfter = 0;                                         ///< `restart.after`, in ms.
	int restartTimes = 0;                                         ///< `restart.times`.
	ServiceState state = ServiceState::Stopped;                   ///< Where it is.
	std::string error;                                            ///< Why it was refused or failed.
	int restarts = 0;                                             ///< How often it was restarted.
	Clock::time_point failedAt;                                   ///< When it last failed.
	std::uint64_t generation = 0;                                 ///< Its start's number: older reports are ignored.
	std::unique_ptr<Service> service;                             ///< The running service, or nullptr.
};

std::optional<std::string> Service::subscribe(const std::string& stream, std::shared_ptr<Call> /*call*/) {
	return "it has no stream '" + stream + "'";
}

ServiceRegistry::ServiceRegistry() {
	add(makeFileProvider());
#ifndef __EMSCRIPTEN__ // a browser runs no programs and watches no folders: declarations of these are refused
	add(makeProcessProvider());
	add(makeJsonRpcProvider());
	add(makeWatchProvider());
	add(makePtyProvider());
#endif
	add(makeSearchProvider());
	add(makeDapProvider());
	add(makeSyntaxProvider());
}

ServiceRegistry::~ServiceRegistry() { entries_.clear(); }

void ServiceRegistry::add(std::unique_ptr<Provider> provider) {
	std::string name(provider->name());
	providers_.insert_or_assign(std::move(name), std::move(provider));
}

void ServiceRegistry::addRoot(std::string name, const std::filesystem::path& folder) {
	std::error_code error;
	std::filesystem::path canonical = std::filesystem::canonical(folder, error);
	if(error || !std::filesystem::is_directory(canonical, error))
		throw std::runtime_error("the root '" + name + "' isn't a folder");
	roots_.insert_or_assign(std::move(name), std::move(canonical));
}

void ServiceRegistry::allowProcess(std::string name, const std::filesystem::path& program,
                                   std::vector<std::string> arguments,
                                   std::optional<std::vector<std::string>> environment, ProcessShaper shape) {
	if(!program.is_absolute()) throw std::runtime_error("the program '" + name + "' needs an absolute path");
	processes_.insert_or_assign(std::move(name), ProcessGrant{program, std::move(arguments), std::move(environment),
	                                                          std::move(shape)});
}

void ServiceRegistry::allowAttach(std::uint16_t port) { attachPorts_.insert(port); }

void ServiceRegistry::setExecutor(Executor* executor) noexcept { executor_ = executor; }

json ServiceRegistry::catalog() const {
	json catalog = json::object();
	for(const auto& [name, provider] : providers_) {
		json methods = json::object();
		for(const MethodSpec& spec : provider->methods(json::object()))
			methods[spec.name] = {{"effect", effectName(spec.effect)}, {"streams", spec.streams},
			                      {"arguments", spec.arguments}, {"about", spec.about}};
		catalog[name] = {{"about", provider->about()}, {"configuration", provider->configurationSchema()},
		                 {"methods", std::move(methods)}};
	}
	return catalog;
}

/// @brief Loads the services a document declares, replacing those loaded before (see Services::load).
std::vector<std::string> ServiceRegistry::load(const json& section) {
	entries_.clear(); // their calls are cancelled, their processes ended
	std::vector<std::string> refused;
	if(section.is_null()) return refused;
	if(!section.is_object()) return {"services: must be an object"};
	for(const auto& [name, declaration] : section.items()) {
		std::unique_ptr<Entry> entry = declare(name, declaration);
		if(entry->state == ServiceState::Refused) refused.push_back(entry->error);
		entries_.insert_or_assign(name, std::move(entry));
	}
	for(const auto& [name, entry] : entries_)
		if(entry->startOnLoad && entry->state == ServiceState::Stopped) (void)ensureStarted(*entry);
	if(statusChanged_) statusChanged_();
	return refused;
}

std::unique_ptr<ServiceRegistry::Entry> ServiceRegistry::declare(const std::string& name,
                                                                 const json& declaration) const {
	auto entry = std::make_unique<Entry>();
	entry->name = name;
	const std::string place = "services." + name;
	const auto refuse = [&](std::string reason) {
		entry->state = ServiceState::Refused;
		entry->error = std::move(reason);
		return std::move(entry);
	};
	if(!isWord(name)) return refuse(place + ": a service's name is a word (letters, digits, _)");
	if(!declaration.is_object()) return refuse(place + ": must be an object");
	const auto provider = findProvider(providers_, place, declaration);
	if(!provider) return refuse(provider.error());
	entry->provider = *provider;
	if(auto problem = readLife(place, declaration, entry->startOnLoad, entry->restartAfter, entry->restartTimes))
		return refuse(*problem);
	entry->configuration = configurationOf(declaration);
	if(holdsBinding(entry->configuration)) return refuse(place + ": a service's configuration is literal (no {{ }})");
	if(auto problem = checkSchema(entry->provider->configurationSchema(), entry->configuration, place))
		return refuse(*problem);
	entry->methods = methodsOf(*entry->provider, entry->configuration);
	entry->unlisted = {"*", Effect::Run, false, false, nullptr, "not declared: a call or a stream, the file's only"};
	if(auto problem = resolve(name, entry->configuration, entry->grant)) return refuse(*problem);
	return entry;
}

std::optional<std::string> ServiceRegistry::resolve(const std::string& name, const json& configuration,
                                                    Grant& grant) const {
	const std::string place = "services." + name;
	if(const auto root = configuration.find("root"); root != configuration.end()) {
		const auto granted = root->is_string() ? roots_.find(root->get<std::string>()) : roots_.end();
		if(granted == roots_.end())
			return place + ".root: " + root->dump() + " isn't a folder the host allows (it allows: " +
			       listNames(roots_) + ")";
		grant.root = granted->second;
		grant.rootName = granted->first;
	}
	if(const auto process = configuration.find("process"); process != configuration.end()) {
		const auto granted = process->is_string() ? processes_.find(process->get<std::string>()) : processes_.end();
		if(granted == processes_.end())
			return place + ".process: " + process->dump() + " isn't a program the host allows (it allows: " +
			       listNames(processes_) + ")";
		grant.process = granted->second;
		grant.processName = granted->first;
	}
	if(const auto attach = configuration.find("attach"); attach != configuration.end()) {
		const auto port = attach->value("port", 0); // the provider's schema made it an integer in 1-65535
		if(!attachPorts_.contains(static_cast<std::uint16_t>(port)))
			return place + ".attach.port: " + std::to_string(port) + " isn't a port the host allows attaching to";
	}
	return std::nullopt;
}

ServiceRegistry::Entry* ServiceRegistry::find(std::string_view name) const {
	const auto entry = entries_.find(name);
	return entry == entries_.end() ? nullptr : entry->second.get();
}

const MethodSpec* ServiceRegistry::method(std::string_view service, std::string_view method) const {
	const Entry* entry = find(service);
	if(!entry) return nullptr;
	if(const auto spec = entry->methods.find(method); spec != entry->methods.end()) return &spec->second;
	if(entry->provider && entry->provider->takesUnlistedMethods() && !method.empty()) return &entry->unlisted;
	return nullptr;
}

std::optional<std::string> ServiceRegistry::invoke(std::string_view service, std::string_view methodName,
                                                   std::shared_ptr<Call> call) {
	Entry* entry = find(service);
	if(!entry) return "no service '" + std::string(service) + "' (declared: " + listNames(entries_) + ")";
	const MethodSpec* spec = method(service, methodName);
	if(!spec) return "the service '" + entry->name + "' has no method '" + std::string(methodName) + "'";
	if(auto problem = checkSchema(spec->arguments, call->arguments(), "with")) return problem;
	if(auto problem = ensureStarted(*entry)) return problem;
	return entry->service->invoke(named(*spec, methodName), std::move(call));
}

std::optional<std::string> ServiceRegistry::subscribe(std::string_view service, std::string_view stream,
                                                      std::shared_ptr<Call> call) {
	Entry* entry = find(service);
	if(!entry) return "no service '" + std::string(service) + "' (declared: " + listNames(entries_) + ")";
	const MethodSpec* spec = method(service, stream);
	if(!spec || (!spec->streams && spec->name != "*")) // an unlisted method may be either
		return "the service '" + entry->name + "' has no stream '" + std::string(stream) + "'";
	if(auto problem = checkSchema(spec->arguments, call->arguments(), "with")) return problem;
	if(auto problem = ensureStarted(*entry)) return problem;
	return entry->service->subscribe(std::string(stream), std::move(call));
}

std::optional<std::string> ServiceRegistry::ensureStarted(Entry& entry) {
	const std::string label = "the service '" + entry.name + "'";
	if(entry.state == ServiceState::Refused) return label + " was refused: " + entry.error;
	if(entry.state == ServiceState::Ready || entry.state == ServiceState::Starting) return std::nullopt;
	if(entry.state == ServiceState::Failed) {
		if(!mayRestart(entry.restarts, entry.restartTimes, entry.restartAfter, entry.failedAt))
			return label + " failed: " + entry.error;
		++entry.restarts;
	}
	if(!executor_) return label + " can't start before the screen has a window";
	entry.service.reset(); // a failed one ends before its replacement starts
	entry.generation = ++generations_;
	setState(entry, ServiceState::Starting, {});
	ServiceContext context{*executor_, entry.grant, {}, {}};
	postToMainThread(
	    context, alive_,
	    [this, name = entry.name, generation = entry.generation](ServiceState state, std::string reason) {
		    if(Entry* current = find(name); current && current->generation == generation)
			    setState(*current, state, std::move(reason));
	    },
	    [this, name = entry.name](const std::string& method, const json& params, Responder respond) {
		    if(!requests_) return respond(std::nullopt, std::string(methodNotFound));
		    requests_(name, method, params, std::move(respond));
	    });
	try {
		entry.service = entry.provider->create(entry.configuration, std::move(context));
	} catch(const std::exception& error) {
		setState(entry, ServiceState::Failed, error.what());
		return label + " failed: " + entry.error;
	}
	setState(entry, ServiceState::Ready, {});
	return std::nullopt;
}

void ServiceRegistry::setState(Entry& entry, ServiceState state, std::string reason) {
	if(entry.state == ServiceState::Refused) return;
	if(state == ServiceState::Failed) {
		entry.failedAt = Clock::now();
		entry.error = std::move(reason);
	} else if(state == ServiceState::Ready) {
		entry.error.clear();
	}
	entry.state = state;
	if(statusChanged_) statusChanged_();
}

/// @brief Every service's state, as the "services" variable shows it (see Services::status).
json ServiceRegistry::status() const {
	json status = json::object();
	for(const auto& [name, entry] : entries_)
		status[name] = {{"state", stateName(entry->state)},
		                {"error", entry->error.empty() ? json() : json(entry->error)},
		                {"restarts", entry->restarts}};
	return status;
}

/// @brief Sets what runs, on the main thread, after a service's state changes (see Services::onStatus).
void ServiceRegistry::onStatus(std::function<void()> callback) { statusChanged_ = std::move(callback); }

/// @brief Sets what handles the requests services send the screen (see Services::onRequest).
void ServiceRegistry::onRequest(RequestHandler handler) { requests_ = std::move(handler); }

} // namespace affresco::services
