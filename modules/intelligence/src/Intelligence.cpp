/// @file
/// @brief The settings' natives and the host's agent (Intelligence.hpp).

#include "affresco/intelligence/Intelligence.hpp"

#include "affresco/intelligence/ClaudeCode.hpp"
#include "affresco/intelligence/Copilot.hpp"
#include "affresco/intelligence/Providers.hpp"
#include "affresco/intelligence/ScreenQueries.hpp"

#include <affresco/net/SignIn.hpp>

#include <cctype>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace affresco::intelligence {

using affresco::json;

namespace {

/// @brief What the natives share.
struct Shared {
	std::filesystem::path dataDir;             ///< Where intelligence.json is.
	Catalog catalog;                           ///< The providers.
	CallsOptions options;                      ///< See registerCalls().
	std::mutex mutex;                          ///< Guards staged.
	std::map<std::string, std::string> staged; ///< Keys typed and not yet saved, by provider. Memory only.
};

/// @brief A text argument.
/// @param arguments The arguments.
/// @param key Its name.
/// @return It, or "".
std::string argument(const json& arguments, const char* key) {
	return arguments.is_object() && arguments.contains(key) && arguments[key].is_string()
	           ? arguments[key].get<std::string>()
			   : "";
}

/// @brief Whether the settings offer a provider: providers.json shows it, or the options show hidden ones too.
/// @param shared The natives' parts.
/// @param provider The provider.
/// @return true if they do.
bool offered(const Shared& shared, const Provider& provider) { return provider.shown || shared.options.hidden; }

/// @brief A provider named in the arguments.
/// @param shared The natives' parts.
/// @param arguments The arguments.
/// @return It.
/// @throws std::runtime_error If there is no such provider.
const Provider& providerOf(const Shared& shared, const json& arguments) {
	const Provider* provider = shared.catalog.find(argument(arguments, "provider"));
	if(provider == nullptr || !offered(shared, *provider)) throw std::runtime_error("no such provider");
	return *provider;
}

/// @brief A key as pasted: without the spaces around it; refused if it holds what a key can't.
/// @param key The key.
/// @return It.
/// @throws std::runtime_error If it isn't one (the message doesn't quote it).
std::string cleanKey(std::string key) {
	while(!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
	std::size_t start = 0;
	while(start < key.size() && std::isspace(static_cast<unsigned char>(key[start]))) ++start;
	key.erase(0, start);
	if(key.size() > 4096) throw std::runtime_error("the key is too long");
	for(const char c : key)
		if(c <= ' ' || c > '~') throw std::runtime_error("the key has characters a key can't have");
	return key;
}

/// @brief A URL without the "/" it ends with.
/// @param url The URL.
/// @return It.
std::string withoutSlash(std::string url) {
	while(url.ends_with('/')) url.pop_back();
	return url;
}

/// @brief Whether a provider can run here: a command line only if it is installed.
/// @param shared The natives' parts.
/// @param provider The provider.
/// @return Whether it can.
bool available(const Shared& shared, const Provider& provider) {
	if(provider.kind == Kind::Copilot)
		return shared.options.copilotInstalled ? shared.options.copilotInstalled() : findCopilot().has_value();
	if(provider.kind != Kind::ClaudeCode) return true;
	return shared.options.claudeInstalled ? shared.options.claudeInstalled() : findClaude().has_value();
}

/// @brief intelligence.test for a command line: it is asked to say OK.
/// @param shared The natives' parts.
/// @param endpoint The provider in use (claude-code or copilot) and its model.
/// @param stopped Whether the test was cancelled.
/// @return `{ ok, message }`.
json commandLineTest(const Shared& shared, const Endpoint& endpoint, const std::function<bool()>& stopped) {
	const bool copilot = endpoint.provider->kind == Kind::Copilot;
	const CommandLineRunner& stand = copilot ? shared.options.copilot : shared.options.claude;
	const std::optional<std::filesystem::path> program = stand ? std::nullopt : copilot ? findCopilot() : findClaude();
	const std::string system = "Answer with the word OK.";
	const auto timeout = std::chrono::seconds(90);
	CommandLineAnswer answer;
	if(stand) answer = stand(endpoint.model, system, "Say OK.", stopped);
	else if(!program)
		return {{"ok", false},
		        {"message", endpoint.provider->name + " isn't installed (the " + (copilot ? "copilot" : "claude") +
		                        " command isn't on the PATH)."}};
	else if(copilot) answer = runCopilot(*program, endpoint.model, system, "Say OK.", timeout, stopped);
	else answer = runClaude(*program, endpoint.model, system, "Say OK.", timeout, stopped);
	return {{"ok", answer.ok},
	        {"message", answer.ok ? "Connected: " + endpoint.provider->name + " answered with " + endpoint.model + "."
	                              : answer.text}};
}

/// @brief A provider's fallback models.
/// @param provider The provider.
/// @return `[{ id, name, description? }]`.
json fallbackOf(const Provider& provider) {
	json list = json::array();
	for(const Model& model : provider.models) {
		json entry = {{"id", model.id}, {"name", model.name}};
		if(model.description.is_object()) entry["description"] = model.description;
		list.push_back(std::move(entry));
	}
	return list;
}

/// @brief A provider's presets: the servers its base URL can be set to.
/// @param provider The provider.
/// @return `[{ id, name, baseUrl }]`.
json presetsOf(const Provider& provider) {
	json list = json::array();
	for(const Preset& preset : provider.presets)
		list.push_back({{"id", preset.id}, {"name", preset.name}, {"baseUrl", preset.baseUrl}});
	return list;
}

/// @brief What intelligence.providers answers.
/// @param shared The natives' parts.
/// @return It.
json providersJson(const Shared& shared) {
	const Settings settings = Settings::load(shared.dataDir);
	json list = json::array();
	for(const Provider& provider : shared.catalog.providers) {
		if(!offered(shared, provider)) continue;
		const std::optional<Endpoint> endpoint = endpointOf(shared.catalog, settings, provider.id);
		const bool hasKey = endpoint && !readKey(*shared.options.keys, *endpoint).empty();
		list.push_back({{"id", provider.id},
		                {"name", provider.name},
		                {"kind", kindName(provider.kind)},
		                {"needsKey", provider.needsKey},
		                {"takesKey", !runsCommandLine(provider.kind)},
		                {"hasKey", hasKey},
		                {"available", available(shared, provider)},
		                {"baseUrl", endpoint ? endpoint->baseUrl : provider.baseUrl},
		                {"editableUrl", provider.kind == Kind::OpenAICompatible},
		                {"model", endpoint ? endpoint->model : provider.model},
		                {"defaultModel", provider.model},
		                {"fixedModels", provider.fixedModels},
		                {"keyHelp", provider.keyHelp},
		                {"presets", presetsOf(provider)},
		                {"models", fallbackOf(provider)}});
	}
	std::optional<Endpoint> current = endpointOf(shared.catalog, settings);
	if(current && !offered(shared, *current->provider)) current.reset(); // a hidden provider isn't in use
	return {{"current", {{"provider", current ? current->provider->id : ""}, {"model", current ? current->model : ""}}},
	        {"keyStore", shared.options.keys->name()},
	        {"persistent", persistent(*shared.options.keys)},
	        {"providers", list}};
}

/// @brief intelligence.save.
/// @param shared The natives' parts.
/// @param arguments `{ provider, model?, baseUrl?, key? }`.
/// @return As intelligence.providers.
json save(Shared& shared, const json& arguments) {
	const Provider& provider = providerOf(shared, arguments);
	Settings settings = Settings::load(shared.dataDir);
	ProviderChoice& choice = settings.providers[provider.id];
	const std::string url = argument(arguments, "baseUrl");
	if(!url.empty() && provider.kind == Kind::OpenAICompatible) {
		const std::string cleaned = cleanBaseUrl(url);
		const std::string defaultUrl = withoutSlash(provider.baseUrl);
		// A key saved for one server never goes to another: a new base URL (the saved one, else the catalogue's)
		// erases it, before anything else is saved
		if(cleaned != (choice.baseUrl.empty() ? defaultUrl : withoutSlash(choice.baseUrl))) {
			shared.options.keys->erase(keyName(provider.id));
			if(shared.options.keys->read(keyName(provider.id)).has_value())
				throw std::runtime_error("the key for the old base URL couldn't be erased from " +
				                         shared.options.keys->name());
		}
		choice.baseUrl = cleaned == defaultUrl ? "" : cleaned;
	}
	const std::string model = argument(arguments, "model");
	if(model.size() > 200) throw std::runtime_error("the model's name is too long");
	if(!model.empty() && provider.fixedModels && !provider.lists(model))
		throw std::runtime_error("choose one of " + provider.name + "'s models");
	if(!model.empty()) choice.model = model;
	std::string key = argument(arguments, "key");
	{
		const std::scoped_lock lock(shared.mutex);
		const auto staged = shared.staged.find(provider.id);
		if(key.empty() && staged != shared.staged.end()) key = staged->second;
		shared.staged.erase(provider.id);
	}
	if(!key.empty() && !runsCommandLine(provider.kind)) {
		key = cleanKey(std::move(key));
		const std::optional<Endpoint> after = endpointOf(shared.catalog, settings, provider.id);
		if(!key.empty() && (!after || !writeKey(*shared.options.keys, *after, key)))
			throw std::runtime_error("the key couldn't be kept in " + shared.options.keys->name());
	}
	settings.provider = provider.id;
	settings.model = choice.model.empty() ? provider.model : choice.model;
	settings.save(shared.dataDir);
	if(shared.options.changed) shared.options.changed();
	return providersJson(shared);
}

/// @brief A provider's live models list.
/// @param shared The natives' parts.
/// @param endpoint The provider, with the base URL asked and its key.
/// @param origin The base URL's origin.
/// @param call The call in progress.
/// @return `[{ id, name }]`, perhaps empty.
/// @throws std::runtime_error If it couldn't be read: why, for people.
json liveModels(const Shared& shared, const Endpoint& endpoint, const std::string& origin,
                affresco::CallProgress& call) {
	HttpSession http(shared.options.network, origin);
	const Outcome outcome = http.fetch(modelsRequest(endpoint), [&call] { return call.cancelled(); });
	if(const auto* failure = std::get_if<affresco::network::Failure>(&outcome))
		throw std::runtime_error(describeFailure(*failure, endpoint.provider->name, endpoint.key));
	json list = json::array();
	for(const Model& model : parseModels(endpoint.provider->kind, std::get<affresco::network::Response>(outcome).body))
		list.push_back({{"id", model.id}, {"name", model.name}});
	return list;
}

/// @brief intelligence.models, on a worker thread.
/// @param shared The natives' parts.
/// @param arguments `{ provider, baseUrl? }`: a base URL given (an openai-compatible provider's, not yet saved) is
/// asked instead of the saved one, with the key only if it was saved for that origin.
/// @param call The call in progress.
/// @return `[{ id, name }]`: the live list, else the fallback. A server that lists no model gives `[]`.
/// @throws std::runtime_error If the live list can't be read and the provider has no fallback (a local server that
/// isn't running): why, for people.
json models(const Shared& shared, const json& arguments, affresco::CallProgress& call) {
	const Provider& provider = providerOf(shared, arguments);
	const auto fallback = [&provider](const std::string& why) {
		if(provider.models.empty()) throw std::runtime_error(why);
		return fallbackOf(provider);
	};
	if(runsCommandLine(provider.kind) || provider.fixedModels) return fallbackOf(provider);
	std::optional<Endpoint> endpoint = endpointOf(shared.catalog, Settings::load(shared.dataDir), provider.id);
	if(!endpoint) return fallback("no such provider");
	const std::string url = argument(arguments, "baseUrl");
	if(!url.empty() && provider.kind == Kind::OpenAICompatible) endpoint->baseUrl = cleanBaseUrl(url);
	if(const std::optional<std::string> refused = checkBaseUrl(*endpoint)) return fallback(*refused);
	endpoint->key = readKey(*shared.options.keys, *endpoint);
	if(provider.needsKey && endpoint->key.empty()) return fallback(provider.name + " needs an API key.");
	const std::optional<std::string> origin = originOf(endpoint->baseUrl);
	if(!origin) return fallback("The base URL isn't an http(s) URL.");
	std::string failed;
	try {
		const json list = liveModels(shared, *endpoint, *origin, call);
		return list.empty() && !provider.models.empty() ? fallbackOf(provider) : list;
	} catch(const std::exception& error) {
		failed = redact(error.what(), endpoint->key);
	}
	return fallback(failed);
}

/// @brief How long a test waits for its answer: a minute, or 90 s for a provider whose answers take longer (a local
/// model loaded on its first request); the settings' call waits 100 s.
/// @param provider The provider.
/// @return It.
std::chrono::milliseconds testTimeout(const Provider& provider) {
	return provider.timeout > std::chrono::seconds(120) ? std::chrono::seconds(90) : std::chrono::seconds(60);
}

/// @brief intelligence.test, on a worker thread: one tiny request to the provider in use.
/// @param shared The natives' parts.
/// @param call The call in progress.
/// @return `{ ok, message }`.
json test(const Shared& shared, affresco::CallProgress& call) {
	std::optional<Endpoint> endpoint = endpointOf(shared.catalog, Settings::load(shared.dataDir));
	if(!endpoint || !offered(shared, *endpoint->provider))
		return {{"ok", false}, {"message", "No provider is chosen yet."}};
	const Provider& provider = *endpoint->provider;
	if(runsCommandLine(provider.kind)) return commandLineTest(shared, *endpoint, [&call] { return call.cancelled(); });
	if(const std::optional<std::string> refused = checkBaseUrl(*endpoint))
		return {{"ok", false}, {"message", *refused}};
	endpoint->key = readKey(*shared.options.keys, *endpoint);
	if(provider.needsKey && endpoint->key.empty())
		return {{"ok", false}, {"message", provider.name + " needs an API key."}};
	if(endpoint->model.empty()) return {{"ok", false}, {"message", "Choose a model first."}};
	const std::optional<std::string> origin = originOf(endpoint->baseUrl);
	if(!origin) return {{"ok", false}, {"message", "The base URL isn't an http(s) URL."}};
	try {
		HttpSession http(shared.options.network, *origin);
		Conversation conversation{"Answer with the word OK.", json::array()};
		addUser(provider.kind, conversation, "Say OK.");
		affresco::network::Request request = chatRequest(*endpoint, conversation, {});
		request.timeout = testTimeout(provider);
		const Outcome outcome = http.fetch(std::move(request), [&call] { return call.cancelled(); });
		if(const auto* failure = std::get_if<affresco::network::Failure>(&outcome))
			return {{"ok", false}, {"message", describeFailure(*failure, provider.name, endpoint->key)}};
		(void)parseTurn(provider.kind, std::get<affresco::network::Response>(outcome).body);
		return {{"ok", true}, {"message", "Connected: " + endpoint->model + " answered."}};
	} catch(const std::exception& error) {
		return {{"ok", false}, {"message", redact(error.what(), endpoint->key)}};
	}
}

} // namespace

json assistantModels(const Catalog& catalog, const Settings& settings) {
	const std::optional<Endpoint> current = endpointOf(catalog, settings);
	if(!current || !current->provider->shown) return {{"provider", ""}, {"default", ""}, {"models", json::array()}};
	json models = json::array();
	for(const Model& model : current->provider->models) models.push_back({{"id", model.id}, {"name", model.name}});
	if(!current->model.empty() && !current->provider->lists(current->model)) // the person's own, e.g. a live one
		models.insert(models.begin(), json{{"id", current->model}, {"name", current->model}});
	return {{"provider", current->provider->name}, {"default", current->model}, {"models", std::move(models)}};
}

void registerAssistantModels(affresco::Screen& screen, const std::filesystem::path& dataDir) {
	auto catalog = std::make_shared<const Catalog>(Catalog::standard());
	screen.onCall("assistant.models", [catalog, dataDir](const std::string&) {
		return assistantModels(*catalog, Settings::load(dataDir)).dump(); // reads only: it changes nothing
	});
}

void registerCalls(affresco::Screen& screen, const std::filesystem::path& dataDir, CallsOptions options) {
	auto shared = std::make_shared<Shared>();
	shared->dataDir = dataDir;
	shared->catalog = Catalog::standard();
	if(!options.keys) options.keys = systemKeys();
	shared->options = std::move(options);

	screen.onCall("intelligence.providers", [shared](const std::string&) { return providersJson(*shared).dump(); });
	screen.onCall("intelligence.stageKey", [shared](const std::string& argumentsJson) {
		const json arguments = json::parse(argumentsJson, nullptr, false);
		const Provider& provider = providerOf(*shared, arguments);
		std::string key = argument(arguments, "key");
		if(key.size() > 4096) throw std::runtime_error("the key is too long");
		const std::scoped_lock lock(shared->mutex);
		if(key.empty()) shared->staged.erase(provider.id);
		else shared->staged[provider.id] = std::move(key);
		return std::string("true");
	});
	screen.onCall("intelligence.save", [shared](const std::string& argumentsJson) {
		return save(*shared, json::parse(argumentsJson, nullptr, false)).dump();
	});
	screen.onCall("intelligence.forget", [shared](const std::string& argumentsJson) {
		const json arguments = json::parse(argumentsJson, nullptr, false);
		const Provider& provider = providerOf(*shared, arguments);
		shared->options.keys->erase(keyName(provider.id));
		{
			const std::scoped_lock lock(shared->mutex);
			shared->staged.erase(provider.id);
		}
		if(shared->options.changed) shared->options.changed();
		return providersJson(*shared).dump();
	});
	screen.onCall("intelligence.openKeyHelp", [shared](const std::string& argumentsJson) {
		// Only the catalogue's own page of a provider: a screen can't make it open a URL of its choosing
		const json arguments = json::parse(argumentsJson, nullptr, false);
		const Provider& provider = providerOf(*shared, arguments);
		if(provider.keyHelp.starts_with("https://") && affresco::net::isSignInPage(provider.keyHelp))
			affresco::net::openInBrowser(provider.keyHelp);
		return std::string("true");
	});
	screen.onAsyncCall("intelligence.models", [shared](const std::string& argumentsJson, affresco::CallProgress& call) {
		return models(*shared, json::parse(argumentsJson, nullptr, false), call).dump();
	});
	screen.onAsyncCall("intelligence.test", [shared](const std::string&, affresco::CallProgress& call) {
		return test(*shared, call).dump();
	});
}

HostAssistant::HostAssistant(affresco::Executor& executor, std::filesystem::path dataDir,
                             std::filesystem::path guideFolder)
    : executor_(executor), dataDir_(std::move(dataDir)), guideFolder_(std::move(guideFolder)) {}

bool HostAssistant::attach(affresco::Screen& screen) {
	if(agent_) return true;
	try {
		AgentOptions options;
		options.dataDir = dataDir_;
		options.catalog = Catalog::standard();
		options.guide = readFormatGuide(guideFolder_);
		options.keys = systemKeys();
		options.hiddenProviders = hiddenProvidersAllowed();
		agent_ = std::make_unique<ProviderAgent>(executor_, std::move(options));
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_launcher: the assistant's agent can't start: " << error.what() << '\n';
		return false;
	}
	screen.connect(*agent_, "agent");
	registerAssistantModels(screen, dataDir_);
	return true;
}

void HostAssistant::enableSettings(affresco::Screen& screen) { registerCalls(screen, dataDir_); }

void askOnStart(affresco::Executor& executor, affresco::Screen& screen, affresco::Window& window, std::string text) {
	executor.post([&screen, &window, text = std::move(text)] {
		screen.runAction("assistant.start", window.root(), json{{"text", text}}.dump());
	});
}

} // namespace affresco::intelligence
