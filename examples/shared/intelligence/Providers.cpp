/// @file
/// @brief The API providers' requests and answers (Providers.hpp).

#include "intelligence/Providers.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <stdexcept>
#include <string_view>

namespace examples::intelligence {

using affresco::json;
namespace network = affresco::network;

namespace {

/// The Messages API's version header.
constexpr const char* anthropicVersion = "2023-06-01";

/// @brief The headers of a provider's requests: its key and content type.
/// @param endpoint The provider.
/// @param json Whether the request has a JSON body.
/// @return The headers.
network::Headers headersOf(const Endpoint& endpoint, bool json) {
	network::Headers headers;
	if(json) headers.emplace_back("content-type", "application/json");
	headers.emplace_back("accept", "application/json");
	if(endpoint.provider->kind == Kind::Anthropic) {
		headers.emplace_back("anthropic-version", anthropicVersion);
		if(!endpoint.key.empty()) headers.emplace_back("x-api-key", endpoint.key);
	} else if(!endpoint.key.empty()) {
		headers.emplace_back("authorization", "Bearer " + endpoint.key);
	}
	return headers;
}

/// @brief The URL of a path under a provider's base: the Messages API's base has no "/v1", Chat Completions' has.
/// @param endpoint The provider.
/// @param path E.g. "/models".
/// @return The URL.
std::string urlOf(const Endpoint& endpoint, std::string_view path) {
	return endpoint.baseUrl + (endpoint.provider->kind == Kind::Anthropic ? "/v1" : "") + std::string(path);
}

/// @brief Whether an OpenAI model id names a chat model (not embeddings, speech, images or moderation).
/// @param id The id.
/// @return Whether it does.
bool isChatModel(const std::string& id) {
	constexpr std::array<std::string_view, 5> starts{"gpt-", "o1", "o3", "o4", "chatgpt-"};
	constexpr std::array<std::string_view, 10> never{"embedding", "whisper",    "tts",   "dall-e",     "audio",
	                                                 "realtime",  "transcribe", "image", "moderation", "search"};
	if(std::ranges::none_of(starts, [&](std::string_view start) { return id.starts_with(start); })) return false;
	return std::ranges::none_of(never, [&](std::string_view word) { return id.find(word) != std::string::npos; });
}

/// @brief The tools in the Messages API's shape.
/// @param tools The tools.
/// @return `[{name, description, input_schema}]`.
json anthropicTools(const std::vector<ToolSpec>& tools) {
	json list = json::array();
	for(const ToolSpec& tool : tools)
		list.push_back({{"name", tool.name}, {"description", tool.description}, {"input_schema", tool.parameters}});
	return list;
}

/// @brief The tools in Chat Completions' shape.
/// @param tools The tools.
/// @return `[{type: "function", function: {name, description, parameters}}]`.
json openaiTools(const std::vector<ToolSpec>& tools) {
	json list = json::array();
	for(const ToolSpec& tool : tools)
		list.push_back(
		    {{"type", "function"},
			 {"function", {{"name", tool.name}, {"description", tool.description}, {"parameters", tool.parameters}}}});
	return list;
}

/// @brief Reads a Messages API answer.
/// @param answer Its body, parsed.
/// @return The turn.
Turn anthropicTurn(const json& answer) {
	if(!answer.is_object() || !answer.contains("content") || !answer["content"].is_array())
		throw std::runtime_error("the answer has no content");
	Turn turn;
	for(const json& block : answer["content"]) {
		const std::string type = block.value("type", "");
		if(type == "text") turn.text += (turn.text.empty() ? "" : "\n") + block.value("text", "");
		else if(type == "tool_use")
			turn.calls.push_back(
			    {block.value("id", ""), block.value("name", ""),
				 block.contains("input") && block["input"].is_object() ? block["input"] : json::object()});
	}
	const std::string stop = answer.value("stop_reason", "");
	turn.stop = stop == "end_turn" || stop == "stop_sequence" ? "end"
	            : stop == "tool_use"                          ? "tools"
	            : stop == "max_tokens"                        ? "length"
	                                                          : stop;
	turn.message = {{"role", "assistant"}, {"content", answer["content"]}}; // whole, thinking blocks included
	if(answer.contains("usage") && answer["usage"].is_object())
		turn.outputTokens = answer["usage"].value("output_tokens", 0LL);
	return turn;
}

/// @brief Reads a Chat Completions answer.
/// @param answer Its body, parsed.
/// @return The turn.
Turn openaiTurn(const json& answer) {
	if(!answer.is_object() || !answer.contains("choices") || !answer["choices"].is_array() ||
	   answer["choices"].empty() || !answer["choices"][0].contains("message"))
		throw std::runtime_error("the answer has no choices");
	const json& choice = answer["choices"][0];
	const json& message = choice["message"];
	Turn turn;
	if(message.contains("content") && message["content"].is_string()) turn.text = message["content"].get<std::string>();
	json kept = {{"role", "assistant"}, {"content", turn.text}};
	if(message.contains("tool_calls") && message["tool_calls"].is_array() && !message["tool_calls"].empty()) {
		kept["tool_calls"] = message["tool_calls"];
		for(const json& call : message["tool_calls"]) {
			const json function = call.value("function", json::object());
			json input = json::parse(function.value("arguments", std::string("{}")), nullptr, false);
			if(!input.is_object()) input = json::object();
			turn.calls.push_back({call.value("id", ""), function.value("name", ""), input});
		}
	}
	const std::string stop = choice.contains("finish_reason") && choice["finish_reason"].is_string()
	                             ? choice["finish_reason"].get<std::string>()
	                             : "";
	turn.stop = stop == "stop" ? "end" : stop == "tool_calls" ? "tools" : stop == "length" ? "length" : stop;
	if(!turn.calls.empty()) turn.stop = "tools";
	turn.message = kept;
	if(answer.contains("usage") && answer["usage"].is_object())
		turn.outputTokens = answer["usage"].value("completion_tokens", 0LL);
	return turn;
}

/// @brief A provider's own error message in an error body, if it has one.
/// @param body The body.
/// @return E.g. "model not found", or "".
std::string errorMessageOf(const std::string& body) {
	const json parsed = json::parse(body, nullptr, false);
	if(!parsed.is_object()) return "";
	const json error = parsed.value("error", json());
	if(error.is_object() && error.contains("message") && error["message"].is_string())
		return error["message"].get<std::string>();
	if(error.is_string()) return error.get<std::string>();
	return parsed.contains("message") && parsed["message"].is_string() ? parsed["message"].get<std::string>() : "";
}

/// @brief Whether a character may be part of a key.
/// @param c The character.
/// @return Whether it may.
bool keyCharacter(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_'; }

} // namespace

network::Request modelsRequest(const Endpoint& endpoint) {
	network::Request request;
	request.method = "GET";
	request.url = urlOf(endpoint, "/models") + (endpoint.provider->kind == Kind::Anthropic ? "?limit=100" : "");
	request.headers = headersOf(endpoint, false);
	request.timeout = std::chrono::milliseconds(15000);
	request.retries = 1;
	request.maxBodyBytes = std::size_t{2} << 20;
	return request;
}

std::vector<Model> parseModels(Kind kind, const std::string& body) {
	const json parsed = json::parse(body, nullptr, false);
	std::vector<Model> models;
	if(!parsed.is_object() || !parsed.contains("data") || !parsed["data"].is_array()) return models;
	for(const json& entry : parsed["data"]) {
		if(!entry.is_object() || !entry.contains("id") || !entry["id"].is_string()) continue;
		const std::string id = entry["id"].get<std::string>();
		if(id.empty() || id.size() > 200 || (kind == Kind::OpenAI && !isChatModel(id))) continue;
		const std::string name = entry.contains("display_name") && entry["display_name"].is_string()
		                             ? entry["display_name"].get<std::string>()
		                             : id;
		models.push_back({id, name});
		if(models.size() >= 500) break;
	}
	std::ranges::sort(models, {}, &Model::id);
	return models;
}

void addUser(Kind kind, Conversation& conversation, const std::string& text) {
	(void)kind; // both shapes take a plain text message
	conversation.messages.push_back({{"role", "user"}, {"content", text}});
}

void addAssistant(Conversation& conversation, const Turn& turn) { conversation.messages.push_back(turn.message); }

void addResults(Kind kind, Conversation& conversation, const std::vector<ToolResult>& results) {
	if(kind == Kind::Anthropic) {
		json blocks = json::array();
		for(const ToolResult& result : results)
			blocks.push_back({{"type", "tool_result"},
			                  {"tool_use_id", result.id},
			                  {"content", result.content},
			                  {"is_error", result.error}});
		conversation.messages.push_back({{"role", "user"}, {"content", blocks}});
		return;
	}
	for(const ToolResult& result : results)
		conversation.messages.push_back({{"role", "tool"},
		                                 {"tool_call_id", result.id},
		                                 {"content", result.error ? "Error: " + result.content : result.content}});
}

network::Request chatRequest(const Endpoint& endpoint, const Conversation& conversation,
                             const std::vector<ToolSpec>& tools) {
	const Kind kind = endpoint.provider->kind;
	json body;
	if(kind == Kind::Anthropic) {
		body = {{"model", endpoint.model},
		        {"max_tokens", maxOutputTokens},
		        {"system", conversation.system},
		        {"messages", conversation.messages}};
		if(!tools.empty()) body["tools"] = anthropicTools(tools);
	} else {
		json messages = json::array({{{"role", "system"}, {"content", conversation.system}}});
		for(const json& message : conversation.messages) messages.push_back(message);
		body = {{"model", endpoint.model}, {"messages", messages}};
		// OpenAI's newer models take max_completion_tokens only; compatible servers (Ollama, LM Studio) max_tokens
		body[kind == Kind::OpenAI ? "max_completion_tokens" : "max_tokens"] = maxOutputTokens;
		if(!tools.empty()) body["tools"] = openaiTools(tools);
	}
	network::Request request;
	request.method = "POST";
	request.url = urlOf(endpoint, kind == Kind::Anthropic ? "/messages" : "/chat/completions");
	request.headers = headersOf(endpoint, true);
	request.body = body.dump(-1, ' ', false, json::error_handler_t::replace);
	request.timeout = std::chrono::milliseconds(120000);
	request.maxBodyBytes = std::size_t{4} << 20;
	return request;
}

Turn parseTurn(Kind kind, const std::string& body) {
	const json parsed = json::parse(body, nullptr, false);
	if(parsed.is_discarded()) throw std::runtime_error("the answer isn't JSON");
	return kind == Kind::Anthropic ? anthropicTurn(parsed) : openaiTurn(parsed);
}

std::string redact(std::string text, const std::string& key) {
	if(!key.empty())
		for(std::size_t at = text.find(key); at != std::string::npos; at = text.find(key, at))
			text.replace(at, key.size(), "[hidden]");
	// A part of the key echoed back (its first 8 characters, or all of a shorter one) hides the word it is in
	const std::string prefix = key.substr(0, std::min<std::size_t>(key.size(), 8));
	std::string out;
	for(std::size_t i = 0; i < text.size();) {
		std::size_t end = i;
		while(end < text.size() && keyCharacter(text[end])) ++end;
		const std::string_view word(text.data() + i, end - i);
		const bool looksLikeKey = word.starts_with("sk-") || word.starts_with("sk_") || word.size() >= 16 ||
		                          (!prefix.empty() && word.find(prefix) != std::string_view::npos);
		if(end > i) {
			out += looksLikeKey ? "[hidden]" : std::string(word);
			i = end;
		} else {
			out += text[i++];
		}
	}
	return out;
}

std::string describeFailure(const network::Failure& failure, const std::string& providerName, const std::string& key) {
	if(failure.code == "status") {
		if(failure.status == 401 || failure.status == 403)
			return std::format("{} refused the API key ({}). Check it in the AffrescoUI Launcher's Settings.",
			                   providerName, failure.status);
		if(failure.status == 429) return providerName + " is rate limiting the requests (429): try again in a moment.";
		std::string said = redact(errorMessageOf(failure.body), key);
		if(said.size() > 300) said = said.substr(0, 300) + "…";
		return std::format("{} answered {}{}", providerName, failure.status, said.empty() ? "." : ": " + said);
	}
	if(failure.code == "timeout") return providerName + " didn't answer in time.";
	if(failure.code == "refused") return "Can't reach " + providerName + ": " + redact(failure.message, key);
	if(failure.code == "tooLarge") return providerName + "'s answer was too large.";
	if(failure.code == "cancelled") return "Stopped.";
	return "Can't reach " + providerName + " (" + redact(failure.message, key) + ").";
}

} // namespace examples::intelligence
