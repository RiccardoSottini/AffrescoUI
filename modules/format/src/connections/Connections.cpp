/// @file
/// @brief format::Connections: requests built from their declarations, WebSocket messages and actions, previews
/// answered from mocks.

#include "Connections.hpp"
#include "ConnectionAuth.hpp"

#include "Engine.hpp"
#include "Expression.hpp"

#include <format>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Percent-encodes a text for a URL path segment or a query (RFC 3986: only unreserved characters stay).
/// @param text The text.
/// @return It, encoded.
std::string encode(std::string_view text) {
	static constexpr char hex[] = "0123456789ABCDEF";
	std::string out;
	out.reserve(text.size());
	for(const char c : text) {
		const auto byte = static_cast<unsigned char>(c);
		const bool unreserved = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
		                        (byte >= '0' && byte <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
		if(unreserved) out += c;
		else {
			out += '%';
			out += hex[byte >> 4];
			out += hex[byte & 15];
		}
	}
	return out;
}

/// @brief A scalar as text for a URL: a string as it is, a number or a boolean written out, a list or an object as
/// JSON.
/// @param value The value.
/// @return The text.
std::string text(const json& value) {
	return value.is_string() ? value.get<std::string>() : value.dump(-1, ' ', false, json::error_handler_t::replace);
}

/// @brief Evaluates a declared value with the call's arguments as its only variables: a declaration can't read the
/// state, so what leaves the machine is only what a step passed in `with`.
/// @param value The value ("{{project}}", or an object holding templates).
/// @param with The arguments.
/// @param at Its place, for errors.
/// @return The value.
/// @throws DocumentError If an expression is wrong.
json evaluateWith(const json& value, const json& with, const std::string& at) {
	const json empty = json::object();
	const Scope scope{nullptr, nullptr, with.is_object() ? with : json::object(), {}, {}};
	WorkBudget budget{stepsPerEvent, operationsPerEvent, bytesPerEvent};
	const Context context{empty, empty, empty, empty, scope, empty, nullptr, nullptr, &budget};
	return evaluate(value, context, at);
}

/// @brief Fills a path's `{name}` placeholders with the arguments, each one path segment ("a/b" becomes "a%2Fb", and
/// "." or ".." can't climb).
/// @param path The declared path.
/// @param with The arguments.
/// @param at Its place, for errors.
/// @return The path.
/// @throws DocumentError If a placeholder has no value, an empty one, or a list or an object.
std::string expandPath(const std::string& path, const json& with, const std::string& at) {
	std::string out;
	for(std::size_t i = 0; i < path.size();) {
		const std::size_t close = path[i] == '{' ? path.find('}', i) : std::string::npos;
		if(close == std::string::npos) {
			out += path[i++];
			continue;
		}
		const std::string name = path.substr(i + 1, close - i - 1);
		const auto found = with.is_object() ? with.find(name) : with.end();
		const std::string value = found != with.end() && found->is_primitive() && !found->is_null() ? text(*found) : "";
		if(value.empty())
			throw DocumentError(std::format(R"({}: "with" has no value for {{{}}} (a text or a number))", at, name));
		out += value == "." ? "%2E" : value == ".." ? "%2E%2E" : encode(value);
		i = close + 1;
	}
	return out;
}

/// @brief Writes an object as `name=value&...`, skipping null values.
/// @param values The object.
/// @return The text.
std::string formEncode(const json& values) {
	std::string out;
	for(const auto& [name, value] : values.items()) {
		if(value.is_null()) continue;
		if(!out.empty()) out += '&';
		out += encode(name) + "=" + encode(text(value));
	}
	return out;
}

/// @brief Reads a body: JSON if it is, else its text; empty is null.
/// @param body The body.
/// @return The value.
json parseBody(const std::string& body) {
	if(body.empty()) return nullptr;
	try {
		return parseUntrusted(body);
	} catch(const json::exception&) {
	} catch(const DocumentError&) {
	}
	return body;
}

/// @brief The part of a response a declaration's "result" names, or the whole of it.
/// @param body The response body.
/// @param result The pointer, or none.
/// @return The result (null if the body has no such part).
json pick(json body, const std::optional<json::json_pointer>& result) {
	if(!result) return body;
	try {
		return body.at(*result);
	} catch(const json::exception&) {
		return nullptr;
	}
}

/// @brief Whether a message's type can name an action: 1 to 64 letters, digits, '_' or '-'.
/// @param type The type.
/// @return Whether it can.
bool actionType(const std::string& type) {
	if(type.empty() || type.size() > 64) return false;
	return std::ranges::all_of(type, [](char c) {
		const auto byte = static_cast<unsigned char>(c);
		return std::isalnum(byte) != 0 || c == '_' || c == '-';
	});
}

/// @brief Reads a message received on a socket, if it names an action.
/// @param text The message.
/// @return It, when it is a JSON object whose "type" can name an action other than "open", "closed" and "error";
/// else nothing.
std::optional<json> namingMessage(const std::string& text) {
	json message;
	try {
		message = parseUntrusted(text);
	} catch(const json::exception&) {
		return std::nullopt; // a message that isn't JSON names no action
	} catch(const DocumentError&) {
		return std::nullopt;
	}
	if(!message.is_object() || !message["type"].is_string()) return std::nullopt;
	const std::string type = message["type"].get<std::string>();
	if(!actionType(type) || type == "open" || type == "closed" || type == "error") return std::nullopt;
	return message;
}

} // namespace

