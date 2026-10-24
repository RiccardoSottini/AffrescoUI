#pragma once

/// @file
/// @brief What the API providers are sent and answer: the Messages API (anthropic) and Chat Completions (openai,
/// openai-compatible), as requests built and responses read without a transport, so tests check their shapes.

#include "intelligence/Settings.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/core/Network.hpp>

#include <string>
#include <vector>

namespace examples::intelligence {

/// @brief A tool the model may call.
struct ToolSpec {
	std::string name;            ///< E.g. "get_screen".
	std::string description;     ///< What it does, for the model.
	affresco::json parameters;   ///< Its arguments, as a JSON Schema object.
};

/// @brief A call the model asked for.
struct ToolCall {
	std::string id;          ///< The provider's id for it, which its result repeats.
	std::string name;        ///< The tool.
	affresco::json input;    ///< Its arguments (an object; {} if the model gave malformed ones).
};

/// @brief The result of a tool call, for the model.
struct ToolResult {
	std::string id;      ///< The call's id.
	std::string content; ///< What the tool gave, as text.
	bool error = false;  ///< Whether the call failed.
};

/// @brief One answer of the model.
struct Turn {
	std::string text;               ///< Its text, the parts joined.
	std::vector<ToolCall> calls;    ///< The tools it calls, in order.
	std::string stop;               ///< Why it stopped: "end", "tools", "length", "refusal" or the provider's word.
	affresco::json message;         ///< The assistant message, as the conversation keeps it.
	long long outputTokens = 0;     ///< The tokens it wrote, if the provider says.
};

/// @brief A conversation in a provider's own shape.
struct Conversation {
	std::string system;                 ///< The system prompt.
	affresco::json messages = affresco::json::array(); ///< The messages, in the provider's shape.
};

/// @brief The most a model writes in one answer.
inline constexpr int maxOutputTokens = 8192;

/// @brief The request that lists a provider's models.
/// @param endpoint The provider (an API one), with its key.
/// @return The request.
[[nodiscard]] affresco::network::Request modelsRequest(const Endpoint& endpoint);
/// @brief Reads a models list: the chat models only, for openai.
/// @param kind The provider's kind.
/// @param body The response's body.
/// @return The models, sorted by id; empty if the body isn't a list.
[[nodiscard]] std::vector<Model> parseModels(Kind kind, const std::string& body);

/// @brief Adds the person's message to a conversation.
/// @param kind The provider's kind.
/// @param conversation The conversation.
/// @param text The message.
void addUser(Kind kind, Conversation& conversation, const std::string& text);
/// @brief Adds the model's answer (its whole message, tool calls included) to a conversation.
/// @param conversation The conversation.
/// @param turn The answer.
void addAssistant(Conversation& conversation, const Turn& turn);
/// @brief Adds the results of a turn's tool calls, all in one message (anthropic) or one each (openai).
/// @param kind The provider's kind.
/// @param conversation The conversation.
/// @param results The results, one per call, in order.
void addResults(Kind kind, Conversation& conversation, const std::vector<ToolResult>& results);

/// @brief The request for the model's next answer.
/// @param endpoint The provider (an API one), with its key and model.
/// @param conversation The conversation so far.
/// @param tools The tools it may call (may be empty).
/// @return The request: a POST with a JSON body and a timeout of two minutes.
[[nodiscard]] affresco::network::Request chatRequest(const Endpoint& endpoint, const Conversation& conversation,
                                                     const std::vector<ToolSpec>& tools);
/// @brief Reads the model's answer.
/// @param kind The provider's kind.
/// @param body The response's body.
/// @return The answer.
/// @throws std::runtime_error If the body isn't an answer.
[[nodiscard]] Turn parseTurn(Kind kind, const std::string& body);

/// @brief A failure said for people, without a secret: a refused key is said so, a provider's own error message is
/// cut short and has anything that looks like a key taken out (and the key itself, if given).
/// @param failure The failure.
/// @param providerName The provider, for people.
/// @param key The key that was sent, or "".
/// @return The message.
[[nodiscard]] std::string describeFailure(const affresco::network::Failure& failure, const std::string& providerName,
                                          const std::string& key);
/// @brief Takes out of a text anything that looks like an API key (sk-..., a long run of key characters) and the key.
/// @param text The text.
/// @param key The key, or "".
/// @return The text, with "[hidden]" in their place.
[[nodiscard]] std::string redact(std::string text, const std::string& key);

} // namespace examples::intelligence
