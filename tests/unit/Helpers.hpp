#pragma once

/// @file
/// @brief What several unit tests share: a small screen with permissions, and a Channel kept in memory.

#include <affresco/core/Channel.hpp>
#include <affresco/core/Json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace affresco::test {

/// @brief A screen with three nodes and permissions: the studio changes anything and reviews; the agent may change
/// #board only; nobody may change #statusBar.
/// @return The document.
inline json permissionsScreen() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"state": { "count": 0 },
		"permissions": {
			"allow": ["studio", "agent(#board)"],
			"deny": ["*(#statusBar)"],
			"reviewers": ["studio"]
		},
		"root": { "type": "panel", "id": "main", "children": [
			{ "type": "panel", "id": "board", "color": "#ffffff" },
			{ "type": "label", "id": "title", "text": "Tasks" },
			{ "type": "panel", "id": "statusBar", "color": "#202020" }
		] }
	})json");
}

/// @brief A Channel that keeps what is sent, for a test to read; receive() plays a message from the other end.
class MemoryChannel final : public Channel {
public:
	/// @brief Keeps a message.
	/// @param message The message.
	void send(std::string_view message) override { sent.push_back(json::parse(message)); }

	/// @brief Delivers a message as if the other end had sent it.
	/// @param line The message, without a line break.
	void receive(const std::string& line) { onMessage(line); }

	std::vector<json> sent; ///< Every message sent, parsed, oldest first.
};

} // namespace affresco::test