Connections::~Connections() {
	alive_.reset();
	running_.clear();
	connections_.clear();
	retired_.clear();
}

void Connections::setNetwork(network::Network* network) {
	network_ = network;
	if(!section_) return;
	const json section = std::move(*section_);
	section_.reset();
	for(const std::string& refused : load(section)) engine_.reportRefused("AffrescoUI screen: connections: " + refused);
}

void Connections::closeAll() {
	++generation_;
	for(auto& [number, running] : running_) {
		if(!running.ended) running.call->fail("cancelled", "the connections were loaded again", nullptr);
		running.stop.reset();
		retire(std::move(running.transfer));
	}
	running_.clear();
	for(auto& [name, live] : connections_) retire(std::move(live.socket));
	connections_.clear();
}

std::vector<std::string> Connections::load(const json& section) {
	if(section_ && *section_ == section) return {};
	closeAll();
	section_ = section;
	ConnectionMap parsed;
	try {
		parsed = parseConnections(section);
	} catch(const DocumentError& error) {
		changed();
		return {error.what()};
	}
	std::vector<std::string> refused;
	for(auto& [name, spec] : parsed) {
		Live live;
		live.spec = std::move(spec);
		for(const auto& [operation, declared] : live.spec.operations) live.methods.emplace(operation, declared.spec());
		if(!network_) live.error = "connections." + name + ": the host gives this screen no network";
		else if(network_->check(live.spec.url))
			live.error = "connections." + name + ".url: '" + live.spec.url + "' isn't a host this app may connect to";
		if(live.error) {
			live.state = "refused";
			refused.push_back(*live.error);
		}
		Live& placed = connections_.emplace(name, std::move(live)).first->second;
		if(placed.spec.socket && placed.state != "refused") open(placed);
	}
	declareSignIns();
	changed();
	return refused;
}

void Connections::declareSignIns() {
	if(!engine_.identity) return;
	ConnectionMap specs;
	for(const auto& [name, live] : connections_) specs.emplace(name, live.spec);
	engine_.identity->declare(declaredProviders(specs));
}

void Connections::authChanged() {
	for(auto& [name, live] : connections_) {
		if(!live.spec.socket || live.spec.authName.empty() || live.state == "refused") continue;
		const bool signedIn = engine_.identity && engine_.identity->bearer(live.spec.authName);
		if(signedIn && live.state == "signedOut") open(live);
		else if(!signedIn && live.socket) {
			retire(std::move(live.socket));
			live.state = "signedOut";
			live.error = "not signed in to '" + live.spec.authName + "'";
			changed();
		}
	}
}

const services::MethodSpec* Connections::method(std::string_view service, std::string_view name) const {
	const auto live = connections_.find(service);
	if(live == connections_.end()) return nullptr;
	const auto found = live->second.methods.find(name);
	return found == live->second.methods.end() ? nullptr : &found->second;
}

std::optional<std::string> Connections::invoke(std::string_view service, std::string_view name,
                                               std::shared_ptr<services::Call> call) {
	return start(service, name, call, false);
}

std::optional<std::string> Connections::subscribe(std::string_view service, std::string_view name,
                                                  std::shared_ptr<services::Call> call) {
	return start(service, name, call, true);
}

json Connections::status() const {
	json out = json::object();
	for(const auto& [name, live] : connections_) {
		const bool online = live.spec.socket ? live.state == "open" : live.state != "refused" && live.online;
		out[name] = {{"online", online},
		             {"pending", live.pending},
		             {"state", live.state},
		             {"error", live.error ? json(*live.error) : json()}};
	}
	return out;
}

