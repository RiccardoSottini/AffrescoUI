/// @file
/// @brief The server side of a remote peer's opening: the upgrade request, its answers and the "auth" message.

#include "peers/PeerHandshake.hpp"

#include "http/Http.hpp"
#include "websocket/WebSocketFrames.hpp"

#include "affresco/core/Json.hpp"

#include <algorithm>

namespace affresco::net::peer {

namespace {

/// @brief The headers of an upgrade request that matter, as found.
struct Fields {
	std::optional<std::string> upgrade;       ///< Upgrade.
	std::optional<std::string> connection;    ///< Connection.
	std::optional<std::string> version;       ///< Sec-WebSocket-Version.
	std::optional<std::string> key;           ///< Sec-WebSocket-Key.
	std::optional<std::string> origin;        ///< Origin.
	std::optional<std::string> authorization; ///< Authorization.
	bool repeated = false;                    ///< One of them came twice.
};

/// @brief Removes spaces and tabs at both ends.
/// @param text The text.
/// @return The trimmed text.
std::string_view trim(std::string_view text) noexcept {
	while(!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
	while(!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
	return text;
}

/// @brief Keeps a header if it is one of the fields.
/// @param fields The fields.
/// @param name Its name.
/// @param value Its value, trimmed.
void keep(Fields& fields, std::string_view name, std::string_view value) {
	const std::pair<std::string_view, std::optional<std::string>*> slots[] = {
	    {"upgrade", &fields.upgrade},   {"connection", &fields.connection},
	    {"sec-websocket-version", &fields.version}, {"sec-websocket-key", &fields.key},
	    {"origin", &fields.origin},     {"authorization", &fields.authorization}};
	for(const auto& [field, slot] : slots) {
		if(!equalsIgnoringCase(name, field)) continue;
		if(slot->has_value()) fields.repeated = true;
		*slot = std::string(value);
		return;
	}
}

/// @brief Reads the header lines after the request line.
/// @param lines The head after the request line.
/// @param fields Receives the fields.
/// @return An error, or empty.
std::string readHeaders(std::string_view lines, Fields& fields) {
	std::size_t count = 0;
	while(!lines.empty()) {
		const std::size_t end = lines.find("\r\n");
		const std::string_view line = lines.substr(0, end);
		lines.remove_prefix(end == std::string_view::npos ? lines.size() : end + 2);
		if(line.empty()) break;
		if(++count > maxHeaderLines) return "too many header lines";
		const std::size_t colon = line.find(':');
		if(colon == std::string_view::npos || colon == 0 || trim(line.substr(0, colon)).size() != colon)
			return "a header line is malformed";
		if(line.find('\0') != std::string_view::npos) return "a header line has a NUL byte";
		keep(fields, line.substr(0, colon), trim(line.substr(colon + 1)));
	}
	return {};
}

/// @brief A refusal.
/// @param status The HTTP status.
/// @param reason Why.
/// @return The upgrade that says so.
Upgrade refuse(int status, std::string reason) {
	Upgrade upgrade;
	upgrade.status = status;
	upgrade.reason = std::move(reason);
	return upgrade;
}

/// @brief The reason phrase of a status.
/// @param status The status.
/// @return Its phrase.
std::string_view phrase(int status) noexcept {
	switch(status) {
	case 400: return "Bad Request";
	case 401: return "Unauthorized";
	case 403: return "Forbidden";
	case 408: return "Request Timeout";
	case 426: return "Upgrade Required";
	case 429: return "Too Many Requests";
	case 431: return "Request Header Fields Too Large";
	case 503: return "Service Unavailable";
	default: return "Error";
	}
}

/// @brief Whether the fields ask to upgrade to WebSocket.
/// @param fields The fields.
bool asksForWebSocket(const Fields& fields) {
	return fields.upgrade && equalsIgnoringCase(*fields.upgrade, "websocket") && fields.connection &&
	       hasToken(*fields.connection, "upgrade");
}

/// @brief Takes the bearer token of an Authorization field into an upgrade.
/// @param upgrade The upgrade.
/// @param authorization The field's value.
/// @return The upgrade with it, or its refusal.
Upgrade withToken(Upgrade upgrade, std::string_view authorization) {
	if(authorization.size() < 7 || !equalsIgnoringCase(authorization.substr(0, 7), "bearer "))
		return refuse(401, "Authorization must be a bearer token");
	const std::string_view token = trim(authorization.substr(7));
	if(token.empty() || token.size() > maxTokenBytes) return refuse(401, "the bearer token is empty or too long");
	upgrade.token = std::string(token);
	return upgrade;
}

/// @brief Checks the fields of an upgrade, and takes its key, origin and token.
/// @param fields The fields.
/// @return The upgrade, or its refusal.
Upgrade checkFields(Fields fields) {
	if(fields.repeated) return refuse(400, "a WebSocket header came twice");
	if(!asksForWebSocket(fields)) return refuse(426, "this address only takes WebSocket connections");
	if(!fields.version || *fields.version != "13") return refuse(426, "only WebSocket version 13 is spoken");
	if(!fields.key || fields.key->size() != 24) return refuse(400, "Sec-WebSocket-Key is missing or malformed");
	Upgrade upgrade;
	upgrade.key = std::move(*fields.key);
	upgrade.hasOrigin = fields.origin.has_value();
	upgrade.origin = fields.origin.value_or("");
	if(fields.authorization) return withToken(std::move(upgrade), *fields.authorization);
	return upgrade;
}

} // namespace

std::optional<std::size_t> headEnd(std::string_view buffer) noexcept {
	const std::size_t end = buffer.find("\r\n\r\n");
	if(end == std::string_view::npos) return std::nullopt;
	return end + 4;
}

Upgrade parseUpgrade(std::string_view head) {
	if(head.size() > maxHandshakeBytes) return refuse(431, "the request headers are too large");
	const std::size_t lineEnd = head.find("\r\n");
	if(lineEnd == std::string_view::npos) return refuse(400, "the request line is malformed");
	const std::string_view line = head.substr(0, lineEnd);
	const std::size_t first = line.find(' ');
	const std::size_t last = line.rfind(' ');
	if(first == std::string_view::npos || first == last || line.substr(0, first) != "GET" ||
	   line.substr(last + 1) != "HTTP/1.1" || line.substr(first + 1, 1) != "/")
		return refuse(400, "the request line must be GET /<path> HTTP/1.1");
	Fields fields;
	if(std::string error = readHeaders(head.substr(lineEnd + 2), fields); !error.empty())
		return refuse(error == "too many header lines" ? 431 : 400, std::move(error));
	return checkFields(std::move(fields));
}

bool originAllowed(std::string_view origin, const std::vector<std::string>& allowed) noexcept {
	return std::ranges::any_of(allowed, [origin](const std::string& entry) { return equalsIgnoringCase(entry, origin); });
}

std::string acceptResponse(std::string_view key) {
	return "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " +
	       acceptKey(key) + "\r\n\r\n";
}

std::string refusalResponse(int status, std::string_view reason) {
	std::string body(reason);
	body += '\n';
	return "HTTP/1.1 " + std::to_string(status) + " " + std::string(phrase(status)) +
	       "\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: " + std::to_string(body.size()) +
	       "\r\nConnection: close\r\n\r\n" + body;
}

std::optional<std::string> authToken(std::string_view message) {
	if(message.size() > maxTokenBytes + 64) return std::nullopt;
	json value;
	try {
		value = parseUntrusted(message);
	} catch(const std::exception&) {
		return std::nullopt;
	}
	if(!value.is_object() || value.value("type", json()) != "auth") return std::nullopt;
	const auto token = value.find("token");
	if(token == value.end() || !token->is_string()) return std::nullopt;
	auto text = token->get<std::string>();
	if(text.empty() || text.size() > maxTokenBytes) return std::nullopt;
	return text;
}

} // namespace affresco::net::peer
