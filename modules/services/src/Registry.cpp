/// @file
/// @brief ServiceRegistry: providers and grants, declarations checked against them, lazy start and restart.

#include "affresco/services/ServiceRegistry.hpp"

#include "Providers.hpp"
#include "SearchProvider.hpp"
#include "affresco/services/Schema.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
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
	if(value.is_string()) return value.get_ref<const std::string&>().find("{{") != std::string::npos;
	if(value.is_structured())
		for(const auto& [key, member] : value.items())
			if(key.find("{{") != std::string::npos || holdsBinding(member)) return true;
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

std::optional<std::string> Service::subscribe(const std::string& stream, std::shared_ptr<Call>) {
	return "it has no stream '" + stream + "'";
}

ServiceRegistry::ServiceRegistry() {
	add(makeFileProvider());
	add(makeProcessProvider());
	add(makeJsonRpcProvider());
	add(makeWatchProvider());
	add(makePtyProvider());
	add(makeSearchProvider());
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

/// @brief Reads a declaration's `start` and `restart`.
/// @param place "services.<name>", for messages.
/// @param declaration The declaration.
/// @param startOnLoad Receives whether it starts on load.
/// @param after Receives `restart.after`.
/// @param times Receives `restart.times`.
/// @return What is wrong, or std::nullopt.
static std::optional<std::string> readLife(const std::string& place, const json& declaration, bool& startOnLoad,
                                           int& after, int& times) {
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
	const json& providerName = declaration.contains("provider") ? declaration["provider"] : json();
	const auto provider = providerName.is_string() ? providers_.find(providerName.get<std::string>()) : providers_.end();
	if(provider == providers_.end())
		return refuse(place + ".provider: no provider " + (providerName.is_string() ? "'" + providerName.get<std::string>() + "'" : "named") +
		              " (registered: " + listNames(providers_) + ")");
	entry->provider = provider->second.get();
	if(auto problem = readLife(place, declaration, entry->startOnLoad, entry->restartAfter, entry->restartTimes))
		return refuse(*problem);
	for(const auto& [key, value] : declaration.items())
		if(std::ranges::find(sharedKeys, key) == std::end(sharedKeys)) entry->configuration[key] = value;
	if(holdsBinding(entry->configuration)) return refuse(place + ": a service's configuration is literal (no {{ }})");
	if(auto problem = checkSchema(entry->provider->configurationSchema(), entry->configuration, place))
		return refuse(*problem);
	for(MethodSpec& spec : entry->provider->methods(entry->configuration)) {
		std::string methodName = spec.name;
		entry->methods.insert_or_assign(std::move(methodName), std::move(spec));
	}
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

/// @brief The spec to hand a service: a listed method's, or the unlisted one under its real name.
/// @param spec What method() found.
/// @param name The method's name.
/// @return The spec.
static MethodSpec named(const MethodSpec& spec, std::string_view name) {
	MethodSpec copy = spec;
	copy.name = std::string(name);
	return copy;
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
		const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - entry.failedAt);
		if(entry.restarts >= entry.restartTimes || waited.count() < entry.restartAfter)
			return label + " failed: " + entry.error;
		++entry.restarts;
	}
	if(!executor_) return label + " can't start before the screen has a window";
	entry.service.reset(); // a failed one ends before its replacement starts
	entry.generation = ++generations_;
	setState(entry, ServiceState::Starting, {});
	ServiceContext context{*executor_, entry.grant, {}, {}};
	const std::weak_ptr<bool> alive = alive_;
	Executor* executor = executor_;
	context.report = [this, alive, executor, name = entry.name, generation = entry.generation](ServiceState state,
	                                                                                          std::string reason) {
		executor->post([this, alive, name, generation, state, reason = std::move(reason)]() mutable {
			if(alive.expired()) return;
			if(Entry* current = find(name); current && current->generation == generation)
				setState(*current, state, std::move(reason));
		});
	};
	context.request = [this, alive, executor, name = entry.name](std::string method, json params, Responder respond) {
		executor->post([this, alive, name, method = std::move(method), params = std::move(params),
		                respond = std::move(respond)]() mutable {
			if(alive.expired()) return;
			if(!requests_) return respond(std::nullopt, std::string(methodNotFound));
			requests_(name, method, params, std::move(respond));
		});
	};
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

json ServiceRegistry::status() const {
	json status = json::object();
	for(const auto& [name, entry] : entries_)
		status[name] = {{"state", stateName(entry->state)},
		                {"error", entry->error.empty() ? json() : json(entry->error)},
		                {"restarts", entry->restarts}};
	return status;
}

void ServiceRegistry::onStatus(std::function<void()> callback) { statusChanged_ = std::move(callback); }

void ServiceRegistry::onRequest(RequestHandler handler) { requests_ = std::move(handler); }

} // namespace affresco::services