std::optional<std::string> Connections::start(std::string_view service, std::string_view name,
                                              const std::shared_ptr<services::Call>& call, bool stream) {
	const auto found = connections_.find(service);
	if(found == connections_.end()) return "no connection '" + std::string(service) + "'";
	Live& live = found->second;
	const auto declared = live.spec.operations.find(name);
	if(declared == live.spec.operations.end())
		return "connection '" + live.spec.name + "' declares no '" + std::string(name) + "'";
	const Operation& operation = declared->second;
	if(engine_.versions.previewing() && previewed(operation, *call, stream)) return std::nullopt;
	if(live.state == "refused") return *live.error;
	if(!admit(live)) return "too many requests on '" + live.spec.name + "' (at most 50 a second)";
	if(operation.message) {
		sendMessage(live, operation, *call);
		return std::nullopt;
	}
	network::Request request;
	try {
		request = build(live, operation, call->arguments());
	} catch(const DocumentError& error) {
		call->fail("wrong", error.what(), nullptr);
		return std::nullopt;
	}
	if(const auto signedOut = authorize(live.spec, engine_.identity, request.headers)) {
		call->fail("signedOut", *signedOut, nullptr);
		return std::nullopt;
	}
	const std::uint64_t number = next_++;
	Running& running = running_[number];
	running.call = call;
	running.connection = live.spec.name;
	++live.pending;
	running.transfer = network_->start(std::move(request), handlers(number, operation.result, stream));
	running.stop.emplace(call->stopToken(), std::function<void()>([this, number, alive = std::weak_ptr<bool>(alive_)] {
		                     if(alive.lock()) end(number); // its call ended (cancelled, timed out): stop it here too
	                     }));
	changed();
	return std::nullopt;
}

bool Connections::previewed(const Operation& operation, services::Call& call, bool stream) const {
	if(operation.mock.is_object()) {
		const int status = operation.mock.value("status", 200);
		const json body = operation.mock.contains("body") ? operation.mock["body"] : json();
		if(status >= 400)
			call.fail("status", "the server answered " + std::to_string(status) + " (mock)",
			          {{"status", status}, {"body", body}});
		else call.finish(pick(body, operation.result));
		return true;
	}
	if(!operation.message && operation.effect == services::Effect::Read && !stream) return false;
	call.fail("preview", "while a proposal is previewed, a connection only reads", nullptr);
	return true;
}

network::Handlers Connections::handlers(std::uint64_t number, const std::optional<json::json_pointer>& result,
                                        bool stream) {
	const auto alive = std::weak_ptr<bool>(alive_);
	network::Handlers out;
	out.done = [this, alive, number, result](network::Response response) {
		if(!alive.lock()) return;
		Running* running = unended(number);
		if(!running) return;
		if(const auto at = connections_.find(running->connection); at != connections_.end()) {
			at->second.online = true;
			at->second.error.reset();
		}
		running->call->finish(pick(parseBody(response.body), result));
		end(number);
	};
	out.failed = [this, alive, number](network::Failure failure) {
		if(!alive.lock()) return;
		Running* running = unended(number);
		if(!running) return;
		if(const auto at = connections_.find(running->connection); at != connections_.end()) {
			at->second.online = failure.code == "status";
			at->second.error = failure.message;
			// a refused token: the sign-in refreshes once, else signs out
			if(failure.status == 401 && engine_.identity && !at->second.spec.authName.empty())
				engine_.identity->refused(at->second.spec.authName);
		}
		running->call->fail(failure.code, failure.message,
		                    {{"status", failure.status}, {"body", parseBody(failure.body)}});
		end(number);
	};
	if(stream)
		out.event = [this, alive, number](network::StreamEvent event) {
			if(!alive.lock()) return;
			if(Running* running = unended(number))
				running->call->event({{"type", event.type}, {"data", parseBody(event.data)}, {"id", event.id}});
		};
	return out;
}

Connections::Running* Connections::unended(std::uint64_t number) {
	const auto it = running_.find(number);
	return it == running_.end() || it->second.ended ? nullptr : &it->second;
}

void Connections::sendMessage(Live& live, const Operation& operation, services::Call& call) {
	if(live.state != "open" || !live.socket)
		return call.fail("network", "connection '" + live.spec.name + "' isn't open", nullptr);
	json value;
	try {
		value = evaluateWith(operation.value, call.arguments(),
		                     "connections." + live.spec.name + ".messages." + operation.name);
	} catch(const DocumentError& error) {
		return call.fail("wrong", error.what(), nullptr);
	}
	const std::string message = value.dump(-1, ' ', false, json::error_handler_t::replace);
	if(message.size() > maxMessageBytes) return call.fail("tooLarge", "the message is over 8 MiB", nullptr);
	if(!live.socket->send(message)) return call.fail("network", "the message couldn't be sent", nullptr);
	call.finish(nullptr);
}

