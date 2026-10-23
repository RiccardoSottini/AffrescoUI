/// @file
/// @brief The Debug Adapter Protocol's messages (DapProtocol.hpp): read, bounded, placed and completed.

#include "dap/DapProtocol.hpp"

#include "Providers.hpp"

#include <array>
#include <exception>
#include <system_error>
#include <utility>

namespace affresco::services::dap {

namespace {

/// @brief Cuts text to at most a size, on a UTF-8 character's boundary.
/// @param text The text.
/// @param most The size.
/// @return The text, cut.
std::string cut(std::string text, std::size_t most) {
	if(text.size() <= most) return text;
	std::size_t end = most;
	while(end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) --end;
	text.resize(end);
	return text;
}

/// @brief Cuts a string member of an object, if there is one.
/// @param object The object.
/// @param key The member.
/// @param most Its largest size.
void cutMember(json& object, const char* key, std::size_t most) {
	if(!object.is_object()) return;
	const auto found = object.find(key);
	if(found == object.end() || !found->is_string()) return;
	std::string text = found->get<std::string>();
	if(text.size() > most) *found = cut(std::move(text), most);
}

/// @brief Cuts a list member of an object to a number of items; `truncated` says it was.
/// @param object The object.
/// @param key The member.
/// @param most How many items at most.
/// @return The list, or nullptr if there is none.
json* cutList(json& object, const char* key, std::size_t most) {
	if(!object.is_object()) return nullptr;
	const auto found = object.find(key);
	if(found == object.end() || !found->is_array()) return nullptr;
	if(found->size() > most) {
		found->erase(found->begin() + static_cast<std::ptrdiff_t>(most), found->end());
		object["truncated"] = true; // can move the members, so the list is found again
		return &object[key];
	}
	return &*found;
}

/// @brief Bounds a `variables` (or `scopes`) list: its names, values and types cut.
/// @param list The list.
void boundVariables(json& list) {
	for(json& variable : list) {
		cutMember(variable, "name", longestText);
		cutMember(variable, "value", longestValue);
		cutMember(variable, "type", longestText);
		cutMember(variable, "evaluateName", longestText);
	}
}

/// @brief Bounds a `stackTrace` answer's frames: their names cut, their sources placed.
/// @param frames The frames.
/// @param root The granted folder.
void boundFrames(json& frames, const std::filesystem::path& root) {
	for(json& frame : frames) {
		cutMember(frame, "name", longestText);
		if(frame.is_object() && frame.contains("source")) place(frame["source"], root);
	}
}

/// @brief Bounds a list of breakpoints: their messages cut, their sources placed.
/// @param breakpoints The breakpoints.
/// @param root The granted folder.
void boundBreakpoints(json& breakpoints, const std::filesystem::path& root) {
	for(json& breakpoint : breakpoints) {
		cutMember(breakpoint, "message", longestText);
		if(breakpoint.is_object() && breakpoint.contains("source")) place(breakpoint["source"], root);
	}
}

/// @brief Replaces `${workspaceFolder}` in every text of a value.
/// @param value The value (changed in place).
/// @param folder The folder's path.
/// @param depth How deep the walk is (it stops at 32).
void substituteFolder(json& value, const std::string& folder, int depth = 0) {
	static constexpr std::string_view variable = "${workspaceFolder}";
	if(depth > 32) return;
	if(value.is_string()) {
		std::string text = value.get<std::string>();
		for(std::size_t at = 0; (at = text.find(variable, at)) != std::string::npos; at += folder.size())
			text.replace(at, variable.size(), folder);
		value = std::move(text);
	} else if(value.is_structured()) {
		for(json& item : value) substituteFolder(item, folder, depth + 1);
	}
}

/// @brief Why the adapter refused a request: the `format` of the body's `error`, else the response's `message`,
/// else a sentence naming the command.
/// @param message The response message.
/// @param response The response read from it.
/// @return The reason, bounded and UTF-8.
std::string refusal(const json& message, const Response& response) {
	const json& body = response.body;
	const bool formatted =
	    body.is_object() && body.contains("error") && body["error"].is_object() && body["error"].contains("format");
	std::string reason = formatted ? textOf(body["error"]["format"], "") : textOf(message.value("message", json()), "");
	if(reason.empty()) reason = "the debug adapter refused '" + cut(response.command, 100) + "'";
	return reason;
}

/// @brief Reads the members every response has.
/// @param message The message.
/// @return The response, or why it is malformed.
Message readResponse(const json& message) {
	const auto requestSeq = message.find("request_seq");
	const auto success = message.find("success");
	const auto command = message.find("command");
	if(requestSeq == message.end() || !requestSeq->is_number_integer())
		return std::string("a response has no request_seq");
	if(success == message.end() || !success->is_boolean()) return std::string("a response has no success");
	if(command == message.end() || !command->is_string()) return std::string("a response has no command");
	Response response{requestSeq->get<std::int64_t>(),
	                  command->get<std::string>(),
	                  success->get<bool>(),
	                  {},
	                  message.value("body", json())};
	if(!response.success) response.message = refusal(message, response);
	return response;
}

/// @brief Reads the members an event has besides those every message has.
/// @param message The message.
/// @return The event, or why it is malformed.
Message readEvent(const json& message) {
	const auto event = message.find("event");
	if(event == message.end() || !event->is_string()) return std::string("an event has no name");
	return Event{event->get<std::string>(), message.value("body", json())};
}

/// @brief Reads the members a reverse request (from the adapter) has besides those every message has.
/// @param message The message.
/// @param seq Its sequence number.
/// @return The request, or why it is malformed.
Message readRequest(const json& message, std::int64_t seq) {
	const auto command = message.find("command");
	if(command == message.end() || !command->is_string()) return std::string("a request has no command");
	return Request{seq, command->get<std::string>(), message.value("arguments", json())};
}

} // namespace

std::string textOf(const json& value, std::string_view fallback) {
	if(!value.is_string()) return std::string(fallback);
	const std::string text = cut(value.get<std::string>(), longestText);
	return isUtf8(text) ? text : std::string(fallback);
}

Message readMessage(std::string_view body) {
	json message;
	try {
		message = parseUntrusted(body);
	} catch(const std::exception&) {
		return std::string("a message isn't JSON");
	}
	if(!message.is_object()) return std::string("a message isn't an object");
	const auto type = message.find("type");
	if(type == message.end() || !type->is_string()) return std::string("a message has no type");
	const auto seq = message.find("seq");
	if(seq == message.end() || !seq->is_number_integer()) return std::string("a message has no seq");
	const std::string& kind = type->get_ref<const std::string&>();
	if(kind == "response") return readResponse(message);
	if(kind == "event") return readEvent(message);
	if(kind == "request") return readRequest(message, seq->get<std::int64_t>());
	return std::string("a message's type isn't request, response or event");
}

json requestMessage(std::int64_t seq, std::string_view command, const json& arguments) {
	json message = {{"seq", seq}, {"type", "request"}, {"command", command}};
	if(!arguments.is_null()) message["arguments"] = arguments;
	return message;
}

json responseMessage(std::int64_t seq, const Request& request, const std::optional<json>& value,
                     const std::string& error) {
	json message = {{"seq", seq},
	                {"type", "response"},
	                {"request_seq", request.seq},
	                {"command", request.command},
	                {"success", value.has_value()}};
	if(value && !value->is_null()) message["body"] = *value;
	if(!value) message["message"] = error.empty() ? std::string("refused") : cut(error, longestText);
	return message;
}

void place(json& source, const std::filesystem::path& root) {
	if(!source.is_object()) return;
	cutMember(source, "name", longestText);
	const auto path = source.find("path");
	if(path == source.end() || !path->is_string()) return;
	if(path->get_ref<const std::string&>().size() > longestText || !isUtf8(path->get_ref<const std::string&>())) {
		source.erase("path"); // a screen can't open what it can't name
		source["inside"] = false;
		return;
	}
	const std::string& text = path->get_ref<const std::string&>();
	const std::filesystem::path given{std::u8string(text.begin(), text.end())};
	std::error_code error;
	const std::filesystem::path resolved = root.empty() || !given.is_absolute()
	                                           ? std::filesystem::path()
	                                           : std::filesystem::weakly_canonical(given, error);
	const std::filesystem::path relative = resolved.empty() || error ? resolved : resolved.lexically_relative(root);
	const bool inside = !relative.empty() && *relative.begin() != "..";
	source["inside"] = inside;
	if(inside) {
		const std::u8string generic = relative.generic_u8string();
		source["relative"] = std::string(generic.begin(), generic.end());
	}
}

namespace {

/// @brief Places the source of each item of a list that has one.
/// @param list The list.
/// @param root The granted folder.
void placeEach(json& list, const std::filesystem::path& root) {
	for(json& item : list)
		if(item.is_object() && item.contains("source")) place(item["source"], root);
}

/// @brief Bounds the lists of a response about the program's state: threads, frames, scopes and variables.
/// @param command The request's command.
/// @param body The body (changed in place).
/// @param root The granted folder.
/// @return Whether the command was one of them.
bool boundState(std::string_view command, json& body, const std::filesystem::path& root) {
	if(command == "threads") {
		if(json* threads = cutList(body, "threads", mostThreads))
			for(json& thread : *threads) cutMember(thread, "name", longestText);
	} else if(command == "stackTrace") {
		if(json* frames = cutList(body, "stackFrames", mostFrames)) boundFrames(*frames, root);
	} else if(command == "scopes") {
		if(json* scopes = cutList(body, "scopes", mostVariables)) {
			boundVariables(*scopes);
			placeEach(*scopes, root);
		}
	} else if(command == "variables") {
		if(json* variables = cutList(body, "variables", mostVariables)) boundVariables(*variables);
	} else {
		return false;
	}
	return true;
}

} // namespace

json boundResponse(std::string_view command, json body, const std::filesystem::path& root) {
	if(!body.is_object()) return body;
	if(boundState(command, body, root)) return body;
	if(command == "setBreakpoints" || command == "setFunctionBreakpoints" || command == "setExceptionBreakpoints" ||
	   command == "setDataBreakpoints") {
		if(json* breakpoints = cutList(body, "breakpoints", mostBreakpoints)) boundBreakpoints(*breakpoints, root);
	} else if(command == "completions") {
		(void)cutList(body, "targets", mostCompletions);
	} else if(command == "loadedSources") {
		if(json* sources = cutList(body, "sources", mostBreakpoints))
			for(json& source : *sources) place(source, root);
	}
	cutMember(body, "result", longestValue); // evaluate
	cutMember(body, "value", longestValue);  // setVariable, setExpression
	return body;
}

json boundEvent(std::string_view event, json body, const std::filesystem::path& root) {
	if(!body.is_object()) return body;
	if(event == "output") {
		cutMember(body, "output", longestOutput);
		if(body.contains("source")) place(body["source"], root);
	} else if(event == "breakpoint") {
		if(body.contains("breakpoint") && body["breakpoint"].is_object()) {
			json& breakpoint = body["breakpoint"];
			cutMember(breakpoint, "message", longestText);
			if(breakpoint.contains("source")) place(breakpoint["source"], root);
		}
	} else if(event == "loadedSource" && body.contains("source")) {
		place(body["source"], root);
	}
	cutMember(body, "description", longestText); // stopped
	cutMember(body, "text", longestText);
	cutMember(body, "name", longestText);
	return body;
}

namespace {

/// @brief Makes a relative `source.path` of a request's arguments absolute, under the granted folder.
/// @param arguments The arguments (changed in place).
/// @param root The granted folder (canonical), or empty.
/// @return Why the request is refused (a relative path with no folder, or one that leaves it), or std::nullopt.
std::optional<std::string> resolveSourcePath(json& arguments, const std::filesystem::path& root) {
	const auto source = arguments.find("source");
	if(source == arguments.end() || !source->is_object()) return std::nullopt;
	const auto path = source->find("path");
	if(path == source->end() || !path->is_string()) return std::nullopt;
	const std::string& text = path->get_ref<const std::string&>();
	if(std::filesystem::path(std::u8string(text.begin(), text.end())).is_absolute()) return std::nullopt;
	if(root.empty()) return "a relative source path needs the service to have a root";
	const std::optional<std::filesystem::path> inside = underRoot(root, text);
	if(!inside) return "the source path " + cut(text, 200) + " leaves the folder";
	const std::u8string absolute = inside->u8string();
	*path = std::string(absolute.begin(), absolute.end());
	return std::nullopt;
}

/// @brief Makes a `variables` or `stackTrace` request ask for one page at most, so an answer stays bounded.
/// @param command The command.
/// @param arguments Its arguments (changed in place).
void limitPage(std::string_view command, json& arguments) {
	const char* page = command == "variables" ? "count" : command == "stackTrace" ? "levels" : nullptr;
	if(!page) return;
	const json asked = arguments.value(page, json());
	const std::int64_t most = command == "variables" ? std::int64_t{mostVariables} : std::int64_t{mostFrames};
	if(!asked.is_number_integer() || asked.get<std::int64_t>() <= 0 || asked.get<std::int64_t>() > most)
		arguments[page] = most;
}

} // namespace

std::optional<std::string> prepareArguments(std::string_view command, json& arguments,
                                            const std::filesystem::path& root) {
	if(!arguments.is_object()) return std::nullopt;
	if((command == "launch" || command == "attach") && !root.empty()) {
		const std::u8string folder = root.u8string();
		substituteFolder(arguments, std::string(folder.begin(), folder.end()));
	}
	if(std::optional<std::string> refused = resolveSourcePath(arguments, root)) return refused;
	limitPage(command, arguments);
	return std::nullopt;
}

std::optional<std::size_t> OutputBudget::admit(std::size_t bytes, Clock::time_point now) noexcept {
	if(now - second_ >= std::chrono::seconds(1)) {
		second_ = now;
		events_ = 0;
		bytes_ = 0;
	}
	if(events_ + 1 > outputEventsPerSecond || bytes_ + bytes > outputBytesPerSecond) {
		++dropped_;
		return std::nullopt;
	}
	++events_;
	bytes_ += bytes;
	return std::exchange(dropped_, 0);
}

} // namespace affresco::services::dap
