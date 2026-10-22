/// @file
/// @brief The "connections" section read and checked (ConnectionSpec.hpp).

#include "ConnectionSpec.hpp"
#include "ConnectionAuth.hpp"
#include "SectionRegistry.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <string_view>

namespace affresco::format {

namespace {

/// @brief Whether two header names are the same, ignoring case.
/// @param a One.
/// @param b The other.
/// @return Whether they are.
bool sameName(std::string_view a, std::string_view b) {
	return std::ranges::equal(a, b, [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}

/// @brief Fails unless a key is one of the allowed ones.
/// @param key The key.
/// @param allowed The keys allowed.
/// @param at Its place.
template<std::size_t N>
void knownKey(const std::string& key, const std::array<std::string_view, N>& allowed, const std::string& at) {
	if(std::ranges::find(allowed, key) != allowed.end()) return;
	std::string list;
	for(const std::string_view name : allowed) list += (list.empty() ? "" : ", ") + std::string(name);
	expect::fail(at + "." + key, "unknown property (expected " + list + ")");
}

/// @brief Fails unless a name can be used after a "." in a step: not empty, no "." or ":", at most 64 bytes.
/// @param name The name.
/// @param at Its place.
void checkName(const std::string& name, const std::string& at) {
	if(name.empty() || name.size() > 64 || name.contains('.') || name.contains(':') || name.contains("{{"))
		expect::fail(at, "a name of 1 to 64 characters, with no \".\", \":\" or {{ }}");
}

/// @brief Reads a whole number in a range.
/// @param value The value.
/// @param low The smallest.
/// @param high The largest.
/// @param at Its place.
/// @return The number.
int bounded(const json& value, int low, int high, const std::string& at) {
	if(!value.is_number_integer() || value.get<long long>() < low || value.get<long long>() > high)
		expect::fail(at, std::format("a whole number from {} to {}", low, high));
	return value.get<int>();
}

/// @brief Reads literal headers: names that are HTTP tokens, values with no line breaks, and none a credential or one
/// the client sets.
/// @param value name -> text.
/// @param at Its place.
/// @return The headers.
network::Headers readHeaders(const json& value, const std::string& at) {
	static constexpr std::array<std::string_view, 6> forbidden = {"Authorization", "Cookie", "Host", "Content-Length",
	                                                              "Transfer-Encoding", "Proxy-Authorization"};
	network::Headers headers;
	for(const auto& [name, text] : expect::object(value, at).items()) {
		const std::string place = at + "." + name;
		const bool token = !name.empty() && name.size() <= 64 && std::ranges::all_of(name, [](char c) {
			return std::isalnum(static_cast<unsigned char>(c)) || std::string_view("!#$%&'*+-.^_`|~").contains(c);
		});
		if(!token) expect::fail(place, "a header name is letters, digits and - (at most 64)");
		for(const std::string_view banned : forbidden)
			if(sameName(name, banned))
				expect::fail(place, "the header " + std::string(banned) + " can't be declared (sign-in or the client sets it)");
		const std::string content = expect::string(text, place);
		if(content.size() > 4096 || content.contains("{{") || content.find_first_of("\r\n") != std::string::npos)
			expect::fail(place, "a header's value is literal text on one line, at most 4096 bytes");
		headers.emplace_back(name, content);
	}
	return headers;
}

/// @brief Reads an operation's `mock`: `{status, body}`.
/// @param value The value.
/// @param at Its place.
/// @return It.
json readMock(const json& value, const std::string& at) {
	for(const auto& [key, item] : expect::object(value, at).items())
		knownKey(key, std::array<std::string_view, 2>{"status", "body"}, at);
	return {{"status", value.contains("status") ? bounded(value["status"], 100, 599, at + ".status") : 200},
	        {"body", value.value("body", json())}};
}

/// @brief Reads an operation's path: "/" first, `{name}` placeholders, no query or fragment (those come from `query`).
/// @param value The value.
/// @param at Its place.
/// @return The path.
std::string readPath(const json& value, const std::string& at) {
	const std::string path = expect::string(value, at);
	if(!path.starts_with('/') || path.contains("{{") || path.find_first_of("?# \r\n") != std::string::npos)
		expect::fail(at, "a path starts with \"/\", with {name} for a value: \"/projects/{id}\" (no ?, # or spaces)");
	for(std::size_t open = path.find('{'); open != std::string::npos; open = path.find('{', open + 1)) {
		const std::size_t close = path.find('}', open);
		const std::string_view name = std::string_view(path).substr(open + 1, close - open - 1);
		if(close == std::string::npos || name.empty() || !std::ranges::all_of(name, [](char c) {
			   return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
		   }))
			expect::fail(at, "a placeholder is {name}, letters, digits and _");
	}
	return path;
}

/// @brief Reads one HTTP operation.
/// @param definition Its definition.
/// @param connection The connection's URL scheme is http(s).
/// @param at Its place.
/// @return It.
Operation readOperation(const json& definition, const std::string& at) {
	static constexpr std::array<std::string_view, 11> keys = {"method", "path",   "query",  "body",   "form", "headers",
	                                                          "effect", "stream", "result", "timeout", "mock"};
	expect::object(definition, at);
	Operation operation;
	for(const auto& [key, item] : definition.items()) knownKey(key, keys, at);
	if(!definition.contains("path")) expect::fail(at, "an operation needs a \"path\"");
	operation.path = readPath(definition["path"], at + ".path");
	operation.method = definition.contains("method") ? expect::string(definition["method"], at + ".method") : "GET";
	if(!std::ranges::contains(std::array<std::string_view, 6>{"GET", "HEAD", "POST", "PUT", "PATCH", "DELETE"},
	                          operation.method))
		expect::fail(at + ".method", "GET, HEAD, POST, PUT, PATCH or DELETE");
	if(definition.contains("query")) operation.query = expect::object(definition["query"], at + ".query");
	operation.value = definition.value("body", json());
	operation.form = definition.contains("form") && expect::boolean(definition["form"], at + ".form");
	if(operation.form && !operation.value.is_object()) expect::fail(at + ".form", "a form body is an object");
	if(definition.contains("headers")) operation.headers = readHeaders(definition["headers"], at + ".headers");
	const bool reads = operation.method == "GET" || operation.method == "HEAD";
	operation.effect = reads ? services::Effect::Read : services::Effect::Write;
	if(definition.contains("effect")) {
		const std::string effect = expect::string(definition["effect"], at + ".effect");
		if(effect != "read" && effect != "write") expect::fail(at + ".effect", "\"read\" or \"write\"");
		operation.effect = effect == "read" ? services::Effect::Read : services::Effect::Write;
	}
	if(definition.contains("stream")) {
		const std::string stream = expect::string(definition["stream"], at + ".stream");
		if(stream != "events" && stream != "lines") expect::fail(at + ".stream", "\"events\" or \"lines\"");
		operation.stream = stream == "events" ? network::Stream::Events : network::Stream::Lines;
	}
	if(definition.contains("result")) try {
			operation.result = json::json_pointer(expect::string(definition["result"], at + ".result"));
		} catch(const json::exception&) { expect::fail(at + ".result", "a JSON Pointer into the body: \"/data\""); }
	if(definition.contains("timeout"))
		operation.timeout = bounded(definition["timeout"], 1, maxConnectionTimeout, at + ".timeout");
	if(definition.contains("mock")) operation.mock = readMock(definition["mock"], at + ".mock");
	return operation;
}

/// @brief Reads a connection's operations or messages.
/// @param definition The connection's definition.
/// @param connection The connection, which receives them.
/// @param at Its place.
void readOperations(const json& definition, ConnectionSpec& connection, const std::string& at) {
	const char* key = connection.socket ? "messages" : "operations";
	if(definition.contains(connection.socket ? "operations" : "messages"))
		expect::fail(at, connection.socket ? "a WebSocket (ws://, wss://) has \"messages\", not \"operations\""
		                                   : "\"messages\" are for a WebSocket (ws://, wss://)");
	if(!definition.contains(key)) return;
	const json& list = expect::object(definition[key], at + "." + key);
	if(list.size() > maxOperations) expect::fail(at + "." + key, "at most 256");
	for(const auto& [name, item] : list.items()) {
		const std::string place = at + "." + key + "." + name;
		checkName(name, place);
		Operation operation;
		if(connection.socket) {
			operation.message = true;
			operation.effect = services::Effect::Write;
			operation.value = item;
		} else {
			operation = readOperation(item, place);
		}
		operation.name = name;
		connection.operations.emplace(name, std::move(operation));
	}
}

/// @brief Reads a connection's URL: literal, http(s) or ws(s), with no query, fragment or user name.
/// @param value The value.
/// @param connection The connection, which receives it.
/// @param at Its place.
void readUrl(const json& value, ConnectionSpec& connection, const std::string& at) {
	connection.url = expect::string(value, at);
	const std::string& url = connection.url;
	const bool web = url.starts_with("https://") || url.starts_with("http://");
	connection.socket = url.starts_with("wss://") || url.starts_with("ws://");
	if(!web && !connection.socket)
		expect::fail(at, "an https:// or wss:// URL (http:// and ws:// reach only this machine)");
	if(url.size() > 2048 || url.contains("{{") || url.find_first_of("?#@ \r\n") != std::string::npos)
		expect::fail(at, "a literal URL with no query, fragment, user name or spaces, at most 2048 bytes");
	while(connection.url.ends_with('/')) connection.url.pop_back(); // the paths start with "/"
}

/// @brief Reads one connection.
/// @param definition Its definition.
/// @param name Its name.
/// @param at Its place.
/// @return It.
ConnectionSpec readConnection(const json& definition, const std::string& name, const std::string& at) {
	static constexpr std::array<std::string_view, 9> keys = {"url",      "auth",       "headers",   "timeout", "retry",
	                                                         "operations", "messages", "reconnect", "note"};
	expect::object(definition, at);
	for(const auto& [key, item] : definition.items()) knownKey(key, keys, at);
	ConnectionSpec connection;
	connection.name = name;
	if(!definition.contains("url")) expect::fail(at, "a connection needs a \"url\"");
	readUrl(definition["url"], connection, at + ".url");
	if(definition.contains("auth") && !definition["auth"].is_object() && !definition["auth"].is_string())
		expect::fail(at + ".auth", "how to sign in, or the name of the connection whose sign-in it shares");
	if(definition.contains("auth") && definition["auth"].is_object())
		connection.provider = readProvider(definition["auth"], at + ".auth");
	if(definition.contains("headers")) connection.headers = readHeaders(definition["headers"], at + ".headers");
	if(definition.contains("timeout"))
		connection.timeout = bounded(definition["timeout"], 1, maxConnectionTimeout, at + ".timeout");
	if(definition.contains("retry")) {
		const json& retry = expect::object(definition["retry"], at + ".retry");
		for(const auto& [key, item] : retry.items())
			knownKey(key, std::array<std::string_view, 2>{"times", "after"}, at + ".retry");
		connection.retries = retry.contains("times") ? bounded(retry["times"], 0, maxRetries, at + ".retry.times") : 1;
		if(retry.contains("after")) connection.retryAfter = bounded(retry["after"], 1, 60000, at + ".retry.after");
	}
	if(definition.contains("reconnect")) {
		const json& reconnect = definition["reconnect"];
		if(!connection.socket) expect::fail(at + ".reconnect", "only a WebSocket (ws://, wss://) reconnects");
		if(reconnect.is_boolean()) connection.reconnect = reconnect.get<bool>() ? defaultReconnect : 0;
		else if(reconnect.is_object() && reconnect.size() == 1 && reconnect.contains("after"))
			connection.reconnect = bounded(reconnect["after"], 100, 3600000, at + ".reconnect.after");
		else expect::fail(at + ".reconnect", "true, or { \"after\": milliseconds }");
	}
	readOperations(definition, connection, at);
	return connection;
}

/// @brief The section's check: parseConnections(), the result dropped.
/// @param value The section.
/// @param name "connections".
void checkConnections(const json& value, const std::string& name) { static_cast<void>(parseConnections(value, name)); }

} // namespace

services::MethodSpec Operation::spec() const {
	services::MethodSpec out;
	out.name = name;
	out.effect = effect;
	out.streams = stream != network::Stream::None;
	out.about = message ? std::string("a WebSocket message") : method + " " + path;
	return out;
}

ConnectionMap parseConnections(const json& section, const std::string& at) {
	ConnectionMap connections;
	if(section.is_null()) return connections;
	if(expect::object(section, at).size() > maxConnections) expect::fail(at, "at most 64 connections");
	for(const auto& [name, definition] : section.items()) {
		const std::string place = std::format("{}.{}", at, name);
		checkName(name, place);
		connections.emplace(name, readConnection(definition, name, place));
	}
	linkSignIns(connections, section, at);
	return connections;
}

SectionType connectionsSection() { return {.name = "connections", .validate = checkConnections, .capability = true}; }

} // namespace affresco::format