network::Request Connections::build(const Live& live, const Operation& operation, const json& with) const {
	const std::string at = "connections." + live.spec.name + ".operations." + operation.name;
	network::Request request;
	request.method = operation.method;
	request.url = live.spec.url + expandPath(operation.path, with, at + ".path");
	if(operation.query.is_object()) {
		const std::string query = formEncode(evaluateWith(operation.query, with, at + ".query"));
		if(!query.empty()) request.url += "?" + query;
	}
	request.headers = live.spec.headers;
	request.headers.insert(request.headers.end(), operation.headers.begin(), operation.headers.end());
	if(!operation.value.is_null()) {
		const json body = evaluateWith(operation.value, with, at + ".body");
		if(operation.form && !body.is_object()) throw DocumentError(at + ".body: a form is an object");
		request.body = operation.form ? formEncode(body) : body.dump(-1, ' ', false, json::error_handler_t::replace);
		request.headers.emplace_back("Content-Type",
		                             operation.form ? "application/x-www-form-urlencoded" : "application/json");
	}
	request.timeout = std::chrono::milliseconds(operation.timeout.value_or(live.spec.timeout));
	if(operation.effect == services::Effect::Read) { // a write is never sent twice
		request.retries = live.spec.retries;
		request.retryAfter = std::chrono::milliseconds(live.spec.retryAfter);
	}
	request.stream = operation.stream;
	return request;
}

bool Connections::admit(Live& live) {
	const double now = engine_.now();
	while(!live.started.empty() && now - live.started.front() >= 1.0) live.started.pop_front();
	if(live.started.size() >= maxRequestsPerSecond) return false;
	live.started.push_back(now);
	return true;
}

void Connections::end(std::uint64_t number) {
	Running* running = unended(number);
	if(!running) return;
	running->ended = true;
	if(const auto live = connections_.find(running->connection); live != connections_.end() && live->second.pending)
		--live->second.pending;
	engine_.scheduler.defer([this, number, alive = std::weak_ptr<bool>(alive_)] {
		if(alive.lock()) running_.erase(number); // destroying the transfer cancels it, if it still runs
	});
	changed();
}

void Connections::open(Live& live) {
	network::Headers headers = live.spec.headers;
	if(const auto signedOut = authorize(live.spec, engine_.identity, headers)) {
		live.state = "signedOut"; // opened by authChanged() once signed in
		live.error = *signedOut;
		changed();
		return;
	}
	live.state = "connecting";
	live.socket = network_->connect(live.spec.url, std::move(headers), socketHandlers(live.spec.name));
	changed();
}

network::SocketHandlers Connections::socketHandlers(const std::string& name) {
	const std::uint64_t generation = generation_;
	const auto alive = std::weak_ptr<bool>(alive_);
	network::SocketHandlers events;
	events.opened = [this, alive, name, generation] {
		if(!alive.lock()) return;
		Live* found = find(name, generation);
		if(!found) return;
		found->state = "open";
		found->error.reset();
		changed();
		runAction(name, "open", json::object());
	};
	events.message = [this, alive, name, generation](const std::string& text) {
		if(!alive.lock() || !find(name, generation) || text.size() > maxMessageBytes) return;
		if(const auto message = namingMessage(text)) runAction(name, (*message)["type"].get<std::string>(), *message);
	};
	events.closed = [this, alive, name, generation](std::optional<network::Failure> failure) {
		if(alive.lock() && find(name, generation)) closed(name, failure);
	};
	return events;
}

void Connections::closed(const std::string& name, const std::optional<network::Failure>& failure) {
	Live* live = find(name, generation_);
	if(!live) return;
	live->state = "closed";
	if(failure) live->error = failure->message;
	retire(std::move(live->socket));
	if(live->spec.reconnect > 0)
		engine_.scheduler.once(live->spec.reconnect, [this, name, generation = generation_,
		                                              alive = std::weak_ptr<bool>(alive_)] {
			if(!alive.lock()) return;
			if(Live* again = find(name, generation); again && again->state == "closed") open(*again);
		});
	changed();
	// the actions come last: they may load the connections again, which ends `live`
	if(failure) runAction(name, "error", {{"code", failure->code}, {"message", failure->message}});
	runAction(name, "closed", json::object());
}

void Connections::runAction(const std::string& connection, const std::string& what, const json& event) {
	const std::string action = connection + ":" + what;
	if(!engine_.window || !engine_.owner.hasAction(action)) return;
	engine_.runner.handle(json(action), engine_.window->root(), event, Scope{}, "connections." + connection);
	if(engine_.document.stateChanged()) engine_.reconciler.requestStateUpdate();
}

Connections::Live* Connections::find(const std::string& name, std::uint64_t generation) {
	if(generation != generation_) return nullptr;
	const auto found = connections_.find(name);
	return found == connections_.end() ? nullptr : &found->second;
}

void Connections::retire(std::unique_ptr<network::Transfer> transfer) {
	if(!transfer) return;
	retired_.push_back(std::move(transfer));
	if(retired_.size() > 1) return; // a clear is already scheduled
	engine_.scheduler.defer([this, alive = std::weak_ptr<bool>(alive_)] {
		if(alive.lock()) retired_.clear();
	});
}

void Connections::changed() const {
	if(onStatus_) onStatus_();
}

} // namespace affresco::format
