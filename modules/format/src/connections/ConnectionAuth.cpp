/// @file
/// @brief A connection's "auth" read and applied (ConnectionAuth.hpp).

#include "ConnectionAuth.hpp"
#include "SectionRegistry.hpp"
#include "NodeKeys.hpp"

#include <format>
#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace affresco::format {

namespace {

/// @brief Whether a URL is http:// to the loopback. The host is compared whole: what follows it must be a port, a
/// path, a query or nothing, so "http://localhost.example.com" and "http://127.0.0.1.example.com" are not.
/// @param url The URL.
/// @return Whether it reaches only this machine.
bool isLoopbackUrl(std::string_view url) {
	constexpr std::array<std::string_view, 3> hosts{"http://127.0.0.1", "http://localhost", "http://[::1]"};
	return std::ranges::any_of(hosts, [url](std::string_view host) {
		return url.starts_with(host) && (url.size() == host.size() || std::string_view(":/?").contains(url[host.size()]));
	});
}

/// @brief Reads one of a provider's URLs: literal https:// (http:// on the loopback), with no fragment, user name or
/// spaces, at most 2048 bytes.
/// @param value The value.
/// @param at Its place.
/// @return The URL.
std::string readEndpoint(const json& value, const std::string& at) {
	const std::string url = expect::string(value, at);
	if(!url.starts_with("https://") && !isLoopbackUrl(url))
		expect::fail(at, "an https:// URL (http:// reaches only this machine)");
	if(url.size() > 2048 || url.contains("{{") || url.find_first_of("#@ \r\n\"<>") != std::string::npos)
		expect::fail(at, "a literal URL with no fragment, user name or spaces, at most 2048 bytes");
	return url;
}

/// @brief Reads a short printable word (a client id, a scope): no spaces, quotes or controls.
/// @param value The value.
/// @param limit Its largest size.
/// @param at Its place.
/// @return The word.
std::string readWord(const json& value, std::size_t limit, const std::string& at) {
	const std::string word = expect::string(value, at);
	const bool printable = std::ranges::all_of(word, [](char c) {
		return std::isgraph(static_cast<unsigned char>(c)) && c != '"' && c != '\\';
	});
	if(word.empty() || word.size() > limit || !printable || word.contains("{{"))
		expect::fail(at, "literal text of 1 to " + std::to_string(limit) + " characters, with no spaces or quotes");
	return word;
}

/// @brief Reads a provider's flow and checks that it has the URLs that flow needs.
/// @param value The provider's declaration.
/// @param at Its place.
/// @return "browser" (the default) or "device".
std::string readFlow(const json& value, const std::string& at) {
	const std::string flow = value.contains("flow") ? expect::string(value["flow"], at + ".flow") : "browser";
	if(flow != "browser" && flow != "device") expect::fail(at + ".flow", R"("browser" (the default) or "device")");
	if(!value.contains("tokenUrl") || !value.contains("clientId"))
		expect::fail(at, R"(a sign-in needs a "tokenUrl" and a "clientId")");
	const char* needed = flow == "browser" ? "signInUrl" : "deviceUrl";
	if(!value.contains(needed)) expect::fail(at, "the " + flow + " flow needs a \"" + needed + "\"");
	return flow;
}

/// @brief Reads a provider's scopes: at most 32 words.
/// @param value The value.
/// @param at Its place.
/// @return The scopes.
std::vector<std::string> readScopes(const json& value, const std::string& at) {
	if(!value.is_array() || value.size() > 32) expect::fail(at, "a list of at most 32 scopes");
	std::vector<std::string> scopes;
	scopes.reserve(value.size());
	for(std::size_t index = 0; index < value.size(); ++index)
		scopes.push_back(readWord(value[index], 128, at + "." + std::to_string(index)));
	return scopes;
}

} // namespace

identity::Provider readProvider(const json& value, const std::string& at) {
	static constexpr std::array<std::string_view, 8> keys = {"flow",     "signInUrl", "deviceUrl", "tokenUrl",
	                                                         "clientId", "scopes",    "issuer",    "note"};
	identity::Provider provider;
	for(const auto& [key, item] : expect::object(value, at).items())
		if(std::ranges::find(keys, key) == keys.end())
			expect::fail(concatenate(at, ".", key),
			             "unknown property (expected flow, signInUrl, deviceUrl, tokenUrl, clientId, "
			             "scopes, issuer, note; never a client secret)");
	provider.flow = readFlow(value, at);
	if(value.contains("signInUrl")) provider.signInUrl = readEndpoint(value["signInUrl"], at + ".signInUrl");
	if(value.contains("deviceUrl")) provider.deviceUrl = readEndpoint(value["deviceUrl"], at + ".deviceUrl");
	provider.tokenUrl = readEndpoint(value["tokenUrl"], at + ".tokenUrl");
	provider.clientId = readWord(value["clientId"], 256, at + ".clientId");
	if(value.contains("issuer")) provider.issuer = readEndpoint(value["issuer"], at + ".issuer");
	if(value.contains("scopes")) provider.scopes = readScopes(value["scopes"], at + ".scopes");
	if(value.contains("note")) static_cast<void>(expect::string(value["note"], at + ".note"));
	return provider;
}

void linkSignIns(ConnectionMap& connections, const json& section, const std::string& at) {
	for(auto& [name, connection] : connections) {
		const json& auth = section[name].contains("auth") ? section[name]["auth"] : json();
		if(auth.is_object()) connection.authName = name;
		if(!auth.is_string()) continue;
		const std::string shared = auth.get<std::string>();
		const auto found = connections.find(shared);
		if(found == connections.end() || !found->second.provider)
			expect::fail(std::format("{}.{}.auth", at, name), "the name of a connection with its own \"auth\" object");
		connection.authName = shared;
	}
}

std::vector<std::pair<std::string, identity::Provider>> declaredProviders(const ConnectionMap& connections) {
	std::vector<std::pair<std::string, identity::Provider>> providers;
	for(const auto& [name, connection] : connections)
		if(connection.provider) {
			identity::Provider provider = *connection.provider;
			provider.sentTo = connection.url; // a kept sign-in is restored only for the same place (SignIn::declare())
			providers.emplace_back(name, std::move(provider));
		}
	return providers;
}

std::optional<std::string> authorize(const ConnectionSpec& connection, const identity::Identity* identity,
                                     network::Headers& headers) {
	if(connection.authName.empty()) return std::nullopt;
	const auto token = identity ? identity->bearer(connection.authName) : std::nullopt;
	if(!token) return "not signed in to '" + connection.authName + "'";
	headers.emplace_back("Authorization", "Bearer " + *token);
	return std::nullopt;
}

} // namespace affresco::format
