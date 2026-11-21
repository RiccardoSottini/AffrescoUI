/// @file
/// @brief The "command" group: what a peer's command run costs beyond the same steps run by the app. A board of cards
/// in its state, shown as a keyed list, has one card moved by an agent's runCommand (parsed, its arguments checked,
/// judged by the run rules, run under the agent's name, logged, answered with what changed) and by the same steps as
/// an action, each with its update and layout. "commands/list" is an agent's getCommands on Forge.

#include "Bench.hpp"

#include <affresco/core/Channel.hpp>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {

namespace {

/// The cards on the board, in its three sizes.
constexpr std::array<int, 3> boardSizes = {100, 400, 1600};

/// @brief A peer's end of a channel: keeps the last message the screen sent.
class Peer final : public Channel {
public:
	/// @brief Keeps a message. @param message It.
	void send(std::string_view message) override { last.assign(message); }
	/// @brief Delivers a message as the peer would. @param line It.
	void receive(const std::string& line) { onMessage(line); }

	std::string last; ///< What the screen sent last.
};

/// @brief A board of cards in its state, with "moveCard" as a command and "moveCardHere" as the app's own action.
/// @param cards How many.
/// @return The document.
json boardDocument(int cards) {
	json document = json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"permissions": { "allow": ["studio"], "reviewers": ["studio"], "run": { "allow": ["agent(moveCard)"] } },
		"actions": {
			"moveCard": { "title": "Move card",
				"arguments": { "card": { "type": "integer" }, "to": { "type": "string", "enum": ["todo", "doing", "done"] } },
				"do": [{ "set": "state.cards[card].column", "to": "{{to}}" }] },
			"moveCardHere": [{ "set": "state.cards[event.card].column", "to": "{{event.to}}" }]
		},
		"root": { "type": "panel", "children": [{ "type": "column", "children": [
			{ "type": "row", "repeat": { "for": "card", "in": "state.cards", "key": "{{card.id}}" }, "children": [
				{ "type": "label", "text": "{{card.title}}" }, { "type": "label", "text": "{{card.column}}" }
			] }
		] }] }
	})json");
	json list = json::array();
	for(int i = 0; i < cards; ++i)
		list.push_back({{"id", "c" + std::to_string(i)}, {"title", "Card " + std::to_string(i)}, {"column", "todo"}});
	document["state"]["cards"] = std::move(list);
	return document;
}

/// @brief Runs the updates the state changes queued, and lays the window out, without drawing.
/// @param window The window.
void settle(Window& window) {
	window.routeKey({Key::Unknown, 0, Action::Release, {}});
	static_cast<void>(window.panelAt({}));
}

/// @brief "command/<cards>": an agent's run of moveCard, and "command/<cards>/action": the same steps as an action.
/// One peer runs them all: its 25 runs and the warm-up fit in its 30 at once (PeerLimits::runBurst).
/// @param context The context.
/// @param cards How many cards.
/// @return The two workloads.
std::vector<Workload> boardWorkloads(Context& context, int cards) {
	const LoadedScreen loaded = context.load(boardDocument(cards), context.templateFolder("kanban"));
	const auto peer = std::make_shared<Peer>();
	loaded.screen.connect(*peer, "agent");
	const auto count = std::make_shared<int>(0);
	const auto target = [cards, count] {
		++*count;
		return json{{"card", (*count * 7919) % cards}, {"to", *count % 2 ? "done" : "todo"}};
	};
	Workload command{.name = "command/" + std::to_string(cards), .runs = 25};
	command.step = [loaded, peer, target] {
		peer->receive(json{{"type", "runCommand"}, {"id", 1}, {"command", "moveCard"}, {"arguments", target()}}.dump());
		settle(loaded.window);
		if(!peer->last.contains("\"ran\"")) throw std::runtime_error("moveCard: " + peer->last);
	};
	Workload action{.name = "command/" + std::to_string(cards) + "/action", .runs = 25};
	action.step = [loaded, target] {
		loaded.screen.runAction("moveCardHere", loaded.window.root(), target().dump());
		settle(loaded.window);
	};
	return {std::move(command), std::move(action)};
}

} // namespace

std::vector<Workload> commandWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const int cards : boardSizes) std::ranges::move(boardWorkloads(context, cards), std::back_inserter(workloads));
	const std::filesystem::path forge = context.examples() / "forge" / "json";
	Screen& screen = context.create(readFile(forge / "index.screen.json"), forge);
	const auto peer = std::make_shared<Peer>();
	screen.connect(*peer, "agent");
	Workload list{.name = "commands/list", .runs = 25};
	list.step = [peer] {
		peer->receive(R"({"type": "getCommands", "id": 1, "all": true})");
		if(!peer->last.contains("\"commands\"")) throw std::runtime_error("getCommands: " + peer->last);
	};
	list.report = [peer](json& result) { result["bytes"] = peer->last.size(); };
	workloads.push_back(std::move(list));
	return workloads;
}

} // namespace affresco::bench
